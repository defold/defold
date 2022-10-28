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

#ifndef DMSDK_SOCKET_H
#define DMSDK_SOCKET_H

#include <stdint.h>
#include <dmsdk/dlib/sockettypes.h>
#include <dmsdk/dlib/file_descriptor.h>

/*# SDK Socket API documentation
 * Socket functions.
 *
 * @document
 * @name Socket
 * @namespace dmSocket
 * @path engine/dlib/src/dmsdk/dlib/socket.h
 */
namespace dmSocket
{
    /*#
     * Socket default timeout value
     * @variable
     * @name SOCKET_TIMEOUT
     */
    const int SOCKET_TIMEOUT = 500 * 1000;

    /*# Socket type definition
     * @typedef
     * @name Socket
     * @note Use dmSocket::INVALID_SOCKET_HANDLE instead of zero for unset values. This is an exception
     * from all other handles.
     */
    typedef int Socket;

    /*#
     * Invalid socket handle
     * @variable
     * @name INVALID_SOCKET_HANDLE
     */
    const Socket INVALID_SOCKET_HANDLE = 0xffffffff;

    enum Flags
    {
        FLAGS_UP = (1 << 0),
        FLAGS_RUNNING = (1 << 1),
        FLAGS_INET = (1 << 2),
        FLAGS_LINK = (1 << 3),
    };

    /*# domain type
     * Domain type
     * @enum
     * @name Domain
     * @member DOMAIN_MISSING
     * @member DOMAIN_IPV4
     * @member DOMAIN_IPV6
     * @member DOMAIN_UNKNOWN
     */
    enum Domain
    {
        DOMAIN_MISSING,
        DOMAIN_IPV4,
        DOMAIN_IPV6,
        DOMAIN_UNKNOWN,
    };

    /*# socket type
     * Socket type
     * @enum
     * @name Type
     * @member TYPE_STREAM
     * @member TYPE_DGRAM
     */
    enum Type
    {
        TYPE_STREAM,
        TYPE_DGRAM,
    };

    /*# network protocol
     * Network protocol
     * @enum
     * @name Protocol
     * @member PROTOCOL_TCP
     * @member PROTOCOL_UDP
     */
    enum Protocol
    {
        PROTOCOL_TCP,
        PROTOCOL_UDP,
    };

    /*# socket shutdown type
     * Socket shutdown type
     * @enum
     * @name ShutdownType
     * @member SHUTDOWNTYPE_READ
     * @member SHUTDOWNTYPE_WRITE
     * @member SHUTDOWNTYPE_READWRITE
     */
    enum ShutdownType
    {
        SHUTDOWNTYPE_READ,
        SHUTDOWNTYPE_WRITE,
        SHUTDOWNTYPE_READWRITE,
    };

    /*# socket result
     * Socket result
     * @enum
     * @name Result
     * @member RESULT_OK 0
     * @member RESULT_ACCES -1
     * @member RESULT_AFNOSUPPORT -2
     * @member RESULT_WOULDBLOCK -3
     * @member RESULT_BADF -4
     * @member RESULT_CONNRESET -5
     * @member RESULT_DESTADDRREQ -6
     * @member RESULT_FAULT -7
     * @member RESULT_HOSTUNREACH -8
     * @member RESULT_INTR -9
     * @member RESULT_INVAL -10
     * @member RESULT_ISCONN -11
     * @member RESULT_MFILE -12
     * @member RESULT_MSGSIZE -13
     * @member RESULT_NETDOWN -14
     * @member RESULT_NETUNREACH -15
     * @member RESULT_NOBUFS -17
     * @member RESULT_NOTCONN -20
     * @member RESULT_NOTSOCK -22
     * @member RESULT_OPNOTSUPP -23
     * @member RESULT_PIPE -24
     * @member RESULT_PROTONOSUPPORT -25
     * @member RESULT_PROTOTYPE -26
     * @member RESULT_TIMEDOUT -27
     * @member RESULT_ADDRNOTAVAIL -28
     * @member RESULT_CONNREFUSED -29
     * @member RESULT_ADDRINUSE -30
     * @member RESULT_CONNABORTED -31
     * @member RESULT_INPROGRESS -32
     * @member RESULT_HOST_NOT_FOUND -100
     * @member RESULT_TRY_AGAIN -101
     * @member RESULT_NO_RECOVERY -102
     * @member RESULT_NO_DATA -103
     * @member RESULT_UNKNOWN -1000
     */
    enum Result
    {
        RESULT_OK             = 0,

