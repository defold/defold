#ifndef DM_SOCKET_H
#define DM_SOCKET_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ostream>

#if defined(__linux__) || defined(__MACH__) || defined(ANDROID) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/select.h>
#include <netinet/in.h>
#elif defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define snprintf _snprintf
typedef int socklen_t;
#else
#error "Unsupported platform"
#endif

/**
 * Socket abstraction
 * @note For Recv* and Send* function ETIMEDOUT is translated to EWOULDBLOCK
 * on win32 for compatibility with BSD sockets.
 */
namespace dmSocket
{
    /**
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
     * Socket handle
     * @note Use INVALID_SOCKET_HANDLE instead of zero for unset values. This is an exception
     * from all other handles.
     */
    typedef int Socket;

    enum SelectorKind
    {
        SELECTOR_KIND_READ   = 0,
        SELECTOR_KIND_WRITE  = 1,
        SELECTOR_KIND_EXCEPT = 2,
    };

    enum Flags
    {
        FLAGS_UP = (1 << 0),
        FLAGS_RUNNING = (1 << 1),
        FLAGS_INET = (1 << 2),
        FLAGS_LINK = (1 << 3),
    };

    struct Selector;
    void SelectorZero(Selector* selector);

    /**
     * Selector. Do not use directly. Use access functions related to Select()
     */
    struct Selector
    {
        fd_set m_FdSets[3];
        int    m_Nfds;

        Selector()
        {
            SelectorZero(this);
        }
    };

    /**
     * Invalid socket handle
     */
    const Socket INVALID_SOCKET_HANDLE = 0xffffffff;

    /**
     * Domain type
     */
    enum Domain
    {
        DOMAIN_MISSING = 0x0,     //!< DOMAIN_MISSING
        DOMAIN_IPV4 = AF_INET,    //!< DOMAIN_IPV4
        DOMAIN_IPV6 = AF_INET6,   //!< DOMAIN_IPV6
        DOMAIN_UNKNOWN = 0xff,    //!< DOMAIN_UNKNOWN
    };

    /**
     * Socket type
     */
    enum Type
    {
        TYPE_STREAM = SOCK_STREAM,//!< TYPE_STREAM
        TYPE_DGRAM = SOCK_DGRAM,  //!< TYPE_DGRAM
    };

    /**
     * Network protocol
     */
    enum Protocol
    {
        PROTOCOL_TCP = IPPROTO_TCP,//!< PROTOCOL_TCP
        PROTOCOL_UDP = IPPROTO_UDP,//!< PROTOCOL_UDP
    };

    /**
     * Socket shutdown type
     */
    enum ShutdownType
    {
#if defined(__linux__) || defined(__MACH__) || defined(__AVM2__) || defined(__EMSCRIPTEN__)
        SHUTDOWNTYPE_READ      = SHUT_RD,
        SHUTDOWNTYPE_WRITE     = SHUT_WR,
        SHUTDOWNTYPE_READWRITE = SHUT_RDWR,
#else
        SHUTDOWNTYPE_READ      = SD_RECEIVE,//!< SHUTDOWNTYPE_READ
        SHUTDOWNTYPE_WRITE     = SD_SEND,   //!< SHUTDOWNTYPE_WRITE
        SHUTDOWNTYPE_READWRITE = SD_BOTH,   //!< SHUTDOWNTYPE_READWRITE
#endif
    };

    /**
     * Network address
     * Network addresses were previously represented as an uint32_t, but in
     * order to support IPv6 the internal representation was changed to a
     * struct.
     */
    struct Address
    {

        Address() {
            m_family = dmSocket::DOMAIN_MISSING;
            memset(m_address, 0x0, sizeof(m_address));
        }

        Domain m_family;
        uint32_t m_address[4];
    };

    /**
     * Comparison operators for dmSocket::Address (network address).
     * These operators are required since network code was initially designed
     * with the assumption that addresses were stored as uint32_t (IPv4), and
     * thus sortable.
     */
    inline bool operator==(const Address& lhs, const Address& rhs)
    {
        return memcmp(lhs.m_address, rhs.m_address, sizeof(struct in6_addr)) == 0;
    }

    inline bool operator< (const Address& lhs, const Address& rhs)
    {
        return memcmp(lhs.m_address, rhs.m_address, sizeof(struct in6_addr)) < 0;
    }

    inline bool operator!=(const Address& lhs, const Address& rhs) { return !operator==(lhs,rhs); }
    inline bool operator> (const Address& lhs, const Address& rhs) { return  operator< (rhs,lhs); }
    inline bool operator<=(const Address& lhs, const Address& rhs) { return !operator> (lhs,rhs); }
    inline bool operator>=(const Address& lhs, const Address& rhs) { return !operator< (lhs,rhs); }

