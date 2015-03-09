#include "socket.h"
#include "math.h"
#include "dstrings.h"
#include "log.h"

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

#include "log.h"
#include "socket_private.h"

namespace dmSocket
{


    Result Initialize()
    {
#ifdef _WIN32
        WORD version_requested = MAKEWORD(2, 2);
        WSADATA wsa_data;
        int err = WSAStartup(version_requested, &wsa_data);
        if (err != 0)
        {
            return NativeToResult(DM_SOCKET_ERRNO);
        }
        else
        {
            return RESULT_OK;
        }
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

    Result New(Type type, Protocol protocol, Socket* socket)
    {
        int s = ::socket(PF_INET, type, protocol);
        if (s < 0)
        {
            return NativeToResult(DM_SOCKET_ERRNO);
        }
        else
        {
#if defined(__MACH__)
            int set = 1;
            // Disable SIGPIPE on socket. On Linux MSG_NOSIGNAL is passed on send(.)
            setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
#endif
            *socket = s;
            return RESULT_OK;
        }
    }

    static Result SetSockoptBool(Socket socket, int level, int name, bool option)
    {
        int on = (int) option;
        int ret = setsockopt(socket, level, name, (char *) &on, sizeof(on));
        if (ret < 0)
        {
            return NativeToResult(DM_SOCKET_ERRNO);
        }
        else
        {
            return RESULT_OK;
        }
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
        struct ip_mreq group;
        group.imr_multiaddr.s_addr = htonl(multi_addr);
        group.imr_interface.s_addr = htonl(interface_addr);

        int ret = setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group));
        if (ret < 0)
            return NativeToResult(DM_SOCKET_ERRNO);

        uint8_t ttl_byte = (uint8_t) ttl;
        ret = setsockopt(socket, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl_byte, sizeof(ttl_byte));
        if (ret < 0)
            return NativeToResult(DM_SOCKET_ERRNO);

