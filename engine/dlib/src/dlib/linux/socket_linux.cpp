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

#include "../socket.h"
#include "../dstrings.h"
#include "../log.h"

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <net/if.h>

namespace dmSocket
{
    void GetIfAddresses(IfAddr* addresses, uint32_t addresses_count, uint32_t* count)
    {
        *count = 0;

        struct ifreq *ifr;
        struct ifconf ifc;
        char buf[2048];
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) {
            dmLogError("Unable to create socket for GetIfAddresses");
            return;
        }

        memset(&ifc, 0, sizeof(ifc));
        ifr = (ifreq*) buf;
        ifc.ifc_ifcu.ifcu_req = ifr;
        ifc.ifc_len = sizeof(buf);
        if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
            close(s);
            return;
        }

        // NOTE: This is not compatible with BSD. You can't assume
        // equivalent size for all items
        int numif = ifc.ifc_len / sizeof(struct ifreq);
        for (int i = 0; i < numif && *count < addresses_count; i++) {
            struct ifreq *r = &ifr[i];

            if (strcmp(r->ifr_name, "lo") == 0) {
                continue;
            }

            IfAddr* a = &addresses[*count];
            memset(a, 0, sizeof(*a));

            dmStrlCpy(a->m_Name, r->ifr_name, sizeof(a->m_Name));

            if(ioctl(s, SIOCGIFADDR, r) < 0)
                continue;

            if (r->ifr_addr.sa_family == AF_INET)
            {
                sockaddr_in* ia = (sockaddr_in*) &r->ifr_ifru.ifru_addr;
                a->m_Flags |= FLAGS_INET;
                a->m_Address.m_family = DOMAIN_IPV4;
                *IPv4(&a->m_Address) = ia->sin_addr.s_addr;
            }
            else if (r->ifr_addr.sa_family == AF_INET6)
            {
                sockaddr_in6* ia = (sockaddr_in6*) &r->ifr_ifru.ifru_addr;
                a->m_Flags |= FLAGS_INET;
                a->m_Address.m_family = DOMAIN_IPV6;
                memcpy(IPv6(&a->m_Address), &ia->sin6_addr, sizeof(struct in6_addr));
            }

            if(ioctl(s, SIOCGIFHWADDR, r) >= 0)
            {
                memcpy(a->m_MacAddress, &r->ifr_hwaddr.sa_data[0], sizeof(unsigned char) * 6);
                a->m_Flags |= FLAGS_LINK;
            }
            else
            {
                memset(a->m_MacAddress, 0x00, sizeof(unsigned char) * 6);
            }

            if(ioctl(s, SIOCGIFFLAGS, r) < 0)
                continue;
            
            if (r->ifr_ifru.ifru_flags & IFF_UP) {
                a->m_Flags |= FLAGS_UP;
            }
            if (r->ifr_ifru.ifru_flags & IFF_RUNNING) {
                a->m_Flags |= FLAGS_RUNNING;
            }

            *count = *count + 1;
        }

        close(s);
        return;
    }
}
