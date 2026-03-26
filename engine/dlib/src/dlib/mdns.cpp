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

#include <string.h>

#include "mdns.h"

#include "array.h"
#include "dstrings.h"
#include "log.h"
#include "socket.h"
#include "time.h"

namespace dmMDNS
{
    // Minimal IPv4-only mDNS/DNS-SD responder and browser.
    //
    // References:
    // RFC 6762: Multicast DNS transport rules, query/response behavior,
    //           cache-flush semantics, and probing/announcing flow.
    // RFC 6763: DNS-Based Service Discovery record layout (PTR + SRV + TXT)
    //           and TXT key/value encoding rules.
    // RFC 1035 section 4.1.4: DNS message compression used by ReadName().
    //
    // The implementation below focuses on basic advertise/browse behavior and
    // keeps names in dotted presentation form between wire-format conversions.
    static const char* MDNS_MULTICAST_IPV4 = "224.0.0.251";
    static const uint16_t MDNS_PORT = 5353;
    static const uint32_t MDNS_MAX_TXT_ENTRIES = 16;
    static const uint32_t MDNS_INTERFACE_REFRESH_INTERVAL = 5;
    static const uint32_t MDNS_MAX_INTERFACES = 32;
    static const uint32_t MDNS_PROBE_INTERVAL_MS = 250;
    static const uint32_t MDNS_PROBE_COUNT = 3;
    static const uint32_t MDNS_PROBE_RESTART_DELAY = 1;
    static const uint32_t MDNS_STARTUP_ANNOUNCEMENTS = 2;
    static const uint32_t MDNS_STARTUP_ANNOUNCE_INTERVAL = 1;
    static const uint64_t MDNS_BROWSER_INITIAL_QUERY_INTERVAL = 1 * 1000000ULL;
    static const uint64_t MDNS_BROWSER_MAX_QUERY_INTERVAL = 60 * 1000000ULL;
    static const uint32_t MDNS_MAX_PROBE_RECORDS = 8;

    static const uint16_t DNS_TYPE_A = 1;
    static const uint16_t DNS_TYPE_PTR = 12;
    static const uint16_t DNS_TYPE_TXT = 16;
    static const uint16_t DNS_TYPE_SRV = 33;
    static const uint16_t DNS_TYPE_ANY = 255;

    static const uint16_t DNS_CLASS_IN = 1;
    static const uint16_t DNS_CLASS_FLUSH = 0x8000;

    static const uint16_t DNS_FLAG_RESPONSE = 0x8400;

    enum ServiceState
    {
        SERVICE_STATE_PROBING = 0,
        SERVICE_STATE_ACTIVE = 1,
        SERVICE_STATE_CONFLICT = 2
    };

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

        uint64_t m_NextProbe;
        uint64_t m_NextAnnounce;
        uint8_t m_ProbeCount;
        uint8_t m_StartupAnnounceCount;
        uint8_t m_State;
        uint8_t m_ProbeHost;
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
        uint64_t m_Refresh;
        uint32_t m_Ttl;
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

