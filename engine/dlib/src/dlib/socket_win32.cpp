// Copyright 2020-2026 The Defold Foundation
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

#include "socket.h"
#include "socket_private.h"

#include <dmsdk/dlib/log.h>

#include <assert.h>
#include <malloc.h> // malloc/free
#include <stdlib.h> // wcstombs
#include <string.h>

namespace dmSocket
{
    void GetIfAddresses(IfAddr* addresses, uint32_t addresses_count, uint32_t* count)
    {
        *count = 0;
        ULONG                 flags = GAA_FLAG_INCLUDE_PREFIX;
        ULONG                 family = AF_INET;

        ULONG                 out_buf_len;
        PIP_ADAPTER_ADDRESSES paddresses = NULL;

        // Ask for the length first
        DWORD ret = GetAdaptersAddresses(family, flags, NULL, NULL, &out_buf_len);

        if (ret == ERROR_BUFFER_OVERFLOW)
        { // the address pointer is null ofc
            paddresses = (IP_ADAPTER_ADDRESSES*)malloc(out_buf_len);
            ret = GetAdaptersAddresses(family, flags, NULL, paddresses, &out_buf_len);
        }

        PIP_ADAPTER_ADDRESSES       pcurraddresses = NULL;
        PIP_ADAPTER_UNICAST_ADDRESS punicast = NULL;

        if (ret == NO_ERROR)
        {
            pcurraddresses = paddresses;
            while (pcurraddresses && *count < addresses_count)
            {
                if (pcurraddresses->IfType != IF_TYPE_SOFTWARE_LOOPBACK)
                {
                    punicast = pcurraddresses->FirstUnicastAddress;
                    if (punicast)
                    {
                        for (unsigned i = 0; punicast != NULL; i++)
                        {
                            IfAddr* a = &addresses[*count];
                            memset(a, 0, sizeof(*a));

                            wcstombs(a->m_Name, pcurraddresses->FriendlyName, sizeof(a->m_Name));
                            a->m_Name[sizeof(a->m_Name) - 1] = 0;

                            if (pcurraddresses->OperStatus == IfOperStatusUp)
                            {
                                a->m_Flags |= FLAGS_UP;
                                a->m_Flags |= FLAGS_RUNNING;
                            }

                            if (punicast->Address.lpSockaddr->sa_family == AF_INET)
                            {
                                a->m_Flags |= FLAGS_INET;
                                a->m_Address.m_family = DOMAIN_IPV4;
                                sockaddr_in* ia = (sockaddr_in*)punicast->Address.lpSockaddr;
                                *IPv4(&a->m_Address) = ia->sin_addr.s_addr;
                            }
                            else if (punicast->Address.lpSockaddr->sa_family == AF_INET6)
                            {
                                a->m_Flags |= FLAGS_INET;
                                a->m_Address.m_family = DOMAIN_IPV6;
                                sockaddr_in6* ia = (sockaddr_in6*)punicast->Address.lpSockaddr;
                                memcpy(IPv6(&a->m_Address), &ia->sin6_addr, sizeof(struct in6_addr));
                            }

                            a->m_Flags |= FLAGS_LINK;
                            a->m_MacAddress[0] = pcurraddresses->PhysicalAddress[0];
                            a->m_MacAddress[1] = pcurraddresses->PhysicalAddress[1];
                            a->m_MacAddress[2] = pcurraddresses->PhysicalAddress[2];
                            a->m_MacAddress[3] = pcurraddresses->PhysicalAddress[3];
                            a->m_MacAddress[4] = pcurraddresses->PhysicalAddress[4];
                            a->m_MacAddress[5] = pcurraddresses->PhysicalAddress[5];

                            punicast = punicast->Next;
                            *count = *count + 1;
                        }
                    }
                }
                pcurraddresses = pcurraddresses->Next;
            }
        }
        else
        {
            if (ret != ERROR_NO_DATA)
            {
                dmLogError("GetAdaptersAddresses failed (%d)\n", ret);
            }
        }

        free(paddresses);
        return;
    }

