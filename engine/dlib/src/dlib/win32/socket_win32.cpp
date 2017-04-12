#include <winsock2.h>
#include <iphlpapi.h>
#include "../socket.h"
#include "../log.h"
#include "../dstrings.h"

#include <stdio.h>

namespace dmSocket
{
    void GetIfAddresses(IfAddr* addresses, uint32_t addresses_count, uint32_t* count)
    {
        *count = 0;
        unsigned int i = 0;
        ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
        ULONG family = AF_INET;
        char buffer[0x8000];
        PIP_ADAPTER_ADDRESSES paddresses = (IP_ADAPTER_ADDRESSES*) buffer;
        ULONG out_buf_len = sizeof(buffer);
        PIP_ADAPTER_ADDRESSES pcurraddresses = NULL;
        DWORD ret = GetAdaptersAddresses(family, flags, NULL, paddresses, &out_buf_len);

        if (ret == ERROR_BUFFER_OVERFLOW) {
            dmLogError("Failed to call GetAdaptersAddresses. Buffer too small.");
            return;
        }

        if (ret == NO_ERROR) {
            pcurraddresses = paddresses;
            while (pcurraddresses && *count < addresses_count) {
                if (pcurraddresses->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
                    if (pcurraddresses->FirstUnicastAddress) {
                        IfAddr* a = &addresses[*count];
                        memset(a, 0, sizeof(*a));

                        wcstombs(a->m_Name, pcurraddresses->FriendlyName, sizeof(a->m_Name));
                        a->m_Name[sizeof(a->m_Name)-1] = 0;
                        sockaddr_in* ia = (sockaddr_in*) pcurraddresses->FirstUnicastAddress->Address.lpSockaddr;
                        a->m_Flags |= FLAGS_INET;
                        a->m_Address.m_family = DOMAIN_IPV4;
                        *IPv4(&a->m_Address) = ia->sin_addr.s_addr;

                        a->m_Flags |= FLAGS_LINK;
                        a->m_MacAddress[0] = pcurraddresses->PhysicalAddress[0];
                        a->m_MacAddress[1] = pcurraddresses->PhysicalAddress[1];
                        a->m_MacAddress[2] = pcurraddresses->PhysicalAddress[2];
                        a->m_MacAddress[3] = pcurraddresses->PhysicalAddress[3];
                        a->m_MacAddress[4] = pcurraddresses->PhysicalAddress[4];
                        a->m_MacAddress[5] = pcurraddresses->PhysicalAddress[5];

                        if (pcurraddresses->OperStatus == IfOperStatusUp) {
                            a->m_Flags |= FLAGS_UP;
                            a->m_Flags |= FLAGS_RUNNING;
                        }
                        *count = *count + 1;
                    }
                }
                pcurraddresses = pcurraddresses->Next;
            }
        } else {
            if (ret != ERROR_NO_DATA) {
                dmLogError("GetAdaptersAddresses failed (%d)\n", ret);
            }
        }
        return;
    }
}
