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

#include "../socket.h"
#include "../dstrings.h"
#include "../log.h"
#include "../time.h"
#include <assert.h>
#include <malloc.h>

#include <nn/nifm/nifm_Api.h>
#include <nn/nifm/nifm_ApiIpAddress.h>
#include <nn/nifm/nifm_ApiNetworkConnection.h>
#include <nn/socket/socket_Api.h>

namespace dmSocket
{
    nn::socket::ConfigDefaultWithMemory g_SocketConfigWithMemory;
    static int g_SocketInitialized = 0;
    static int g_NetworkInitialized = 0;

    static nn::socket::Type TypeToNative(Type type)
    {
        switch(type)
        {
            case TYPE_STREAM:  return nn::socket::Type::Sock_Stream;
            case TYPE_DGRAM:  return nn::socket::Type::Sock_Dgram;
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

    Result PlatformInitialize()
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

    Result PlatformFinalize()
    {
        if (g_SocketInitialized)
            nn::socket::Finalize();         // close the socket interface
        g_SocketInitialized = 0;

        if (g_NetworkInitialized)
            nn::nifm::CancelNetworkRequest();   // close the network interface
        g_NetworkInitialized = 0;
        return RESULT_OK;
    }

    static int ProcessAddrInfo(IfAddr* a, nn::socket::AddrInfo* pAddrinfo)
    {
        nn::socket::Family family = pAddrinfo->ai_family;
        if (!(family == nn::socket::Family::Af_Inet || family == nn::socket::Family::Af_Inet6 || family == nn::socket::Family::Af_Link)) {
            return 0;
        }

        memset(a, 0, sizeof(IfAddr));
        a->m_Address.m_family = DOMAIN_MISSING;

        /*
        if (ifa->ifa_flags & IFF_UP) {
            a->m_Flags |= FLAGS_UP;
        }

        if (ifa->ifa_flags & IFF_RUNNING) {
            a->m_Flags |= FLAGS_RUNNING;
        }
        */
        a->m_Flags |= FLAGS_UP;
        a->m_Flags |= FLAGS_RUNNING;

        nn::socket::SockAddrIn* pSin = (nn::socket::SockAddrIn*)pAddrinfo->ai_addr;

        const char* name = pAddrinfo->ai_canonname;
        if (!name && pSin != 0)
            name = nn::socket::InetNtoa(pSin->sin_addr);
        dmStrlCpy(a->m_Name, name, sizeof(a->m_Name));

        // only nn::socket::Family::Af_Inet (IPv4) addresses are supported
        if (nn::socket::Family::Af_Inet == family)
        {
            if (pSin)
            {
                a->m_Flags |= FLAGS_INET;
                a->m_Address.m_family = DOMAIN_IPV4;
                *IPv4(&a->m_Address) = pSin->sin_addr.S_addr;
            }
        } else if (family == nn::socket::Family::Af_Inet6) {
            a->m_Flags |= FLAGS_INET; // as opposed to "link"
            a->m_Address.m_family = DOMAIN_IPV6;
            //memcpy(IPv6(&a->m_Address), &sock_addr->sin6_addr, sizeof(struct in6_addr));
        } else if (family == nn::socket::Family::Af_Link) {
            a->m_Flags |= FLAGS_LINK;
            // copy mac address
        }
        return 1;
    }

    // See SocketResolver.cpp example: https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Api/HtmlNX/_socket_resolver_8cpp-example.html#a28
    void GetIfAddresses(IfAddr* addresses, uint32_t addresses_count, uint32_t* count)
    {
        *count = 0;

        if (addresses == 0 || addresses_count == 0)
            return;

        const char* ip = "localhost";

        nn::socket::AiErrno rc = nn::socket::AiErrno::EAi_Success; // was -1;
        nn::socket::AddrInfo* pAddressInfoResult = 0;
        nn::socket::AddrInfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = nn::socket::Family::Af_Unspec;

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

        nn::socket::InAddr primary_ip;
        nn::Result result = nn::nifm::GetCurrentPrimaryIpAddress(&primary_ip);
        if( result.IsSuccess() )
        {
            IfAddr* a = &addresses[*count];
            a->m_Flags |= FLAGS_INET;
            a->m_Address.m_family = DOMAIN_IPV4;
            a->m_Flags |= FLAGS_UP;
            a->m_Flags |= FLAGS_RUNNING;
            *IPv4(&a->m_Address) = primary_ip.S_addr;
            a->m_Name[0] = 0;
            *count += 1;
        }

        for (nn::socket::AddrInfo* pAddrinfo = pAddressInfoResult; pAddrinfo != 0; pAddrinfo = pAddrinfo->ai_next)
        {
            IfAddr* a = &addresses[*count];
            int added = ProcessAddrInfo(a, pAddrinfo);
            *count += added ? 1 : 0;
        }

        if (pAddressInfoResult)
            nn::socket::FreeAddrInfo(pAddressInfoResult);
    };

    static Result GetLocalAddressNative(Address* address)
    {
        nn::socket::InAddr ipaddr = { 0 };
        nn::Result result = nn::nifm::GetCurrentPrimaryIpAddress(&ipaddr);
        if (!result.IsSuccess() ) {
            return RESULT_UNKNOWN;
        }
        address->m_family = DOMAIN_IPV4;
        *IPv4(address) = ipaddr.S_addr;
        return RESULT_OK;
    }

    // to make the api a bit more like the regular one, we recreate a "gethostname()" function here
    int gethostname(char* hostname, int hostname_length)
    {
        dmSocket::Address local_address;
        dmSocket::Result sockr = GetLocalAddressNative(&local_address);
        if (sockr != dmSocket::RESULT_OK)
        {
            return -1;
        }

        const char* addr = dmSocket::AddressToIPString(local_address);
        dmStrlCpy(hostname, addr, hostname_length);
        free((void*)addr);

        return 0;
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

        nn::socket::AiErrno aierr = nn::socket::GetAddrInfo(name, 0, &hints, &sa);
        if (aierr == nn::socket::AiErrno::EAi_Success)
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
        else
        {
            dmLogError("Failed to get host by name '%s': %s", name, nn::socket::GAIStrError(aierr));
        }

        return result;
    }
}