    /**
     * Stream operator for dmSocket::Address (network address).
     * This operator is required so that a GTest failure can print out relevant
     * information. The operator is not, and should never, be used in production
     * code.
     */
    inline std::ostream & operator<<(std::ostream &os, const dmSocket::Address& address) {
        char buffer[39 + 1] = { 0 };
        snprintf(buffer, sizeof(buffer), "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
            address.m_address[0] & 0xff00, address.m_address[0] & 0x00ff,
            address.m_address[1] & 0xff00, address.m_address[1] & 0x00ff,
            address.m_address[2] & 0xff00, address.m_address[2] & 0x00ff,
            address.m_address[3] & 0xff00, address.m_address[3] & 0x00ff);
        return os << buffer;
    }

    struct IfAddr
    {
        char     m_Name[128];
        uint32_t m_Flags;
        Address  m_Address;
        uint8_t  m_MacAddress[6];
    };

    /**
     * Initialize socket system. Network initialization is required on some platforms.
     * @return RESULT_OK on success
     */
    Result Initialize();

    /**
     * Finalize socket system.
     * @return RESULT_OK on success
     */
    Result Finalize();

    /**
     * Create a new socket. Corresponds to BSD socket function socket().
     * @note SIGPIPE is disabled on applicable platforms. This has the implication
     * that Receive can return zero bytes when the connection is closed by remote peer.
     * @param type Soccket type
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

    /**
     * Add multicast membership
     * @param socket socket to add membership on
     * @param multi_addr multicast address
     * @param interface_addr interface address
     * @param ttl multicast package time to live
     * @return RESULT_OK
     */
    Result AddMembership(Socket socket, Address multi_addr, Address interface_addr, int ttl);

    /**
     * Set address for outgoing multicast datagrams
     * @param socket socket to set multicast address for
     * @param address address of network interface to use
     * @return RESULT_OK
     */
    Result SetMulticastIf(Socket socket, Address address);

    /**
     * Accept a connection on a socket
     * @param socket Socket to accept connections on
     * @param address Result address parameter
     * @param accept_socket Pointer to accepted socket (result)
     * @return RESULT_OK on success
     */
    Result Accept(Socket socket, Address* address, Socket* accept_socket);

    /**
     * Bind a name to a socket
     * @param socket Socket to bind name to
     * @param address Address to bind
     * @param port Port to bind to
     * @return RESULT_OK on success
     */
    Result Bind(Socket socket, Address address, int port);

    /**
     * Initiate a connection on a socket
     * @param socket Socket to initiate connection on
     * @param address Address to connect to
     * @param port Port to connect to
     * @return RESULT_OK on success
     */
    Result Connect(Socket socket, Address address, int port);

    /**
     * Listen for connections on a socket
     * @param socket Socket to listen on
     * @param backlog Maximum length for the queue of pending connections
     * @return RESULT_OK on success
     */
    Result Listen(Socket socket, int backlog);

    /**
     * Shutdown part of a socket connection
     * @param socket Socket to shutdown connection ow
     * @param how Shutdown type
     * @return RESULT_OK on success
     */
    Result Shutdown(Socket socket, ShutdownType how);

    /**
     * Send a message on a socket
     * @param socket Socket to send a message on
     * @param buffer Buffer to send
     * @param length Length of buffer to send
     * @param sent_bytes Number of bytes sent (result)
     * @return RESULT_OK on success
     */
    Result Send(Socket socket, const void* buffer, int length, int* sent_bytes);

    /**
     * Send a message to a specific address
     * @param socket Socket to send a message on
     * @param buffer Buffer to send
     * @param length Length of buffer to send
     * @param sent_bytes Number of bytes sent (result)
     * @param to_addr To address
     * @param to_port From addres
     * @return RESULT_OK on success
     */
    Result SendTo(Socket socket, const void* buffer, int length, int* sent_bytes, Address to_addr, uint16_t to_port);

    /**
     * Receive data on a socket
     * @param socket Socket to receive data on
     * @param buffer Buffer to receive to
     * @param length Receive buffer length
     * @param received_bytes Number of received bytes (result)
     * @return RESULT_OK on success
     */
    Result Receive(Socket socket, void* buffer, int length, int* received_bytes);

    /**
     * Receive from socket
     * @param socket Socket to receive data on
     * @param buffer Buffer to receive to
     * @param length Receive buffer length
     * @param received_bytes Number of received bytes (result)
     * @param from_addr From address (result)
     * @param from_port To address (result)
     * @return RESULT_OK on success
     */
    Result ReceiveFrom(Socket socket, void* buffer, int length, int* received_bytes,
                       Address* from_addr, uint16_t* from_port);


