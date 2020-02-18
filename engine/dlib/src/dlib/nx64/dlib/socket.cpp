// https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Api/HtmlNX/_socket_basic_8cpp-example.html#a10

#include <dlib/socket.h>
#include <dlib/sockettypes.h>
#include <dlib/math.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <assert.h>
#include <string.h>

#include <nn/nifm/nifm_Api.h>
#include <nn/nifm/nifm_ApiIpAddress.h>
#include <nn/nifm/nifm_ApiNetworkConnection.h>
#include <nn/socket/socket_Api.h>

// Helper and utility functions
namespace dmSocket
{
    nn::socket::ConfigDefaultWithMemory g_SocketConfigWithMemory;
    static int g_SocketInitialized = 0;
    static int g_NetworkInitialized = 0;

    #define DM_SOCKET_ERRNO nn::socket::GetLastError()
    #define DM_SOCKET_HERRNO nn::socket::GetLastError()

    // should be static!
    Result NativeToResult(const char* filename, int line, nn::socket::Errno r)
    {
        // From socket_Errno.h
        switch (r)
        {
        case nn::socket::Errno::ESuccess:          return RESULT_OK;
        case nn::socket::Errno::EIntr:             return RESULT_INTR;
        case nn::socket::Errno::EAgain:            return RESULT_WOULDBLOCK;
        case nn::socket::Errno::EAcces:            return RESULT_ACCES;
        case nn::socket::Errno::EFault:            return RESULT_FAULT;
        case nn::socket::Errno::EInval:            return RESULT_INVAL;
        case nn::socket::Errno::EMFile:            return RESULT_MFILE;
        case nn::socket::Errno::EPipe:             return RESULT_PIPE;
        //case nn::socket::Errno::EWouldBlock:       return RESULT_WOULDBLOCK; // Equals EAgain
        case nn::socket::Errno::EBadf:             return RESULT_BADF;
        case nn::socket::Errno::ENotSock:          return RESULT_NOTSOCK;
        case nn::socket::Errno::EDestAddrReq:      return RESULT_DESTADDRREQ;
        case nn::socket::Errno::EMsgSize:          return RESULT_MSGSIZE;
        case nn::socket::Errno::EPrototype:        return RESULT_PROTOTYPE;
        case nn::socket::Errno::EProtoNoSupport:   return RESULT_PROTONOSUPPORT;
        case nn::socket::Errno::EOpNotSupp:        return RESULT_OPNOTSUPP;
        case nn::socket::Errno::EAfNoSupport:      return RESULT_AFNOSUPPORT;
        case nn::socket::Errno::EAddrInUse:        return RESULT_ADDRINUSE;
        case nn::socket::Errno::EAddrNotAvail:     return RESULT_ADDRNOTAVAIL;
        case nn::socket::Errno::ENetDown:          return RESULT_NETDOWN;
        case nn::socket::Errno::ENetUnreach:       return RESULT_NETUNREACH;
        case nn::socket::Errno::EConnAborted:      return RESULT_CONNABORTED;
        case nn::socket::Errno::EConnReset:        return RESULT_CONNRESET;
        case nn::socket::Errno::ENoBufs:           return RESULT_NOBUFS;
        case nn::socket::Errno::EIsConn:           return RESULT_ISCONN;
        case nn::socket::Errno::ENotConn:          return RESULT_NOTCONN;
        case nn::socket::Errno::ETimedOut:         return RESULT_TIMEDOUT;
        case nn::socket::Errno::EConnRefused:      return RESULT_CONNREFUSED;
        case nn::socket::Errno::EHostUnreach:      return RESULT_HOSTUNREACH;
        case nn::socket::Errno::EInProgress:       return RESULT_INPROGRESS;
        default:
            break;
        }

        dmLogError("%s( %d ): SOCKET: Unknown result code %d\n", filename, line, r);
        return RESULT_UNKNOWN;
    }

    #define NATIVETORESULT(_R_) NativeToResult(__FILE__, __LINE__, _R_)


