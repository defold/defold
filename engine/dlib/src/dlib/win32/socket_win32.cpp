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

#if defined(_WIN32)

#include <dmsdk/dlib/sockettypes.h>
#include <dmsdk/dlib/log.h>

#include "../socket.h"
#include "../socket_private.h"

#include <stdio.h>

namespace dmSocket
{
    void GetIfAddresses(IfAddr* addresses, uint32_t addresses_count, uint32_t* count)
    {
        *count = 0;
        ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
        ULONG family = AF_INET;

        ULONG out_buf_len;
        PIP_ADAPTER_ADDRESSES paddresses;

        // Ask for the length first
        DWORD ret = GetAdaptersAddresses(family, flags, NULL, NULL, &out_buf_len);

        if(ret == ERROR_BUFFER_OVERFLOW) { // the address pointer is null ofc
            paddresses = (IP_ADAPTER_ADDRESSES*)malloc(out_buf_len);
            ret = GetAdaptersAddresses(family, flags, NULL, paddresses, &out_buf_len);
        }

        PIP_ADAPTER_ADDRESSES pcurraddresses = NULL;
        PIP_ADAPTER_UNICAST_ADDRESS punicast = NULL;

        if (ret == NO_ERROR) {
            pcurraddresses = paddresses;
            while (pcurraddresses && *count < addresses_count) {
                if (pcurraddresses->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
                    punicast = pcurraddresses->FirstUnicastAddress;
                    if (punicast) {
                        for (unsigned i = 0; punicast != NULL; i++) {
                            IfAddr* a = &addresses[*count];
                            memset(a, 0, sizeof(*a));

                            wcstombs(a->m_Name, pcurraddresses->FriendlyName, sizeof(a->m_Name));
                            a->m_Name[sizeof(a->m_Name)-1] = 0;

                            if (pcurraddresses->OperStatus == IfOperStatusUp) {
                                a->m_Flags |= FLAGS_UP;
                                a->m_Flags |= FLAGS_RUNNING;
                            }

                            if (punicast->Address.lpSockaddr->sa_family == AF_INET) {
                                a->m_Flags |= FLAGS_INET;
                                a->m_Address.m_family = DOMAIN_IPV4;
                                sockaddr_in *ia = (sockaddr_in *)punicast->Address.lpSockaddr;
                                *IPv4(&a->m_Address) = ia->sin_addr.s_addr;
                            }
                            else if (punicast->Address.lpSockaddr->sa_family == AF_INET6) {
                                a->m_Flags |= FLAGS_INET;
                                a->m_Address.m_family = DOMAIN_IPV6;
                                sockaddr_in6 *ia = (sockaddr_in6 *)punicast->Address.lpSockaddr;
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
        } else {
            if (ret != ERROR_NO_DATA) {
                dmLogError("GetAdaptersAddresses failed (%d)\n", ret);
            }
        }

        free(paddresses);
        return;
    }

    Result PlatformInitialize()
    {
        WORD version_requested = MAKEWORD(2, 2);
        WSADATA wsa_data;
        int result = WSAStartup(version_requested, &wsa_data);
        return result == 0 ? RESULT_OK : NATIVETORESULT(DM_SOCKET_ERRNO);
    }

    Result PlatformFinalize()
    {
        WSACleanup();
        return RESULT_OK;
    }
}

#endif