        RESULT_ACCES          = -1,
        RESULT_AFNOSUPPORT    = -2,
        RESULT_WOULDBLOCK     = -3,
        RESULT_BADF           = -4,
        RESULT_CONNRESET      = -5,
        RESULT_DESTADDRREQ    = -6,
        RESULT_FAULT          = -7,
        RESULT_HOSTUNREACH    = -8,
        RESULT_INTR           = -9,
        RESULT_INVAL          = -10,
        RESULT_ISCONN         = -11,
        RESULT_MFILE          = -12,
        RESULT_MSGSIZE        = -13,
        RESULT_NETDOWN        = -14,
        RESULT_NETUNREACH     = -15,
        //RESULT_NFILE          = -16,
        RESULT_NOBUFS         = -17,
        //RESULT_NOENT          = -18,
        //RESULT_NOMEM          = -19,
        RESULT_NOTCONN        = -20,
        //RESULT_NOTDIR         = -21,
        RESULT_NOTSOCK        = -22,
        RESULT_OPNOTSUPP      = -23,
        RESULT_PIPE           = -24,
        RESULT_PROTONOSUPPORT = -25,
        RESULT_PROTOTYPE      = -26,
        RESULT_TIMEDOUT       = -27,

        RESULT_ADDRNOTAVAIL   = -28,
        RESULT_CONNREFUSED    = -29,
        RESULT_ADDRINUSE      = -30,
        RESULT_CONNABORTED    = -31,
        RESULT_INPROGRESS     = -32,

        // gethostbyname errors
        RESULT_HOST_NOT_FOUND = -100,
        RESULT_TRY_AGAIN      = -101,
        RESULT_NO_RECOVERY    = -102,
        RESULT_NO_DATA        = -103,

        RESULT_UNKNOWN        = -1000,
    };

    /*# network address
     * Network addresses were previously represented as an uint32_t, but in
     * order to support IPv6 the internal representation was changed to a struct.
     *
     * @struct
     * @name Address
     */
    struct Address
    {
        Address();
        Domain m_family;
        uint32_t m_address[4];
    };

    /*# create a socket
     * Create a new socket. Corresponds to BSD socket function socket().
     * @note SIGPIPE is disabled on applicable platforms. This has the implication
     * that Receive can return zero bytes when the connection is closed by remote peer.
     * @name New
     * @param type [type:Type] Socket type
     * @param protocol [type:Protocol] Protocol
     * @param socket [type:Socket*] Pointer to socket
     * @return [type:Result] RESULT_OK on succcess
     */
    Result New(Domain domain, Type type, enum Protocol protocol, Socket* socket);

    /*# delete a socket
     * Delete a socket. Corresponds to BSD socket function close()
     * @name Delete
     * @param socket [type:Socket] Socket to close
     * @return [type:Result] RESULT_OK on success
     */
    Result Delete(Socket socket);

    /*# make a connection
     * Initiate a connection on a socket
     * @name Connect
     * @param socket [type:Socket] Socket to initiate connection on
     * @param address [type:Address] Address to connect to
     * @param port [type:int] Port to connect to
     * @return [type:Result] RESULT_OK on success
     */
    Result Connect(Socket socket, Address address, int port);

    /*# close socket
     * Shutdown part of a socket connection
     * @name Shutdown
     * @param socket [type:Socket] Socket to shutdown connection ow
     * @param how [type:ShutdownType] Shutdown type
     * @return [type:Result] RESULT_OK on success
     */
    Result Shutdown(Socket socket, ShutdownType how);

