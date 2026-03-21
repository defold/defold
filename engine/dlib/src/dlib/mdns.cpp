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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdns.h"

#include "array.h"
#include "dstrings.h"
#include "log.h"
#include "socket.h"
#include "time.h"

namespace dmMDNS
{
    static const char* MDNS_MULTICAST_IPV4 = "224.0.0.251";
    static const uint16_t MDNS_PORT = 5353;
    static const uint32_t MDNS_MAX_TXT_ENTRIES = 16;
    static const uint32_t MDNS_INTERFACE_REFRESH_INTERVAL = 5;
    static const uint32_t MDNS_MAX_INTERFACES = 32;

    static const uint16_t DNS_TYPE_A = 1;
    static const uint16_t DNS_TYPE_PTR = 12;
    static const uint16_t DNS_TYPE_TXT = 16;
    static const uint16_t DNS_TYPE_SRV = 33;
    static const uint16_t DNS_TYPE_ANY = 255;

    static const uint16_t DNS_CLASS_IN = 1;
    static const uint16_t DNS_CLASS_FLUSH = 0x8000;

    static const uint16_t DNS_FLAG_RESPONSE = 0x8400;

    struct StoredTxt
    {
        char m_Key[64];
        char m_Value[256];
    };

    struct RegisteredService
    {
        RegisteredService()
        {
            memset(this, 0, sizeof(*this));
        }

        char m_Id[128];
        char m_InstanceName[128];
        char m_ServiceType[64];
        char m_ServiceTypeLocal[128];
        char m_FullServiceName[256];
        char m_Host[256];
        char m_HostLocal[256];
        dmSocket::Address m_HostAddress;
        uint16_t m_Port;
        uint32_t m_Ttl;
        StoredTxt m_Txt[MDNS_MAX_TXT_ENTRIES];
        uint32_t m_TxtCount;
    };

    struct MDNS
    {
        MDNS()
        {
            memset(this, 0, sizeof(*this));
            m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
            m_Services.SetCapacity(4);
            m_InterfaceAddresses.SetCapacity(4);
            m_MembershipAddresses.SetCapacity(4);
        }

        Params m_Params;
        dmSocket::Socket m_Socket;
        dmArray<RegisteredService> m_Services;
        dmArray<dmSocket::Address> m_InterfaceAddresses;
        dmArray<dmSocket::Address> m_MembershipAddresses;
        uint8_t m_Buffer[1500];

        char m_DefaultHost[256];
        char m_DefaultHostLocal[256];
        dmSocket::Address m_DefaultAddress;

        uint64_t m_NextAnnounce;
        uint64_t m_NextInterfaceRefresh;
    };

    struct BrowserHost
    {
        BrowserHost()
        {
            memset(this, 0, sizeof(*this));
        }

        char m_Host[256];
        char m_Address[64];
        uint64_t m_Expires;
    };

    struct BrowserService
    {
        BrowserService()
        {
            memset(this, 0, sizeof(*this));
        }

        char m_FullServiceName[256];
        char m_InstanceName[128];
        char m_ServiceTypeLocal[128];
        char m_Host[256];
        char m_HostAddress[64];
        char m_Id[128];
        uint16_t m_Port;

        StoredTxt m_Txt[MDNS_MAX_TXT_ENTRIES];
        uint32_t m_TxtCount;

        uint64_t m_Expires;

        uint8_t m_HasPtr : 1;
        uint8_t m_HasSrv : 1;
        uint8_t m_HasTxt : 1;
        uint8_t m_HasAddress : 1;
        uint8_t m_Resolved : 1;
    };

    struct Browser
    {
        Browser()
        {
            memset(this, 0, sizeof(*this));
            m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
            m_Services.SetCapacity(16);
            m_Hosts.SetCapacity(16);
            m_InterfaceAddresses.SetCapacity(4);
            m_MembershipAddresses.SetCapacity(4);
            m_QueryInterval = 2 * 1000000ULL;
        }

        dmSocket::Socket m_Socket;
        ServiceCallback m_Callback;
        void* m_Context;

        char m_ServiceType[64];
        char m_ServiceTypeLocal[128];

        dmArray<BrowserService> m_Services;
        dmArray<BrowserHost> m_Hosts;
        dmArray<dmSocket::Address> m_InterfaceAddresses;
        dmArray<dmSocket::Address> m_MembershipAddresses;

        uint8_t m_Buffer[1500];

        uint64_t m_NextQuery;
        uint64_t m_QueryInterval;
        uint64_t m_NextInterfaceRefresh;
    };

    static uint64_t GetNow()
    {
        return dmTime::GetTime();
    }

    static uint64_t SecondsToMicroSeconds(uint32_t seconds)
    {
        return ((uint64_t) seconds) * 1000000ULL;
    }

    static bool NameEquals(const char* lhs, const char* rhs)
    {
        return dmStrCaseCmp(lhs, rhs) == 0;
    }

    static bool EndsWithLocal(const char* value)
    {
        uint32_t n = (uint32_t) strlen(value);
        static const char suffix[] = ".local";
        uint32_t suffix_n = (uint32_t) strlen(suffix);
        if (n < suffix_n)
            return false;
        return dmStrCaseCmp(value + n - suffix_n, suffix) == 0;
    }

    static void BuildLocalName(const char* value, char* out, uint32_t out_size)
    {
        if (value == 0)
        {
            out[0] = 0;
            return;
        }

        dmStrlCpy(out, value, out_size);
        if (!EndsWithLocal(out))
        {
            dmStrlCat(out, ".local", out_size);
        }
    }

    static void BuildFullServiceName(const char* instance_name, const char* service_type_local, char* out, uint32_t out_size)
    {
        dmStrlCpy(out, instance_name, out_size);
        if (out[0] && out[strlen(out) - 1] != '.')
            dmStrlCat(out, ".", out_size);
        dmStrlCat(out, service_type_local, out_size);
    }

    static void BuildInstanceName(const char* full_name, const char* service_type_local, char* out, uint32_t out_size)
    {
        uint32_t full_name_len = (uint32_t) strlen(full_name);
        uint32_t service_type_len = (uint32_t) strlen(service_type_local);

        if (full_name_len > service_type_len + 1)
        {
            const char* suffix = full_name + full_name_len - service_type_len;
            if (dmStrCaseCmp(suffix, service_type_local) == 0 && *(suffix - 1) == '.')
            {
                uint32_t instance_len = (uint32_t) (suffix - full_name - 1);
                if (instance_len >= out_size)
                    instance_len = out_size - 1;
                memcpy(out, full_name, instance_len);
                out[instance_len] = 0;
                return;
            }
        }

        dmStrlCpy(out, full_name, out_size);
    }