    Result PlatformInitialize()
    {
        WORD    version_requested = MAKEWORD(2, 2);
        WSADATA wsa_data;
        int     result = WSAStartup(version_requested, &wsa_data);
        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result PlatformFinalize()
    {
        WSACleanup();
        return RESULT_OK;
    }
} // namespace dmSocket

// Helper and utility functions
namespace dmSocket
{
#define DM_SOCKET_NATIVE_TO_RESULT_CASE(x) \
    case WSAE##x: \
        return RESULT_##x
    Result NativeToResult(const char* filename, int line, int r)
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
            // DM_SOCKET_NATIVE_TO_RESULT_CASE(NFILE);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(NOBUFS);
            // DM_SOCKET_NATIVE_TO_RESULT_CASE(NOENT);
            // DM_SOCKET_NATIVE_TO_RESULT_CASE(NOMEM);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(NOTCONN);
            // DM_SOCKET_NATIVE_TO_RESULT_CASE(NOTDIR);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(NOTSOCK);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(OPNOTSUPP);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(PROTONOSUPPORT);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(PROTOTYPE);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(TIMEDOUT);

            DM_SOCKET_NATIVE_TO_RESULT_CASE(ADDRNOTAVAIL);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(CONNREFUSED);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(ADDRINUSE);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(CONNABORTED);
            DM_SOCKET_NATIVE_TO_RESULT_CASE(INPROGRESS);
            default:
                // TODO: Add log-domain support
                dmLogError("%s( %d ): SOCKET: Unknown result code %d", filename, line, r);
                return RESULT_UNKNOWN;
        }
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
        if (res == RESULT_TIMEDOUT)
        {
            res = RESULT_WOULDBLOCK;
        }
        return res;
    }

    bool IsSocketIPv4(Socket socket)
    {
        WSAPROTOCOL_INFO ss = { 0 };
        socklen_t        sslen = sizeof(ss);
        int              result = getsockopt(socket, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&ss, (int*)&sslen);
        if (result == 0)
        {
            return ss.iAddressFamily == AF_INET;
        }
        dmLogError("Failed to retrieve address family (%d): %s",
                   NATIVETORESULT(DM_SOCKET_ERRNO),
                   ResultToString(NATIVETORESULT(DM_SOCKET_ERRNO)));

        return false;
    }

    bool IsSocketIPv6(Socket socket)
    {
        WSAPROTOCOL_INFO ss = { 0 };
        socklen_t        sslen = sizeof(ss);
        int              result = getsockopt(socket, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&ss, (int*)&sslen);
        if (result == 0)
        {
            return ss.iAddressFamily == AF_INET6;
        }

        dmLogError("Failed to retrieve address family (%d): %s",
                   NATIVETORESULT(DM_SOCKET_ERRNO),
                   ResultToString(NATIVETORESULT(DM_SOCKET_ERRNO)));
        return false;
    }

    static int TypeToNative(Type type)
    {
        switch (type)
        {
            case TYPE_STREAM:
                return SOCK_STREAM;
            case TYPE_DGRAM:
                return SOCK_DGRAM;
        }
        return 0;
    }
    static int ProtocolToNative(Protocol protocol)
    {
        switch (protocol)
        {
            case PROTOCOL_TCP:
                return IPPROTO_TCP;
            case PROTOCOL_UDP:
                return IPPROTO_UDP;
        }
        return 0;
    }
    static int DomainToNative(Domain domain)
    {
        switch (domain)
        {
            case DOMAIN_MISSING:
                return 0x0;
            case DOMAIN_IPV4:
                return AF_INET;
            case DOMAIN_IPV6:
                return AF_INET6;
            case DOMAIN_UNKNOWN:
                return 0xff;
        }
        return 0;
    }

    Result New(Domain domain, Type type, Protocol protocol, Socket* socket)
    {
        Socket sock = (Socket)::socket(DomainToNative(domain), TypeToNative(type), ProtocolToNative(protocol));
        *socket = sock;
        if (sock >= 0)
        {
            return RESULT_OK;
        }

        int result = DM_SOCKET_ERRNO;
        return NATIVETORESULT(result);
    }

    static Result SetSockoptBool(Socket socket, int level, int name, bool option)
    {
        int on = (int)option;
        int ret = setsockopt(socket, level, name, (char*)&on, sizeof(on));
        return ret >= 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result SetReuseAddress(Socket socket, bool reuse)
    {
        return SetSockoptBool(socket, SOL_SOCKET, SO_REUSEADDR, reuse);
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
            result = setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&group, sizeof(group));
            if (result == 0)
            {
                uint8_t ttl_byte = (uint8_t)ttl;
                result = setsockopt(socket, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl_byte, sizeof(ttl_byte));
            }
        }
        else if (IsSocketIPv6(socket))
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
            result = setsockopt(socket, IPPROTO_IP, IP_MULTICAST_IF, (char*)&inaddr, sizeof(inaddr));
        }
