#include "socket.h"
#include "math.h"
#include "dstrings.h"
#include "log.h"
#include <assert.h>

#include <fcntl.h>
#include <string.h>

#if defined(__linux__)
#include <linux/if.h>
#endif

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>

#endif

#if defined(_WIN32)
#include <Winsock2.h>
#endif

#include "log.h"
#include "socket_private.h"

// Helper and utility functions
namespace dmSocket
{
#if defined(_WIN32)
    #define DM_SOCKET_NATIVE_TO_RESULT_CASE(x) case WSAE##x: return RESULT_##x
#else
    #define DM_SOCKET_NATIVE_TO_RESULT_CASE(x) case E##x: return RESULT_##x
#endif
    static Result NativeToResult(const char* filename, int line, int r)
    {
        switch (r)
        {
            DM_SOCKET_NATIVE_TO_RESULT_CASE(ACCES);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(AFNOSUPPORT);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(WOULDBLOCK);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(BADF);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(CONNRESET);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(DESTADDRREQ);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(FAULT);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(HOSTUNREACH);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(INTR);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(INVAL);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(ISCONN);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(MFILE);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(MSGSIZE);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(NETDOWN);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(NETUNREACH);
            //DM_SOCKET_NATIVE_TO_RESULT_CASE(NFILE);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(NOBUFS);
            //DM_SOCKET_NATIVE_TO_RESULT_CASE(NOENT);
            //DM_SOCKET_NATIVE_TO_RESULT_CASE(NOMEM);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(NOTCONN);
            //DM_SOCKET_NATIVE_TO_RESULT_CASE(NOTDIR);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(NOTSOCK);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(OPNOTSUPP);
#ifndef _WIN32
            // NOTE: EPIPE is not availble on winsock
            DM_SOCKET_NATIVE_TO_RESULT_CASE(PIPE);
#endif
            DM_SOCKET_NATIVE_TO_RESULT_CASE(PROTONOSUPPORT);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(PROTOTYPE);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(TIMEDOUT);

            DM_SOCKET_NATIVE_TO_RESULT_CASE(ADDRNOTAVAIL);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(CONNREFUSED);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(ADDRINUSE);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(CONNABORTED);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(INPROGRESS);
        }

        // TODO: Add log-domain support
        dmLogError("%s( %d ): SOCKET: Unknown result code %d\n", filename, line, r);
        return RESULT_UNKNOWN;
    }
    #undef DM_SOCKET_NATIVE_TO_RESULT_CASE

    #define NATIVETORESULT(_R_) NativeToResult(__FILE__, __LINE__, _R_)

    // Use this function for BSD socket compatibility
    // However, don't use it blindly as we return code ETIMEDOUT is "lost".
    // The background is that ETIMEDOUT is returned rather than EWOULDBLOCK on windows
    // on socket timeout.
    static Result NativeToResultCompat(int r)
    {
        Result res = NativeToResult(__FILE__, __LINE__, r);
        if (res == RESULT_TIMEDOUT) {
            res = RESULT_WOULDBLOCK;
        }
        return res;
    }

    bool Empty(Address address)
    {
        return address.m_address[0] == 0x0 && address.m_address[1] == 0x0
            && address.m_address[2] == 0x0 && address.m_address[3] == 0x0;
    }

    uint32_t* IPv4(Address* address)
    {
        assert(address->m_family == DOMAIN_IPV4);
        return &address->m_address[3];
    }

    uint32_t* IPv6(Address* address)
    {
        assert(address->m_family == DOMAIN_IPV6);
        return &address->m_address[0];
    }

    bool IsSocketIPv4(Socket socket)
    {
#if defined(_WIN32)
        WSAPROTOCOL_INFO ss = {0};
        socklen_t sslen = sizeof(ss);
        int result = getsockopt( socket, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&ss, (int*)&sslen );
        if (result == 0) {
            return ss.iAddressFamily == AF_INET;
        }
#else
        struct sockaddr_storage ss = { 0 };
        socklen_t sslen = sizeof(ss);
        int result = getsockname(socket, (struct sockaddr*) &ss, &sslen);
        if (result == 0)
        {
            return ss.ss_family == AF_INET;
        }
#endif
        dmLogError("Failed to retrieve address family (%d): %s",
            NATIVETORESULT(DM_SOCKET_ERRNO), ResultToString(NATIVETORESULT(DM_SOCKET_ERRNO)));

        return false;
    }