    /**
     * Clear selector for socket. Similar to FD_CLR
     * @param selector Selector
     * @param selector_kind Kind to clear
     * @param socket Socket to clear
     */
    void SelectorClear(Selector* selector, SelectorKind selector_kind, Socket socket);

    /**
     * Set selector for socket. Similar to FD_SET
     * @param selector Selector
     * @param selector_kind Kind to clear
     * @param socket Socket to set
     */
    void SelectorSet(Selector* selector, SelectorKind selector_kind, Socket socket);

    /**
     * Check if selector is set. Similar to FD_ISSET
     * @param selector Selector
     * @param selector_kind Selector kind
     * @param socket Socket to check for
     * @return True if set.
     */
    bool SelectorIsSet(Selector* selector, SelectorKind selector_kind, Socket socket);

    /**
     * Clear selector (all kinds). Similar to FD_ZERO
     * @param selector Selector
     */
    void SelectorZero(Selector* selector);

    /**
     * Select for pending data
     * @param selector Selector
     * @param timeout Timeout. For blocking pass -1
     * @return RESULT_OK on success
     */
    Result Select(Selector* selector, int32_t timeout);

    /**
     * Get name, address and port for socket
     * @param socket Socket to get name for
     * @param address Address (result)
     * @param port Socket (result)
     * @return RESULT_OK on success
     */
    Result GetName(Socket socket, Address*address, uint16_t* port);

    /**
     * Get local hostname
     * @param hostname hostname buffer
     * @param hostname_length hostname buffer length
     * @return RESULT_OK on success
     */
    Result GetHostname(char* hostname, int hostname_length);

    /**
     * Get first local IP address
     * The function tries to determine the local IP address. If several
     * IP addresses are available only a single is returned
     * @note This function might fallback to 127.0.0.1 if no adapter is found
     *       Sometimes it might be appropriate to run this function periodically
     * @param address address result
     * @return RESULT_OK on success
     */
    Result GetLocalAddress(Address* address);

    /**
     * Get address from ip string
     * @param address IP-string
     * @return Address
     */
    Address AddressFromIPString(const char* address);

    /**
     * Convert address to ip string
     * @param address address to convert
     * @return IP string. The caller is responsible to free the string using free()
     */
    char* AddressToIPString(Address address);

    /**
     * Get host by name.
     * @param name  Hostname to resolve
     * @param address Host address result
     * @param ipv4 Whether or not to search for IPv4 addresses
     * @param ipv6 Whether or not to search for IPv6 addresses
     * @return RESULT_OK on success
     */
    Result GetHostByName(const char* name, Address* address, bool ipv4 = true, bool ipv6 = true);

    /**
     * Get information about network adapters (loopback devices are not included)
     * @note Make sure that addresses is large enough. If too small
     * the result is capped.
     * @param addresses array of if-addresses
     * @param addresses_count count
     * @param count actual count
     */
    void GetIfAddresses(IfAddr* addresses, uint32_t addresses_count, uint32_t* count);

    /**
     * Convert result value to string
     * @param result Result to convert
     * @return Result as string
     */
    const char* ResultToString(Result result);

    /**
     * Check if a network address is empty (all zeroes).
     * @param address The address to check
     * @return True if the address is empty, false otherwise
     */
    bool Empty(Address address);

    /**
     * Return a pointer to the IPv4 buffer of address.
     * @note Make sure the address family of address is actually AF_INET before
     * attempting to retrieve the IPv4 buffer, otherwise an assert will trigger.
     * @param address Pointer to the address containing the buffer
     * @return Pointer to the buffer that holds the IPv4 address
     */
    uint32_t* IPv4(Address* address);

    /**
     * Return a pointer to the IPv6 buffer of address.
     * @note Make sure the address family of address is actually AF_INET6 before
     * attempting to retrieve the IPv6 buffer, otherwise an assert will trigger.
     * @param address Pointer to the address containing the buffer
     * @return Pointer to the buffer that holds the IPv6 address
     */
    uint32_t* IPv6(Address* address);

    /**
     * Checks if a socket was created for IPv4 (AF_INET).
     * @param socket The socket to check
     * @return True if the socket was created for IPv4 communication, false otherwise
     */
    bool IsSocketIPv4(Socket socket);

    /**
     * Checks if a socket was created for IPv6 (AF_INET6).
     * @param socket The socket to check
     * @return True if the socket was created for IPv6 communication, false otherwise
     */
    bool IsSocketIPv6(Socket socket);

    /**
     * Calculate the number of bits that differs between address a and b.
     * @note This is used for the Hamming Distance.
     * @param a The first address to compare
     * @param b The second address to compare
     * @return Number of bits that differs between a and b
     */
    uint32_t BitDifference(Address a, Address b);

}

#endif // DM_SOCKET_H
