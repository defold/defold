// Copyright 2020-2024 The Defold Foundation
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

#include <strings.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <netdb.h>

#include "../log.h"
#include "../socket.h"
#include "../socket_private.h"
#include "../dstrings.h"

namespace dmSocket
{
    static bool IncludeDevice(struct ifaddrs *ifaddr, ifaddrs* device) {
        bool has_link = false;

        for (ifaddrs* ifa = ifaddr; ifa != 0; ifa = ifa->ifa_next) {
            int family = ifa->ifa_addr->sa_family;
            if (strcmp(device->ifa_name, ifa->ifa_name) == 0) {
                if (family == AF_LINK) {
                    unsigned char* ptr = (unsigned char *)LLADDR((struct sockaddr_dl *)(ifa)->ifa_addr);
                    if (ptr[0] || ptr[1] || ptr[2] || ptr[3] || ptr[4] || ptr[5]) {
                        has_link = true;
                    }
                }
            }
        }

        return has_link;
    }

    void GetIfAddresses(IfAddr* addresses, uint32_t addresses_count, uint32_t* count)
    {
        *count = 0;
        struct ifaddrs *ifaddr, *ifa;

        if (getifaddrs(&ifaddr) < 0) {
            return;
        }

        for (ifa = ifaddr; ifa != 0 && *count < addresses_count; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == 0)
                continue;

            int family = ifa->ifa_addr->sa_family;
            if (!(family == AF_LINK || family == AF_INET || family == AF_INET6)) {
                continue;
            }

            if (!IncludeDevice(ifaddr, ifa)) {
                continue;
            }

            if (*count >= addresses_count) {
                dmLogWarning("Can't fill all if-addresses. Supplied buffer too small.");
                break;
            }

            IfAddr* a = &addresses[*count];
            memset(a, 0, sizeof(*a));
            dmStrlCpy(a->m_Name, ifa->ifa_name, sizeof(a->m_Name));
            *count = *count + 1;

            a->m_Address.m_family = DOMAIN_MISSING;

            if (ifa->ifa_flags & IFF_UP) {
                a->m_Flags |= FLAGS_UP;
            }

            if (ifa->ifa_flags & IFF_RUNNING) {
                a->m_Flags |= FLAGS_RUNNING;
            }

            if (family == AF_INET) {
                sockaddr_in* sock_addr = (sockaddr_in*) ifa->ifa_addr;
                a->m_Flags |= FLAGS_INET;
                a->m_Address.m_family = DOMAIN_IPV4;
                *IPv4(&a->m_Address) = sock_addr->sin_addr.s_addr;
            } else if (family == AF_INET6) {
                sockaddr_in6* sock_addr = (sockaddr_in6*) ifa->ifa_addr;
                a->m_Flags |= FLAGS_INET;
                a->m_Address.m_family = DOMAIN_IPV6;
                memcpy(IPv6(&a->m_Address), &sock_addr->sin6_addr, sizeof(struct in6_addr));
            } else if (family == AF_LINK) {
                a->m_Flags |= FLAGS_LINK;

                unsigned char* ptr = (unsigned char *)LLADDR((struct sockaddr_dl *)(ifa)->ifa_addr);
                if (ptr[0] || ptr[1] || ptr[2] || ptr[3] || ptr[4] || ptr[5]) {
                    a->m_MacAddress[0] = ptr[0];
                    a->m_MacAddress[1] = ptr[1];
                    a->m_MacAddress[2] = ptr[2];
                    a->m_MacAddress[3] = ptr[3];
                    a->m_MacAddress[4] = ptr[4];
                    a->m_MacAddress[5] = ptr[5];
                }
            }
        }
        freeifaddrs(ifaddr);
        return;
    }
}
