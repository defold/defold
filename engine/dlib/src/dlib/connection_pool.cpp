#include <stdlib.h>
#include <stdio.h>
#include "connection_pool.h"
#include "hashtable.h"
#include "array.h"
#include "time.h"
#include "log.h"
#include "mutex.h"
#include "hash.h"
#include "../axtls/ssl/os_port.h"
#include "../axtls/ssl/ssl.h"

namespace dmConnectionPool
{
    enum State
    {
        STATE_FREE = 0,
        STATE_CONNECTED = 1,
        STATE_INUSE = 2,
    };

    struct Connection
    {
        Connection()
        {
            Clear();
        }

        void Clear()
        {
            memset(this, 0, sizeof(*this));
            m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
            m_State = STATE_FREE;
        }

        uint16_t            m_Version;
        uint16_t            m_ReuseCount;
        dmhash_t            m_ID;
        dmSocket::Address   m_Address;
        uint16_t            m_Port;
        uint64_t            m_Expires;
        dmSocket::Socket    m_Socket;
        dmAxTls::SSL*       m_SSLConnection;
        State               m_State;
        uint16_t            m_WasShutdown:1;
    };

    static void DoClose(HPool pool, Connection* c);

    struct ConnectionPool
    {
        uint64_t            m_MaxKeepAlive;
        dmArray<Connection> m_Connections;
        uint16_t            m_NextVersion;
        dmAxTls::SSL_CTX*   m_SSLContext;
        dmMutex::Mutex      m_Mutex;

        uint16_t            m_AllowNewConnections:1;

        ConnectionPool(const Params* params)
        {
            uint32_t max_connections = params->m_MaxConnections;
            m_MaxKeepAlive = params->m_MaxKeepAlive;
            m_Mutex = dmMutex::New();

            m_Connections.SetCapacity(max_connections);
            m_Connections.SetSize(max_connections);

            for (uint32_t i = 0; i < max_connections; ++i) {
                m_Connections[i].Clear();
            }

            m_NextVersion = 0;

            // We set SSL_SERVER_VERIFY_LATER to handle self-signed certificates
            // We should manually and optionally verify certificates
            // with ssl_verify_cert() but it require that root-certs
            // are bundled in the engine and some kind of lazy loading
            // mechanism as we can't load every possible certificate for
            // every connections. It's possible to introspect the SSL object
            // to find out which certificates to load.

            // We set SSL_CONNECT_IN_PARTS to make the socket non blocking during the ssl handshake
            int options = SSL_SERVER_VERIFY_LATER | SSL_CONNECT_IN_PARTS;

            // NOTE: CONFIG_SSL_CTX_MUTEXING must be configured as the
            // ssl-context is shared among all threads
            // 0 is number of sessions to cache and currently arbitrary.
            // Since we don't use sessions (see Connect) we don't need any.
            m_SSLContext = dmAxTls::ssl_ctx_new(options, 0);

            // This is to block new connections when shutting down the pool.
            m_AllowNewConnections = 1;
        }

        ~ConnectionPool()
        {
            uint32_t n = m_Connections.Size();
            int in_use = 0;
            for (uint32_t i = 0; i < n; ++i) {
                Connection* c = &m_Connections[i];
                if (c->m_State == STATE_INUSE) {
                    in_use++;
                } else if (c->m_State == STATE_CONNECTED) {
                    DoClose(this, c);
                }
            }

            if (in_use > 0) {
                dmLogError("Leaking %d connections from connection pool", in_use);
            }

            dmAxTls::ssl_ctx_free(m_SSLContext);

            dmMutex::Delete(m_Mutex);
        }
    };

    Result New(const Params* params, HPool* pool)
    {
        *pool = new ConnectionPool(params);
        return RESULT_OK;
    }

    Result Delete(HPool pool)
    {
        delete pool;
        return RESULT_OK;
    }

    void SetMaxKeepAlive(HPool pool, uint32_t max_keep_alive)
    {
        pool->m_MaxKeepAlive = max_keep_alive;
    }

    void GetStats(HPool pool, Stats* stats)
    {
        DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);