        uint64_t m_PtrExpires;
        uint64_t m_SrvExpires;
        uint64_t m_TxtExpires;
        uint64_t m_PtrRefresh;
        uint64_t m_SrvRefresh;
        uint64_t m_TxtRefresh;
        uint32_t m_PtrTtl;
        uint32_t m_SrvTtl;
        uint32_t m_TxtTtl;

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
            m_QueryInterval = MDNS_BROWSER_INITIAL_QUERY_INTERVAL;
        }

        dmSocket::Socket m_Socket;
        FServiceCallback m_Callback;
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

    struct ProbeRecord
    {
        ProbeRecord()
        : m_Type(0)
        , m_Class(0)
        , m_RDataLength(0)
        {
            memset(m_RData, 0, sizeof(m_RData));
        }

        uint16_t m_Type;
        uint16_t m_Class;
        uint16_t m_RDataLength;
        uint8_t  m_RData[1024];
    };

    static uint64_t GetNow()
    {
        return dmTime::GetTime();
    }

    static uint64_t SecondsToMicroSeconds(uint32_t seconds)
    {
        return ((uint64_t) seconds) * 1000000ULL;
    }

    static uint64_t HalfSecondsToMicroSeconds(uint32_t seconds)
    {
        return (((uint64_t) seconds) * 1000000ULL) / 2ULL;
    }

    static uint32_t RemainingTtlSeconds(uint64_t expires, uint64_t now)
    {
        if (expires <= now)
            return 0;

        const uint64_t remaining = expires - now;
        return (uint32_t) ((remaining + 1000000ULL - 1ULL) / 1000000ULL);
    }

    static bool NameEquals(const char* lhs, const char* rhs)
    {
        return dmStrCaseCmp(lhs, rhs) == 0;
    }

    static bool EndsWithLocal(const char* value)
    {
        const char* last_dot = strrchr(value, '.');
        return last_dot && dmStrCaseCmp(last_dot, ".local") == 0;
    }

    static bool IsDigit(char c)
    {
        return c >= '0' && c <= '9';
    }

    static bool DecodeEscapedChar(const char** cursor, char* out)
    {
        if (**cursor != '\\')
        {
            *out = **cursor;
            ++(*cursor);
            return true;
        }

        ++(*cursor);
        if (**cursor == 0)
            return false;

        if (IsDigit((*cursor)[0]) && IsDigit((*cursor)[1]) && IsDigit((*cursor)[2]))
        {
            *out = (char) ((((*cursor)[0] - '0') * 100) + (((*cursor)[1] - '0') * 10) + ((*cursor)[2] - '0'));
            *cursor += 3;
            return true;
        }

        *out = **cursor;
        ++(*cursor);
        return true;
    }

    static void EscapeDnsText(const char* value, char* out, uint32_t out_size)
    {
        if (out_size == 0)
            return;

        if (value == 0)
        {
            out[0] = 0;
            return;
        }

        uint32_t out_len = 0;
        while (*value && out_len + 1 < out_size)
        {
            const char c = *value++;
            if (c == '.' || c == '\\')
            {
                if (out_len + 2 >= out_size)
                    break;
                out[out_len++] = '\\';
            }
            out[out_len++] = c;
        }
        out[out_len] = 0;
    }

    static void UnescapeDnsText(const char* value, char* out, uint32_t out_size)
    {
        if (out_size == 0)
            return;

        if (value == 0)
        {
            out[0] = 0;
            return;
        }

        uint32_t out_len = 0;
        const char* cursor = value;
        while (*cursor && out_len + 1 < out_size)
        {
            char decoded = 0;
            if (!DecodeEscapedChar(&cursor, &decoded))
                break;
            out[out_len++] = decoded;
        }
        out[out_len] = 0;
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
        // RFC 6763 composes a service instance name as:
        // <Instance>.<Service>.<Domain>
        //
        // This helper currently concatenates presentation-form strings and
        // relies on WriteName() to turn them into DNS labels on the wire.
        char escaped_instance[256];
        EscapeDnsText(instance_name, escaped_instance, sizeof(escaped_instance));

        dmStrlCpy(out, escaped_instance, out_size);
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
                char escaped_instance[256];
                if (instance_len >= out_size)
                    instance_len = out_size - 1;
                memcpy(escaped_instance, full_name, instance_len);
                escaped_instance[instance_len] = 0;
                UnescapeDnsText(escaped_instance, out, out_size);
                return;
            }
        }

        UnescapeDnsText(full_name, out, out_size);
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
            const dmSocket::IfAddr& if_addr = interfaces[i];
            if (if_addr.m_Address.m_family != dmSocket::DOMAIN_IPV4)
                continue;
            if (dmSocket::Empty(if_addr.m_Address))
                continue;
            if ((if_addr.m_Flags & dmSocket::FLAGS_UP) == 0)
                continue;
            if (ContainsAddress(*addresses, if_addr.m_Address))
                continue;

            if (addresses->Full())
                addresses->OffsetCapacity(4);
            addresses->Push(if_addr.m_Address);
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
        // RFC 1035 section 4.1.4 name parser with support for compression
        // pointers. The jump limit keeps malformed packets from looping
        // forever when they contain cyclic pointers.
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

            for (uint32_t i = 0; i < len; ++i)
            {
                const char c = (char) data[pos + i];
                if (c == '.' || c == '\\')
                {
                    if (out_len + 2 >= out_size)
                        return false;
                    out[out_len++] = '\\';
                }
                else if (out_len + 1 >= out_size)
                {
                    return false;
                }
                out[out_len++] = c;
            }

            if (out_len + 1 >= out_size)
                return false;
            out[out_len++] = '.';
            pos += len;
        }
    }

    static bool WriteName(uint8_t* data, uint32_t size, uint32_t* offset, const char* name)
    {
        const char* cursor = name;
        while (*cursor)
        {
            char label[63];
            uint32_t label_len = 0;
            while (*cursor && *cursor != '.')
            {
                if (label_len >= sizeof(label))
                    return false;

                if (!DecodeEscapedChar(&cursor, &label[label_len]))
                    return false;
                ++label_len;
            }

            if (*offset + 1 + label_len > size)
                return false;

            data[*offset] = (uint8_t) label_len;
            ++(*offset);
            memcpy(data + *offset, label, label_len);
            *offset += label_len;

            if (*cursor == '.')
                ++cursor;
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

    // Writes prebuilt rdata bytes after a record header. Probe construction uses
    // this so the same canonical rdata can be shared between comparisons and I/O.
    static bool WriteRecordData(uint8_t* buffer, uint32_t buffer_size, uint32_t* offset,
                                const uint8_t* data, uint16_t data_length)
    {
        if (*offset + data_length > buffer_size)
            return false;

        memcpy(buffer + *offset, data, data_length);
        *offset += data_length;
        return true;
    }

    // Builds canonical SRV rdata once so probes, responses, and tiebreak
    // comparisons all operate on the same byte representation.
    static bool BuildSrvRData(const RegisteredService& service, uint8_t* data, uint32_t data_size, uint16_t* data_length)
    {
        uint32_t data_offset = 0;
        if (!WriteU16(data, data_size, &data_offset, 0)   // priority
            || !WriteU16(data, data_size, &data_offset, 0) // weight
            || !WriteU16(data, data_size, &data_offset, service.m_Port)
            || !WriteName(data, data_size, &data_offset, service.m_HostLocal))
        {
            return false;
        }

        *data_length = (uint16_t) data_offset;
        return true;
    }

    // Encodes the TXT payload exactly as it will appear on the wire so probe
    // authority claims and incoming tiebreak records can be compared bytewise.
    static bool BuildTxtRData(const RegisteredService& service, uint8_t* data, uint32_t data_size, uint16_t* data_length)
    {
        uint32_t data_offset = 0;

        for (uint32_t i = 0; i < service.m_TxtCount; ++i)
        {
            char kv[320];
            dmSnPrintf(kv, sizeof(kv), "%s=%s", service.m_Txt[i].m_Key, service.m_Txt[i].m_Value);
            uint32_t len = (uint32_t) strlen(kv);
            if (len > 255 || data_offset + 1 + len > data_size)
                return false;

            data[data_offset++] = (uint8_t) len;
            memcpy(data + data_offset, kv, len);
            data_offset += len;
        }

        if (data_offset == 0)
        {
            if (data_offset + 1 > data_size)
                return false;
            data[data_offset++] = 0;
        }

        *data_length = (uint16_t) data_offset;
        return true;
    }

    // Produces canonical IPv4 A-record rdata for host probing and announcements.
    static bool BuildARData(const dmSocket::Address& host_address, uint8_t* data, uint32_t data_size, uint16_t* data_length)
    {
        if (data_size < 4 || host_address.m_family != dmSocket::DOMAIN_IPV4 || dmSocket::Empty(host_address))
            return false;

        dmSocket::Address address = host_address;
        uint32_t ipv4 = *dmSocket::IPv4(&address);
        data[0] = (uint8_t) ((ipv4 >> 0) & 0xff);
        data[1] = (uint8_t) ((ipv4 >> 8) & 0xff);
        data[2] = (uint8_t) ((ipv4 >> 16) & 0xff);
        data[3] = (uint8_t) ((ipv4 >> 24) & 0xff);
        *data_length = 4;
        return true;
    }

    // Materializes one tentative unique record owned by a service. These
    // records are the RFC 6762 "claims" used in the authority section.
    static bool BuildProbeRecord(const RegisteredService& service, const char* name, uint16_t type, ProbeRecord* record)
    {
        memset(record, 0, sizeof(*record));
        record->m_Type = type;
        record->m_Class = DNS_CLASS_IN;

        if (NameEquals(name, service.m_FullServiceName))
        {
            if (type == DNS_TYPE_TXT)
                return BuildTxtRData(service, record->m_RData, sizeof(record->m_RData), &record->m_RDataLength);
            if (type == DNS_TYPE_SRV)
                return BuildSrvRData(service, record->m_RData, sizeof(record->m_RData), &record->m_RDataLength);
            return false;
        }

        if (service.m_ProbeHost && NameEquals(name, service.m_HostLocal) && type == DNS_TYPE_A)
        {
            return BuildARData(service.m_HostAddress, record->m_RData, sizeof(record->m_RData), &record->m_RDataLength);
        }

        return false;
    }

    // DNS-SD service-instance ownership is defined by the TXT and SRV records
    // at the full instance name. Build and return that sortable record set.
    static uint32_t BuildServiceProbeRecordSet(const RegisteredService& service, ProbeRecord* records, uint32_t max_records)
    {
        uint32_t count = 0;
        if (count < max_records && BuildProbeRecord(service, service.m_FullServiceName, DNS_TYPE_TXT, &records[count]))
            ++count;
        if (count < max_records && BuildProbeRecord(service, service.m_FullServiceName, DNS_TYPE_SRV, &records[count]))
            ++count;
        return count;
    }

    // Host ownership is currently represented by an IPv4 A record.
    static uint32_t BuildHostProbeRecordSet(const RegisteredService& service, ProbeRecord* records, uint32_t max_records)
    {
        uint32_t count = 0;
        if (service.m_ProbeHost && count < max_records && BuildProbeRecord(service, service.m_HostLocal, DNS_TYPE_A, &records[count]))
            ++count;
        return count;
    }

    // RFC 6762 compares tentative records lexicographically as unsigned bytes.
    static int CompareProbeData(const uint8_t* a, uint16_t a_length, const uint8_t* b, uint16_t b_length)
    {
        const uint16_t min_length = a_length < b_length ? a_length : b_length;
        for (uint16_t i = 0; i < min_length; ++i)
        {
            if (a[i] != b[i])
                return a[i] < b[i] ? -1 : 1;
        }

        if (a_length != b_length)
            return a_length < b_length ? -1 : 1;
        return 0;
    }

    // Records sort by class, then type, then raw rdata bytes, matching the
    // simultaneous-probe tiebreak order from RFC 6762 section 8.2.
    static int CompareProbeRecord(const ProbeRecord& a, const ProbeRecord& b)
    {
        const uint16_t a_class = a.m_Class & ~DNS_CLASS_FLUSH;
        const uint16_t b_class = b.m_Class & ~DNS_CLASS_FLUSH;
        if (a_class != b_class)
            return a_class < b_class ? -1 : 1;

        if (a.m_Type != b.m_Type)
            return a.m_Type < b.m_Type ? -1 : 1;

        return CompareProbeData(a.m_RData, a.m_RDataLength, b.m_RData, b.m_RDataLength);
    }

    // Record sets must be sorted before pairwise tiebreak comparison so hosts
    // reach the same outcome even when authority records arrive in any order.
    static void SortProbeRecords(ProbeRecord* records, uint32_t count)
    {
        for (uint32_t i = 1; i < count; ++i)
        {
            ProbeRecord current = records[i];
            uint32_t j = i;
            while (j > 0 && CompareProbeRecord(current, records[j - 1]) < 0)
            {
                records[j] = records[j - 1];
                --j;
            }
            records[j] = current;
        }
    }

    // Compares two sorted claim sets and returns which side is later in the
    // RFC-defined ordering. Identical sets are not treated as conflicts.
    static int CompareProbeRecordSets(const ProbeRecord* a, uint32_t a_count, const ProbeRecord* b, uint32_t b_count)
    {
        const uint32_t min_count = a_count < b_count ? a_count : b_count;
        for (uint32_t i = 0; i < min_count; ++i)
        {
            int cmp = CompareProbeRecord(a[i], b[i]);
            if (cmp != 0)
                return cmp;
        }

        if (a_count != b_count)
            return a_count < b_count ? -1 : 1;
        return 0;
    }

    // Decodes an incoming authority claim into the same canonical form used for
    // locally generated records. SRV names are uncompressed before comparison.
    static bool DecodeIncomingProbeRecord(const uint8_t* data, uint32_t size, uint16_t type, uint16_t klass,
                                          uint32_t rdata_offset, uint16_t rdlength, ProbeRecord* record)
    {
        memset(record, 0, sizeof(*record));
        record->m_Type = type;
        record->m_Class = klass;

        if (rdlength > sizeof(record->m_RData))
            return false;

        memcpy(record->m_RData, data + rdata_offset, rdlength);
        record->m_RDataLength = rdlength;

        if (type == DNS_TYPE_SRV)
        {
            uint32_t parse_offset = rdata_offset;
            uint16_t priority = 0;
            uint16_t weight = 0;
            uint16_t port = 0;
            char target[256];
            uint32_t canonical_offset = 0;
            if (!ReadU16(data, size, &parse_offset, &priority)
                || !ReadU16(data, size, &parse_offset, &weight)
                || !ReadU16(data, size, &parse_offset, &port)
                || !ReadName(data, size, &parse_offset, target, sizeof(target))
                || !WriteU16(record->m_RData, sizeof(record->m_RData), &canonical_offset, priority)
                || !WriteU16(record->m_RData, sizeof(record->m_RData), &canonical_offset, weight)
                || !WriteU16(record->m_RData, sizeof(record->m_RData), &canonical_offset, port)
                || !WriteName(record->m_RData, sizeof(record->m_RData), &canonical_offset, target))
            {
                return false;
            }
            record->m_RDataLength = (uint16_t) canonical_offset;
        }

        return true;
    }

    // Writes a canonical record header plus already-encoded rdata. Probes and
    // responses both use this so SRV/TXT bytes are built in one place.
    static bool WriteCanonicalRecord(uint8_t* buffer, uint32_t buffer_size, uint32_t* offset,
                                     const char* name, uint16_t type, uint16_t klass, uint32_t ttl,
                                     const uint8_t* rdata, uint16_t rdlength)
    {
        if (!WriteRecordHeader(buffer, buffer_size, offset, name, type, klass, ttl, rdlength))
            return false;

        return WriteRecordData(buffer, buffer_size, offset, rdata, rdlength);
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

    static bool WriteRecordA(uint8_t* buffer, uint32_t buffer_size, uint32_t* offset,
                             const RegisteredService& service, const dmSocket::Address& host_address, uint32_t ttl)
    {
        uint8_t data[4];
        uint16_t data_length = 0;
        if (!BuildARData(host_address, data, sizeof(data), &data_length))
            return false;

        return WriteCanonicalRecord(buffer, buffer_size, offset,
                                    service.m_HostLocal,
                                    DNS_TYPE_A,
                                    (uint16_t) (DNS_CLASS_IN | DNS_CLASS_FLUSH),
                                    ttl,
                                    data,
                                    data_length);
    }

    static uint32_t BuildResponseMessage(const RegisteredService& service, const dmSocket::Address* host_address, uint32_t ttl, uint8_t* buffer, uint32_t buffer_size)
    {
        // RFC 6763 service advertisement record set:
        // PTR(<service type>) -> <instance>, then SRV/TXT/A for that instance.
        uint32_t offset = 0;
        uint16_t answer_count = 0;
        ProbeRecord service_records[2];
        const uint32_t service_record_count = BuildServiceProbeRecordSet(service, service_records, DM_ARRAY_SIZE(service_records));

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

        for (uint32_t i = 0; i < service_record_count; ++i)
        {
            if (WriteCanonicalRecord(buffer, buffer_size, &offset,
                                     service.m_FullServiceName,
                                     service_records[i].m_Type,
                                     (uint16_t) (service_records[i].m_Class | DNS_CLASS_FLUSH),
                                     ttl,
                                     service_records[i].m_RData,
                                     service_records[i].m_RDataLength))
            {
                ++answer_count;
            }
        }

        const dmSocket::Address& service_address = host_address ? *host_address : service.m_HostAddress;
        if (WriteRecordA(buffer, buffer_size, &offset, service, service_address, ttl))
            ++answer_count;

        if (answer_count == 0)
            return 0;

        buffer[6] = (uint8_t) ((answer_count >> 8) & 0xff);
        buffer[7] = (uint8_t) (answer_count & 0xff);

        return offset;
    }

    static bool ShouldIncludeKnownAnswer(const BrowserService& service, uint64_t now)
    {
        if (!service.m_HasPtr || service.m_PtrExpires <= now || service.m_PtrTtl == 0)
            return false;

        const uint64_t remaining = service.m_PtrExpires - now;
        return remaining > HalfSecondsToMicroSeconds(service.m_PtrTtl);
    }

    static uint32_t BuildQueryMessage(const Browser* browser, uint64_t now, uint8_t* buffer, uint32_t buffer_size)
    {
        uint32_t offset = 0;
        uint16_t answer_count = 0;
        if (!WriteU16(buffer, buffer_size, &offset, 0)   // id
            || !WriteU16(buffer, buffer_size, &offset, 0) // query
            || !WriteU16(buffer, buffer_size, &offset, 1) // one question
            || !WriteU16(buffer, buffer_size, &offset, 0) // answers (known answers patched later)
            || !WriteU16(buffer, buffer_size, &offset, 0)
            || !WriteU16(buffer, buffer_size, &offset, 0)
            || !WriteName(buffer, buffer_size, &offset, browser->m_ServiceTypeLocal)
            || !WriteU16(buffer, buffer_size, &offset, DNS_TYPE_PTR)
            || !WriteU16(buffer, buffer_size, &offset, DNS_CLASS_IN))
        {
            return 0;
        }

        for (uint32_t i = 0; i < browser->m_Services.Size(); ++i)
        {
            const BrowserService& service = browser->m_Services[i];
            if (!ShouldIncludeKnownAnswer(service, now))
                continue;

            const uint32_t ttl = RemainingTtlSeconds(service.m_PtrExpires, now);
            if (ttl == 0)
                continue;

            if (!WriteRecordPtr(buffer, buffer_size, &offset, browser->m_ServiceTypeLocal, service.m_FullServiceName, ttl))
                return 0;
            ++answer_count;
        }

        buffer[6] = (uint8_t) ((answer_count >> 8) & 0xff);
        buffer[7] = (uint8_t) (answer_count & 0xff);

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

    static bool IsServiceActive(const RegisteredService& service)
    {
        return service.m_State == SERVICE_STATE_ACTIVE;
    }

    static uint32_t BuildProbeMessage(const RegisteredService& service, uint8_t* buffer, uint32_t buffer_size)
    {
        uint32_t offset = 0;
        ProbeRecord service_records[2];
        ProbeRecord host_records[1];
        const uint32_t service_record_count = BuildServiceProbeRecordSet(service, service_records, DM_ARRAY_SIZE(service_records));
        const uint32_t host_record_count = BuildHostProbeRecordSet(service, host_records, DM_ARRAY_SIZE(host_records));

        // RFC 6762 probing is done against the unique RRsets we intend to own.
        // For DNS-SD that means the SRV/TXT pair at the full service instance
        // name, not the shared PTR at the service type. Authority records carry
        // the tentative rdata so simultaneous probes can be tie-broken.
        const uint16_t question_count = host_record_count > 0 ? 2 : 1;
        const uint16_t authority_count = (uint16_t) (service_record_count + host_record_count);
        if (service_record_count != 2)
            return 0;

        if (!WriteU16(buffer, buffer_size, &offset, 0)
            || !WriteU16(buffer, buffer_size, &offset, 0)
            || !WriteU16(buffer, buffer_size, &offset, question_count)
            || !WriteU16(buffer, buffer_size, &offset, 0)
            || !WriteU16(buffer, buffer_size, &offset, authority_count)
            || !WriteU16(buffer, buffer_size, &offset, 0)
            || !WriteName(buffer, buffer_size, &offset, service.m_FullServiceName)
            || !WriteU16(buffer, buffer_size, &offset, DNS_TYPE_ANY)
            || !WriteU16(buffer, buffer_size, &offset, DNS_CLASS_IN))
        {
            return 0;
        }

        if (host_record_count > 0
            && (!WriteName(buffer, buffer_size, &offset, service.m_HostLocal)
                || !WriteU16(buffer, buffer_size, &offset, DNS_TYPE_ANY)
                || !WriteU16(buffer, buffer_size, &offset, DNS_CLASS_IN)))
        {
            return 0;
        }

        for (uint32_t i = 0; i < service_record_count; ++i)
        {
            if (!WriteCanonicalRecord(buffer, buffer_size, &offset,
                                      service.m_FullServiceName,
                                      service_records[i].m_Type,
                                      service_records[i].m_Class,
                                      service.m_Ttl,
                                      service_records[i].m_RData,
                                      service_records[i].m_RDataLength))
                return 0;
        }

        for (uint32_t i = 0; i < host_record_count; ++i)
        {
            if (!WriteCanonicalRecord(buffer, buffer_size, &offset,
                                      service.m_HostLocal,
                                      host_records[i].m_Type,
                                      host_records[i].m_Class,
                                      service.m_Ttl,
                                      host_records[i].m_RData,
                                      host_records[i].m_RDataLength))
                return 0;
        }

        return offset;
    }

    // Centralizes probe-state transitions so conflict and tiebreak paths reset
    // counters consistently before either retrying or stopping.
    static void ResetProbeState(RegisteredService* service, uint64_t next_probe, uint8_t state)
    {
        service->m_State = state;
        service->m_ProbeCount = 0;
        service->m_StartupAnnounceCount = 0;
        service->m_NextProbe = next_probe;
        service->m_NextAnnounce = 0;
    }

    static void DeferProbeAfterTiebreak(RegisteredService* service)
    {
        // When we lose a simultaneous probe tiebreak, wait one second before
        // probing again. This avoids treating stale looped-back probe packets
        // as permanent conflicts while still allowing the real winner to defend.
        ResetProbeState(service, GetNow() + SecondsToMicroSeconds(MDNS_PROBE_RESTART_DELAY), SERVICE_STATE_PROBING);
    }

    static void MarkServiceConflict(RegisteredService* service)
    {
        if (service->m_State != SERVICE_STATE_CONFLICT)
        {
            dmLogWarning("mDNS name conflict detected for service '%s' (%s)", service->m_Id, service->m_FullServiceName);
        }
        ResetProbeState(service, 0, SERVICE_STATE_CONFLICT);
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

    static void SendProbe(MDNS* mdns, const RegisteredService& service)
    {
        const uint32_t size = BuildProbeMessage(service, mdns->m_Buffer, sizeof(mdns->m_Buffer));
        if (size > 0)
        {
            SendPacketOnInterfaces(mdns->m_Socket, mdns->m_Buffer, size, mdns->m_InterfaceAddresses);
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
        if (!IsServiceActive(service))
            return false;

        switch (qtype)
        {
        case DNS_TYPE_PTR:
            return NameEquals(qname, service.m_ServiceTypeLocal);
        case DNS_TYPE_SRV:
        case DNS_TYPE_TXT:
            return NameEquals(qname, service.m_FullServiceName);
        case DNS_TYPE_A:
            return NameEquals(qname, service.m_HostLocal);
        case DNS_TYPE_ANY:
            return NameEquals(qname, service.m_ServiceTypeLocal)
                   || NameEquals(qname, service.m_FullServiceName)
                   || NameEquals(qname, service.m_HostLocal);
        default:
            return false;
        }
    }

    static bool IsValidTxtPayload(const RegisteredService& service)
    {
        uint32_t encoded_size = 0;
        for (uint32_t i = 0; i < service.m_TxtCount; ++i)
        {
            uint32_t key_len = (uint32_t) strlen(service.m_Txt[i].m_Key);
            uint32_t value_len = (uint32_t) strlen(service.m_Txt[i].m_Value);
            uint32_t entry_len = key_len + 1 + value_len;
            if (entry_len > 255)
                return false;
            if (encoded_size + 1 + entry_len > 1024)
                return false;
            encoded_size += 1 + entry_len;
        }

        return true;
    }

    // Extracts and sorts only the authority records that answer one probe
    // question. This gives the exact claim set needed for pairwise tiebreaking.
    static bool CollectProbeClaims(const uint8_t* data, uint32_t size, uint32_t section_offset, uint16_t record_count,
                                   const char* target_name, ProbeRecord* records, uint32_t max_records, uint32_t* out_count)
    {
        *out_count = 0;

        uint32_t offset = section_offset;
        for (uint16_t i = 0; i < record_count && offset < size; ++i)
        {
            char name[256];
            uint16_t type = 0;
            uint16_t klass = 0;
            uint32_t ttl = 0;
            uint16_t rdlength = 0;
            if (!ReadName(data, size, &offset, name, sizeof(name))
                || !ReadU16(data, size, &offset, &type)
                || !ReadU16(data, size, &offset, &klass)
                || !ReadU32(data, size, &offset, &ttl)
                || !ReadU16(data, size, &offset, &rdlength))
            {
                return false;
            }

            (void) ttl;

            if (offset + rdlength > size)
                return false;

            const uint32_t rdata_offset = offset;
            offset += rdlength;

            if (!NameEquals(name, target_name) || (klass & ~DNS_CLASS_FLUSH) != DNS_CLASS_IN)
                continue;

            if (*out_count >= max_records || !DecodeIncomingProbeRecord(data, size, type, klass, rdata_offset, rdlength, &records[*out_count]))
                return false;

            ++(*out_count);
        }

        SortProbeRecords(records, *out_count);
        return true;
    }

    // Returns true when the remote authority claims outrank our tentative data,
    // meaning we lost the simultaneous-probe tiebreak for this unique name.
    static bool HandleSimultaneousProbeClaim(const uint8_t* data, uint32_t size, uint32_t section_offset, uint16_t record_count,
                                             const char* target_name, const ProbeRecord* local_records, uint32_t local_count)
    {
        ProbeRecord incoming[MDNS_MAX_PROBE_RECORDS];
        uint32_t incoming_count = 0;
        if (!CollectProbeClaims(data, size, section_offset, record_count, target_name, incoming, MDNS_MAX_PROBE_RECORDS, &incoming_count))
            return false;

        if (incoming_count == 0)
            return false;

        return CompareProbeRecordSets(local_records, local_count, incoming, incoming_count) < 0;
    }

    static void HandleIncomingResponseRecords(MDNS* mdns, const uint8_t* data, uint32_t size, uint32_t* offset, uint32_t record_count)
    {
        for (uint32_t i = 0; i < record_count && *offset < size; ++i)
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

            if (*offset + rdlength > size)
            {
                *offset = size;
                return;
            }

            const uint32_t rdata_offset = *offset;
            *offset += rdlength;

            if (ttl == 0 || (klass & ~DNS_CLASS_FLUSH) != DNS_CLASS_IN)
                continue;

            for (uint32_t service_index = 0; service_index < mdns->m_Services.Size(); ++service_index)
            {
                RegisteredService& service = mdns->m_Services[service_index];
                if (service.m_State != SERVICE_STATE_PROBING)
                    continue;

                if (NameEquals(name, service.m_FullServiceName)
                    || (service.m_ProbeHost && NameEquals(name, service.m_HostLocal)))
                {
                    ProbeRecord expected;
                    ProbeRecord actual;
                    if (!BuildProbeRecord(service, name, type, &expected))
                    {
                        MarkServiceConflict(&service);
                        continue;
                    }

                    if (!DecodeIncomingProbeRecord(data, size, type, klass, rdata_offset, rdlength, &actual))
                        continue;

                    if (CompareProbeRecord(actual, expected) != 0)
                    {
                        MarkServiceConflict(&service);
                    }
                }
            }
        }
    }

    static void SuppressKnownAnswers(MDNS* mdns, const uint8_t* data, uint32_t size, uint32_t* offset, uint16_t answer_count, dmArray<uint8_t>* should_announce)
    {
        // Implement known-answer suppression for PTR records only. Browsers use
        // the advertised PTR as the cache key for service presence, so skipping
        // duplicate PTRs avoids the noisiest redundant replies while keeping
        // the rest of the record set simple.
        for (uint16_t i = 0; i < answer_count && *offset < size; ++i)
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

            if (*offset + rdlength > size)
            {
                *offset = size;
                return;
            }

            const uint32_t rdata_offset = *offset;
            *offset += rdlength;

            if (type != DNS_TYPE_PTR || ttl == 0 || (klass & ~DNS_CLASS_FLUSH) != DNS_CLASS_IN)
                continue;

            uint32_t ptr_offset = rdata_offset;
            char full_service_name[256];
            if (!ReadName(data, size, &ptr_offset, full_service_name, sizeof(full_service_name)))
                continue;

            for (uint32_t service_index = 0; service_index < mdns->m_Services.Size(); ++service_index)
            {
                if (!(*should_announce)[service_index])
                    continue;

                const RegisteredService& service = mdns->m_Services[service_index];
                if (!IsServiceActive(service))
                    continue;

                if (NameEquals(name, service.m_ServiceTypeLocal)
                    && NameEquals(full_service_name, service.m_FullServiceName)
                    && ttl * 2 >= service.m_Ttl)
                {
                    // RFC 6762 allows suppressing an answer when the querier's
                    // cached TTL is at least half of what we would advertise.
                    (*should_announce)[service_index] = 0;
                }
            }
        }
    }

    static void HandleIncomingQueries(MDNS* mdns)
    {
        // Minimal RFC 6762 responder loop:
        // parse multicast questions, match them against locally registered
        // authoritative records, and emit the corresponding record set.
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
            if ((flags & 0x8000) != 0)
            {
                const uint32_t records = (uint32_t) ancount + (uint32_t) nscount + (uint32_t) arcount;
                HandleIncomingResponseRecords(mdns, mdns->m_Buffer, size, &offset, records);
                continue;
            }

            dmArray<uint8_t> should_announce;
            dmArray<uint8_t> probe_service_questions;
            dmArray<uint8_t> probe_host_questions;
            bool should_respond = false;
            should_announce.SetCapacity(mdns->m_Services.Size());
            should_announce.SetSize(mdns->m_Services.Size());
            probe_service_questions.SetCapacity(mdns->m_Services.Size());
            probe_service_questions.SetSize(mdns->m_Services.Size());
            probe_host_questions.SetCapacity(mdns->m_Services.Size());
            probe_host_questions.SetSize(mdns->m_Services.Size());
            for (uint32_t s = 0; s < should_announce.Size(); ++s)
            {
                should_announce[s] = 0;
                probe_service_questions[s] = 0;
                probe_host_questions[s] = 0;
            }

            for (uint16_t i = 0; i < qdcount; ++i)
            {
                char qname[256];
                uint16_t qtype = 0;
                uint16_t qclass = 0;

                if (!ReadName(mdns->m_Buffer, size, &offset, qname, sizeof(qname))
                    || !ReadU16(mdns->m_Buffer, size, &offset, &qtype)
                    || !ReadU16(mdns->m_Buffer, size, &offset, &qclass))
                {
                    should_announce.SetSize(0);
                    probe_service_questions.SetSize(0);
                    probe_host_questions.SetSize(0);
                    should_respond = false;
                    break;
                }

                (void) qclass;

                for (uint32_t s = 0; s < mdns->m_Services.Size(); ++s)
                {
                    if (QueryMatchesService(mdns->m_Services[s], qname, qtype))
                    {
                        should_announce[s] = 1;
                        should_respond = true;
                    }

                    RegisteredService& service = mdns->m_Services[s];
                    if (service.m_State != SERVICE_STATE_PROBING)
                        continue;

                    if (qtype == DNS_TYPE_ANY && NameEquals(qname, service.m_FullServiceName))
                        probe_service_questions[s] = 1;
                    if (service.m_ProbeHost && qtype == DNS_TYPE_ANY && NameEquals(qname, service.m_HostLocal))
                        probe_host_questions[s] = 1;
                }
            }

            SuppressKnownAnswers(mdns, mdns->m_Buffer, size, &offset, ancount, &should_announce);
            if (nscount > 0)
            {
                // Simultaneous probe tiebreaking is driven by the tentative
                // records in the query authority section. Compare those claims
                // against our own proposed rdata before deciding whether to probe.
                const uint32_t authority_offset = offset;
                for (uint32_t s = 0; s < mdns->m_Services.Size(); ++s)
                {
                    RegisteredService& service = mdns->m_Services[s];
                    if (service.m_State != SERVICE_STATE_PROBING)
                        continue;

                    bool lost_tiebreak = false;
                    if (probe_service_questions[s])
                    {
                        ProbeRecord local_records[2];
                        uint32_t local_count = BuildServiceProbeRecordSet(service, local_records, 2);
                        lost_tiebreak = local_count > 0
                                        && HandleSimultaneousProbeClaim(mdns->m_Buffer, size, authority_offset, nscount,
                                                                        service.m_FullServiceName, local_records, local_count);
                    }

                    if (!lost_tiebreak && probe_host_questions[s])
                    {
                        ProbeRecord local_records[1];
                        uint32_t local_count = BuildHostProbeRecordSet(service, local_records, 1);
                        lost_tiebreak = local_count > 0
                                        && HandleSimultaneousProbeClaim(mdns->m_Buffer, size, authority_offset, nscount,
                                                                        service.m_HostLocal, local_records, local_count);
                    }

                    if (lost_tiebreak)
                        DeferProbeAfterTiebreak(&service);
                }
            }
            for (uint32_t s = 0; s < should_announce.Size(); ++s)
            {
                if (should_announce[s])
                {
                    should_respond = true;
                    break;
                }
            }

            if (should_respond)
            {
                const dmSocket::Address* response_interface = 0;
                if (mdns->m_InterfaceAddresses.Size() > 0)
                {
                    response_interface = &mdns->m_InterfaceAddresses[0];

                    if (from_address.m_family == dmSocket::DOMAIN_IPV4)
                    {
                        // Prefer replying on the interface whose address is
                        // closest to the sender so multihomed hosts answer from
                        // the network that best matches the incoming query.
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
                    if (!should_announce[s])
                        continue;

                    // Always multicast responses for compatibility with mDNS browsers
                    // that ignore unicast replies to multicast queries.
                    // The unqualified send keeps the existing default-routing
                    // behavior, while the interface-specific send nudges the
                    // packet out on the subnet that best matches the requester.
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

    static void EmitBrowserRemoved(Browser* browser, BrowserService* service)
    {
        if (service->m_HasPtr || service->m_Resolved)
        {
            EmitBrowserEvent(browser, *service, EVENT_REMOVED);
        }
        service->m_Resolved = 0;
    }

    static uint64_t GetBrowserRefreshDeadline(const Browser* browser)
    {
        uint64_t deadline = 0;

        for (uint32_t i = 0; i < browser->m_Hosts.Size(); ++i)
        {
            const BrowserHost& host = browser->m_Hosts[i];
            if (host.m_Refresh != 0 && (deadline == 0 || host.m_Refresh < deadline))
                deadline = host.m_Refresh;
        }

        for (uint32_t i = 0; i < browser->m_Services.Size(); ++i)
        {
            const BrowserService& service = browser->m_Services[i];
            if (service.m_PtrRefresh != 0 && (deadline == 0 || service.m_PtrRefresh < deadline))
                deadline = service.m_PtrRefresh;
            if (service.m_SrvRefresh != 0 && (deadline == 0 || service.m_SrvRefresh < deadline))
                deadline = service.m_SrvRefresh;
            if (service.m_TxtRefresh != 0 && (deadline == 0 || service.m_TxtRefresh < deadline))
                deadline = service.m_TxtRefresh;
        }

        return deadline;
    }

    static void RefreshServiceAddress(Browser* browser, BrowserService* service)
    {
        service->m_HasAddress = 0;
        service->m_HostAddress[0] = 0;

        int32_t host_index = FindBrowserHost(browser->m_Hosts, service->m_Host);
        if (host_index >= 0)
        {
            dmStrlCpy(service->m_HostAddress, browser->m_Hosts[(uint32_t) host_index].m_Address, sizeof(service->m_HostAddress));
            service->m_HasAddress = 1;
        }
    }

    static void TryResolveService(Browser* browser, BrowserService* service)
    {
        // RFC 6763 service resolution completes when browsing has yielded:
        // PTR  -> the service instance exists
        // SRV  -> host name + port
        // TXT  -> metadata for the instance
        // A    -> an IPv4 address for the SRV target host
        RefreshServiceAddress(browser, service);

        const bool complete = service->m_HasPtr && service->m_HasSrv && service->m_HasTxt && service->m_HasAddress;
        if (complete)
        {
            if (!service->m_Resolved)
            {
                service->m_Resolved = 1;
                EmitBrowserEvent(browser, *service, EVENT_RESOLVED);
            }
        }
        else if (service->m_Resolved)
        {
            EmitBrowserEvent(browser, *service, EVENT_REMOVED);
            service->m_Resolved = 0;
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
                EmitBrowserRemoved(browser, &service);
                browser->m_Services.EraseSwap((uint32_t) service_index);
                return;
            }

            service.m_HasPtr = 1;
            service.m_PtrExpires = GetNow() + SecondsToMicroSeconds(ttl);
            service.m_PtrRefresh = GetNow() + HalfSecondsToMicroSeconds(ttl);
            service.m_PtrTtl = ttl;
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

            if (ttl == 0)
            {
                service.m_HasSrv = 0;
                service.m_SrvExpires = 0;
                service.m_SrvRefresh = 0;
                service.m_SrvTtl = 0;
                service.m_Host[0] = 0;
                service.m_Port = 0;
                TryResolveService(browser, &service);
                return;
            }

            service.m_Port = port;
            service.m_HasSrv = 1;
            service.m_SrvExpires = GetNow() + SecondsToMicroSeconds(ttl);
            service.m_SrvRefresh = GetNow() + HalfSecondsToMicroSeconds(ttl);
            service.m_SrvTtl = ttl;
            TryResolveService(browser, &service);
            return;
        }

        if (type == DNS_TYPE_TXT)
        {
            int32_t service_index = FindBrowserService(browser->m_Services, name);
            if (service_index < 0)
                return;

            BrowserService& service = browser->m_Services[(uint32_t) service_index];
            if (ttl == 0)
            {
                service.m_HasTxt = 0;
                service.m_TxtExpires = 0;
                service.m_TxtRefresh = 0;
                service.m_TxtTtl = 0;
                service.m_TxtCount = 0;
                service.m_Id[0] = 0;
                TryResolveService(browser, &service);
                return;
            }

            uint32_t txt_count = 0;
            if (!ParseTxtRData(data + rdata_offset, rdlength, service.m_Txt, MDNS_MAX_TXT_ENTRIES, &txt_count))
                return;

            service.m_TxtCount = txt_count;
            service.m_HasTxt = 1;
            service.m_TxtExpires = GetNow() + SecondsToMicroSeconds(ttl);
            service.m_TxtRefresh = GetNow() + HalfSecondsToMicroSeconds(ttl);
            service.m_TxtTtl = ttl;

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
                for (uint32_t i = 0; i < browser->m_Services.Size(); ++i)
                {
                    TryResolveService(browser, &browser->m_Services[i]);
                }
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
            host.m_Refresh = GetNow() + HalfSecondsToMicroSeconds(ttl);
            host.m_Ttl = ttl;

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

            if ((flags & 0x8000) == 0)
                continue;

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
        bool hosts_changed = false;

        for (uint32_t i = 0; i < browser->m_Hosts.Size();)
        {
            if (now >= browser->m_Hosts[i].m_Expires)
            {
                browser->m_Hosts.EraseSwap(i);
                hosts_changed = true;
            }
            else
            {
                ++i;
            }
        }

        for (uint32_t i = 0; i < browser->m_Services.Size();)
        {
            BrowserService& service = browser->m_Services[i];
            if (service.m_HasPtr && now >= service.m_PtrExpires)
            {
                EmitBrowserRemoved(browser, &service);
                browser->m_Services.EraseSwap(i);
            }
            else
            {
                if (service.m_HasSrv && now >= service.m_SrvExpires)
                {
                    service.m_HasSrv = 0;
                    service.m_SrvExpires = 0;
                    service.m_SrvRefresh = 0;
                    service.m_SrvTtl = 0;
                    service.m_Host[0] = 0;
                    service.m_Port = 0;
                }

                if (service.m_HasTxt && now >= service.m_TxtExpires)
                {
                    service.m_HasTxt = 0;
                    service.m_TxtExpires = 0;
                    service.m_TxtRefresh = 0;
                    service.m_TxtTtl = 0;
                    service.m_TxtCount = 0;
                    service.m_Id[0] = 0;
                }

                if (hosts_changed || (service.m_Resolved && (!service.m_HasSrv || !service.m_HasTxt)))
                {
                    TryResolveService(browser, &service);
                }
                ++i;
            }
        }
    }

    static bool ResolveCurrentHostAddress(const MDNS* mdns, dmSocket::Address* out)
    {
        if (mdns->m_InterfaceAddresses.Size() > 0)
        {
            *out = mdns->m_InterfaceAddresses[0];
            return true;
        }

        if (!dmSocket::Empty(mdns->m_DefaultAddress) && mdns->m_DefaultAddress.m_family == dmSocket::DOMAIN_IPV4)
        {
            *out = mdns->m_DefaultAddress;
            return true;
        }

        return false;
    }

    static void RefreshRegisteredServiceAddresses(MDNS* mdns)
    {
        dmSocket::Address address;
        if (!ResolveCurrentHostAddress(mdns, &address))
            return;

        for (uint32_t i = 0; i < mdns->m_Services.Size(); ++i)
        {
            RegisteredService& service = mdns->m_Services[i];
            if (service.m_HostAddress != address)
            {
                service.m_HostAddress = address;
                if (service.m_State == SERVICE_STATE_ACTIVE)
                {
                    service.m_StartupAnnounceCount = MDNS_STARTUP_ANNOUNCEMENTS;
                    service.m_NextAnnounce = GetNow();
                }
            }
        }
    }

    static void AdvanceServiceLifecycle(MDNS* mdns, uint64_t now)
    {
        for (uint32_t i = 0; i < mdns->m_Services.Size(); ++i)
        {
            RegisteredService& service = mdns->m_Services[i];
            if (service.m_State == SERVICE_STATE_PROBING && now >= service.m_NextProbe)
            {
                if (service.m_ProbeCount >= MDNS_PROBE_COUNT)
                {
                    service.m_State = SERVICE_STATE_ACTIVE;
                    service.m_StartupAnnounceCount = MDNS_STARTUP_ANNOUNCEMENTS;
                    service.m_NextAnnounce = now;
                }
                else
                {
                    SendProbe(mdns, service);
                    ++service.m_ProbeCount;
                    service.m_NextProbe = now + SecondsToMicroSeconds(MDNS_PROBE_INTERVAL_MS) / 1000ULL;
                }
            }

            if (service.m_State == SERVICE_STATE_ACTIVE
                && service.m_StartupAnnounceCount > 0
                && now >= service.m_NextAnnounce)
            {
                AnnounceService(mdns, service, service.m_Ttl);
                --service.m_StartupAnnounceCount;
                service.m_NextAnnounce = now + SecondsToMicroSeconds(MDNS_STARTUP_ANNOUNCE_INTERVAL);
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
            service.m_ProbeHost = 1;
        }
        else
        {
            dmStrlCpy(service.m_Host, mdns->m_DefaultHost, sizeof(service.m_Host));
            dmStrlCpy(service.m_HostLocal, mdns->m_DefaultHostLocal, sizeof(service.m_HostLocal));
            service.m_ProbeHost = 0;
        }

        if (mdns->m_InterfaceAddresses.Size() > 0)
            service.m_HostAddress = mdns->m_InterfaceAddresses[0];
        else
            service.m_HostAddress = mdns->m_DefaultAddress;
        service.m_Port = desc->m_Port;
        service.m_Ttl = desc->m_Ttl ? desc->m_Ttl : mdns->m_Params.m_Ttl;
        service.m_State = SERVICE_STATE_PROBING;
        service.m_ProbeCount = 0;
        service.m_NextProbe = GetNow();
        service.m_StartupAnnounceCount = 0;
        service.m_NextAnnounce = 0;

        if (!IsEncodableDnsName(service.m_ServiceTypeLocal)
            || !IsEncodableDnsName(service.m_FullServiceName)
            || !IsEncodableDnsName(service.m_HostLocal))
        {
            return RESULT_INVALID_ARGS;
        }

        if (desc->m_TxtCount > MDNS_MAX_TXT_ENTRIES)
            return RESULT_INVALID_ARGS;

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

        if (!IsValidTxtPayload(service))
            return RESULT_INVALID_ARGS;

        mdns->m_Services.Push(service);
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
            RefreshRegisteredServiceAddresses(mdns);
            mdns->m_NextInterfaceRefresh = now + SecondsToMicroSeconds(MDNS_INTERFACE_REFRESH_INTERVAL);
        }

        HandleIncomingQueries(mdns);
        AdvanceServiceLifecycle(mdns, now);
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
        instance->m_QueryInterval = MDNS_BROWSER_INITIAL_QUERY_INTERVAL;
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
            browser->m_NextQuery = 0;
            browser->m_QueryInterval = MDNS_BROWSER_INITIAL_QUERY_INTERVAL;
            browser->m_NextInterfaceRefresh = now + SecondsToMicroSeconds(MDNS_INTERFACE_REFRESH_INTERVAL);
        }

        uint64_t next_query_due = browser->m_NextQuery;
        const uint64_t refresh_deadline = GetBrowserRefreshDeadline(browser);
        if (next_query_due == 0 || (refresh_deadline != 0 && refresh_deadline < next_query_due))
            next_query_due = refresh_deadline;

        if (next_query_due == 0 || now >= next_query_due)
        {
            uint32_t query_size = BuildQueryMessage(browser, now, browser->m_Buffer, sizeof(browser->m_Buffer));
            if (query_size > 0)
            {
                SendPacketOnInterfaces(browser->m_Socket, browser->m_Buffer, query_size, browser->m_InterfaceAddresses);
            }
            browser->m_NextQuery = now + browser->m_QueryInterval;
            if (browser->m_QueryInterval < MDNS_BROWSER_MAX_QUERY_INTERVAL)
            {
                browser->m_QueryInterval *= 3;
                if (browser->m_QueryInterval > MDNS_BROWSER_MAX_QUERY_INTERVAL)
                    browser->m_QueryInterval = MDNS_BROWSER_MAX_QUERY_INTERVAL;
            }
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
