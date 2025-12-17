// Copyright 2020-2025 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <stdlib.h>
#include <stdio.h>
#include "connection_pool.h"
#include "hashtable.h"
#include "array.h"
#include "time.h"
#include "log.h"
#include "math.h"
#include "hash.h"

#include <dmsdk/dlib/mutex.h>
#include <dlib/socket.h>
#include <dlib/sslsocket.h>

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
            m_SSLSocket = dmSSLSocket::INVALID_SOCKET_HANDLE;
            m_State = STATE_FREE;
        }

        dmSocket::Address       m_Address;
        dmhash_t                m_ID;
        uint64_t                m_Expires;
        dmSSLSocket::Socket     m_SSLSocket;
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
                dmLogWarning("Leaking %d connections from connection pool", in_use);
            }

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
                if (c->m_Socket != dmSocket::INVALID_SOCKET_HANDLE)
                    stats->m_InUseAndValid++;
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
        if (c->m_SSLSocket != dmSSLSocket::INVALID_SOCKET_HANDLE) {
            dmSSLSocket::Delete(c->m_SSLSocket);
            c->m_SSLSocket = dmSSLSocket::INVALID_SOCKET_HANDLE;
        }
        if (c->m_Socket != dmSocket::INVALID_SOCKET_HANDLE) {
            dmSocket::Shutdown(c->m_Socket, dmSocket::SHUTDOWNTYPE_READWRITE);
            dmSocket::Delete(c->m_Socket);
            c->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
        }
        c->Clear();
    }

    static void PurgeExpired(HPool pool) {
        uint32_t n = pool->m_Connections.Size();

        uint64_t now = dmTime::GetMonotonicTime();
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

    static Result ConnectSocket(HPool pool, dmSocket::Address address, uint16_t port, int timeout, dmSocket::Socket* socket, dmSocket::Result* sr)
    {
        *sr = dmSocket::New(address.m_family, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, socket);
        if (*sr != dmSocket::RESULT_OK) {
            return RESULT_SOCKET_ERROR;
        }

        if( timeout > 0 )
        {
            *sr = dmSocket::SetBlocking(*socket, false);
            if (*sr != dmSocket::RESULT_OK) {
                dmSocket::Delete(*socket);
                return RESULT_SOCKET_ERROR;
            }

            *sr = dmSocket::Connect(*socket, address, port);
            if (*sr != dmSocket::RESULT_OK) {
                dmSocket::Delete(*socket);
                return RESULT_SOCKET_ERROR;
            }

            dmSocket::Selector selector;
            dmSocket::SelectorZero(&selector);
            dmSocket::SelectorSet(&selector, dmSocket::SELECTOR_KIND_WRITE, *socket);

            *sr = dmSocket::Select(&selector, timeout);
            if( *sr == dmSocket::RESULT_WOULDBLOCK )
            {
                dmSocket::Delete(*socket);
                return RESULT_SOCKET_ERROR;
            }

            *sr = dmSocket::SetBlocking(*socket, true);
            if (*sr != dmSocket::RESULT_OK) {
                dmSocket::Delete(*socket);
                return RESULT_SOCKET_ERROR;
            }
        }
        else
        {
            *sr = dmSocket::Connect(*socket, address, port);
            if (*sr != dmSocket::RESULT_OK) {
                dmSocket::Delete(*socket);
                return RESULT_SOCKET_ERROR;
            }
        }

        return RESULT_OK;
    }


    static Result CreateSSLSocket(dmSocket::Socket socket, const char* host, int timeout, dmSSLSocket::Socket* sslsocket, dmSocket::Result* sock_res)
    {
        dmSSLSocket::Result result = dmSSLSocket::New(socket, host, timeout, sslsocket);
        if (dmSSLSocket::RESULT_OK != result)
        {
            if (dmSSLSocket::RESULT_WOULDBLOCK == result)
            {
                *sock_res = dmSocket::RESULT_WOULDBLOCK;
            }
            else
            {
                *sock_res = dmSocket::RESULT_UNKNOWN;
            }
            return RESULT_HANDSHAKE_FAILED;
        }
        return RESULT_OK;
    }


    Result CreateSSLSocket(HPool pool, HConnection connection, const char* host, int timeout, dmSocket::Result* sock_res)
    {
        DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);

        Connection* c = GetConnection(pool, connection);

        return CreateSSLSocket(c->m_Socket, host, timeout, &c->m_SSLSocket, sock_res);
    }

    static Result Connect(HPool pool, const char* host, dmSocket::Address address, uint16_t port, int timeout,
                                    dmSocket::Socket* socket, dmSocket::Result* sr)
    {
        uint64_t connectstart = dmTime::GetMonotonicTime();

        Result r = ConnectSocket(pool, address, port, timeout, socket, sr);
        if( r != RESULT_OK )
        {
            *socket = dmSocket::INVALID_SOCKET_HANDLE;
            return r;
        }

        uint64_t now = dmTime::GetMonotonicTime();
        if( timeout > 0 && (now - connectstart) > (uint64_t)timeout )
        {
            dmSocket::Delete(*socket);
            *socket = dmSocket::INVALID_SOCKET_HANDLE;
            return RESULT_SOCKET_ERROR;
        }
        return r;
    }

    Result DoDial(HPool pool, const char* host, uint16_t port, bool ssl, int timeout, int* cancelflag, HConnection* connection, dmSocket::Result* sock_res, bool ipv4, bool ipv6)
    {
        if (!pool->m_AllowNewConnections) {
            return RESULT_SHUT_DOWN;
        }

        // This function would previously not specify ipv4 and/or ipv6 when calling GetHostByName.
        // GetHostByName would then assume both ipv4 and ipv6 and always prefer and return an ipv4
        // address.
        // Connecting to the returned address would fail when on an ipv6 network.
        // This is why when calling DoDial we now have the ability to specify ipv4 and/or ipv6 so
        // that the caller can try to connect first to ipv4 and then to ipv6 if ipv4 failed.
        dmSocket::Address address;

        uint64_t dial_started = dmTime::GetMonotonicTime();
        bool gethost_did_succeed = dmSocket::GetHostByNameT(host, &address, timeout, cancelflag, ipv4, ipv6) == dmSocket::RESULT_OK;
        if (timeout > 0)
        {
            timeout = timeout - (int)(dmTime::GetMonotonicTime() - dial_started);
            if (timeout <= 0)
            {
                return RESULT_SOCKET_ERROR;
            }
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
            c->m_State = STATE_INUSE; // The connection might fail, but we need to reserve this in the meantime

        } else {
            *sock_res = dmSocket::RESULT_HOST_NOT_FOUND; // Only error that dmSocket::getHostByName returns
            return RESULT_SOCKET_ERROR;
        }

        dmSocket::Socket socket = dmSocket::INVALID_SOCKET_HANDLE;
        dmSSLSocket::Socket sslsocket = dmSSLSocket::INVALID_SOCKET_HANDLE;

        Result r = Connect(pool, host, address, port, timeout, &socket, sock_res);
        if ((r == RESULT_OK) && ssl)
        {
            r = CreateSSLSocket(socket, host, timeout, &sslsocket, sock_res);
        }

        {
            DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);

            if (r == RESULT_OK)
            {
                *connection = MakeHandle(pool, index, c);
                c->m_Socket = socket;
                c->m_SSLSocket = sslsocket;
                c->m_ID = conn_id;
                c->m_ReuseCount = 0;
                c->m_State = STATE_INUSE;
                c->m_Expires = pool->m_MaxKeepAlive * 1000000U + dmTime::GetMonotonicTime();
                c->m_Address = address;
                c->m_Port = port;
                c->m_WasShutdown = 0;
            }
            else
            {
                c->m_State = STATE_FREE;
                DoClose(pool, c);
            }
        }

        return r;
    }

    Result Dial(HPool pool, const char* host, uint16_t port, bool ssl, int timeout, int* cancelflag, HConnection* connection, dmSocket::Result* sock_res)
    {
        // try connecting to the host using ipv4 first
        bool ipv4 = true;
        bool ipv6 = false;
        uint64_t dial_started = dmTime::GetMonotonicTime();
        Result r = DoDial(pool, host, port, ssl, timeout, cancelflag, connection, sock_res, ipv4, ipv6);
        // Only if handshake failed NOT because of timeout
        if (r == RESULT_OK || r == RESULT_SHUT_DOWN || r == RESULT_OUT_OF_RESOURCES ||
            (r == RESULT_HANDSHAKE_FAILED && *sock_res != dmSocket::RESULT_WOULDBLOCK))
        {
            return r;
        }
        // ipv4 connection failed - reduce timeout (if needed) and try using ipv6 instead
        ipv4 = false;
        ipv6 = true;
        if (timeout > 0)
        {
            timeout = timeout - (int)(dmTime::GetMonotonicTime() - dial_started);
            if (timeout <= 0)
            {
                return RESULT_SOCKET_ERROR;
            }
        }
        return DoDial(pool, host, port, ssl, timeout, cancelflag, connection, sock_res, ipv4, ipv6);
    }

    Result Dial(HPool pool, const char* host, uint16_t port, bool ssl, int timeout, HConnection* connection, dmSocket::Result* sock_res)
    {
        return Dial(pool, host, port, ssl, timeout, 0, connection, sock_res);
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

    dmSSLSocket::Socket GetSSLSocket(HPool pool, HConnection connection)
    {
        DM_MUTEX_SCOPED_LOCK(pool->m_Mutex);

        Connection* c = GetConnection(pool, connection);
        assert(c->m_State == STATE_INUSE);
        return c->m_SSLSocket;
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
                    // Due to threaded behavior, the connection may be in use,
                    // but the socket's haven't been assigned yet
                    if(c->m_Socket != dmSocket::INVALID_SOCKET_HANDLE)
                    {
                        dmSocket::Shutdown(c->m_Socket, how);
                        c->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
                    }
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
            dmSSLSocket::Delete(c->m_SSLSocket);
            dmSocket::Delete(c->m_Socket);
            c->Clear();
        }
        pool->m_AllowNewConnections = 1;
    }
}