    /*# get underlying file descriptor
     * Get underlying file descriptor
     * @name GetFD
     * @param socket [type:Socket] socket to get fd for
     * @return [type:int] file-descriptor
     */
    int GetFD(Socket socket);

    /*#
     * Set reuse socket address option on socket. Socket option SO_REUSEADDR on most platforms
     * @name SetReuseAddress
     * @param socket [type:Socket] Socket to set reuse address to
     * @param reuse [type:bool] True if reuse
     * @return [type:Result] RESULT_OK on success
     */
    Result SetReuseAddress(Socket socket, bool reuse);

    /*#
     * Set broadcast address option on socket. Socket option SO_BROADCAST on most platforms.
     * @name SetBroadcast
     * @param socket [type:Socket] Socket to set reuse address to
     * @param broadcast [type:bool] True if broadcast
     * @return [type:Result] RESULT_OK on success
     */
    Result SetBroadcast(Socket socket, bool broadcast);

    /*#
     * Set blocking option on a socket
     * @name SetBlocking
     * @param socket [type:Socket] Socket to set blocking on
     * @param blocking [type:bool] True to block
     * @return [type:Result] RESULT_OK on success
     */
    Result SetBlocking(Socket socket, bool blocking);

    /*#
     * Set TCP_NODELAY on socket
     * @name SetNoDelay
     * @param socket [type:Socket] Socket to set TCP_NODELAY on
     * @param no_delay [type:bool] True for no delay
     * @return [type:Result] RESULT_OK on success
     */
    Result SetNoDelay(Socket socket, bool no_delay);

    /*#
     * Set TCP_QUICKACK on socket
     * @note This is a no op on platforms that doesn't support it
     * @name SetQuickAck
     * @param socket [type:Socket] Socket to set TCP_QUICKACK on
     * @param use_quick_ack [type:bool] False to disable quick ack
     * @return [type:Result] RESULT_OK on success
     */
    Result SetQuickAck(Socket socket, bool use_quick_ack);

    /*#
     * Set socket send timeout
     * @note Timeout resolution might be in milliseconds, e.g. windows. Use values
     *       larger than or equal to 1000.
     * @name SetSendTimeout
     * @param socket [type:Socket] socket
     * @param timeout [type:uint64_t] timeout in microseconds
     * @return [type:Result] RESULT_OK on success
     */
    Result SetSendTimeout(Socket socket, uint64_t timeout);

    /*#
     * Set socket receive timeout
     * @note Timeout resolution might be in milliseconds, e.g. windows. Use values
     *       larger than or equal to 1000
     * @name SetReceiveTimeout
     * @param socket [type:Socket] socket
     * @param timeout [type:uint64_t] timeout in microseconds
     * @return [type:Result] RESULT_OK on success
     */
    Result SetReceiveTimeout(Socket socket, uint64_t timeout);

    /*# Send a message on a socket
     * @name Send
     * @param socket [type:Socket] Socket to send a message on
     * @param buffer [type:void*] Buffer to send
     * @param length [type:int] Length of buffer to send
     * @param sent_bytes[out] [type:int*] Number of bytes sent (result)
     * @return [type:Result] RESULT_OK on success
     * @note For dmSocket::Recv() and dmSocket::Send() function ETIMEDOUT is translated to EWOULDBLOCK
     * on win32 for compatibility with BSD sockets.
     */
    Result Send(Socket socket, const void* buffer, int length, int* sent_bytes);

    /*# Receive data on a socket
     * @name Receive
     * @param socket [type:Socket] Socket to receive data on
     * @param buffer[out] [type:void*] Buffer to receive to
     * @param length [type:int] Receive buffer length
     * @param received_bytes[out] [type:int*] Number of received bytes (result)
     * @return [type:Result] RESULT_OK on success
     * @note For dmSocket::Recv() and dmSocket::Send() function ETIMEDOUT is translated to EWOULDBLOCK
     * on win32 for compatibility with BSD sockets.
     */
    Result Receive(Socket socket, void* buffer, int length, int* received_bytes);