    bool IsSocketIPv6(Socket socket)
    {
#if defined(_WIN32)
        WSAPROTOCOL_INFO ss = {0};
        socklen_t sslen = sizeof(ss);
        int result = getsockopt( socket, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&ss, (int*)&sslen );
        if (result == 0) {
            return ss.iAddressFamily == AF_INET6;
        }
#else
        struct sockaddr_storage ss = { 0 };
        socklen_t sslen = sizeof(ss);
        int result = getsockname(socket, (struct sockaddr*) &ss, &sslen);
        if (result == 0)
        {
            return ss.ss_family == AF_INET6;
        }
#endif

        dmLogError("Failed to retrieve address family (%d): %s",
            NATIVETORESULT(DM_SOCKET_ERRNO), ResultToString(NATIVETORESULT(DM_SOCKET_ERRNO)));
        return false;
    }

    uint32_t BitDifference(Address a, Address b)
    {
        // Implements the Hamming Distance algorithm.
        // https://en.wikipedia.org/wiki/Hamming_distance
        uint32_t difference = 0;
        for (int i = 0; i < (sizeof(a.m_address) / sizeof(uint32_t)); ++i)
        {
            uint32_t current = a.m_address[i] ^ b.m_address[i];
            while (current != 0)
            {
                difference += current % 2;
                current = current >> 1;
            }
        }

        return difference;
    }

};


namespace dmSocket
{


    Result Initialize()
    {
#ifdef _WIN32
        WORD version_requested = MAKEWORD(2, 2);
        WSADATA wsa_data;
        int result = WSAStartup(version_requested, &wsa_data);
        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
#else
        return RESULT_OK;
#endif
    }

    Result Finalize()
    {
#ifdef _WIN32
        WSACleanup();
#endif
        return RESULT_OK;
    }

