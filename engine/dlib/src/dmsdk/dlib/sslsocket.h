// Copyright 2020-2022 The Defold Foundation
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

#ifndef DMSDK_SSLSOCKET_H
#define DMSDK_SSLSOCKET_H

#include <stdint.h>

// Cannot forward declate enums
#include <dmsdk/dlib/socket.h>

/*# SDK Secure socket API documentation
 *
 * Secure socket functions.
 *
 * @document
 * @name SSLSocket
 * @namespace dmSSLSocket
 * @path engine/dlib/src/dmsdk/dlib/sslsocket.h
 */

namespace dmSSLSocket
{
    /*# result enumeration
     *
     * Result enumeration.
     *
     * @enum
     * @name dmSSLSocket::Result
     * @member dmSSLSocket::RESULT_OK (0)
     * @member dmSSLSocket::RESULT_SSL_INIT_FAILED (-2000)
     * @member dmSSLSocket::RESULT_HANDSHAKE_FAILED (-2001)
     * @member dmSSLSocket::RESULT_WOULDBLOCK (-2002)
     * @member dmSSLSocket::RESULT_CONNREFUSED (-2003)
     *
     */
    enum Result
    {
        RESULT_OK = 0,
        RESULT_SSL_INIT_FAILED = -2000,
        RESULT_HANDSHAKE_FAILED = -2001,
        RESULT_WOULDBLOCK = -2002,
        RESULT_CONNREFUSED = -2003,
    };

    /*# Socket type definition
     * @typedef
     * @name Socket
     */
    typedef struct SSLSocket* Socket;

    /*# SSLSocket socket handle
     * @variable
     * @name dmSSLSocket::INVALID_SOCKET_HANDLE
     */
    const Socket INVALID_SOCKET_HANDLE = 0;

    /*# create a secure socket
     * Create a new secure socket
     * @name dmSSLSocket::New
     * @param socket [type:dmSocket::Socket] The socket to wrap
     * @param host [type:const char*] The name of the host (e.g. "httpbin.org")
     * @param timeout [type:uint64_t] The timeout for the handshake procedure. (microseconds)
     * @param sslsocket [type:dmSSLSocket::Socket*] Pointer to a secure socket
     * @return RESULT_OK on succcess
     * @examples
     * ```cpp
     * dmSSLSocket::Result result;
     * dmSSLSocket::Socket sslsocket;
     * result = dmSSLSocket::New(socket, "httpbin.org", 500*1000, &sslsocket);
     * if (dmSSLSocket::RESULT_OK == result)
     * {
     *     // ...
     * } else {
     *     // ...
     * }
     * ```
     */
    Result New(dmSocket::Socket socket, const char* host, uint64_t timeout, Socket* sslsocket);

    /*# delete a secure socket
     * Delete a secure socket. Does not close the underlying socket
     * @name dmSSLSocket::Delete
     * @param socket [type:dmSSLSocket::Socket] Secure socket to close
     * @return RESULT_OK on success
     * @examples
     * ```cpp
     * dmSSLSocket::Delete(sslsocket);
     * ```
     */
    Result Delete(Socket socket);

    /*# send a message on a secure socket
     * Send a message on a secure socket
     * @name dmSSLSocket::Send
     * @param socket [type:dmSSLSocket::Socket] SSL socket to send a message on
     * @param buffer Buffer to send
     * @param length Length of buffer to send
     * @param sent_bytes Number of bytes sent (result)
     * @return RESULT_OK on success
     */
    dmSocket::Result Send(Socket socket, const void* buffer, int length, int* sent_bytes);

    /*# receive data on a secure socket
     * Receive data on a secure socket
     * @name dmSSLSocket::Receive
     * @param socket [type:dmSSLSocket::Socket] Socket to receive data on
     * @param buffer Buffer to receive to
     * @param length Receive buffer length
     * @param received_bytes Number of received bytes (result)
     * @return RESULT_OK on success
     */
    dmSocket::Result Receive(Socket socket, void* buffer, int length, int* received_bytes);

    /*#
     * Set socket receive timeout
     * @note Timeout resolution might be in milliseconds, e.g. windows. Use values
     *       larger than or equal to 1000
     * @name dmSocket::SetReceiveTimeout
     * @param socket [type:dmSocket::Socket] socket
     * @param timeout [type:uint64_t] timeout in microseconds
     * @return RESULT_OK on success
     */
    dmSocket::Result SetReceiveTimeout(Socket socket, uint64_t timeout);
}

#endif // DMSDK_SSLSOCKET_H