    static bool ContainsAddress(const dmArray<dmSocket::Address>& addresses, dmSocket::Address address)
    {
        for (uint32_t i = 0; i < addresses.Size(); ++i)
        {
            if (addresses[i] == address)
                return true;
        }
        return false;
    }

    static void CollectInterfaceAddresses(dmArray<dmSocket::Address>* addresses)
    {
        dmSocket::IfAddr interfaces[MDNS_MAX_INTERFACES];
        uint32_t interface_count = 0;
        dmSocket::GetIfAddresses(interfaces, MDNS_MAX_INTERFACES, &interface_count);

        addresses->SetSize(0);

        for (uint32_t i = 0; i < interface_count; ++i)
        {
            const dmSocket::IfAddr& interface = interfaces[i];
            if (interface.m_Address.m_family != dmSocket::DOMAIN_IPV4)
                continue;
            if (dmSocket::Empty(interface.m_Address))
                continue;
            if ((interface.m_Flags & dmSocket::FLAGS_UP) == 0)
                continue;
            if (ContainsAddress(*addresses, interface.m_Address))
                continue;

            if (addresses->Full())
                addresses->OffsetCapacity(4);
            addresses->Push(interface.m_Address);
        }
    }

    static void RefreshInterfaceAddresses(MDNS* mdns)
    {
        CollectInterfaceAddresses(&mdns->m_InterfaceAddresses);
        if (mdns->m_InterfaceAddresses.Size() > 0)
        {
            mdns->m_DefaultAddress = mdns->m_InterfaceAddresses[0];
        }
    }

    static void RefreshBrowserInterfaceAddresses(Browser* browser)
    {
        CollectInterfaceAddresses(&browser->m_InterfaceAddresses);
    }

    static bool ReadU16(const uint8_t* data, uint32_t size, uint32_t* offset, uint16_t* out)
    {
        if (*offset + 2 > size)
            return false;
        *out = (uint16_t) ((data[*offset] << 8) | data[*offset + 1]);
        *offset += 2;
        return true;
    }

    static bool ReadU32(const uint8_t* data, uint32_t size, uint32_t* offset, uint32_t* out)
    {
        if (*offset + 4 > size)
            return false;
        *out = (uint32_t) ((data[*offset] << 24) |
                           (data[*offset + 1] << 16) |
                           (data[*offset + 2] << 8) |
                           data[*offset + 3]);
        *offset += 4;
        return true;
    }

    static bool WriteU16(uint8_t* data, uint32_t size, uint32_t* offset, uint16_t value)
    {
        if (*offset + 2 > size)
            return false;
        data[*offset] = (uint8_t) ((value >> 8) & 0xff);
        data[*offset + 1] = (uint8_t) (value & 0xff);
        *offset += 2;
        return true;
    }

    static bool WriteU32(uint8_t* data, uint32_t size, uint32_t* offset, uint32_t value)
    {
        if (*offset + 4 > size)
            return false;
        data[*offset] = (uint8_t) ((value >> 24) & 0xff);
        data[*offset + 1] = (uint8_t) ((value >> 16) & 0xff);
        data[*offset + 2] = (uint8_t) ((value >> 8) & 0xff);
        data[*offset + 3] = (uint8_t) (value & 0xff);
        *offset += 4;
        return true;
    }

    static bool ReadName(const uint8_t* data, uint32_t size, uint32_t* offset, char* out, uint32_t out_size)
    {
        uint32_t pos = *offset;
        uint32_t out_len = 0;
        bool jumped = false;
        uint32_t jump_count = 0;

        while (true)
        {
            if (pos >= size)
                return false;

            uint8_t len = data[pos];

            if ((len & 0xC0) == 0xC0)
            {
                if (pos + 1 >= size)
                    return false;

                uint16_t pointer = (uint16_t) (((len & 0x3F) << 8) | data[pos + 1]);
                if (pointer >= size)
                    return false;

                if (!jumped)
                {
                    *offset = pos + 2;
                    jumped = true;
                }

                pos = pointer;
                ++jump_count;
                if (jump_count > 16)
                    return false;
                continue;
            }

            ++pos;

            if (len == 0)
            {
                if (!jumped)
                    *offset = pos;

                if (out_len == 0)
                {
                    if (out_size < 1)
                        return false;
                    out[0] = 0;
                }
                else
                {
                    out[out_len - 1] = 0;
                }
                return true;
            }

            if (len > 63 || pos + len > size)
                return false;

            if (out_len + len + 1 >= out_size)
                return false;

            memcpy(out + out_len, data + pos, len);
            out_len += len;
            out[out_len++] = '.';
            pos += len;
        }
    }

    static bool WriteName(uint8_t* data, uint32_t size, uint32_t* offset, const char* name)
    {
        const char* cursor = name;
        while (*cursor)
        {
            const char* dot = strchr(cursor, '.');
            uint32_t label_len = dot ? (uint32_t) (dot - cursor) : (uint32_t) strlen(cursor);
            if (label_len > 63 || *offset + 1 + label_len > size)
                return false;

            data[*offset] = (uint8_t) label_len;
            ++(*offset);
            memcpy(data + *offset, cursor, label_len);
            *offset += label_len;

            if (!dot)
                break;
            cursor = dot + 1;
        }

        if (*offset + 1 > size)
            return false;
        data[*offset] = 0;
        ++(*offset);
        return true;
    }

    static dmSocket::Socket CreateSocket()
    {
        dmSocket::Socket socket = dmSocket::INVALID_SOCKET_HANDLE;
        dmSocket::Result sr = dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_DGRAM, dmSocket::PROTOCOL_UDP, &socket);
        if (sr != dmSocket::RESULT_OK)
            return dmSocket::INVALID_SOCKET_HANDLE;

        sr = dmSocket::SetReuseAddress(socket, true);
        if (sr != dmSocket::RESULT_OK)
            goto bail;

        sr = dmSocket::Bind(socket, dmSocket::AddressFromIPString("0.0.0.0"), MDNS_PORT);
        if (sr != dmSocket::RESULT_OK)
            goto bail;

        return socket;