    static nn::socket::Type TypeToNative(Type type)
    {
        switch(type)
        {
            case TYPE_STREAM:  return nn::socket::Type::Sock_Stream;
            case TYPE_DGRAM:  return nn::socket::Type::Sock_Dgram;
        }
    }
    static nn::socket::Protocol ProtocolToNative(Protocol protocol)
    {
        switch(protocol)
        {
            case PROTOCOL_TCP:  return nn::socket::Protocol::IpProto_Tcp;
            case PROTOCOL_UDP:  return nn::socket::Protocol::IpProto_Udp;
        }
    }
    static nn::socket::Family DomainToNative(Domain domain)
    {
        switch(domain)
        {
            case DOMAIN_IPV4:     return nn::socket::Family::Af_Inet;
            case DOMAIN_IPV6:     return nn::socket::Family::Af_Inet6;
            default:
                assert(false && "Unknown domain type");
                break;
        }
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
        nn::socket::SockAddrIn  sa = { 0 };
        nn::socket::SockLenT    saLen = sizeof(sa);
        if (nn::socket::GetSockName(socket, (nn::socket::SockAddr*)&sa, &saLen) >= 0)
        {
            return sa.sin_family == nn::socket::Family::Af_Inet;
        }
        dmLogError("Failed to retrieve address family (%d): %s", NATIVETORESULT(DM_SOCKET_ERRNO), ResultToString(NATIVETORESULT(DM_SOCKET_ERRNO)));
        return false;
    }

    bool IsSocketIPv6(Socket socket)
    {
        nn::socket::SockAddrIn  sa = { 0 };
        nn::socket::SockLenT    saLen = sizeof(sa);
        if (nn::socket::GetSockName(socket, (nn::socket::SockAddr*)&sa, &saLen) >= 0)
        {
            return sa.sin_family == nn::socket::Family::Af_Inet6;
        }
        dmLogError("Failed to retrieve address family (%d): %s", NATIVETORESULT(DM_SOCKET_ERRNO), ResultToString(NATIVETORESULT(DM_SOCKET_ERRNO)));
        return false;
    }