#if !defined(DM_IPV6_UNSUPPORTED)
        else if (IsSocketIPv6(socket))
        {
            struct in6_addr inaddr;
            memcpy(&inaddr, IPv6(&address), sizeof(struct in6_addr));
            result = setsockopt(socket, IPPROTO_IP, IP_MULTICAST_IF, (char*)&inaddr, sizeof(inaddr));
        }
#endif // no ipv6
        else
        {
            dmLogError("Failed to enable multicast interface, unsupported address family!");
            return RESULT_AFNOSUPPORT;
        }

        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result Delete(Socket socket)
    {
        if (socket == INVALID_SOCKET_HANDLE)
        {
            return RESULT_BADF;
        }
        int result = closesocket(socket);
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
            socklen_t          addr_len = sizeof(sock_addr);
            result = accept(socket, (struct sockaddr*)&sock_addr, &addr_len);
            address->m_family = DOMAIN_IPV4;
            *IPv4(address) = sock_addr.sin_addr.s_addr;
        }
#if !defined(DM_IPV6_UNSUPPORTED)
        else if (IsSocketIPv6(socket))
        {
            struct sockaddr_in6 sock_addr = { 0 };
            socklen_t           addr_len = sizeof(sock_addr);
            result = accept(socket, (struct sockaddr*)&sock_addr, &addr_len);
            address->m_family = DOMAIN_IPV6;
            memcpy(IPv6(address), &sock_addr.sin6_addr, sizeof(struct in6_addr));
        }
#endif // no ipv6
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
            result = bind(socket, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
        }
