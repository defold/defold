// Copyright 2020 The Defold Foundation
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
 * [file:<dmsdk/dlib/sslsocket.h>]
 *
 * Secure socket functions.
 *
 * @document
 * @name SSLSocket
 * @namespace dmSSLSocket
 */

namespace dmSSLSocket
{
    enum Result
    {
        RESULT_OK,
        RESULT_SSL_INIT_FAILED = -2000,//!< RESULT_SSL_INIT_FAILED
        RESULT_HANDSHAKE_FAILED = -2001,//!< RESULT_HANDSHAKE_FAILED
        RESULT_WOULDBLOCK = -2002,//!< RESULT_WOULDBLOCK
        RESULT_CONNREFUSED = -2003,//!< RESULT_WOULDBLOCK
    };

    typedef struct SSLSocket* Socket;

    const Socket INVALID_SOCKET_HANDLE = 0;

    /*#
     * Create a new secure socket
     * @name dmSSLSocket::New
     * @param socket The socket to wrap
     * @param handshake_timeout The timeout for the handshake procedure
     * @param sslsocket Pointer to created socket
     * @return RESULT_OK on succcess
     */
    Result New(dmSocket::Socket socket, const char* host, uint64_t handshake_timeout, Socket* sslsocket);

    /*#
     * Delete a secure socket. Does not close the underlying socket
     * @name dmSSLSocket::Delete
     * @param socket Secure socket to close
     * @return RESULT_OK on success
     */
    Result Delete(Socket socket);

    /**
     * Send a message on a secure socket
     * @param socket SSL socket to send a message on
     * @param buffer Buffer to send
     * @param length Length of buffer to send
     * @param sent_bytes Number of bytes sent (result)
     * @return RESULT_OK on success
     */
    dmSocket::Result Send(Socket socket, const void* buffer, int length, int* sent_bytes);

    /*#
     * Receive data on a secure socket
     * @param socket Socket to receive data on
     * @param buffer Buffer to receive to
     * @param length Receive buffer length
     * @param received_bytes Number of received bytes (result)
     * @return RESULT_OK on success
     */
    dmSocket::Result Receive(Socket socket, void* buffer, int length, int* received_bytes);

}

#endif // DMSDK_SSLSOCKET_H