        return RESULT_OK;
    }

    Result SetMulticastIf(Socket socket, Address address)
    {
        struct in_addr if_addr;
        if_addr.s_addr = htonl(address);
        int ret = setsockopt(socket, IPPROTO_IP, IP_MULTICAST_IF, &if_addr, sizeof(if_addr));
        if (ret != 0)
            return NativeToResult(DM_SOCKET_ERRNO);
        return RESULT_OK;
    }

    Result Delete(Socket socket)
    {
#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
        int ret = close(socket);
#else
        int ret = closesocket(socket);
#endif
        if (ret < 0)
        {
            return NativeToResult(DM_SOCKET_ERRNO);
        }
        else
        {
            return RESULT_OK;
        }
    }

    int GetFD(Socket socket)
    {
        return socket;
    }

    Result Accept(Socket socket, Address* address, Socket* accept_socket)
    {
        struct sockaddr_in sock_addr;
#ifdef _WIN32
        int sock_addr_len = sizeof(sock_addr);
#else
        socklen_t sock_addr_len = sizeof(sock_addr);
#endif
        int s = accept(socket, (struct sockaddr *) &sock_addr, &sock_addr_len);

        if (s < 0)
        {
            *address = ntohl(sock_addr.sin_addr.s_addr);
            *accept_socket = s;
            return NativeToResult(DM_SOCKET_ERRNO);
        }
        else
        {
            *accept_socket = s;
            return RESULT_OK;
        }
    }

    Result Bind(Socket socket, Address address, int port)
    {
        struct sockaddr_in sock_addr;
        sock_addr.sin_family = AF_INET;
        sock_addr.sin_addr.s_addr = htonl(address);
        sock_addr.sin_port  = htons(port);
        int ret = bind(socket, (struct sockaddr *) &sock_addr, sizeof(sock_addr));

        if (ret < 0)
        {
            return NativeToResult(DM_SOCKET_ERRNO);
        }
        else
        {
            return RESULT_OK;
        }
    }

    Result Connect(Socket socket, Address address, int port)
    {
        struct sockaddr_in sock_addr;
        sock_addr.sin_family = AF_INET;
        sock_addr.sin_addr.s_addr = htonl(address);
        sock_addr.sin_port  = htons(port);
        int ret =connect(socket, (struct sockaddr *) &sock_addr, sizeof(sock_addr));

        if (ret < 0)
        {
            return NativeToResult(DM_SOCKET_ERRNO);
        }
        else
        {
            return RESULT_OK;
        }
    }

    Result Listen(Socket socket, int backlog)
    {
        int ret = listen(socket, backlog);
        if (ret < 0)
        {
            return NativeToResult(DM_SOCKET_ERRNO);
        }
        else
        {
            return RESULT_OK;
        }
    }

    Result Shutdown(Socket socket, ShutdownType how)
    {
        int ret = shutdown(socket, how);
        if (ret < 0)
        {
            return NativeToResult(DM_SOCKET_ERRNO);
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
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(to_addr);
        addr.sin_port = htons(to_port);

        *sent_bytes = 0;
#if defined(_WIN32)
        int s = sendto(socket, (const char*) buffer, length, 0,  (const sockaddr*) &addr, sizeof(addr));
#else
        ssize_t s = sendto(socket, buffer, length, 0, (const sockaddr*) &addr, sizeof(addr));
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
        struct sockaddr_in from;

        *received_bytes = 0;
#ifdef _WIN32
        int fromlen = sizeof(from);
        int r = recvfrom(socket, (char*) buffer, length, 0, (struct sockaddr*) &from, &fromlen);
#else
        socklen_t fromlen = sizeof(from);
        int r = recvfrom(socket, buffer, length, 0, (struct sockaddr*) &from, &fromlen);
#endif

        if (r < 0)
        {
            return NativeToResultCompat(DM_SOCKET_ERRNO);
        }
        else
        {
            *received_bytes = r;
            *from_addr = ntohl(from.sin_addr.s_addr);
            *from_port = ntohs(from.sin_port);
            return RESULT_OK;
        }
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
        timeout_val.tv_sec = 0;
        timeout_val.tv_usec = timeout;

        int r;
        if (timeout < 0)
            r = select(selector->m_Nfds + 1, &selector->m_FdSets[SELECTOR_KIND_READ], &selector->m_FdSets[SELECTOR_KIND_WRITE], &selector->m_FdSets[SELECTOR_KIND_EXCEPT], 0);
        else
            r = select(selector->m_Nfds + 1, &selector->m_FdSets[SELECTOR_KIND_READ], &selector->m_FdSets[SELECTOR_KIND_WRITE], &selector->m_FdSets[SELECTOR_KIND_EXCEPT], &timeout_val);

        if (r < 0)
        {
            return NativeToResult(DM_SOCKET_ERRNO);
        }
        else
        {
            return RESULT_OK;
        }
    }

    Result GetName(Socket socket, Address* address, uint16_t* port)
    {
        struct sockaddr_in addr;
#ifdef _WIN32
        int addr_len = sizeof(addr);
#else
        socklen_t addr_len = sizeof(addr);
#endif
        int r = getsockname(socket, (sockaddr *) &addr, &addr_len);
        if (r < 0)
        {
            return NativeToResult(DM_SOCKET_ERRNO);
        }
        else
        {
            *address = ntohl(addr.sin_addr.s_addr);
            *port = ntohs(addr.sin_port);
            return RESULT_OK;
        }
    }

    Result GetHostname(char* hostname, int hostname_length)
    {
        int r = gethostname(hostname, hostname_length);
        if (hostname_length > 0)
            hostname[hostname_length-1] = '\0';

        if (r < 0)
        {
            return NativeToResult(DM_SOCKET_ERRNO);
        }
        else
        {
            return RESULT_OK;
        }
    }

#if !(defined(__MACH__) && (defined(__arm__) || defined(__arm64__)))
    Result GetLocalAddress(Address* address)
    {
#ifdef __ANDROID__
        // NOTE: This method should probably be used on Linux as well
        // fall-back to 127.0.0.1
        *address = AddressFromIPString("127.0.0.1");

        struct ifreq *ifr;
        struct ifconf ifc;
        char buf[2048];
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) {
            // We just fall-back to 127.0.0.1
            return RESULT_OK;
        }

        memset(&ifc, 0, sizeof(ifc));
        ifr = (ifreq*) buf;
        ifc.ifc_ifcu.ifcu_req = ifr;
        ifc.ifc_len = sizeof(buf);
        if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
            // We just fall-back to 127.0.0.1
            return RESULT_OK;
        }

        // NOTE: This is not compatible with BSD. You can't assume
        // equivalent size for all items
        int numif = ifc.ifc_len / sizeof(struct ifreq);
        for (int i = 0; i < numif; i++) {
          struct ifreq *r = &ifr[i];
          struct sockaddr_in *sin = (struct sockaddr_in *)&r->ifr_addr;
          if (strcmp(r->ifr_name, "lo") != 0) {
              *address = ntohl(sin->sin_addr.s_addr);
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
        char hostname[256];
        Result r = dmSocket::GetHostname(hostname, sizeof(hostname));
        if (r != dmSocket::RESULT_OK)
            return r;

        r = dmSocket::GetHostByName(hostname, address);
        if (r != RESULT_OK)
        {
            *address = AddressFromIPString("127.0.0.1");
            static bool warning_emitted = false;
            if (!warning_emitted)
            {
                dmLogWarning("No IP found for local hostname %s. Fallbacks to 127.0.0.1", hostname);
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
            return NativeToResult(DM_SOCKET_ERRNO);
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
            return NativeToResult(DM_SOCKET_ERRNO);
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
            return NativeToResult(DM_SOCKET_ERRNO);
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
            return NativeToResult(DM_SOCKET_ERRNO);
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

    Address AddressFromIPString(const char* address)
    {
        return ntohl(inet_addr(address));
    }

    char* AddressToIPString(Address address)
    {
        struct in_addr a;
        a.s_addr = htonl(address);
        return strdup(inet_ntoa(a));
    }

    Result GetHostByName(const char* name, Address* address)
    {
#ifdef __linux__
        // gethostbyname is *not* reentrant on linux

        struct hostent hbuf, *host;
        char tmp[1024];
        int gh_errno;
        int r = gethostbyname_r(name, &hbuf, tmp, sizeof(tmp), &host, &gh_errno);
        if (r == 0) {
            if (host) {
                *address = ntohl(*((unsigned long *) host->h_addr_list[0]));
                return RESULT_OK;
            } else {
                // gethostbyname_r is strange. Might return ok but with no host
                return RESULT_HOST_NOT_FOUND;
            }
        } else {
            return HNativeToResult(gh_errno);
        }
#else
        struct hostent* host = gethostbyname(name);
        if (host) {
            *address = ntohl(*((unsigned long *) host->h_addr_list[0]));
            return RESULT_OK;
        } else {
            return HNativeToResult(DM_SOCKET_HERRNO);
        }
#endif
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
