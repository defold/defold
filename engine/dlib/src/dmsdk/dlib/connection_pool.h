// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_CONNECTION_POOL
#define DMSDK_CONNECTION_POOL

#include <stdint.h>

// Cannot forward declare enums
#include <dmsdk/dlib/socket.h>

// Please note that the underlying types may change at our discretion
namespace dmSSLSocket
{
    typedef struct SSLSocket* Socket;
}

/*# SDK Connection pool API documentation
 *
 * Connection pool
 *
 * @document
 * @name Connection Pool
 * @namespace dmConnectionPool
 * @path engine/dlib/src/dmsdk/dlib/connection_pool.h
 */

namespace dmConnectionPool
{
    /*# Connection pool handle
     * @typedef
     * @name HPool
     */
    typedef struct ConnectionPool* HPool;

    /*# Connection handle
     * @typedef
     * @name HConnection
     */
    typedef uint32_t HConnection;


    /*# result enumeration
     * Result enumeration.
     * @enum
     * @name dmConnectionPool::Result
     * @member dmConnectionPool::RESULT_OK 0
     * @member dmConnectionPool::RESULT_OUT_OF_RESOURCES -1
     * @member dmConnectionPool::RESULT_SOCKET_ERROR -2
     * @member dmConnectionPool::RESULT_HANDSHAKE_FAILED -3
     * @member dmConnectionPool::RESULT_SHUT_DOWN -4
     */
    enum Result
    {
        RESULT_OK = 0,
        RESULT_OUT_OF_RESOURCES = -1,
        RESULT_SOCKET_ERROR = -2,
        RESULT_HANDSHAKE_FAILED = -3,
        RESULT_SHUT_DOWN = -4,
    };

    /*#
     * Creation parameters
     * @struct
     * @name dmConnectionPool::Params
     * @member m_MaxConnections [type:int] Max connection in pool
     * @member m_MaxKeepAlive [type:int] Default max-keep-alive time in seconds
     */
    struct Params
    {
        Params()
        {
            m_MaxConnections = 64;
            m_MaxKeepAlive = 10;
        }
        uint32_t m_MaxConnections;
        uint32_t m_MaxKeepAlive;
    };

    /*#
     * Create a new connection pool
     * @name dmConnectionPool::New
     * @param params
     * @param pool [type:dmConnectionPool::HPool*] pool (out)
     * @return dmConnectionPool::RESULT_OK on success
     */
    Result New(const Params* params, HPool* pool);

    /*#
     * Delete connnection pool
     * @name dmConnectionPool::Delete
     * @param pool [type:dmConnectionPool::HPool] pool
     * @return dmConnectionPool::RESULT_OK on success
     */
    Result Delete(HPool pool);

    /*#
     * Connection to a host/port
     * @name dmConnectionPool::Dial
     * @param pool [type:dmConnectionPool::HPool] pool
     * @param host [type:const char*] host
     * @param port [type:uint16_t] port
     * @param ssl [type:bool] true for ssl connection
     * @param timeout [type:int] The timeout (micro seconds) for the connection and ssl handshake
     * @param connection [type:dmConnectionPool::HConnection*] connection (out)
     * @param sock_res [type:dmSocket::Result*] socket-result code on failure
     * @return dmConnectionPool::RESULT_OK on success
     */
    Result Dial(HPool pool, const char* host, uint16_t port, bool ssl, int timeout, HConnection* connection, dmSocket::Result* sock_res);

    /*#
     * Connection to a host/port
     * @name dmConnectionPool::Dial
     * @param pool [type:dmConnectionPool::HPool] pool
     * @param host [type:const char*] host
     * @param port [type:uint16_t] port
     * @param ssl [type:bool] true for ssl connection
     * @param timeout [type:int] The timeout (micro seconds) for the connection and ssl handshake
     * @param cancelflag [type:int*] If set and not null, will make the request early out
     * @param connection [type:dmConnectionPool::HConnection*] connection (out)
     * @param sock_res [type:dmSocket::Result*] socket-result code on failure
     * @return dmConnectionPool::RESULT_OK on success
     */
    Result Dial(HPool pool, const char* host, uint16_t port, bool ssl, int timeout, int* cancelflag, HConnection* connection, dmSocket::Result* sock_res);

    /*#
     * Return connection to pool
     * @name dmConnectionPool::Return
     * @param pool [type:dmConnectionPool::HPool] pool
     * @param connection [type:dmConnectionPool::HConnection]
     */
    void Return(HPool pool, HConnection connection);

    /*#
     * Close connection. Use this function whenever an error occur in eg http.
     * @name dmConnectionPool::Close
     * @param pool [type:dmConnectionPool::HPool] pool
     * @param connection [type:dmConnectionPool::HConnection]
     */
    void Close(HPool pool, HConnection connection);

    /*#
     * Get socket for connection
     * @name dmConnectionPool::GetSocket
     * @param pool [type:dmConnectionPool::HPool] pool
     * @param connection [type:dmConnectionPool::HConnection]
     * @return [type:dmSocket::Socket] on success
     */
    dmSocket::Socket GetSocket(HPool pool, HConnection connection);

    /*#
     * Get secure socket.
     * @name dmConnectionPool::GetSSLSocket
     * @param pool [type:dmConnectionPool::HPool] pool
     * @param connection [type:dmConnectionPool::HConnection]
     * @return [type:dmSSLSocket::Socket] on success
     */
    dmSSLSocket::Socket GetSSLSocket(HPool pool, HConnection connection);

    /**
     * Get reuse count for a connection
     * @name dmConnectionPool::GetReuseCount
     * @param pool [type:dmConnectionPool::HPool] pool
     * @param connection [type:dmConnectionPool::HConnection]
     * @return reuse count
     */
    uint32_t GetReuseCount(HPool pool, HConnection connection);

    /*#
     * Shuts down all open sockets in the pool and block new connection attempts. The function can be
     * called repeatedly on the same pool until it returns no more connections in use.
     *
     * @name dmConnectionPool::Shutdown
     * @param pool [type:dmConnectionPool::HPool] pool
     * @param how [type:dmSocket::ShutdownType] shutdown type to pass to socket shutdown function
     * @return current number of connections in use
     */
    uint32_t Shutdown(HPool pool, dmSocket::ShutdownType how);
}

#endif // #ifndef DMSDK_CONNECTION_POOL