#if !defined(DM_IPV6_UNSUPPORTED)
        else if (IsSocketIPv6(socket))
        {
            assert(address.m_family == DOMAIN_IPV6);

            struct sockaddr_in6 sock_addr = { 0 };
            sock_addr.sin6_family = AF_INET6;
            memcpy(&sock_addr.sin6_addr, IPv6(&address), sizeof(struct in6_addr));
            sock_addr.sin6_port = htons(port);
            result = bind(socket, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
        }
#endif // no ipv6
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
            result = connect(socket, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
        }
#if !defined(DM_IPV6_UNSUPPORTED)
        else if (IsSocketIPv6(socket))
        {
            assert(address.m_family == DOMAIN_IPV6);

            struct sockaddr_in6 sock_addr = { 0 };
            sock_addr.sin6_family = AF_INET6;
            memcpy(&sock_addr.sin6_addr, IPv6(&address), sizeof(struct in6_addr));
            sock_addr.sin6_port = htons(port);
            result = connect(socket, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
        }
#endif // no ipv6
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

    static int ShutdownTypeToNative(ShutdownType type)
    {
        switch (type)
        {
            case SHUTDOWNTYPE_READ:
                return SD_RECEIVE;
            case SHUTDOWNTYPE_WRITE:
                return SD_SEND;
            case SHUTDOWNTYPE_READWRITE:
                return SD_BOTH;
        }
        return 0;
    }

    Result Shutdown(Socket socket, ShutdownType how)
    {
        int ret = shutdown(socket, ShutdownTypeToNative(how));
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
        int s = send(socket, (const char*)buffer, length, 0);
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

            result = (int)sendto(socket, (const char*)buffer, length, 0, (const sockaddr*)&sock_addr, sizeof(sock_addr));
        }
#if !defined(DM_IPV6_UNSUPPORTED)
        else if (IsSocketIPv6(socket))
        {
            assert(to_addr.m_family == DOMAIN_IPV6);

            struct sockaddr_in6 sock_addr = { 0 };
            sock_addr.sin6_family = AF_INET6;
            memcpy(&sock_addr.sin6_addr, IPv6(&to_addr), sizeof(struct in6_addr));
            sock_addr.sin6_port = htons(to_port);

            result = (int)sendto(socket, (const char*)buffer, length, 0, (const sockaddr*)&sock_addr, sizeof(sock_addr));
        }
#endif // no ipv6
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
        int r = recv(socket, (char*)buffer, length, 0);

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

    Result ReceiveFrom(Socket socket, void* buffer, int length, int* received_bytes, Address* from_addr, uint16_t* from_port)
    {
        int result = 0;
        *received_bytes = 0;

        if (IsSocketIPv4(socket))
        {
            struct sockaddr_in sock_addr = { 0 };
            socklen_t          addr_len = sizeof(sock_addr);

            result = recvfrom(socket, (char*)buffer, length, 0, (struct sockaddr*)&sock_addr, &addr_len);
            if (result >= 0)
            {
                from_addr->m_family = dmSocket::DOMAIN_IPV4;
                *IPv4(from_addr) = sock_addr.sin_addr.s_addr;
                *from_port = ntohs(sock_addr.sin_port);
                *received_bytes = result;
            }
        }
#if !defined(DM_IPV6_UNSUPPORTED)
        else if (IsSocketIPv6(socket))
        {
            struct sockaddr_in6 sock_addr = { 0 };
            socklen_t           addr_len = sizeof(sock_addr);

            result = recvfrom(socket, (char*)buffer, length, 0, (struct sockaddr*)&sock_addr, &addr_len);
            if (result >= 0)
            {
                from_addr->m_family = dmSocket::DOMAIN_IPV6;
                memcpy(IPv6(from_addr), &sock_addr.sin6_addr, sizeof(struct in6_addr));
                *from_port = ntohs(sock_addr.sin6_port);
                *received_bytes = result;
            }
        }
#endif // no ipv6
        else
        {
            dmLogError("Failed to receive from remote host, unsupported address family!");
            return RESULT_AFNOSUPPORT;
        }

        return result >= 0 ? RESULT_OK : NativeToResultCompat(DM_SOCKET_ERRNO);
    }

    Result GetName(Socket socket, Address* address, uint16_t* port)
    {
        int result = -1;
        if (IsSocketIPv4(socket))
        {
            struct sockaddr_in sock_addr = { 0 };
            socklen_t          addr_len = sizeof(sock_addr);
            result = getsockname(socket, (struct sockaddr*)&sock_addr, &addr_len);
            if (result == 0)
            {
                address->m_family = dmSocket::DOMAIN_IPV4;
                *IPv4(address) = sock_addr.sin_addr.s_addr;
                *port = ntohs(sock_addr.sin_port);
            }
        }
#if !defined(DM_IPV6_UNSUPPORTED)
        else if (IsSocketIPv6(socket))
        {
            struct sockaddr_in6 sock_addr = { 0 };
            socklen_t           addr_len = sizeof(sock_addr);
            result = getsockname(socket, (struct sockaddr*)&sock_addr, &addr_len);
            if (result == 0)
            {
                address->m_family = dmSocket::DOMAIN_IPV6;
                memcpy(IPv6(address), &sock_addr.sin6_addr, sizeof(struct in6_addr));
                *port = ntohs(sock_addr.sin6_port);
            }
        }
#endif // no ipv6
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

    Result GetLocalAddress(Address* address)
    {
        /*
         * Get local address from reverse lookup of hostname
         * The method is potentially fragile. On iOS we iterate
         * over network adapter to the find actual address for en0
         * See socket_apple.mm
         */
        char   hostname[256] = { 0 };
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
    }

    Result SetBlocking(Socket socket, bool blocking)
    {
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
    }

    Result SetNoDelay(Socket socket, bool no_delay)
    {
        return SetSockoptBool(socket, IPPROTO_TCP, TCP_NODELAY, no_delay);
    }

    Result SetQuickAck(Socket socket, bool use_quick_ack)
    {
        return RESULT_OK;
    }

    static Result SetSockoptTime(Socket socket, int level, int name, uint64_t time)
    {
        DWORD timeval = time / 1000;
        if (time > 0 && timeval == 0)
        {
            dmLogWarning("Socket timeout requested less than 1ms. Timeout set to 1ms.");
            timeval = 1;
        }
        int ret = setsockopt(socket, level, name, (char*)&timeval, sizeof(timeval));
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
        struct addrinfo  hints;
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
                    sockaddr_in* saddr = (struct sockaddr_in*)iterator->ai_addr;
                    address->m_family = dmSocket::DOMAIN_IPV4;
                    *IPv4(address) = saddr->sin_addr.s_addr;
                    result = RESULT_OK;
                }
#if !defined(DM_IPV6_UNSUPPORTED)
                else if (ipv6 && iterator->ai_family == AF_INET6)
                {
                    sockaddr_in6* saddr = (struct sockaddr_in6*)iterator->ai_addr;
                    address->m_family = dmSocket::DOMAIN_IPV6;
                    memcpy(IPv6(address), &saddr->sin6_addr, sizeof(struct in6_addr));
                    result = RESULT_OK;
                }
#endif // no ipv6

                iterator = iterator->ai_next;
            }

            freeaddrinfo(res); // Free the head of the linked list
        }

        return result;
    }
} // namespace dmSocket