        memset(stats, 0, sizeof(*stats));

        uint32_t n = pool->m_Connections.Size();
        for (uint32_t i = 0; i < n; ++i) {
            Connection* c = &pool->m_Connections[i];
            switch (c->m_State) {
            case STATE_FREE:
                stats->m_Free++;
                break;
            case STATE_CONNECTED:
                stats->m_Connected++;
                break;
            case STATE_INUSE:
                stats->m_InUse++;
                break;
            default:
                assert(false);
            }
        }
    }

    static dmhash_t CalculateConnectionID(dmSocket::Address address, uint16_t port, bool ssl)
    {
        HashState64 hs;
        dmHashInit64(&hs, false);
        dmHashUpdateBuffer64(&hs, &address, sizeof(address));
        dmHashUpdateBuffer64(&hs, &port, sizeof(port));
        dmHashUpdateBuffer64(&hs, &ssl, sizeof(ssl));

        return dmHashFinal64(&hs);
    }

    static HConnection MakeHandle(HPool pool, uint32_t index, Connection* c) {
        if (pool->m_NextVersion == 0) {
            pool->m_NextVersion = 1;
        }
        uint16_t v = pool->m_NextVersion;
        pool->m_NextVersion++;
        c->m_Version = v;
        return (v << 16) | (index & 0xffff);
    }

    static Connection* GetConnection(HPool pool, HConnection c) {
        uint16_t v = c >> 16;
        uint16_t i = c & 0xffff;

        Connection* ret = &pool->m_Connections[i];
        assert(ret->m_Version == v);
        return ret;
    }

    static bool FindConnection(HPool pool, dmhash_t id, dmSocket::Address address, uint16_t port, bool ssl, HConnection* connection)
    {
        uint32_t n = pool->m_Connections.Size();
        for (uint32_t i = 0; i < n; ++i) {
            Connection* c = &pool->m_Connections[i];
            if (c->m_State == STATE_CONNECTED && c->m_ID == id) {
                // We have to return a socket with the correct address family
                bool ipv4_match = address.m_family == dmSocket::DOMAIN_IPV4 && dmSocket::IsSocketIPv4(c->m_Socket);
                bool ipv6_match = address.m_family == dmSocket::DOMAIN_IPV6 && dmSocket::IsSocketIPv6(c->m_Socket);
                if (ipv4_match || ipv6_match)
                {
                    c->m_State = STATE_INUSE;
                    c->m_ReuseCount++;
                    *connection = MakeHandle(pool, i, c);
                    return true;
                }
            }
        }

        return false;
    }

    static void DoClose(HPool pool, Connection* c)
    {
        if (c->m_Socket != dmSocket::INVALID_SOCKET_HANDLE) {
            dmSocket::Shutdown(c->m_Socket, dmSocket::SHUTDOWNTYPE_READWRITE);
            dmSocket::Delete(c->m_Socket);
        }
        if (c->m_SSLConnection) {
            ssl_free(c->m_SSLConnection);
        }
        c->Clear();
    }

    static void PurgeExpired(HPool pool) {
        uint32_t n = pool->m_Connections.Size();

        uint64_t now = dmTime::GetTime();
        for (uint32_t i = 0; i < n; ++i) {
            Connection* c = &pool->m_Connections[i];
            if (c->m_State == STATE_CONNECTED && now >= c->m_Expires) {
                DoClose(pool, c);
            }
        }
    }

    static bool FindSlot(HPool pool, uint32_t* index, Connection** connection)
    {
        uint32_t n = pool->m_Connections.Size();

        for (uint32_t i = 0; i < n; ++i) {
            Connection* c = &pool->m_Connections[i];
            if (c->m_State == STATE_FREE) {
                *connection = c;
                *index = i;
                return true;
            }
        }
        return false;
    }

    static Result ConnectSocket(HPool pool, dmSocket::Address address, uint16_t port, int timeout, Connection* c, dmSocket::Result* sr)
    {
        *sr = dmSocket::New(address.m_family, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &c->m_Socket);
        if (*sr != dmSocket::RESULT_OK) {
            return RESULT_SOCKET_ERROR;
        }

        if( timeout > 0 )
        {
            *sr = dmSocket::SetBlocking(c->m_Socket, false);
            if (*sr != dmSocket::RESULT_OK) {
                dmSocket::Delete(c->m_Socket);
                return RESULT_SOCKET_ERROR;
            }

            *sr = dmSocket::Connect(c->m_Socket, address, port);
            if (*sr != dmSocket::RESULT_OK) {
                dmSocket::Delete(c->m_Socket);
                return RESULT_SOCKET_ERROR;
            }

            dmSocket::Selector selector;
            dmSocket::SelectorZero(&selector);
            dmSocket::SelectorSet(&selector, dmSocket::SELECTOR_KIND_WRITE, c->m_Socket);

            *sr = dmSocket::Select(&selector, timeout);
            if( *sr == dmSocket::RESULT_WOULDBLOCK )
            {
                dmSocket::Delete(c->m_Socket);
                return RESULT_SOCKET_ERROR;
            }

            *sr = dmSocket::SetBlocking(c->m_Socket, true);
            if (*sr != dmSocket::RESULT_OK) {
                dmSocket::Delete(c->m_Socket);
                return RESULT_SOCKET_ERROR;
            }
        }
        else
        {
            *sr = dmSocket::Connect(c->m_Socket, address, port);
            if (*sr != dmSocket::RESULT_OK) {
                dmSocket::Delete(c->m_Socket);
                return RESULT_SOCKET_ERROR;
            }
        }

        return RESULT_OK;
    }

    static Result Connect(HPool pool, const char* host, dmSocket::Address address, uint16_t port, bool ssl, int timeout, Connection* c, dmSocket::Result* sr)
    {
        uint64_t connectstart = dmTime::GetTime();

        Result r = ConnectSocket(pool, address, port, timeout, c, sr);
        if( r != RESULT_OK )
        {
            c->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
            return r;
        }

        uint64_t handshakestart = dmTime::GetTime();
        if( timeout > 0 && (handshakestart - connectstart) > timeout )
        {
            r = RESULT_SOCKET_ERROR;
            dmSocket::Delete(c->m_Socket);
            c->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
            return RESULT_SOCKET_ERROR;
        }

        if (r == RESULT_OK) {

            if (ssl) {

                // Consume the amount of time spent in the connect code
                int ssl_handshake_timeout = timeout == 0 ? 0 : timeout - int(handshakestart - connectstart);

                // In order to not have it block (unless timeout == 0)
                dmSocket::SetSendTimeout(c->m_Socket, (int)ssl_handshake_timeout);
                dmSocket::SetReceiveTimeout(c->m_Socket, (int)ssl_handshake_timeout);

                dmAxTls::SSL_EXTENSIONS *ext = dmAxTls::ssl_ext_new();
                ext->host_name = host;
                // NOTE: No session resume support. We would require a pool of session-id's or similar.
                dmAxTls::SSL* ssl = dmAxTls::ssl_client_new(pool->m_SSLContext,
                                          dmSocket::GetFD(c->m_Socket),
                                          0,
                                          0,
                                          ext);

                *sr = dmSocket::RESULT_UNKNOWN;

                // Since the socket is non blocking (and the way axtls is implemented)
                // we need do the hand shake ourselves
                while( ssl_handshake_status(ssl) == SSL_NOT_OK )
                {
                    int ret = ssl_read(ssl, 0);
                    if( ret < 0 )
                        break;

                    uint64_t currenttime = dmTime::GetTime();
                    if( ssl_handshake_timeout > 0 && int(currenttime - handshakestart) > ssl_handshake_timeout )
                    {
                        *sr = dmSocket::RESULT_WOULDBLOCK;
                        break;
                    }
                }

                int hs = ssl_handshake_status(ssl);
                if (hs != SSL_OK) {
                    r = RESULT_HANDSHAKE_FAILED;
                    dmLogWarning("SSL handshake failed (%d)", hs);
                    dmAxTls::ssl_free(ssl);
                    ssl = 0;
                    dmSocket::Delete(c->m_Socket);
                    c->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
                }
                else
                {
                    *sr = dmSocket::RESULT_OK;
                }
                c->m_SSLConnection = ssl;
            }
        }
        return r;
    }

    Result Dial(HPool pool, const char* host, uint16_t port, bool ssl, int timeout, HConnection* connection, dmSocket::Result* sock_res)
    {
        if (!pool->m_AllowNewConnections) {
            return RESULT_SHUT_DOWN;
        }

        // TODO: Make this async, making it able to timeout (it can take up to 30 seconds). Look into getaddrinfo_a
        dmSocket::Address address;
        dmSocket::Result sr = dmSocket::GetHostByName(host, &address);
        dmhash_t conn_id = CalculateConnectionID(address, port, ssl);

        Connection* c = 0;
        uint32_t index;

        if (sr == dmSocket::RESULT_OK) {
            DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);

            PurgeExpired(pool);

            if (FindConnection(pool, conn_id, address, port, ssl, connection)) {
                return RESULT_OK;
            }

            if (!FindSlot(pool, &index, &c)) {
                return RESULT_OUT_OF_RESOURCES;
            }
            c->m_State = STATE_INUSE;

        } else {
            *sock_res = sr;
            return RESULT_SOCKET_ERROR;
        }

        Result r = Connect(pool, host, address, port, ssl, timeout, c, sock_res);

        {
            DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);

            if (r == RESULT_OK) {
                *connection = MakeHandle(pool, index, c);
                c->m_ID = conn_id;
                c->m_ReuseCount = 0;
                c->m_State = STATE_INUSE;
                c->m_Expires = pool->m_MaxKeepAlive * 1000000U + dmTime::GetTime();
                c->m_Address = address;
                c->m_Port = port;
                c->m_WasShutdown = 0;
            } else {
                c->Clear();
            }
        }

        return r;
    }

    void Return(HPool pool, HConnection connection)
    {
        DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);

        Connection* c = GetConnection(pool, connection);
        assert(c->m_State == STATE_INUSE);
        c->m_State = STATE_CONNECTED;
    }

    void Close(HPool pool, HConnection connection)
    {
        DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);

        Connection* c = GetConnection(pool, connection);
        assert(c->m_State == STATE_INUSE);
        DoClose(pool, c);
    }

    dmSocket::Socket GetSocket(HPool pool, HConnection connection)
    {
        DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);

        Connection* c = GetConnection(pool, connection);
        assert(c->m_State == STATE_INUSE);
        return c->m_Socket;
    }

    // NOTE: void* in order to avoid exposing axTLS headers from dlib
    // dmConnectionPool is regarded as private but used from tests
    void* GetSSLConnection(HPool pool, HConnection connection)
    {
        DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);

        Connection* c = GetConnection(pool, connection);
        assert(c->m_State == STATE_INUSE);
        return c->m_SSLConnection;
    }

    uint32_t GetReuseCount(HPool pool, HConnection connection)
    {
        DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);

        Connection* c = GetConnection(pool, connection);
        assert(c->m_State == STATE_INUSE);
        return c->m_ReuseCount;
    }

    uint32_t Shutdown(HPool pool, dmSocket::ShutdownType how)
    {
        // Shut down all in use and prevent new ones. Return total count of
        // in-use connections.
        DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);
        uint32_t count = 0;
        for (uint32_t i=0;i!=pool->m_Connections.Size();i++) {
            Connection* c = &pool->m_Connections[i];
            if (c->m_State == STATE_INUSE) {
                count++;
                if (!c->m_WasShutdown) {
                    assert(c->m_Socket != dmSocket::INVALID_SOCKET_HANDLE);
                    dmSocket::Shutdown(c->m_Socket, how);
                    c->m_WasShutdown = 1;
                }
            }
        }
        pool->m_AllowNewConnections = 0;
        return count;
    }

    void Reopen(HPool pool)
    {
        DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);
        pool->m_AllowNewConnections = 1;
    }
}