    uint32_t BitDifference(Address a, Address b)
    {
        // Implements the Hamming Distance algorithm.
        // https://en.wikipedia.org/wiki/Hamming_distance
        uint32_t difference = 0;
        for (uint32_t i = 0; i < (sizeof(a.m_address) / sizeof(uint32_t)); ++i)
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

    Result Initialize()
    {
        g_NetworkInitialized = 0;
        g_SocketInitialized = 0;

        nn::Result result = nn::nifm::Initialize();
        if( result.IsFailure() )
        {
            dmLogError("Error: nn::nifm::Initialize() failed (error %d)", result.GetDescription());
            return RESULT_NETUNREACH;
        }

        // Request to use the network interface
        nn::nifm::SubmitNetworkRequest();

        uint32_t max_time = 2 * 1000000;
        uint32_t wait_time = 0;
        while(nn::nifm::IsNetworkRequestOnHold() && wait_time < max_time)
        {
            dmLogInfo("Waiting for network interface availability %u/%u", wait_time, max_time);
            dmTime::Sleep(250000);
            wait_time += 250000;
        }

        if( !nn::nifm::IsNetworkAvailable() )
        {
            dmLogError("The network is not available");
            return RESULT_NETUNREACH;
        }

        g_NetworkInitialized = 1;

        result = nn::socket::Initialize(g_SocketConfigWithMemory);
        if( result.IsFailure() )
        {
            dmLogError("nn::socket::Initialize() failed (error %d)", result.GetDescription());
            return RESULT_NETUNREACH;
        }

        g_SocketInitialized = 1;
        return RESULT_OK;
    }

    Result Finalize()
    {
        if (g_SocketInitialized)
            nn::socket::Finalize();         // close the socket interface
        g_SocketInitialized = 0;

        if (g_NetworkInitialized)
            nn::nifm::CancelNetworkRequest();   // close the network interface
        g_NetworkInitialized = 0;
        return RESULT_OK;
    }

    Result New(Domain domain, Type type, Protocol protocol, Socket* socket)
    {
        *socket = -1;
        if (domain == DOMAIN_MISSING || domain == DOMAIN_UNKNOWN)
            return RESULT_AFNOSUPPORT;

        if ((*socket = nn::socket::Socket(DomainToNative(domain), TypeToNative(type), ProtocolToNative(protocol))) < 0)
        {
            return NATIVETORESULT(nn::socket::GetLastError());
        }
        return RESULT_OK;
    }

    static Result SetSockoptBool(Socket socket, nn::socket::Level level, nn::socket::Option name, bool option)
    {
        int on = (int) option;
        int ret = nn::socket::SetSockOpt(socket, level, name, &on, sizeof(on));
        return ret >= 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result SetReuseAddress(Socket socket, bool reuse)
    {
        Result r = SetSockoptBool(socket, nn::socket::Level::Sol_Socket, nn::socket::Option::So_ReuseAddr, reuse);
#ifdef SO_REUSEPORT
        if (r != RESULT_OK)
            return r;
        r = SetSockoptBool(socket, nn::socket::Level::Sol_Socket, nn::socket::Option::So_ReusePort, reuse);
#endif
        return r;
    }

    Result SetBroadcast(Socket socket, bool broadcast)
    {
        return SetSockoptBool(socket, nn::socket::Level::Sol_Socket, nn::socket::Option::So_Broadcast, broadcast);
    }

    Result AddMembership(Socket socket, Address multi_addr, Address interface_addr, int ttl)
    {
        int result = -1;
        if (IsSocketIPv4(socket))
        {
            assert(multi_addr.m_family == DOMAIN_IPV4 && interface_addr.m_family == DOMAIN_IPV4);
            nn::socket::IpMreq group;
            group.imr_multiaddr.S_addr = *IPv4(&multi_addr);
            group.imr_interface.S_addr = *IPv4(&interface_addr);
            result = nn::socket::SetSockOpt(socket, nn::socket::Level::Sol_Ip, nn::socket::Option::Ip_Add_Membership, (char*)&group, sizeof(group));
            if (result == 0)
            {
                uint8_t ttl_byte = (uint8_t) ttl;
                result = nn::socket::SetSockOpt(socket, nn::socket::Level::Sol_Ip, nn::socket::Option::Ip_Multicast_Ttl, (char*)&ttl_byte, sizeof(ttl_byte));
            }
        }
        else
        {
            dmLogError("%s: unsupported address family", __FUNCTION__);
            return RESULT_AFNOSUPPORT;
        }

        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result SetMulticastIf(Socket socket, Address address)
    {
        int result = -1;
        if (IsSocketIPv4(socket))
        {
            nn::socket::InAddr inaddr;
            memset(&inaddr, 0, sizeof(inaddr));
            inaddr.S_addr = *IPv4(&address);
            result = nn::socket::SetSockOpt(socket, nn::socket::Level::Sol_Ip, nn::socket::Option::Ip_Multicast_If, (char*) &inaddr, sizeof(inaddr));
        }
        else
        {
            dmLogError("%s: unsupported address family: %d", __FUNCTION__, address.m_family);
            return RESULT_AFNOSUPPORT;
        }

        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result Delete(Socket socket)
    {
        int result = nn::socket::Close(socket);
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
            nn::socket::SockAddrIn sa = { 0 };
            nn::socket::SockLenT saLen = sizeof(sa);
            result = nn::socket::Accept(socket, (nn::socket::SockAddr*)&sa, &saLen);

            if (result < 0)
            {
                dmLogError("Accept failed (error %d)\n", NATIVETORESULT(DM_SOCKET_ERRNO));
            }

            dmLogInfo("Connection accepted from %s\n", nn::socket::InetNtoa(sa.sin_addr));

            address->m_family = DOMAIN_IPV4;
            *IPv4(address) = sa.sin_addr.S_addr;
        }
        else
        {
            dmLogError("%s: unsupported address family", __FUNCTION__);
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

            nn::socket::SockAddrIn  sa = { 0 };
            sa.sin_addr.S_addr = *IPv4(&address);//nn::socket::InetHtonl(nn::socket::InAddr_Any);
            sa.sin_port        = nn::socket::InetHtons(port);
            sa.sin_family      = nn::socket::Family::Af_Inet;
            result = nn::socket::Bind(socket, (nn::socket::SockAddr*)&sa, sizeof(sa));
            if (result < 0)
            {
                dmLogError("Bind failed (error %d)\n", NATIVETORESULT(DM_SOCKET_ERRNO));
            }
        }
        else
        {
            dmLogError("%s: unsupported address family: %d", __FUNCTION__, address.m_family);
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

            nn::socket::SockAddrIn  sa = { 0 };
            sa.sin_addr.S_addr  = *IPv4(&address);
            sa.sin_port         = nn::socket::InetHtons((uint16_t)port);
            sa.sin_family       = nn::socket::Family::Af_Inet;
            result = nn::socket::Connect(socket, (nn::socket::SockAddr*)&sa, sizeof(sa));
            if (result < 0)
            {
                dmLogError("Connect failed (error %d)\n", NATIVETORESULT(DM_SOCKET_ERRNO));
            }
        }
        else
        {
            dmLogError("%s: unsupported address family: %d", __FUNCTION__, address.m_family);
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
        int result = nn::socket::Listen(socket, backlog);
        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    static nn::socket::ShutdownMethod ShutdownTypeToNative(ShutdownType type)
    {
        switch(type)
        {
            case SHUTDOWNTYPE_READ:         return nn::socket::ShutdownMethod::Shut_Rd;
            case SHUTDOWNTYPE_WRITE:        return nn::socket::ShutdownMethod::Shut_Wr;
            case SHUTDOWNTYPE_READWRITE:    return nn::socket::ShutdownMethod::Shut_RdWr;
        }
    }

    Result Shutdown(Socket socket, ShutdownType how)
    {
        int result = nn::socket::Shutdown(socket, ShutdownTypeToNative(how));
        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result Send(Socket socket, const void* buffer, int length, int* sent_bytes)
    {
        *sent_bytes = 0;

        ssize_t s = nn::socket::Send(socket, buffer, length, nn::socket::MsgFlag::Msg_NoSignal);
        if (s < 0)
        {
            return NATIVETORESULT(DM_SOCKET_ERRNO);
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

            nn::socket::SockAddrIn  sa = { 0 };
            sa.sin_addr.S_addr  = *IPv4(&to_addr);
            sa.sin_port         = nn::socket::InetHtons(to_port);
            sa.sin_family       = nn::socket::Family::Af_Inet;
            result = (int) nn::socket::SendTo(socket, buffer, (size_t)length, nn::socket::MsgFlag::Msg_None, (nn::socket::SockAddr*)&sa, sizeof(sa));
        }
        else
        {
            dmLogError("%s: unsupported address family", __FUNCTION__);
            return RESULT_AFNOSUPPORT;
        }

        *sent_bytes = result >= 0 ? result : 0;
        return result >= 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result Receive(Socket socket, void* buffer, int length, int* received_bytes)
    {
        *received_bytes = 0;

        ssize_t r = nn::socket::Recv(socket, buffer, length, 0);
        if (r < 0)
        {
            return NATIVETORESULT(DM_SOCKET_ERRNO);
        }
        else
        {
            *received_bytes = (int)r;
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
            nn::socket::SockAddrIn  sa = { 0 };
            nn::socket::SockLenT saLen = sizeof(sa);
            result = (int) nn::socket::RecvFrom(socket, buffer, length, nn::socket::MsgFlag::Msg_None, (nn::socket::SockAddr*)&sa, &saLen);
            if (result >= 0)
            {
                from_addr->m_family = dmSocket::DOMAIN_IPV4;
                *IPv4(from_addr) = sa.sin_addr.S_addr;
                *from_port = nn::socket::InetNtohs(sa.sin_port);
                *received_bytes = result;
            }
        }
        else
        {
            dmLogError("%s: unsupported address family", __FUNCTION__);
            return RESULT_AFNOSUPPORT;
        }

        return result >= 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result GetName(Socket socket, Address* address, uint16_t* port)
    {
        int result = -1;
        if (IsSocketIPv4(socket))
        {
            nn::socket::SockAddrIn  sa = { 0 };
            nn::socket::SockLenT    saLen = sizeof(sa);
            result = nn::socket::GetSockName(socket, (nn::socket::SockAddr*)&sa, &saLen);

            if (result == 0)
            {
                address->m_family = dmSocket::DOMAIN_IPV4;
                *IPv4(address) = sa.sin_addr.S_addr;
                *port = nn::socket::InetNtohs(sa.sin_port);
            }
        }
        else
        {
            dmLogError("%s: unsupported address family", __FUNCTION__);
            return RESULT_AFNOSUPPORT;
        }

        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result GetHostname(char* hostname, int hostname_length)
    {
        // TODO: what about other tools that use this function? Do we need to expose our own version of gethostname()
        return RESULT_HOSTUNREACH;
        /*
        int r = gethostname(hostname, hostname_length);
        if (hostname_length > 0)
            hostname[hostname_length - 1] = '\0';
        return r == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
        */
    }

    Result GetLocalAddress(Address* address)
    {
        nn::socket::InAddr ipaddr = { 0 };
        nn::Result result = nn::nifm::GetCurrentPrimaryIpAddress(&ipaddr);
        if (!result.IsSuccess() ) {
            return RESULT_UNKNOWN;
        }
        address->m_family = DOMAIN_IPV4;
        address->m_address[0] = ipaddr.S_addr;
        return RESULT_OK;
    }

    Result SetBlocking(Socket socket, bool blocking)
    {
        int flags = nn::socket::Fcntl(socket, nn::socket::FcntlCommand::F_GetFl, 0);
        if (flags < 0)
        {
            return NATIVETORESULT(DM_SOCKET_ERRNO);
        }

        if (blocking)
        {
            flags &= ~(int)nn::socket::FcntlFlag::O_NonBlock;
        }
        else
        {
            flags |= (int)nn::socket::FcntlFlag::O_NonBlock;
        }

        if (nn::socket::Fcntl(socket, nn::socket::FcntlCommand::F_SetFl, flags) < 0)
        {
            return NATIVETORESULT(DM_SOCKET_ERRNO);
        }
        else
        {
            return RESULT_OK;
        }
    }

    Result SetNoDelay(Socket socket, bool no_delay)
    {
        return SetSockoptBool(socket, nn::socket::Level::Sol_Ip, nn::socket::Option::Tcp_NoDelay, no_delay);
    }

    static Result SetSockoptTime(Socket socket, nn::socket::Level level, nn::socket::Option name, uint64_t time)
    {
        nn::socket::TimeVal timeval;
        timeval.tv_sec = time / 1000000;
        timeval.tv_usec = time % 1000000;

        int result = nn::socket::SetSockOpt(socket, level, name, (char*)&timeval, sizeof(timeval));
        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result SetSendTimeout(Socket socket, uint64_t timeout)
    {
        return SetSockoptTime(socket, nn::socket::Level::Sol_Socket, nn::socket::Option::So_SndTimeo, timeout);
    }

    Result SetReceiveTimeout(Socket socket, uint64_t timeout)
    {
        return SetSockoptTime(socket,  nn::socket::Level::Sol_Socket, nn::socket::Option::So_RcvTimeo, timeout);
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
            nn::socket::InetNtop(DomainToNative(address.m_family), IPv4(&address), addrstr, sizeof(addrstr));
            return strdup(addrstr);
        }
        else
        {
            dmLogError("%s: unsupported address family: %d", __FUNCTION__, address.m_family);
            return 0x0;
        }
    }

    Result GetHostByName(const char* name, Address* address, bool ipv4, bool ipv6)
    {
        Result result = RESULT_HOST_NOT_FOUND;

        memset(address, 0x0, sizeof(Address));
        nn::socket::AddrInfo hints;
        nn::socket::AddrInfo* sa;

        memset(&hints, 0x0, sizeof(hints));
        hints.ai_family = DomainToNative(DOMAIN_IPV4);
        hints.ai_socktype = TypeToNative(TYPE_STREAM);

        if (nn::socket::GetAddrInfo(name, 0, &hints, &sa) == nn::socket::AiErrno::EAi_Success)
        {
            nn::socket::AddrInfo* iterator = sa;
            while (iterator && result == RESULT_HOST_NOT_FOUND)
            {
                // There could be (and probably are) multiple results for the same
                // address. The correct way to handle this would be to return a
                // list of addresses and then try each of them until one succeeds.
                if (ipv4 && iterator->ai_family == nn::socket::Family::Af_Inet)
                {
                    nn::socket::SockAddrIn* saddr = (nn::socket::SockAddrIn*) iterator->ai_addr;
                    address->m_family = dmSocket::DOMAIN_IPV4;
                    *IPv4(address) = saddr->sin_addr.S_addr;
                    result = RESULT_OK;
                }

                iterator = iterator->ai_next;
            }

            nn::socket::FreeAddrInfo(sa); // Free the head of the linked list
        }

        return result;
    }

    // See SocketResolver.cpp example: https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Api/HtmlNX/_socket_resolver_8cpp-example.html#a28
    void GetIfAddresses(IfAddr* addresses, uint32_t addresses_count, uint32_t* count)
    {
        *count = 0;

        if (addresses == 0 || addresses_count)
            return;

        const char* ip = "localhost";

        nn::socket::AiErrno rc = nn::socket::AiErrno::EAi_Success; // was -1;
        nn::socket::AddrInfo* pAddressInfoResult = 0;
        nn::socket::AddrInfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = nn::socket::Family::Af_Inet;
        hints.ai_socktype = nn::socket::Type::Sock_Stream;

        if (nn::socket::AiErrno::EAi_Success != (rc = nn::socket::GetAddrInfo(ip, 0, &hints, &pAddressInfoResult)))
        {
            dmLogError("GetAddrInfo failed: %d", nn::socket::GetLastError());
            nn::socket::FreeAddrInfo(pAddressInfoResult);
            return;
        }
        else if (0 == pAddressInfoResult)
        {
            dmLogError("GetAddrInfo returned no addresses");
            return;
        }

        nn::socket::SockAddrIn* pSin = 0;
        for (nn::socket::AddrInfo* pAddrinfo = pAddressInfoResult; pAddrinfo != 0; pAddrinfo = pAddrinfo->ai_next)
        {
            // only nn::socket::Family::Af_Inet (IPv4) addresses are supported
            if (nn::socket::Family::Af_Inet == pAddrinfo->ai_family)
            {
                pSin = (nn::socket::SockAddrIn*)pAddrinfo->ai_addr;
                if (pSin)
                {
                    IfAddr* a = &addresses[*count];
                    memset(a, 0, sizeof(IfAddr));
                    a->m_Address.m_family = DOMAIN_IPV4;
                    *IPv4(&a->m_Address) = pSin->sin_addr.S_addr;

                    if (pAddrinfo->ai_canonname)
                    {
                        size_t namesize = strlen(pAddrinfo->ai_canonname);
                        if (namesize > sizeof(a->m_Name))
                            namesize = sizeof(a->m_Name);
                        dmStrlCpy(a->m_Name, pAddrinfo->ai_canonname, namesize);
                    }
                    (*count)++;
                }
            }
        }

        if (pAddressInfoResult)
            nn::socket::FreeAddrInfo(pAddressInfoResult);
    };

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

    Selector::Selector()
    {
        SelectorZero(this);
    }

    void SelectorClear(Selector* selector, SelectorKind selector_kind, Socket socket)
    {
        nn::socket::FdSetClr(socket, &selector->m_FdSets[selector_kind]);
    }

    void SelectorSet(Selector* selector, SelectorKind selector_kind, Socket socket)
    {
        selector->m_Nfds = dmMath::Max(selector->m_Nfds, socket);
        nn::socket::FdSetSet(socket, &selector->m_FdSets[selector_kind]);
    }

    bool SelectorIsSet(Selector* selector, SelectorKind selector_kind, Socket socket)
    {
        return  nn::socket::FdSetIsSet(socket, &selector->m_FdSets[selector_kind]);
    }

    void SelectorZero(Selector* selector)
    {
        nn::socket::FdSetZero(&selector->m_FdSets[SELECTOR_KIND_READ]);
        nn::socket::FdSetZero(&selector->m_FdSets[SELECTOR_KIND_WRITE]);
        nn::socket::FdSetZero(&selector->m_FdSets[SELECTOR_KIND_EXCEPT]);
        selector->m_Nfds = 0;
    }

    Result Select(Selector* selector, int32_t timeout)
    {
        // example code: https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Api/HtmlNX/_socket_event_fd_8cpp-example.html#a52
        nn::socket::TimeVal timeout_val;
        timeout_val.tv_sec = timeout / 1000000;
        timeout_val.tv_usec = timeout % 1000000;

        int r;
        if (timeout < 0)
            r = nn::socket::Select(selector->m_Nfds + 1, &selector->m_FdSets[SELECTOR_KIND_READ], &selector->m_FdSets[SELECTOR_KIND_WRITE], &selector->m_FdSets[SELECTOR_KIND_EXCEPT], 0);
        else
            r = nn::socket::Select(selector->m_Nfds + 1, &selector->m_FdSets[SELECTOR_KIND_READ], &selector->m_FdSets[SELECTOR_KIND_WRITE], &selector->m_FdSets[SELECTOR_KIND_EXCEPT], &timeout_val);

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
}
