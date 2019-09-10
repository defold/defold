#include <stdlib.h>
#include <stdio.h>
#include "connection_pool.h"
#include "hashtable.h"
#include "array.h"
#include "time.h"
#include "log.h"
#include "math.h"
#include "mutex.h"
#include "hash.h"

#include <mbedtls/net_sockets.h>
#include <mbedtls/debug.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/certs.h>

#define MBED_DEBUG_LEVEL 1

const int SOCKET_TIMEOUT = 500 * 1000;

#if defined(MBEDTLS_DEBUG_C)
static void mbedtls_debug( void* ctx, int level, const char* file, int line, const char* str )
{
    switch (level) {
    case 0: dmLogError("%s:%d: %s", file, line, str); return;
    case 1: dmLogWarning("%s:%d: %s", file, line, str); return;
    case 2: dmLogDebug("%s:%d: %s", file, line, str); return;
    }
    dmLogInfo("%s:%d: %s", file, line, str);
}
#endif

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

        dmSocket::Address       m_Address;
        dmhash_t                m_ID;
        uint64_t                m_Expires;
        mbedtls_net_context*    m_SSLNetContext;
        mbedtls_ssl_context*    m_SSLContext;
        dmSocket::Socket        m_Socket;
        State                   m_State;
        uint16_t                m_Port;
        uint16_t                m_Version;
        uint16_t                m_ReuseCount;
        uint16_t                m_WasShutdown:1;
    };

    static void DoClose(HPool pool, Connection* c);

    struct ConnectionPool
    {
        uint64_t            m_MaxKeepAlive;
        dmArray<Connection> m_Connections;
        uint16_t            m_NextVersion;

        mbedtls_entropy_context     m_MbedEntropy;
        mbedtls_ctr_drbg_context    m_MbedCtrDrbg;
        mbedtls_ssl_config          m_MbedConf;

        dmMutex::HMutex     m_Mutex;

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

            // We should manually and optionally verify certificates
            // but it require that root-certs
            // are bundled in the engine and some kind of lazy loading
            // mechanism as we can't load every possible certificate for
            // every connections. It's possible to introspect the SSL object
            // to find out which certificates to load.

            mbedtls_ssl_config_init( &m_MbedConf );
            mbedtls_ctr_drbg_init( &m_MbedCtrDrbg );
            mbedtls_entropy_init( &m_MbedEntropy );

#if defined(MBEDTLS_DEBUG_C)
            mbedtls_debug_set_threshold( MBED_DEBUG_LEVEL );
            mbedtls_ssl_conf_dbg( &m_MbedConf, mbedtls_debug, 0 );
#endif
            int ret = 0;

            const char* pers = "defold_ssl_client";
            if( ( ret = mbedtls_ctr_drbg_seed( &m_MbedCtrDrbg, mbedtls_entropy_func, &m_MbedEntropy,
                                       (const unsigned char *) pers,
                                       strlen( pers ) ) ) != 0 )
            {
                dmLogError("mbedtls_ctr_drbg_seed failed: %d", ret);
                return;
            }

            if( ( ret = mbedtls_ssl_config_defaults( &m_MbedConf,
                            MBEDTLS_SSL_IS_CLIENT,
                            MBEDTLS_SSL_TRANSPORT_STREAM,
                            MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 )
            {
                dmLogError("mbedtls_ssl_config_defaults failed: %d", ret);
                return;
            }

            mbedtls_ssl_conf_rng( &m_MbedConf, mbedtls_ctr_drbg_random, &m_MbedCtrDrbg );
            mbedtls_ssl_conf_authmode( &m_MbedConf, MBEDTLS_SSL_VERIFY_NONE );

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

            mbedtls_ssl_config_free( &m_MbedConf );
            mbedtls_ctr_drbg_free( &m_MbedCtrDrbg );
            mbedtls_entropy_free( &m_MbedEntropy );

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
        if (c->m_SSLContext) {
            mbedtls_ssl_close_notify( c->m_SSLContext );
            mbedtls_net_free( c->m_SSLNetContext );
            mbedtls_ssl_free( c->m_SSLContext );
            free(c->m_SSLNetContext);
            free(c->m_SSLContext);
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
        if( timeout > 0 && (handshakestart - connectstart) > (uint64_t)timeout )
        {
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

                int max_ssl_handshake_timeout = dmMath::Max(ssl_handshake_timeout, SOCKET_TIMEOUT);
                if (ssl_handshake_timeout != 0)
                {
                    int mbed_ssl_timeout = dmMath::Max(ssl_handshake_timeout, SOCKET_TIMEOUT) / 1000;
                    mbed_ssl_timeout = dmMath::Max(1, mbed_ssl_timeout);
                    // Never go below 1 second, since that's the lowest supported by thus function anyways
                    mbedtls_ssl_conf_handshake_timeout(&pool->m_MbedConf, 1, mbed_ssl_timeout);
                }

                c->m_SSLContext     = (mbedtls_ssl_context*)malloc(sizeof(mbedtls_ssl_context));
                c->m_SSLNetContext  = (mbedtls_net_context*)malloc(sizeof(mbedtls_net_context));

                mbedtls_ssl_init( c->m_SSLContext );

                int ret = 0;
                if( ( ret = mbedtls_ssl_setup( c->m_SSLContext, &pool->m_MbedConf ) ) != 0 )
                {
                    dmLogError("mbedtls_ssl_setup returned %d\n", ret );
                    return RESULT_SOCKET_ERROR;
                }

                if( ( ret = mbedtls_ssl_set_hostname( c->m_SSLContext, host) ) != 0 )
                {
                    dmLogError("mbedtls_ssl_set_hostname returned %d\n", ret );
                    return RESULT_SOCKET_ERROR;
                }

                // Normally, in mbedtls, you'd call mbedtls_net_connect
                // but we already have our socket created and setup
                mbedtls_net_init( c->m_SSLNetContext );
                c->m_SSLNetContext->fd = dmSocket::GetFD(c->m_Socket);

                mbedtls_ssl_set_bio( c->m_SSLContext, c->m_SSLNetContext, mbedtls_net_send, mbedtls_net_recv, NULL );

                do
                {
                    ret = mbedtls_ssl_handshake( c->m_SSLContext );
                } while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
                         ret == MBEDTLS_ERR_SSL_WANT_WRITE);

                uint64_t currenttime = dmTime::GetTime();
                if( ssl_handshake_timeout > 0 && int(currenttime - handshakestart) > ssl_handshake_timeout )
                {
                    ret = MBEDTLS_ERR_SSL_TIMEOUT;
                }

                if (ret != 0)
                {
                    // see net_sockets.h, ssl.h and x509.h for error codes
                    dmLogError("mbedtls_ssl_handshake returned -0x%04X\n", -ret );
                    if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED)
                    {
                        dmLogError("Unable to verify the server's certificate.");
                        *sr = dmSocket::RESULT_CONNREFUSED;
                    }
                    else if (ret == MBEDTLS_ERR_SSL_TIMEOUT)
                    {
                        dmLogError("SSL handshake timeout");
                        *sr = dmSocket::RESULT_WOULDBLOCK;
                    }
                    DoClose(pool, c);
                    return RESULT_HANDSHAKE_FAILED;
                }

                int flags = 0;
                if( ( flags = mbedtls_ssl_get_verify_result( c->m_SSLContext ) ) != 0 )
                {
                    char vrfy_buf[512];
                    mbedtls_x509_crt_verify_info( vrfy_buf, sizeof( vrfy_buf ), "  ! ", flags );
                    dmLogError("mbedtls_ssl_get_verify_result failed:\n    %s\n", vrfy_buf );
                }
            }
        }
        return r;
    }

    Result DoDial(HPool pool, const char* host, uint16_t port, dmDNS::HChannel dns_channel, bool ssl, int timeout, HConnection* connection, dmSocket::Result* sock_res, bool ipv4, bool ipv6)
    {
        if (!pool->m_AllowNewConnections) {
            return RESULT_SHUT_DOWN;
        }

        dmSocket::Address address;
        bool gethost_did_succeed;

        // This function would previously not specify ipv4 and/or ipv6 when calling GetHostByName.
        // GetHostByName would then assume both ipv4 and ipv6 and always prefer and return an ipv4
        // address.
        // Connecting to the returned address would fail when on an ipv6 network.
        // This is why when calling DoDial we now have the ability to specify ipv4 and/or ipv6 so
        // that the caller can try to connect first to ipv4 and then to ipv6 if ipv4 failed.
        if (dns_channel)
        {
            gethost_did_succeed = dmDNS::GetHostByName(host, &address, dns_channel, ipv4, ipv6) == dmDNS::RESULT_OK;

            // If the DNS request failed, we might need to update the DNS configuration for this channel since something
            // might have happened with the network adapter since the last time a HTTP request happened.
            if (!gethost_did_succeed)
            {
                dmDNS::RefreshChannel(dns_channel);
                gethost_did_succeed = dmDNS::GetHostByName(host, &address, dns_channel, ipv4, ipv6) == dmDNS::RESULT_OK;
            }
        }
        else
        {
            gethost_did_succeed = dmSocket::GetHostByName(host, &address, ipv4, ipv6) == dmSocket::RESULT_OK;
        }

        dmhash_t conn_id = CalculateConnectionID(address, port, ssl);

        Connection* c = 0;
        uint32_t index;

        if (gethost_did_succeed) {
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
            *sock_res = dmSocket::RESULT_HOST_NOT_FOUND; // Only error that dmSocket::getHostByName returns
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

    Result Dial(HPool pool, const char* host, uint16_t port, dmDNS::HChannel dns_channel, bool ssl, int timeout, HConnection* connection, dmSocket::Result* sock_res)
    {
        // try connecting to the host using ipv4 first
        uint64_t dial_started = dmTime::GetTime();
        Result r = DoDial(pool, host, port, dns_channel, ssl, timeout, connection, sock_res, 1, 0);
        if (r == RESULT_OK || r == RESULT_SHUT_DOWN || r == RESULT_OUT_OF_RESOURCES)
        {
            return r;
        }
        // ipv4 connection failed - reduce timeout (if needed) and try using ipv6 instead
        if (timeout > 0)
        {
            timeout = timeout - (int)(dmTime::GetTime() - dial_started);
            if (timeout <= 0)
            {
                return RESULT_SOCKET_ERROR;
            }
        }
        return DoDial(pool, host, port, dns_channel, ssl, timeout, connection, sock_res, 0, 1);
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

    // NOTE: void* in order to avoid exposing ssl headers from dlib
    void* GetSSLConnection(HPool pool, HConnection connection)
    {
        DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);

        Connection* c = GetConnection(pool, connection);
        assert(c->m_State == STATE_INUSE);
        return c->m_SSLContext;
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
        uint32_t n = pool->m_Connections.Size();
        for (uint32_t i=0; i != n; i++) {
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
        // This function is used by tests to restore usage of a pool
        // We purge any live connections so the test does not run out
        // of connection pool items
        DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);
        uint32_t n = pool->m_Connections.Size();
        for (uint32_t i=0; i != n; i++) {
            Connection* c = &pool->m_Connections[i];
            if (c->m_State == STATE_CONNECTED)
            {
                dmSocket::Delete(c->m_Socket);
                if (c->m_SSLNetContext) {
                    mbedtls_ssl_close_notify( c->m_SSLContext );
                    mbedtls_net_free( c->m_SSLNetContext );
                    mbedtls_ssl_free( c->m_SSLContext );
                    free(c->m_SSLNetContext);
                    free(c->m_SSLContext);
                }
                c->Clear();
            }
        }
        pool->m_AllowNewConnections = 1;
    }
}