    /*# get host by name
     * Get host by name
     * @name GetHostByName
     * @param name [type:const char*] Hostname to resolve
     * @param address [type:Address*] Host address result
     * @param ipv4 [type:bool] Whether or not to search for IPv4 addresses
     * @param ipv6 [type:bool] Whether or not to search for IPv6 addresses
     * @return [type:Result] RESULT_OK on success
     */
    Result GetHostByName(const char* name, Address* address, bool ipv4 = true, bool ipv6 = true);

    /*# get host by name with timeout and cancelability
     * Get host by name with timeout and cancelability
     * @note On HTML5, this function is a wrapper for dmSocket::GetHostByName
     * @name GetHostByName
     * @param name [type:const char*] Hostname to resolve
     * @param address [type:Address*] Host address result
     * @param timeout [type:uint64_t] Timeout in microseconds
     * @param cancelflag [type:int*] if non null and set, will abort the call
     * @param ipv4 [type:bool] Whether or not to search for IPv4 addresses
     * @param ipv6 [type:bool] Whether or not to search for IPv6 addresses
     * @return [type:Result] RESULT_OK on success
     */
    Result GetHostByNameT(const char* name, Address* address, uint64_t timeout, int* cancelflag, bool ipv4 = true, bool ipv6 = true);

    /*# Convert result value to string
     * @name ResultToString
     * @param result [type:Result] Result to convert
     * @return [type:const char*] Result as string
     */
    const char* ResultToString(Result result);

    /*#
     * Selector kind
     * @enum
     * @name SelectorKind
     * @member SELECTOR_KIND_READ
     * @member SELECTOR_KIND_WRITE
     * @member SELECTOR_KIND_EXCEPT
     */
    enum SelectorKind
    {
        SELECTOR_KIND_READ   = 0,
        SELECTOR_KIND_WRITE  = 1,
        SELECTOR_KIND_EXCEPT = 2,
    };

    /*#
     * Selector
     * @struct
     * @name Selector
     */
    struct Selector
    {
        dmFileDescriptor::Poller m_Poller;
        Selector();
    };

    /*#
     * Clear selector for socket. Similar to FD_CLR
     * @name SelectorClear
     * @param selector [type:Selector*] Selector
     * @param selector_kind [type:SelectorKind] Kind to clear
     * @param socket [type:Socket] Socket to clear
     * @return [type:void]
     */
    void SelectorClear(Selector* selector, SelectorKind selector_kind, Socket socket);

    /*#
     * Set selector for socket. Similar to FD_SET
     * @name SelectorSet
     * @param selector [type:Selector*] Selector
     * @param selector_kind [type:SelectorKind] Kind to clear
     * @param socket [type:Socket] Socket to set
     * @return [type:void]
     */
    void SelectorSet(Selector* selector, SelectorKind selector_kind, Socket socket);

    /*#
     * Check if selector is set. Similar to FD_ISSET
     * @name SelectorIsSet
     * @param selector [type:Selector*] Selector
     * @param selector_kind [type:SelectorKind] Selector kind
     * @param socket [type:Socket] Socket to check for
     * @return [type:bool] True if set.
     */
    bool SelectorIsSet(Selector* selector, SelectorKind selector_kind, Socket socket);

    /*#
     * Clear selector (all kinds). Similar to FD_ZERO
     * @name SelectorZero
     * @param selector [type:Selector*] Selector
     * @return [type:void]
     */
    void SelectorZero(Selector* selector);

    /*#
     * Select for pending data
     * @name Select
     * @param selector [type:Selector*] Selector
     * @param timeout [type:int] Timeout. For blocking pass -1. (microseconds)
     * @return [type:Result] RESULT_OK on success
     */
    Result Select(Selector* selector, int32_t timeout);
}

#endif // DMSDK_SOCKET_H
