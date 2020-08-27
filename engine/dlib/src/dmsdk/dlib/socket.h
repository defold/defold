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

#ifndef DMSDK_SOCKET_H
#define DMSDK_SOCKET_H

/*# SDK Socket API documentation
 * [file:<dmsdk/dlib/socket.h>]
 *
 * Socket functions.
 *
 * @document
 * @name Socket
 * @namespace dmSocket
 */

/**
 * @note For Recv* and Send* function ETIMEDOUT is translated to EWOULDBLOCK
 * on win32 for compatibility with BSD sockets.
 */
namespace dmSocket
{
    const int SOCKET_TIMEOUT = 500 * 1000;

    /*#
     * Socket handle
     * @note Use INVALID_SOCKET_HANDLE instead of zero for unset values. This is an exception
     * from all other handles.
     */
    typedef int Socket;

    /*#
     * Invalid socket handle
     */
    const Socket INVALID_SOCKET_HANDLE = 0xffffffff;

    enum Flags
    {
        FLAGS_UP = (1 << 0),
        FLAGS_RUNNING = (1 << 1),
        FLAGS_INET = (1 << 2),
        FLAGS_LINK = (1 << 3),
    };

    /**
     * Domain type
     */
    enum Domain
    {
        DOMAIN_MISSING, //!< DOMAIN_MISSING
        DOMAIN_IPV4,    //!< DOMAIN_IPV4
        DOMAIN_IPV6,    //!< DOMAIN_IPV6
        DOMAIN_UNKNOWN, //!< DOMAIN_UNKNOWN
    };

    /**
     * Socket type
     */
    enum Type
    {
        TYPE_STREAM, //!< TYPE_STREAM
        TYPE_DGRAM,  //!< TYPE_DGRAM
    };

    /**
     * Network protocol
     */
    enum Protocol
    {
        PROTOCOL_TCP, //!< PROTOCOL_TCP
        PROTOCOL_UDP, //!< PROTOCOL_UDP
    };

    /**
     * Socket shutdown type
     */
    enum ShutdownType
    {
        SHUTDOWNTYPE_READ,
        SHUTDOWNTYPE_WRITE,
        SHUTDOWNTYPE_READWRITE,
    };

    /*#
     * Socket result
     */
    enum Result
    {
        RESULT_OK             = 0,    //!< RESULT_OK

        RESULT_ACCES          = -1,   //!< RESULT_ACCES
        RESULT_AFNOSUPPORT    = -2,   //!< RESULT_AFNOSUPPORT
        RESULT_WOULDBLOCK     = -3,   //!< RESULT_WOULDBLOCK
        RESULT_BADF           = -4,   //!< RESULT_BADF
        RESULT_CONNRESET      = -5,   //!< RESULT_CONNRESET
        RESULT_DESTADDRREQ    = -6,   //!< RESULT_DESTADDRREQ
        RESULT_FAULT          = -7,   //!< RESULT_FAULT
        RESULT_HOSTUNREACH    = -8,   //!< RESULT_HOSTUNREACH
        RESULT_INTR           = -9,   //!< RESULT_INTR
        RESULT_INVAL          = -10,  //!< RESULT_INVAL
        RESULT_ISCONN         = -11,  //!< RESULT_ISCONN
        RESULT_MFILE          = -12,  //!< RESULT_MFILE
        RESULT_MSGSIZE        = -13,  //!< RESULT_MSGSIZE
        RESULT_NETDOWN        = -14,  //!< RESULT_NETDOWN
        RESULT_NETUNREACH     = -15,  //!< RESULT_NETUNREACH
        //RESULT_NFILE          = -16,
        RESULT_NOBUFS         = -17,  //!< RESULT_NOBUFS
        //RESULT_NOENT          = -18,
        //RESULT_NOMEM          = -19,
        RESULT_NOTCONN        = -20,  //!< RESULT_NOTCONN
        //RESULT_NOTDIR         = -21,
        RESULT_NOTSOCK        = -22,  //!< RESULT_NOTSOCK
        RESULT_OPNOTSUPP      = -23,  //!< RESULT_OPNOTSUPP
        RESULT_PIPE           = -24,  //!< RESULT_PIPE
        RESULT_PROTONOSUPPORT = -25,  //!< RESULT_PROTONOSUPPORT
        RESULT_PROTOTYPE      = -26,  //!< RESULT_PROTOTYPE
        RESULT_TIMEDOUT       = -27,  //!< RESULT_TIMEDOUT