    Result New(Domain domain, Type type, Protocol protocol, Socket* socket)
    {
        Socket sock = ::socket(domain, type, protocol);
        *socket = sock;
        if (sock >= 0)
        {
#if defined(__MACH__)
            int set = 1;
            // Disable SIGPIPE on socket. On Linux MSG_NOSIGNAL is passed on send(.)
            setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
#endif
            return RESULT_OK;
        }

        return NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    static Result SetSockoptBool(Socket socket, int level, int name, bool option)
    {
        int on = (int) option;
        int ret = setsockopt(socket, level, name, (char *) &on, sizeof(on));
        return ret >= 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result SetReuseAddress(Socket socket, bool reuse)
    {
        Result r = SetSockoptBool(socket, SOL_SOCKET, SO_REUSEADDR, reuse);
#ifdef SO_REUSEPORT
        if (r != RESULT_OK)
            return r;
        r = SetSockoptBool(socket, SOL_SOCKET, SO_REUSEPORT, reuse);
#endif
        return r;
    }

    Result SetBroadcast(Socket socket, bool broadcast)
    {
        return SetSockoptBool(socket, SOL_SOCKET, SO_BROADCAST, broadcast);
    }

    Result AddMembership(Socket socket, Address multi_addr, Address interface_addr, int ttl)
    {
        int result = -1;
        if (IsSocketIPv4(socket))
        {
            assert(multi_addr.m_family == DOMAIN_IPV4 && interface_addr.m_family == DOMAIN_IPV4);
            struct ip_mreq group;
            group.imr_multiaddr.s_addr = *IPv4(&multi_addr);
            group.imr_interface.s_addr = *IPv4(&interface_addr);
            result = setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group));
            if (result == 0)
            {
                uint8_t ttl_byte = (uint8_t) ttl;
                result = setsockopt(socket, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl_byte, sizeof(ttl_byte));
            }
        } else if (IsSocketIPv6(socket))
        {
            assert(multi_addr.m_family == DOMAIN_IPV6 && interface_addr.m_family == DOMAIN_IPV6);
            assert(false && "Interface membership not implemented for IPv6");
        }
        else
        {
            dmLogError("Failed to add interface membership, unsupported address family!");
            return RESULT_AFNOSUPPORT;
        }

        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result SetMulticastIf(Socket socket, Address address)
    {
        int result = -1;
        if (IsSocketIPv4(socket))
        {
            struct in_addr inaddr;
            memset(&inaddr, 0, sizeof(inaddr));
            inaddr.s_addr = *IPv4(&address);
            result = setsockopt(socket, IPPROTO_IP, IP_MULTICAST_IF, (char *) &inaddr, sizeof(inaddr));
        }
        else if (IsSocketIPv6(socket))
        {
            struct in6_addr inaddr;
            memcpy(&inaddr, IPv6(&address), sizeof(struct in6_addr));
            result = setsockopt(socket, IPPROTO_IP, IP_MULTICAST_IF, (char *) &inaddr, sizeof(inaddr));
        }
        else
        {
            dmLogError("Failed to enable multicast interface, unsupported address family!");
            return RESULT_AFNOSUPPORT;
        }

        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result Delete(Socket socket)
    {
#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
        int result = close(socket);
#else
        int result = closesocket(socket);
#endif
        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    int GetFD(Socket socket)
    {
        return socket;
    }

    Result Accept(Socket socket, Address* address, Socket* accept_socket)
    {
        int result = 0;
        if (IsSocketIPv4(socket))
        {
            struct sockaddr_in sock_addr = { 0 };
            socklen_t addr_len = sizeof(sock_addr);
            result = accept(socket, (struct sockaddr *) &sock_addr, &addr_len);
            address->m_family = DOMAIN_IPV4;
            *IPv4(address) = sock_addr.sin_addr.s_addr;
        }
        else if (IsSocketIPv6(socket))
        {
            struct sockaddr_in6 sock_addr = { 0 };
            socklen_t addr_len = sizeof(sock_addr);
            result = accept(socket, (struct sockaddr *) &sock_addr, &addr_len);
            address->m_family = DOMAIN_IPV6;
            memcpy(IPv6(address), &sock_addr.sin6_addr, sizeof(struct in6_addr));
        }
        else
        {
            dmLogError("Failed to accept connections, unsupported address family!");
            return RESULT_AFNOSUPPORT;
        }

        *accept_socket = result;
        return result >= 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result Bind(Socket socket, Address address, int port)
    {
        int result = 0;
        if (IsSocketIPv4(socket))
        {
            assert(address.m_family == DOMAIN_IPV4);

            struct sockaddr_in sock_addr = { 0 };
            sock_addr.sin_family = AF_INET;
            sock_addr.sin_addr.s_addr = *IPv4(&address);
            sock_addr.sin_port = htons(port);
            result = bind(socket, (struct sockaddr *) &sock_addr, sizeof(sock_addr));
        }
        else if (IsSocketIPv6(socket))
        {
            assert(address.m_family == DOMAIN_IPV6);

            struct sockaddr_in6 sock_addr = { 0 };
            sock_addr.sin6_family = AF_INET6;
            memcpy(&sock_addr.sin6_addr, IPv6(&address), sizeof(struct in6_addr));
            sock_addr.sin6_port = htons(port);
            result = bind(socket, (struct sockaddr *) &sock_addr, sizeof(sock_addr));
        }
        else
        {
            dmLogError("Failed to bind socket, unsupported address family!");
            return RESULT_AFNOSUPPORT;
        }

        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result Connect(Socket socket, Address address, int port)
    {
        int result = 0;
        if (IsSocketIPv4(socket))
        {
            assert(address.m_family == DOMAIN_IPV4);

            struct sockaddr_in sock_addr = { 0 };
            sock_addr.sin_family = AF_INET;
            sock_addr.sin_addr.s_addr = *IPv4(&address);
            sock_addr.sin_port = htons(port);
            result = connect(socket, (struct sockaddr *) &sock_addr, sizeof(sock_addr));
        }
        else if (IsSocketIPv6(socket))
        {
            assert(address.m_family == DOMAIN_IPV6);

            struct sockaddr_in6 sock_addr = { 0 };
            sock_addr.sin6_family = AF_INET6;
            memcpy(&sock_addr.sin6_addr, IPv6(&address), sizeof(struct in6_addr));
            sock_addr.sin6_port = htons(port);
            result = connect(socket, (struct sockaddr *) &sock_addr, sizeof(sock_addr));
        }
        else
        {
            dmLogError("Failed to connect to remote host, unsupported address family!");
            return RESULT_AFNOSUPPORT;
        }

        if (result == -1 && !((NATIVETORESULT(DM_SOCKET_ERRNO) == RESULT_INPROGRESS) || (NATIVETORESULT(DM_SOCKET_ERRNO) == RESULT_WOULDBLOCK)))
        {
            return NATIVETORESULT(DM_SOCKET_ERRNO);
        }

        return RESULT_OK;
    }

    Result Listen(Socket socket, int backlog)
    {
        int result = listen(socket, backlog);
        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result Shutdown(Socket socket, ShutdownType how)
    {
        int ret = shutdown(socket, how);
        if (ret < 0)
        {
            return NATIVETORESULT(DM_SOCKET_ERRNO);
        }
        else
        {
            return RESULT_OK;
        }
    }

    Result Send(Socket socket, const void* buffer, int length, int* sent_bytes)
    {
        *sent_bytes = 0;
#if defined(__linux__)
        ssize_t s = send(socket, buffer, length, MSG_NOSIGNAL);
#elif defined(_WIN32)
        int s = send(socket, (const char*) buffer, length, 0);
#else
        ssize_t s = send(socket, buffer, length, 0);
#endif
        if (s < 0)
        {
            return NativeToResultCompat(DM_SOCKET_ERRNO);
        }
        else
        {
            *sent_bytes = s;
            return RESULT_OK;
        }
    }

    Result SendTo(Socket socket, const void* buffer, int length, int* sent_bytes, Address to_addr, uint16_t to_port)
    {
        int result = 0;
        if (IsSocketIPv4(socket))
        {
            assert(to_addr.m_family == DOMAIN_IPV4);

            struct sockaddr_in sock_addr = { 0 };
            sock_addr.sin_family = AF_INET;
            sock_addr.sin_addr.s_addr = *IPv4(&to_addr);
            sock_addr.sin_port = htons(to_port);

#ifdef _WIN32
            result = (int) sendto(socket, (const char*) buffer, length, 0, (const sockaddr*) &sock_addr, sizeof(sock_addr));
#else
            result = (int) sendto(socket, buffer, length, 0, (const sockaddr*) &sock_addr, sizeof(sock_addr));
#endif
        }
        else if (IsSocketIPv6(socket))
        {
            assert(to_addr.m_family == DOMAIN_IPV6);

            struct sockaddr_in6 sock_addr = { 0 };
            sock_addr.sin6_family = AF_INET6;
            memcpy(&sock_addr.sin6_addr, IPv6(&to_addr), sizeof(struct in6_addr));
            sock_addr.sin6_port = htons(to_port);

#ifdef _WIN32
            result = (int) sendto(socket, (const char*) buffer, length, 0,  (const sockaddr*) &sock_addr, sizeof(sock_addr));
#else
            result = (int) sendto(socket, buffer, length, 0, (const sockaddr*) &sock_addr, sizeof(sock_addr));
#endif
        }
        else
        {
            dmLogError("Failed to send to remote host, unsupported address family!");
            return RESULT_AFNOSUPPORT;
        }

        *sent_bytes = result >= 0 ? result : 0;
        return result >= 0 ? RESULT_OK : NativeToResultCompat(DM_SOCKET_ERRNO);
    }

    Result Receive(Socket socket, void* buffer, int length, int* received_bytes)
    {
        *received_bytes = 0;
#ifdef _WIN32
        int r = recv(socket, (char*) buffer, length, 0);
#else
        int r = recv(socket, buffer, length, 0);
#endif

        if (r < 0)
        {
            return NativeToResultCompat(DM_SOCKET_ERRNO);
        }
        else
        {
            *received_bytes = r;
            return RESULT_OK;
        }
    }

    Result ReceiveFrom(Socket socket, void* buffer, int length, int* received_bytes,
                       Address* from_addr, uint16_t* from_port)
    {
        int result = 0;
        *received_bytes = 0;

        if (IsSocketIPv4(socket))
        {
            struct sockaddr_in sock_addr = { 0 };
            socklen_t addr_len = sizeof(sock_addr);

#ifdef _WIN32
            result = recvfrom(socket, (char*) buffer, length, 0, (struct sockaddr*) &sock_addr, &addr_len);
#else
            result = recvfrom(socket, buffer, length, 0, (struct sockaddr*) &sock_addr, &addr_len);
#endif
            if (result >= 0)
            {
                from_addr->m_family = dmSocket::DOMAIN_IPV4;
                *IPv4(from_addr) = sock_addr.sin_addr.s_addr;
                *from_port = ntohs(sock_addr.sin_port);
                *received_bytes = result;
            }
        }
        else if (IsSocketIPv6(socket))
        {
            struct sockaddr_in6 sock_addr = { 0 };
            socklen_t addr_len = sizeof(sock_addr);

#ifdef _WIN32
            result = recvfrom(socket, (char*) buffer, length, 0, (struct sockaddr*) &sock_addr, &addr_len);
#else
            result = recvfrom(socket, buffer, length, 0, (struct sockaddr*) &sock_addr, &addr_len);
#endif
            if (result >= 0)
            {
                from_addr->m_family = dmSocket::DOMAIN_IPV6;
                memcpy(IPv6(from_addr), &sock_addr.sin6_addr, sizeof(struct in6_addr));
                *from_port = ntohs(sock_addr.sin6_port);
                *received_bytes = result;
            }
        }
        else
        {
            dmLogError("Failed to receive from remote host, unsupported address family!");
            return RESULT_AFNOSUPPORT;
        }

        return result >= 0 ? RESULT_OK : NativeToResultCompat(DM_SOCKET_ERRNO);
    }

    void SelectorClear(Selector* selector, SelectorKind selector_kind, Socket socket)
    {
        FD_CLR(socket, &selector->m_FdSets[selector_kind]);
    }

    void SelectorSet(Selector* selector, SelectorKind selector_kind, Socket socket)
    {
        selector->m_Nfds = dmMath::Max(selector->m_Nfds, socket);
        FD_SET(socket, &selector->m_FdSets[selector_kind]);
    }

    bool SelectorIsSet(Selector* selector, SelectorKind selector_kind, Socket socket)
    {
        return (bool) FD_ISSET(socket, &selector->m_FdSets[selector_kind]);
    }

    void SelectorZero(Selector* selector)
    {
        FD_ZERO(&selector->m_FdSets[SELECTOR_KIND_READ]);
        FD_ZERO(&selector->m_FdSets[SELECTOR_KIND_WRITE]);
        FD_ZERO(&selector->m_FdSets[SELECTOR_KIND_EXCEPT]);
        selector->m_Nfds = 0;
    }

    Result Select(Selector* selector, int32_t timeout)
    {
        timeval timeout_val;
        timeout_val.tv_sec = timeout / 1000000;
        timeout_val.tv_usec = timeout % 1000000;

        int r;
        if (timeout < 0)
            r = select(selector->m_Nfds + 1, &selector->m_FdSets[SELECTOR_KIND_READ], &selector->m_FdSets[SELECTOR_KIND_WRITE], &selector->m_FdSets[SELECTOR_KIND_EXCEPT], 0);
        else
            r = select(selector->m_Nfds + 1, &selector->m_FdSets[SELECTOR_KIND_READ], &selector->m_FdSets[SELECTOR_KIND_WRITE], &selector->m_FdSets[SELECTOR_KIND_EXCEPT], &timeout_val);

        if (r < 0)
        {
            return NATIVETORESULT(DM_SOCKET_ERRNO);
        }
        else if(timeout > 0 && r == 0)
        {
            return RESULT_WOULDBLOCK;
        }
        else
        {
            return RESULT_OK;
        }
    }

    Result GetName(Socket socket, Address* address, uint16_t* port)
    {
        int result = -1;
        if (IsSocketIPv4(socket))
        {
            struct sockaddr_in sock_addr = { 0 };
            socklen_t addr_len = sizeof(sock_addr);
            result = getsockname(socket, (struct sockaddr *) &sock_addr, &addr_len);
            if (result == 0)
            {
                address->m_family = dmSocket::DOMAIN_IPV4;
                *IPv4(address) = sock_addr.sin_addr.s_addr;
                *port = ntohs(sock_addr.sin_port);
            }
        }
        else if (IsSocketIPv6(socket))
        {
            struct sockaddr_in6 sock_addr = { 0 };
            socklen_t addr_len = sizeof(sock_addr);
            result = getsockname(socket, (struct sockaddr *) &sock_addr, &addr_len);
            if (result == 0)
            {
                address->m_family = dmSocket::DOMAIN_IPV6;
                memcpy(IPv6(address), &sock_addr.sin6_addr, sizeof(struct in6_addr));
                *port = ntohs(sock_addr.sin6_port);
            }
        }
        else
        {
            dmLogError("Failed to retrieve socket information, unsupported address family!");
            return RESULT_AFNOSUPPORT;
        }

        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result GetHostname(char* hostname, int hostname_length)
    {
        int r = gethostname(hostname, hostname_length);
        if (hostname_length > 0)
            hostname[hostname_length - 1] = '\0';
        return r == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

#if !(defined(__MACH__) && (defined(__arm__) || defined(__arm64__) || defined(IOS_SIMULATOR)))
    Result GetLocalAddress(Address* address)
    {
#ifdef __ANDROID__
        // NOTE: This method should probably be used on Linux as well
        // We just fall-back to localhost
        dmSocket::GetHostByName("localhost", address);

        struct ifreq *ifr;
        struct ifconf ifc;
        char buf[2048] = { 0 };
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) {
            // We just fall-back to localhost
            return RESULT_OK;
        }

        memset(&ifc, 0, sizeof(ifc));
        ifr = (ifreq*) buf;
        ifc.ifc_ifcu.ifcu_req = ifr;
        ifc.ifc_len = sizeof(buf);
        if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
            // We just fall-back to localhost
            return RESULT_OK;
        }

        // NOTE: This is not compatible with BSD. You can't assume
        // equivalent size for all items
        int numif = ifc.ifc_len / sizeof(struct ifreq);
        for (int i = 0; i < numif; i++) {
            struct ifreq *r = &ifr[i];
            struct sockaddr_in *sin = (struct sockaddr_in *)&r->ifr_addr;
            if (strcmp(r->ifr_name, "lo") != 0)
            {
                if (r->ifr_addr.sa_family == AF_INET)
                {
                    // This is used exclusively for SSDP, and our current SSDP
                    // implementation does not support IPv6. Therefore we'll
                    // only manage IPv4 interfaces.
                    address->m_family = DOMAIN_IPV4;
                    *IPv4(address) = sin->sin_addr.s_addr;
                }
            }
        }

        close(s);
        return RESULT_OK;
#else
        /*
         * Get local address from reverse lookup of hostname
         * The method is potentially fragile. On iOS we iterate
         * over network adapter to the find actual address for en0
         * See socket_cocoa.mm
         */
        char hostname[256] = { 0 };
        Result r = dmSocket::GetHostname(hostname, sizeof(hostname));
        if (r != dmSocket::RESULT_OK)
            return r;

        r = dmSocket::GetHostByName(hostname, address);
        if (r != RESULT_OK)
        {
            dmSocket::GetHostByName("localhost", address);
            static bool warning_emitted = false;
            if (!warning_emitted)
            {
                dmLogWarning("No IP found for local hostname %s. Fallbacks to localhost", hostname);
            }
            warning_emitted = true;
        }

        return RESULT_OK;
#endif
    }
#endif

    Result SetBlocking(Socket socket, bool blocking)
    {
#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
        int flags = fcntl(socket, F_GETFL, 0);
        if (flags < 0)
        {
            return NATIVETORESULT(DM_SOCKET_ERRNO);
        }

        if (blocking)
        {
            flags &= ~O_NONBLOCK;
        }
        else
        {
            flags |= O_NONBLOCK;
        }

        if (fcntl(socket, F_SETFL, flags) < 0)
        {
            return NATIVETORESULT(DM_SOCKET_ERRNO);
        }
        else
        {
            return RESULT_OK;
        }
#else
        u_long arg;
        if (blocking)
        {
            arg = 0;
        }
        else
        {
            arg = 1;
        }

        int ret = ioctlsocket(socket, FIONBIO, &arg);
        if (ret == 0)
        {
            return RESULT_OK;
        }
        else
        {
            return NATIVETORESULT(DM_SOCKET_ERRNO);
        }

#endif
    }

    Result SetNoDelay(Socket socket, bool no_delay)
    {
        return SetSockoptBool(socket, IPPROTO_TCP, TCP_NODELAY, no_delay);
    }

    static Result SetSockoptTime(Socket socket, int level, int name, uint64_t time)
    {
#ifdef WIN32
        DWORD timeval = time / 1000;
        if (time > 0 && timeval == 0) {
            dmLogWarning("Socket timeout requested less than 1ms. Timeout set to 1ms.");
            timeval = 1;
        }
#else
        struct timeval timeval;
        timeval.tv_sec = time / 1000000;
        timeval.tv_usec = time % 1000000;
#endif
        int ret = setsockopt(socket, level, name, (char *) &timeval, sizeof(timeval));
        if (ret < 0)
        {
            return NATIVETORESULT(DM_SOCKET_ERRNO);
        }
        else
        {
            return RESULT_OK;
        }
    }

    Result SetSendTimeout(Socket socket, uint64_t timeout)
    {
        return SetSockoptTime(socket, SOL_SOCKET, SO_SNDTIMEO, timeout);
    }

    Result SetReceiveTimeout(Socket socket, uint64_t timeout)
    {
        return SetSockoptTime(socket, SOL_SOCKET, SO_RCVTIMEO, timeout);
    }

    Address AddressFromIPString(const char* hostname)
    {
        Address address;
        GetHostByName(hostname, &address);
        return address;
    }

    char* AddressToIPString(Address address)
    {
        if (address.m_family == dmSocket::DOMAIN_IPV4)
        {
            // Maximum length of an IPv4 address is 15 characters.
            // 255.255.255.255
            char addrstr[15 + 1] = { 0 };
            inet_ntop(AF_INET, IPv4(&address), addrstr, sizeof(addrstr));
            return strdup(addrstr);
        }
        else if (address.m_family == dmSocket::DOMAIN_IPV6)
        {
            // Maximum (theoretical) length of an IPv6 address is 45 characters.
            // ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255
            char addrstr[45 + 1] = { 0 };
            inet_ntop(AF_INET6, IPv6(&address), addrstr, sizeof(addrstr));
            return strdup(addrstr);
        }
        else
        {
            dmLogError("Failed to convert address from binary, unsupported address family!");
            return 0x0;
        }
    }

    Result GetHostByName(const char* name, Address* address, bool ipv4, bool ipv6)
    {
        Result result = RESULT_HOST_NOT_FOUND;

        memset(address, 0x0, sizeof(Address));
        struct addrinfo hints;
        struct addrinfo* res;

        memset(&hints, 0x0, sizeof(hints));
        if (ipv4 == ipv6)
            hints.ai_family = AF_UNSPEC;
        else if (ipv4)
            hints.ai_family = AF_INET;
        else if (ipv6)
            hints.ai_family = AF_INET6;

        hints.ai_socktype = SOCK_STREAM;

        // getaddrinfo_a is an asynchronous alternative, but it is specific to glibc.
        if (getaddrinfo(name, NULL, &hints, &res) == 0)
        {
            struct addrinfo* iterator = res;
            while (iterator && result == RESULT_HOST_NOT_FOUND)
            {
                // There could be (and probably are) multiple results for the same
                // address. The correct way to handle this would be to return a
                // list of addresses and then try each of them until one succeeds.
                if (ipv4 && iterator->ai_family == AF_INET)
                {
                    sockaddr_in* saddr = (struct sockaddr_in *) iterator->ai_addr;
                    address->m_family = dmSocket::DOMAIN_IPV4;
                    *IPv4(address) = saddr->sin_addr.s_addr;
                    result = RESULT_OK;
                }
                else if (ipv6 && iterator->ai_family == AF_INET6)
                {
                    sockaddr_in6* saddr = (struct sockaddr_in6 *) iterator->ai_addr;
                    address->m_family = dmSocket::DOMAIN_IPV6;
                    memcpy(IPv6(address), &saddr->sin6_addr, sizeof(struct in6_addr));
                    result = RESULT_OK;
                }

                iterator = iterator->ai_next;
            }

            freeaddrinfo(res); // Free the head of the linked list
        }

        return result;
    }

    #define DM_SOCKET_RESULT_TO_STRING_CASE(x) case RESULT_##x: return #x;
    const char* ResultToString(Result r)
    {
        switch (r)
        {
            DM_SOCKET_RESULT_TO_STRING_CASE(OK);

            DM_SOCKET_RESULT_TO_STRING_CASE(ACCES);
            DM_SOCKET_RESULT_TO_STRING_CASE(AFNOSUPPORT);
            DM_SOCKET_RESULT_TO_STRING_CASE(WOULDBLOCK);
            DM_SOCKET_RESULT_TO_STRING_CASE(BADF);
            DM_SOCKET_RESULT_TO_STRING_CASE(CONNRESET);
            DM_SOCKET_RESULT_TO_STRING_CASE(DESTADDRREQ);
            DM_SOCKET_RESULT_TO_STRING_CASE(FAULT);
            DM_SOCKET_RESULT_TO_STRING_CASE(HOSTUNREACH);
            DM_SOCKET_RESULT_TO_STRING_CASE(INTR);
            DM_SOCKET_RESULT_TO_STRING_CASE(INVAL);
            DM_SOCKET_RESULT_TO_STRING_CASE(ISCONN);
            DM_SOCKET_RESULT_TO_STRING_CASE(MFILE);
            DM_SOCKET_RESULT_TO_STRING_CASE(MSGSIZE);
            DM_SOCKET_RESULT_TO_STRING_CASE(NETDOWN);
            DM_SOCKET_RESULT_TO_STRING_CASE(NETUNREACH);
            //DM_SOCKET_RESULT_TO_STRING_CASE(NFILE);
            DM_SOCKET_RESULT_TO_STRING_CASE(NOBUFS);
            //DM_SOCKET_RESULT_TO_STRING_CASE(NOENT);
            //DM_SOCKET_RESULT_TO_STRING_CASE(NOMEM);
            DM_SOCKET_RESULT_TO_STRING_CASE(NOTCONN);
            //DM_SOCKET_RESULT_TO_STRING_CASE(NOTDIR);
            DM_SOCKET_RESULT_TO_STRING_CASE(NOTSOCK);
            DM_SOCKET_RESULT_TO_STRING_CASE(OPNOTSUPP);
            DM_SOCKET_RESULT_TO_STRING_CASE(PIPE);
            DM_SOCKET_RESULT_TO_STRING_CASE(PROTONOSUPPORT);
            DM_SOCKET_RESULT_TO_STRING_CASE(PROTOTYPE);
            DM_SOCKET_RESULT_TO_STRING_CASE(TIMEDOUT);

            DM_SOCKET_RESULT_TO_STRING_CASE(ADDRNOTAVAIL);
            DM_SOCKET_RESULT_TO_STRING_CASE(CONNREFUSED);
            DM_SOCKET_RESULT_TO_STRING_CASE(ADDRINUSE);
            DM_SOCKET_RESULT_TO_STRING_CASE(CONNABORTED);
            DM_SOCKET_RESULT_TO_STRING_CASE(INPROGRESS);

            DM_SOCKET_RESULT_TO_STRING_CASE(HOST_NOT_FOUND);
            DM_SOCKET_RESULT_TO_STRING_CASE(TRY_AGAIN);
            DM_SOCKET_RESULT_TO_STRING_CASE(NO_RECOVERY);
            DM_SOCKET_RESULT_TO_STRING_CASE(NO_DATA);

            DM_SOCKET_RESULT_TO_STRING_CASE(UNKNOWN);
        }
        // TODO: Add log-domain support
        dmLogError("Unable to convert result %d to string", r);

        return "RESULT_UNDEFINED";
    }
    #undef DM_SOCKET_RESULT_TO_STRING_CASE
}