    bail:
        dmSocket::Delete(socket);
        return dmSocket::INVALID_SOCKET_HANDLE;
    }

    static bool SendPacket(dmSocket::Socket socket, const uint8_t* data, uint32_t size)
    {
        int sent = 0;
        dmSocket::Result sr = dmSocket::SendTo(socket, data, (int) size, &sent, dmSocket::AddressFromIPString(MDNS_MULTICAST_IPV4), MDNS_PORT);
        return sr == dmSocket::RESULT_OK;
    }

    static bool SendPacketOnInterface(dmSocket::Socket socket, const uint8_t* data, uint32_t size, const dmSocket::Address& interface_address)
    {
        dmSocket::Result sr = dmSocket::SetMulticastIf(socket, interface_address);
        if (sr != dmSocket::RESULT_OK)
            return false;

        return SendPacket(socket, data, size);
    }

    static void EnsureMemberships(dmSocket::Socket socket, const dmArray<dmSocket::Address>& interface_addresses, dmArray<dmSocket::Address>* membership_addresses)
    {
        const dmSocket::Address multicast_address = dmSocket::AddressFromIPString(MDNS_MULTICAST_IPV4);
        const dmSocket::Address any_address = dmSocket::AddressFromIPString("0.0.0.0");

        if (!ContainsAddress(*membership_addresses, any_address))
        {
            dmSocket::Result sr = dmSocket::AddMembership(socket, multicast_address, any_address, 255);
            if (sr == dmSocket::RESULT_OK)
            {
                if (membership_addresses->Full())
                    membership_addresses->OffsetCapacity(4);
                membership_addresses->Push(any_address);
            }
        }

        for (uint32_t i = 0; i < interface_addresses.Size(); ++i)
        {
            const dmSocket::Address& interface_address = interface_addresses[i];
            if (ContainsAddress(*membership_addresses, interface_address))
                continue;

            dmSocket::Result sr = dmSocket::AddMembership(socket, multicast_address, interface_address, 255);
            if (sr == dmSocket::RESULT_OK)
            {
                if (membership_addresses->Full())
                    membership_addresses->OffsetCapacity(4);
                membership_addresses->Push(interface_address);
            }
        }
    }

    static void SendPacketOnInterfaces(dmSocket::Socket socket, const uint8_t* data, uint32_t size, const dmArray<dmSocket::Address>& interface_addresses)
    {
        SendPacket(socket, data, size);

        for (uint32_t i = 0; i < interface_addresses.Size(); ++i)
        {
            SendPacketOnInterface(socket, data, size, interface_addresses[i]);
        }
    }

    static bool WriteRecordHeader(uint8_t* buffer, uint32_t buffer_size, uint32_t* offset,
                                  const char* name, uint16_t type, uint16_t klass, uint32_t ttl, uint16_t rdlength)
    {
        return WriteName(buffer, buffer_size, offset, name)
               && WriteU16(buffer, buffer_size, offset, type)
               && WriteU16(buffer, buffer_size, offset, klass)
               && WriteU32(buffer, buffer_size, offset, ttl)
               && WriteU16(buffer, buffer_size, offset, rdlength);
    }

    static bool WriteRecordPtr(uint8_t* buffer, uint32_t buffer_size, uint32_t* offset,
                               const char* service_type_local, const char* full_service_name, uint32_t ttl)
    {
        uint8_t name_buffer[256];
        uint32_t name_offset = 0;
        if (!WriteName(name_buffer, sizeof(name_buffer), &name_offset, full_service_name))
            return false;

        if (!WriteRecordHeader(buffer, buffer_size, offset,
                               service_type_local,
                               DNS_TYPE_PTR,
                               DNS_CLASS_IN,
                               ttl,
                               (uint16_t) name_offset))
            return false;

        if (*offset + name_offset > buffer_size)
            return false;

        memcpy(buffer + *offset, name_buffer, name_offset);
        *offset += name_offset;
        return true;
    }

    static bool WriteRecordSrv(uint8_t* buffer, uint32_t buffer_size, uint32_t* offset,
                               const RegisteredService& service, uint32_t ttl)
    {
        uint8_t data[512];
        uint32_t data_offset = 0;

        if (!WriteU16(data, sizeof(data), &data_offset, 0)   // priority
            || !WriteU16(data, sizeof(data), &data_offset, 0) // weight
            || !WriteU16(data, sizeof(data), &data_offset, service.m_Port)
            || !WriteName(data, sizeof(data), &data_offset, service.m_HostLocal))
        {
            return false;
        }

        if (!WriteRecordHeader(buffer, buffer_size, offset,
                               service.m_FullServiceName,
                               DNS_TYPE_SRV,
                               (uint16_t) (DNS_CLASS_IN | DNS_CLASS_FLUSH),
                               ttl,
                               (uint16_t) data_offset))
        {
            return false;
        }

        if (*offset + data_offset > buffer_size)
            return false;

        memcpy(buffer + *offset, data, data_offset);
        *offset += data_offset;
        return true;
    }

    static bool WriteRecordTxt(uint8_t* buffer, uint32_t buffer_size, uint32_t* offset,
                               const RegisteredService& service, uint32_t ttl)
    {
        uint8_t data[1024];
        uint32_t data_offset = 0;

        for (uint32_t i = 0; i < service.m_TxtCount; ++i)
        {
            char kv[320];
            dmSnPrintf(kv, sizeof(kv), "%s=%s", service.m_Txt[i].m_Key, service.m_Txt[i].m_Value);
            uint32_t len = (uint32_t) strlen(kv);
            if (len > 255 || data_offset + 1 + len > sizeof(data))
                return false;

            data[data_offset++] = (uint8_t) len;
            memcpy(data + data_offset, kv, len);
            data_offset += len;
        }

        if (data_offset == 0)
        {
            if (data_offset + 1 > sizeof(data))
                return false;
            data[data_offset++] = 0;
        }

        if (!WriteRecordHeader(buffer, buffer_size, offset,
                               service.m_FullServiceName,
                               DNS_TYPE_TXT,
                               (uint16_t) (DNS_CLASS_IN | DNS_CLASS_FLUSH),
                               ttl,
                               (uint16_t) data_offset))
        {
            return false;
        }

        if (*offset + data_offset > buffer_size)
            return false;

        memcpy(buffer + *offset, data, data_offset);
        *offset += data_offset;
        return true;
    }

    static bool WriteRecordA(uint8_t* buffer, uint32_t buffer_size, uint32_t* offset,
                             const RegisteredService& service, const dmSocket::Address& host_address, uint32_t ttl)
    {
        if (host_address.m_family != dmSocket::DOMAIN_IPV4 || dmSocket::Empty(host_address))
            return false;

        if (!WriteRecordHeader(buffer, buffer_size, offset,
                               service.m_HostLocal,
                               DNS_TYPE_A,
                               (uint16_t) (DNS_CLASS_IN | DNS_CLASS_FLUSH),
                               ttl,
                               4))
        {
            return false;
        }

        dmSocket::Address address = host_address;
        uint32_t ipv4 = *dmSocket::IPv4(&address);
        if (*offset + 4 > buffer_size)
            return false;

        buffer[*offset + 0] = (uint8_t) ((ipv4 >> 0) & 0xff);
        buffer[*offset + 1] = (uint8_t) ((ipv4 >> 8) & 0xff);
        buffer[*offset + 2] = (uint8_t) ((ipv4 >> 16) & 0xff);
        buffer[*offset + 3] = (uint8_t) ((ipv4 >> 24) & 0xff);
        *offset += 4;
        return true;
    }

    static uint32_t BuildResponseMessage(const RegisteredService& service, const dmSocket::Address* host_address, uint32_t ttl, uint8_t* buffer, uint32_t buffer_size)
    {
        uint32_t offset = 0;
        uint16_t answer_count = 0;

        if (!WriteU16(buffer, buffer_size, &offset, 0)                  // id
            || !WriteU16(buffer, buffer_size, &offset, DNS_FLAG_RESPONSE)
            || !WriteU16(buffer, buffer_size, &offset, 0)               // questions
            || !WriteU16(buffer, buffer_size, &offset, 0)               // answers (patched later)
            || !WriteU16(buffer, buffer_size, &offset, 0)               // authorities
            || !WriteU16(buffer, buffer_size, &offset, 0))              // additionals
        {
            return 0;
        }

        if (WriteRecordPtr(buffer, buffer_size, &offset, service.m_ServiceTypeLocal, service.m_FullServiceName, ttl))
            ++answer_count;

        if (WriteRecordSrv(buffer, buffer_size, &offset, service, ttl))
            ++answer_count;

        if (WriteRecordTxt(buffer, buffer_size, &offset, service, ttl))
            ++answer_count;

        const dmSocket::Address& service_address = host_address ? *host_address : service.m_HostAddress;
        if (WriteRecordA(buffer, buffer_size, &offset, service, service_address, ttl))
            ++answer_count;

        if (answer_count == 0)
            return 0;

        buffer[6] = (uint8_t) ((answer_count >> 8) & 0xff);
        buffer[7] = (uint8_t) (answer_count & 0xff);

        return offset;
    }

    static uint32_t BuildQueryMessage(const char* service_type_local, uint8_t* buffer, uint32_t buffer_size)
    {
        uint32_t offset = 0;
        if (!WriteU16(buffer, buffer_size, &offset, 0)   // id
            || !WriteU16(buffer, buffer_size, &offset, 0) // query
            || !WriteU16(buffer, buffer_size, &offset, 1) // one question
            || !WriteU16(buffer, buffer_size, &offset, 0)
            || !WriteU16(buffer, buffer_size, &offset, 0)
            || !WriteU16(buffer, buffer_size, &offset, 0)
            || !WriteName(buffer, buffer_size, &offset, service_type_local)
            || !WriteU16(buffer, buffer_size, &offset, DNS_TYPE_PTR)
            || !WriteU16(buffer, buffer_size, &offset, DNS_CLASS_IN))
        {
            return 0;
        }

        return offset;
    }

    static int32_t FindRegisteredService(dmArray<RegisteredService>& services, const char* id)
    {
        for (uint32_t i = 0; i < services.Size(); ++i)
        {
            if (strcmp(services[i].m_Id, id) == 0)
                return (int32_t) i;
        }
        return -1;
    }

    static void HandleAnnounceResponse(MDNS* mdns, const RegisteredService& service, const dmSocket::Address* interface_address, uint32_t ttl)
    {
        uint32_t size = BuildResponseMessage(service, interface_address, ttl, mdns->m_Buffer, sizeof(mdns->m_Buffer));
        if (size > 0)
        {
            if (interface_address != 0)
                SendPacketOnInterface(mdns->m_Socket, mdns->m_Buffer, size, *interface_address);
            else
                SendPacket(mdns->m_Socket, mdns->m_Buffer, size);
        }
    }

    static void AnnounceService(MDNS* mdns, const RegisteredService& service, uint32_t ttl)
    {
        HandleAnnounceResponse(mdns, service, 0, ttl);

        for (uint32_t i = 0; i < mdns->m_InterfaceAddresses.Size(); ++i)
        {
            HandleAnnounceResponse(mdns, service, &mdns->m_InterfaceAddresses[i], ttl);
        }
    }

    static bool ParseTxtRData(const uint8_t* data, uint16_t data_len, StoredTxt* txt, uint32_t max_txt, uint32_t* txt_count)
    {
        *txt_count = 0;

        uint32_t offset = 0;
        while (offset < data_len)
        {
            uint8_t len = data[offset++];
            if (offset + len > data_len)
                return false;

            if (len > 0 && *txt_count < max_txt)
            {
                char kv[320];
                uint32_t copy_len = len;
                if (copy_len >= sizeof(kv))
                    copy_len = sizeof(kv) - 1;

                memcpy(kv, data + offset, copy_len);
                kv[copy_len] = 0;

                const char* eq = strchr(kv, '=');
                if (eq)
                {
                    uint32_t key_len = (uint32_t) (eq - kv);
                    uint32_t value_len = (uint32_t) strlen(eq + 1);

                    if (key_len >= sizeof(txt[*txt_count].m_Key))
                        key_len = sizeof(txt[*txt_count].m_Key) - 1;
                    if (value_len >= sizeof(txt[*txt_count].m_Value))
                        value_len = sizeof(txt[*txt_count].m_Value) - 1;

                    memcpy(txt[*txt_count].m_Key, kv, key_len);
                    txt[*txt_count].m_Key[key_len] = 0;
                    memcpy(txt[*txt_count].m_Value, eq + 1, value_len);
                    txt[*txt_count].m_Value[value_len] = 0;
                }
                else
                {
                    uint32_t key_len = copy_len;
                    if (key_len >= sizeof(txt[*txt_count].m_Key))
                        key_len = sizeof(txt[*txt_count].m_Key) - 1;

                    memcpy(txt[*txt_count].m_Key, kv, key_len);
                    txt[*txt_count].m_Key[key_len] = 0;
                    txt[*txt_count].m_Value[0] = 0;
                }

                ++(*txt_count);
            }

            offset += len;
        }

        return true;
    }

    static bool QueryMatchesService(const RegisteredService& service, const char* qname, uint16_t qtype)
    {
        if (qtype != DNS_TYPE_PTR && qtype != DNS_TYPE_SRV && qtype != DNS_TYPE_TXT && qtype != DNS_TYPE_A && qtype != DNS_TYPE_ANY)
            return false;

        return NameEquals(qname, service.m_ServiceTypeLocal)
               || NameEquals(qname, service.m_FullServiceName)
               || NameEquals(qname, service.m_HostLocal);
    }

    static void HandleIncomingQueries(MDNS* mdns)
    {
        bool incoming_data = false;
        do
        {
            incoming_data = false;
            dmSocket::Selector selector;
            dmSocket::SelectorZero(&selector);
            dmSocket::SelectorSet(&selector, dmSocket::SELECTOR_KIND_READ, mdns->m_Socket);

            if (dmSocket::Select(&selector, 0) != dmSocket::RESULT_OK)
                return;

            if (!dmSocket::SelectorIsSet(&selector, dmSocket::SELECTOR_KIND_READ, mdns->m_Socket))
                return;

            dmSocket::Address from_address;
            uint16_t from_port = 0;
            int received = 0;
            dmSocket::Result sr = dmSocket::ReceiveFrom(mdns->m_Socket, mdns->m_Buffer, sizeof(mdns->m_Buffer), &received, &from_address, &from_port);
            if (sr != dmSocket::RESULT_OK || received <= 0)
                return;

            (void) from_address;
            (void) from_port;

            incoming_data = true;

            uint32_t size = (uint32_t) received;
            if (size < 12)
                continue;

            uint32_t offset = 0;
            uint16_t id = 0;
            uint16_t flags = 0;
            uint16_t qdcount = 0;
            uint16_t ancount = 0;
            uint16_t nscount = 0;
            uint16_t arcount = 0;

            if (!ReadU16(mdns->m_Buffer, size, &offset, &id)
                || !ReadU16(mdns->m_Buffer, size, &offset, &flags)
                || !ReadU16(mdns->m_Buffer, size, &offset, &qdcount)
                || !ReadU16(mdns->m_Buffer, size, &offset, &ancount)
                || !ReadU16(mdns->m_Buffer, size, &offset, &nscount)
                || !ReadU16(mdns->m_Buffer, size, &offset, &arcount))
            {
                continue;
            }

            (void) id;
            (void) ancount;
            (void) nscount;
            (void) arcount;

            if ((flags & 0x8000) != 0)
                continue;

            bool should_announce = false;
            for (uint16_t i = 0; i < qdcount; ++i)
            {
                char qname[256];
                uint16_t qtype = 0;
                uint16_t qclass = 0;

                if (!ReadName(mdns->m_Buffer, size, &offset, qname, sizeof(qname))
                    || !ReadU16(mdns->m_Buffer, size, &offset, &qtype)
                    || !ReadU16(mdns->m_Buffer, size, &offset, &qclass))
                {
                    should_announce = false;
                    break;
                }

                (void) qclass;

                for (uint32_t s = 0; s < mdns->m_Services.Size(); ++s)
                {
                    if (QueryMatchesService(mdns->m_Services[s], qname, qtype))
                    {
                        should_announce = true;
                        break;
                    }
                }
            }

            if (should_announce)
            {
                const dmSocket::Address* response_interface = 0;
                if (mdns->m_InterfaceAddresses.Size() > 0)
                {
                    response_interface = &mdns->m_InterfaceAddresses[0];

                    if (from_address.m_family == dmSocket::DOMAIN_IPV4)
                    {
                        uint32_t best_match = ~0u;
                        for (uint32_t i = 0; i < mdns->m_InterfaceAddresses.Size(); ++i)
                        {
                            uint32_t match = dmSocket::BitDifference(mdns->m_InterfaceAddresses[i], from_address);
                            if (match < best_match)
                            {
                                best_match = match;
                                response_interface = &mdns->m_InterfaceAddresses[i];
                            }
                        }
                    }
                }

                for (uint32_t s = 0; s < mdns->m_Services.Size(); ++s)
                {
                    // Always multicast responses for compatibility with mDNS browsers
                    // that ignore unicast replies to multicast queries.
                    HandleAnnounceResponse(mdns, mdns->m_Services[s], 0, mdns->m_Services[s].m_Ttl);
                    if (response_interface != 0)
                        HandleAnnounceResponse(mdns, mdns->m_Services[s], response_interface, mdns->m_Services[s].m_Ttl);
                }
            }
        } while (incoming_data);
    }

    static bool IsEncodableDnsName(const char* name)
    {
        uint8_t buffer[512];
        uint32_t offset = 0;
        return WriteName(buffer, sizeof(buffer), &offset, name);
    }

    static int32_t FindBrowserHost(dmArray<BrowserHost>& hosts, const char* host)
    {
        for (uint32_t i = 0; i < hosts.Size(); ++i)
        {
            if (NameEquals(hosts[i].m_Host, host))
                return (int32_t) i;
        }
        return -1;
    }

    static int32_t FindBrowserService(dmArray<BrowserService>& services, const char* full_name)
    {
        for (uint32_t i = 0; i < services.Size(); ++i)
        {
            if (NameEquals(services[i].m_FullServiceName, full_name))
                return (int32_t) i;
        }
        return -1;
    }

    static void EmitBrowserEvent(Browser* browser, const BrowserService& service, EventType event_type)
    {
        if (!browser->m_Callback)
            return;

        TxtEntry txt_entries[MDNS_MAX_TXT_ENTRIES];
        for (uint32_t i = 0; i < service.m_TxtCount; ++i)
        {
            txt_entries[i].m_Key = service.m_Txt[i].m_Key;
            txt_entries[i].m_Value = service.m_Txt[i].m_Value;
        }

        ServiceEvent event;
        memset(&event, 0, sizeof(event));
        event.m_Type = event_type;
        event.m_Id = service.m_Id[0] ? service.m_Id : service.m_FullServiceName;
        event.m_InstanceName = service.m_InstanceName;
        event.m_ServiceType = browser->m_ServiceType;
        event.m_Host = service.m_HostAddress[0] ? service.m_HostAddress : service.m_Host;
        event.m_Port = service.m_Port;
        event.m_Txt = txt_entries;
        event.m_TxtCount = service.m_TxtCount;

        browser->m_Callback(browser->m_Context, &event);
    }

    static void RefreshServiceAddress(Browser* browser, BrowserService* service)
    {
        service->m_HasAddress = 0;

        int32_t host_index = FindBrowserHost(browser->m_Hosts, service->m_Host);
        if (host_index >= 0)
        {
            dmStrlCpy(service->m_HostAddress, browser->m_Hosts[(uint32_t) host_index].m_Address, sizeof(service->m_HostAddress));
            service->m_HasAddress = 1;
        }
    }

    static void TryResolveService(Browser* browser, BrowserService* service)
    {
        RefreshServiceAddress(browser, service);

        const bool complete = service->m_HasPtr && service->m_HasSrv && service->m_HasTxt && service->m_HasAddress;
        if (complete && !service->m_Resolved)
        {
            service->m_Resolved = 1;
            EmitBrowserEvent(browser, *service, EVENT_RESOLVED);
        }
    }

    static void HandleBrowserRecord(Browser* browser, const uint8_t* data, uint32_t size, uint32_t* offset)
    {
        char name[256];
        uint16_t type = 0;
        uint16_t klass = 0;
        uint32_t ttl = 0;
        uint16_t rdlength = 0;

        if (!ReadName(data, size, offset, name, sizeof(name))
            || !ReadU16(data, size, offset, &type)
            || !ReadU16(data, size, offset, &klass)
            || !ReadU32(data, size, offset, &ttl)
            || !ReadU16(data, size, offset, &rdlength))
        {
            *offset = size;
            return;
        }

        (void) klass;

        if (*offset + rdlength > size)
        {
            *offset = size;
            return;
        }

        uint32_t rdata_offset = *offset;
        *offset += rdlength;

        if (type == DNS_TYPE_PTR && NameEquals(name, browser->m_ServiceTypeLocal))
        {
            uint32_t name_offset = rdata_offset;
            char full_service_name[256];
            if (!ReadName(data, size, &name_offset, full_service_name, sizeof(full_service_name)))
                return;

            int32_t service_index = FindBrowserService(browser->m_Services, full_service_name);
            bool created = false;
            if (service_index < 0)
            {
                if (browser->m_Services.Full())
                    browser->m_Services.OffsetCapacity(8);

                BrowserService service;
                dmStrlCpy(service.m_FullServiceName, full_service_name, sizeof(service.m_FullServiceName));
                dmStrlCpy(service.m_ServiceTypeLocal, browser->m_ServiceTypeLocal, sizeof(service.m_ServiceTypeLocal));
                BuildInstanceName(service.m_FullServiceName, service.m_ServiceTypeLocal, service.m_InstanceName, sizeof(service.m_InstanceName));
                browser->m_Services.Push(service);
                service_index = (int32_t) (browser->m_Services.Size() - 1);
                created = true;
            }

            BrowserService& service = browser->m_Services[(uint32_t) service_index];

            if (ttl == 0)
            {
                EmitBrowserEvent(browser, service, EVENT_REMOVED);
                browser->m_Services.EraseSwap((uint32_t) service_index);
                return;
            }

            service.m_HasPtr = 1;
            service.m_Expires = GetNow() + SecondsToMicroSeconds(ttl);
            if (created)
            {
                EmitBrowserEvent(browser, service, EVENT_ADDED);
            }
            TryResolveService(browser, &service);
            return;
        }

        if (type == DNS_TYPE_SRV)
        {
            int32_t service_index = FindBrowserService(browser->m_Services, name);
            if (service_index < 0)
                return;

            BrowserService& service = browser->m_Services[(uint32_t) service_index];

            uint32_t tmp = rdata_offset;
            uint16_t priority = 0;
            uint16_t weight = 0;
            uint16_t port = 0;
            if (!ReadU16(data, size, &tmp, &priority)
                || !ReadU16(data, size, &tmp, &weight)
                || !ReadU16(data, size, &tmp, &port)
                || !ReadName(data, size, &tmp, service.m_Host, sizeof(service.m_Host)))
            {
                return;
            }

            (void) priority;
            (void) weight;

            service.m_Port = port;
            service.m_HasSrv = 1;
            service.m_Expires = GetNow() + SecondsToMicroSeconds(ttl);
            TryResolveService(browser, &service);
            return;
        }

        if (type == DNS_TYPE_TXT)
        {
            int32_t service_index = FindBrowserService(browser->m_Services, name);
            if (service_index < 0)
                return;

            BrowserService& service = browser->m_Services[(uint32_t) service_index];
            uint32_t txt_count = 0;
            if (!ParseTxtRData(data + rdata_offset, rdlength, service.m_Txt, MDNS_MAX_TXT_ENTRIES, &txt_count))
                return;

            service.m_TxtCount = txt_count;
            service.m_HasTxt = 1;
            service.m_Expires = GetNow() + SecondsToMicroSeconds(ttl);

            service.m_Id[0] = 0;
            for (uint32_t i = 0; i < txt_count; ++i)
            {
                if (strcmp(service.m_Txt[i].m_Key, "id") == 0)
                {
                    dmStrlCpy(service.m_Id, service.m_Txt[i].m_Value, sizeof(service.m_Id));
                    break;
                }
            }

            TryResolveService(browser, &service);
            return;
        }

        if (type == DNS_TYPE_A)
        {
            if (rdlength != 4)
                return;

            int32_t host_index = FindBrowserHost(browser->m_Hosts, name);
            if (ttl == 0)
            {
                if (host_index >= 0)
                    browser->m_Hosts.EraseSwap((uint32_t) host_index);
                return;
            }

            BrowserHost host;
            if (host_index >= 0)
                host = browser->m_Hosts[(uint32_t) host_index];

            dmStrlCpy(host.m_Host, name, sizeof(host.m_Host));
            dmSnPrintf(host.m_Address, sizeof(host.m_Address), "%u.%u.%u.%u",
                       data[rdata_offset + 0],
                       data[rdata_offset + 1],
                       data[rdata_offset + 2],
                       data[rdata_offset + 3]);
            host.m_Expires = GetNow() + SecondsToMicroSeconds(ttl);

            if (host_index >= 0)
            {
                browser->m_Hosts[(uint32_t) host_index] = host;
            }
            else
            {
                if (browser->m_Hosts.Full())
                    browser->m_Hosts.OffsetCapacity(8);
                browser->m_Hosts.Push(host);
            }

            for (uint32_t i = 0; i < browser->m_Services.Size(); ++i)
            {
                TryResolveService(browser, &browser->m_Services[i]);
            }
        }
    }

    static void HandleBrowserResponses(Browser* browser)
    {
        bool incoming_data = false;
        do
        {
            incoming_data = false;
            dmSocket::Selector selector;
            dmSocket::SelectorZero(&selector);
            dmSocket::SelectorSet(&selector, dmSocket::SELECTOR_KIND_READ, browser->m_Socket);

            if (dmSocket::Select(&selector, 0) != dmSocket::RESULT_OK)
                return;

            if (!dmSocket::SelectorIsSet(&selector, dmSocket::SELECTOR_KIND_READ, browser->m_Socket))
                return;

            dmSocket::Address from_address;
            uint16_t from_port = 0;
            int received = 0;
            dmSocket::Result sr = dmSocket::ReceiveFrom(browser->m_Socket, browser->m_Buffer, sizeof(browser->m_Buffer), &received, &from_address, &from_port);
            if (sr != dmSocket::RESULT_OK || received <= 0)
                return;

            incoming_data = true;

            (void) from_address;
            (void) from_port;

            uint32_t size = (uint32_t) received;
            if (size < 12)
                continue;

            uint32_t offset = 0;
            uint16_t id = 0;
            uint16_t flags = 0;
            uint16_t qdcount = 0;
            uint16_t ancount = 0;
            uint16_t nscount = 0;
            uint16_t arcount = 0;

            if (!ReadU16(browser->m_Buffer, size, &offset, &id)
                || !ReadU16(browser->m_Buffer, size, &offset, &flags)
                || !ReadU16(browser->m_Buffer, size, &offset, &qdcount)
                || !ReadU16(browser->m_Buffer, size, &offset, &ancount)
                || !ReadU16(browser->m_Buffer, size, &offset, &nscount)
                || !ReadU16(browser->m_Buffer, size, &offset, &arcount))
            {
                continue;
            }

            (void) id;

            // Skip query sections.
            for (uint16_t i = 0; i < qdcount; ++i)
            {
                char qname[256];
                uint16_t qtype = 0;
                uint16_t qclass = 0;
                if (!ReadName(browser->m_Buffer, size, &offset, qname, sizeof(qname))
                    || !ReadU16(browser->m_Buffer, size, &offset, &qtype)
                    || !ReadU16(browser->m_Buffer, size, &offset, &qclass))
                {
                    offset = size;
                    break;
                }
            }

            if (offset >= size)
                continue;

            const uint32_t records = (uint32_t) ancount + (uint32_t) nscount + (uint32_t) arcount;
            for (uint32_t i = 0; i < records; ++i)
            {
                HandleBrowserRecord(browser, browser->m_Buffer, size, &offset);
                if (offset >= size)
                    break;
            }
        } while (incoming_data);
    }

    static void ExpireBrowserEntries(Browser* browser)
    {
        const uint64_t now = GetNow();

        for (uint32_t i = 0; i < browser->m_Hosts.Size();)
        {
            if (now >= browser->m_Hosts[i].m_Expires)
            {
                browser->m_Hosts.EraseSwap(i);
            }
            else
            {
                ++i;
            }
        }

        for (uint32_t i = 0; i < browser->m_Services.Size();)
        {
            BrowserService& service = browser->m_Services[i];
            if (now >= service.m_Expires)
            {
                EmitBrowserEvent(browser, service, EVENT_REMOVED);
                browser->m_Services.EraseSwap(i);
            }
            else
            {
                ++i;
            }
        }
    }

    Result New(const Params* params, HMDNS* mdns)
    {
        if (mdns == 0)
            return RESULT_INVALID_ARGS;

        *mdns = 0;

        MDNS* instance = new MDNS();
        if (instance == 0)
            return RESULT_OUT_OF_RESOURCES;

        if (params)
            instance->m_Params = *params;

        if (!instance->m_Params.m_UseIPv4)
        {
            delete instance;
            return RESULT_INVALID_ARGS;
        }

        char host_name[128];
        if (dmSocket::GetHostname(host_name, sizeof(host_name)) != dmSocket::RESULT_OK)
        {
            dmStrlCpy(host_name, "defold", sizeof(host_name));
        }

        BuildLocalName(host_name, instance->m_DefaultHostLocal, sizeof(instance->m_DefaultHostLocal));
        dmStrlCpy(instance->m_DefaultHost, host_name, sizeof(instance->m_DefaultHost));

        dmSocket::Address local_address;
        if (dmSocket::GetLocalAddress(&local_address) == dmSocket::RESULT_OK && local_address.m_family == dmSocket::DOMAIN_IPV4)
            instance->m_DefaultAddress = local_address;

        RefreshInterfaceAddresses(instance);

        instance->m_Socket = CreateSocket();
        if (instance->m_Socket == dmSocket::INVALID_SOCKET_HANDLE)
        {
            delete instance;
            return RESULT_NETWORK_ERROR;
        }

        EnsureMemberships(instance->m_Socket, instance->m_InterfaceAddresses, &instance->m_MembershipAddresses);

        instance->m_NextAnnounce = GetNow();
        instance->m_NextInterfaceRefresh = GetNow();
        *mdns = instance;
        return RESULT_OK;
    }

    Result RegisterService(HMDNS mdns, const ServiceDesc* desc)
    {
        if (mdns == 0 || desc == 0 || desc->m_Id == 0 || desc->m_InstanceName == 0 || desc->m_ServiceType == 0 || desc->m_Port == 0)
            return RESULT_INVALID_ARGS;

        if (FindRegisteredService(mdns->m_Services, desc->m_Id) >= 0)
            return RESULT_ALREADY_REGISTERED;

        if (mdns->m_Services.Full())
            mdns->m_Services.OffsetCapacity(4);

        RegisteredService service;
        dmStrlCpy(service.m_Id, desc->m_Id, sizeof(service.m_Id));
        dmStrlCpy(service.m_InstanceName, desc->m_InstanceName, sizeof(service.m_InstanceName));
        dmStrlCpy(service.m_ServiceType, desc->m_ServiceType, sizeof(service.m_ServiceType));

        BuildLocalName(service.m_ServiceType, service.m_ServiceTypeLocal, sizeof(service.m_ServiceTypeLocal));
        BuildFullServiceName(service.m_InstanceName, service.m_ServiceTypeLocal, service.m_FullServiceName, sizeof(service.m_FullServiceName));

        if (desc->m_Host && desc->m_Host[0])
        {
            dmStrlCpy(service.m_Host, desc->m_Host, sizeof(service.m_Host));
            BuildLocalName(service.m_Host, service.m_HostLocal, sizeof(service.m_HostLocal));
        }
        else
        {
            dmStrlCpy(service.m_Host, mdns->m_DefaultHost, sizeof(service.m_Host));
            dmStrlCpy(service.m_HostLocal, mdns->m_DefaultHostLocal, sizeof(service.m_HostLocal));
        }

        if (mdns->m_InterfaceAddresses.Size() > 0)
            service.m_HostAddress = mdns->m_InterfaceAddresses[0];
        else
            service.m_HostAddress = mdns->m_DefaultAddress;
        service.m_Port = desc->m_Port;
        service.m_Ttl = desc->m_Ttl ? desc->m_Ttl : mdns->m_Params.m_Ttl;

        if (!IsEncodableDnsName(service.m_ServiceTypeLocal)
            || !IsEncodableDnsName(service.m_FullServiceName)
            || !IsEncodableDnsName(service.m_HostLocal))
        {
            return RESULT_INVALID_ARGS;
        }

        service.m_TxtCount = 0;
        if (desc->m_Txt)
        {
            for (uint32_t i = 0; i < desc->m_TxtCount && service.m_TxtCount < MDNS_MAX_TXT_ENTRIES; ++i)
            {
                if (!desc->m_Txt[i].m_Key)
                    continue;
                dmStrlCpy(service.m_Txt[service.m_TxtCount].m_Key, desc->m_Txt[i].m_Key, sizeof(service.m_Txt[service.m_TxtCount].m_Key));
                dmStrlCpy(service.m_Txt[service.m_TxtCount].m_Value, desc->m_Txt[i].m_Value ? desc->m_Txt[i].m_Value : "", sizeof(service.m_Txt[service.m_TxtCount].m_Value));
                ++service.m_TxtCount;
            }
        }

        mdns->m_Services.Push(service);
        AnnounceService(mdns, mdns->m_Services.Back(), mdns->m_Services.Back().m_Ttl);
        return RESULT_OK;
    }

    Result DeregisterService(HMDNS mdns, const char* id)
    {
        if (mdns == 0 || id == 0)
            return RESULT_INVALID_ARGS;

        int32_t index = FindRegisteredService(mdns->m_Services, id);
        if (index < 0)
            return RESULT_NOT_REGISTERED;

        AnnounceService(mdns, mdns->m_Services[(uint32_t) index], 0);
        mdns->m_Services.EraseSwap((uint32_t) index);
        return RESULT_OK;
    }

    void Update(HMDNS mdns)
    {
        if (mdns == 0)
            return;

        const uint64_t now = GetNow();
        if (now >= mdns->m_NextInterfaceRefresh)
        {
            RefreshInterfaceAddresses(mdns);
            EnsureMemberships(mdns->m_Socket, mdns->m_InterfaceAddresses, &mdns->m_MembershipAddresses);
            mdns->m_NextInterfaceRefresh = now + SecondsToMicroSeconds(MDNS_INTERFACE_REFRESH_INTERVAL);
        }

        HandleIncomingQueries(mdns);

        if (now >= mdns->m_NextAnnounce)
        {
            for (uint32_t i = 0; i < mdns->m_Services.Size(); ++i)
            {
                AnnounceService(mdns, mdns->m_Services[i], mdns->m_Services[i].m_Ttl);
            }
            mdns->m_NextAnnounce = now + SecondsToMicroSeconds(mdns->m_Params.m_AnnounceInterval);
        }
    }

    Result Delete(HMDNS mdns)
    {
        if (mdns == 0)
            return RESULT_INVALID_ARGS;

        for (uint32_t i = 0; i < mdns->m_Services.Size(); ++i)
        {
            AnnounceService(mdns, mdns->m_Services[i], 0);
        }

        if (mdns->m_Socket != dmSocket::INVALID_SOCKET_HANDLE)
        {
            dmSocket::Delete(mdns->m_Socket);
        }

        delete mdns;
        return RESULT_OK;
    }

    Result NewBrowser(const BrowserParams* params, HBrowser* browser)
    {
        if (browser == 0 || params == 0 || params->m_ServiceType == 0)
            return RESULT_INVALID_ARGS;

        *browser = 0;

        Browser* instance = new Browser();
        if (instance == 0)
            return RESULT_OUT_OF_RESOURCES;

        dmStrlCpy(instance->m_ServiceType, params->m_ServiceType, sizeof(instance->m_ServiceType));
        BuildLocalName(params->m_ServiceType, instance->m_ServiceTypeLocal, sizeof(instance->m_ServiceTypeLocal));
        instance->m_Callback = params->m_Callback;
        instance->m_Context = params->m_Context;

        RefreshBrowserInterfaceAddresses(instance);
        instance->m_Socket = CreateSocket();
        if (instance->m_Socket == dmSocket::INVALID_SOCKET_HANDLE)
        {
            delete instance;
            return RESULT_NETWORK_ERROR;
        }

        EnsureMemberships(instance->m_Socket, instance->m_InterfaceAddresses, &instance->m_MembershipAddresses);
        instance->m_NextQuery = 0;
        instance->m_NextInterfaceRefresh = 0;
        *browser = instance;
        return RESULT_OK;
    }

    void UpdateBrowser(HBrowser browser)
    {
        if (browser == 0)
            return;

        const uint64_t now = GetNow();
        if (now >= browser->m_NextInterfaceRefresh)
        {
            RefreshBrowserInterfaceAddresses(browser);
            EnsureMemberships(browser->m_Socket, browser->m_InterfaceAddresses, &browser->m_MembershipAddresses);
            browser->m_NextInterfaceRefresh = now + SecondsToMicroSeconds(MDNS_INTERFACE_REFRESH_INTERVAL);
        }

        if (now >= browser->m_NextQuery)
        {
            uint32_t query_size = BuildQueryMessage(browser->m_ServiceTypeLocal, browser->m_Buffer, sizeof(browser->m_Buffer));
            if (query_size > 0)
            {
                SendPacketOnInterfaces(browser->m_Socket, browser->m_Buffer, query_size, browser->m_InterfaceAddresses);
            }
            browser->m_NextQuery = now + browser->m_QueryInterval;
        }

        HandleBrowserResponses(browser);
        ExpireBrowserEntries(browser);
    }

    Result DeleteBrowser(HBrowser browser)
    {
        if (browser == 0)
            return RESULT_INVALID_ARGS;

        if (browser->m_Socket != dmSocket::INVALID_SOCKET_HANDLE)
            dmSocket::Delete(browser->m_Socket);

        delete browser;
        return RESULT_OK;
    }
}