        RESULT_ADDRNOTAVAIL   = -28,  //!< RESULT_ADDRNOTAVAIL
        RESULT_CONNREFUSED    = -29,  //!< RESULT_CONNREFUSED
        RESULT_ADDRINUSE      = -30,  //!< RESULT_ADDRINUSE
        RESULT_CONNABORTED    = -31,  //!< RESULT_CONNABORTED
        RESULT_INPROGRESS     = -32,  //!< RESULT_INPROGRESS

        // gethostbyname errors
        RESULT_HOST_NOT_FOUND = -100, //!< RESULT_HOST_NOT_FOUND
        RESULT_TRY_AGAIN      = -101, //!< RESULT_TRY_AGAIN
        RESULT_NO_RECOVERY    = -102, //!< RESULT_NO_RECOVERY
        RESULT_NO_DATA        = -103, //!< RESULT_NO_DATA

        RESULT_UNKNOWN        = -1000,//!< RESULT_UNKNOWN
    };


    /**
     * Create a new socket. Corresponds to BSD socket function socket().
     * @note SIGPIPE is disabled on applicable platforms. This has the implication
     * that Receive can return zero bytes when the connection is closed by remote peer.
     * @param type Socket type
     * @param protocol Protocol
     * @param socket Pointer to created socket
     * @return RESULT_OK on succcess
     */
    Result New(Domain domain, Type type, enum Protocol protocol, Socket* socket);

    /**
     * Delete a socket. Corresponds to BSD socket function close()
     * @param socket Socket to close
     * @return RESULT_OK on success
     */
    Result Delete(Socket socket);

    /**
     * Get underlying file descriptor
     * @param socket socket to get fd for
     * @return file-descriptor
     */
    int GetFD(Socket socket);

    /**
     * Set reuse socket address option on socket. Socket option SO_REUSEADDR on most platforms
     * @param socket Socket to set reuse address to
     * @param reuse True if reuse
     * @return RESULT_OK on success
     */
    Result SetReuseAddress(Socket socket, bool reuse);


    /**
     * Set broadcast address option on socket. Socket option SO_BROADCAST on most platforms.
     * @param socket Socket to set reuse address to
     * @param broadcast True if broadcast
     * @return RESULT_OK on success
     */
    Result SetBroadcast(Socket socket, bool broadcast);

    /**
     * Set blocking option on a socket
     * @param socket Socket to set blocking on
     * @param blocking True to block
     * @return RESULT_OK on success
     */
    Result SetBlocking(Socket socket, bool blocking);

    /**
     * Set TCP_NODELAY on socket
     * @param socket Socket to set TCP_NODELAY on
     * @param no_delay True for no delay
     * @return RESULT_OK on success
     */
    Result SetNoDelay(Socket socket, bool no_delay);

    /**
     * Set socket send timeout
     * @note Timeout resolution might be in milliseconds, e.g. windows. Use values
     *       larger than or equal to 1000.
     * @param socket socket
     * @param timeout timeout in microseconds
     * @return RESULT_OK on success
     */
    Result SetSendTimeout(Socket socket, uint64_t timeout);

    /**
     * Set socket receive timeout
     * @note Timeout resolution might be in milliseconds, e.g. windows. Use values
     *       larger than or equal to 1000
     * @param socket socket
     * @param timeout timeout in microseconds
     * @return RESULT_OK on success
     */
    Result SetReceiveTimeout(Socket socket, uint64_t timeout);


}

#endif // DMSDK_SOCKET_H