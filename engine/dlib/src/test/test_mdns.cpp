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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

#include <dlib/mdns.h>
#include <dlib/atomic.h>
#include <dlib/dstrings.h>
#include <dlib/network_constants.h>
#include <dlib/socket.h>
#include <dlib/thread.h>
#include <dlib/time.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#if defined(GITHUB_CI) && defined(__MACH__)
#define DM_SKIP_MDNS_DISCOVERY_TESTS
#endif

namespace
{
    static const char* SERVICE_TYPE = "_defoldtest._tcp";
    static const char* MDNS_MULTICAST_IPV4 = "224.0.0.251";
    static const uint16_t MDNS_PORT = 5353;

    static const uint16_t DNS_TYPE_A = 1;
    static const uint16_t DNS_TYPE_PTR = 12;
    static const uint16_t DNS_TYPE_TXT = 16;
    static const uint16_t DNS_TYPE_SRV = 33;
    static const uint16_t DNS_TYPE_ANY = 255;
    static const uint16_t DNS_CLASS_IN = 1;
    static const uint16_t DNS_FLAG_RESPONSE = 0x8400;

    struct EventSnapshot
    {
        dmMDNS::EventType             m_Type;
        std::string                   m_Id;
        std::string                   m_InstanceName;
        std::string                   m_Host;
        uint16_t                      m_Port;
        std::map<std::string, std::string> m_Txt;
    };

    struct EventLog
    {
        std::vector<EventSnapshot> m_Events;

        static void Callback(void* context, const dmMDNS::ServiceEvent* event)
        {
            EventLog* self = (EventLog*) context;
            EventSnapshot snapshot;
            snapshot.m_Type = event->m_Type;
            snapshot.m_Id = event->m_Id ? event->m_Id : "";
            snapshot.m_InstanceName = event->m_InstanceName ? event->m_InstanceName : "";
            snapshot.m_Host = event->m_Host ? event->m_Host : "";
            snapshot.m_Port = event->m_Port;
            for (uint32_t i = 0; i < event->m_TxtCount; ++i)
            {
                const char* key = event->m_Txt[i].m_Key ? event->m_Txt[i].m_Key : "";
                const char* value = event->m_Txt[i].m_Value ? event->m_Txt[i].m_Value : "";
                snapshot.m_Txt[key] = value;
            }
            self->m_Events.push_back(snapshot);
        }

        bool Has(dmMDNS::EventType type, const char* id) const
        {
            for (uint32_t i = 0; i < m_Events.size(); ++i)
            {
                const EventSnapshot& e = m_Events[i];
                if (e.m_Type == type && e.m_Id == id)
                    return true;
            }
            return false;
        }

        uint32_t Count(dmMDNS::EventType type, const char* id) const
        {
            uint32_t count = 0;
            for (uint32_t i = 0; i < m_Events.size(); ++i)
            {
                const EventSnapshot& e = m_Events[i];
                if (e.m_Type == type && e.m_Id == id)
                    ++count;
            }
            return count;
        }

        const EventSnapshot* Find(dmMDNS::EventType type, const char* id) const
        {
            for (uint32_t i = 0; i < m_Events.size(); ++i)
            {
                const EventSnapshot& e = m_Events[i];
                if (e.m_Type == type && e.m_Id == id)
                    return &e;
            }
            return 0;
        }

        bool HasInstance(dmMDNS::EventType type, const char* instance_name) const
        {
            for (uint32_t i = 0; i < m_Events.size(); ++i)
            {
                const EventSnapshot& e = m_Events[i];
                if (e.m_Type == type && e.m_InstanceName == instance_name)
                    return true;
            }
            return false;
        }

        uint32_t CountInstance(dmMDNS::EventType type, const char* instance_name) const
        {
            uint32_t count = 0;
            for (uint32_t i = 0; i < m_Events.size(); ++i)
            {
                const EventSnapshot& e = m_Events[i];
                if (e.m_Type == type && e.m_InstanceName == instance_name)
                    ++count;
            }
            return count;
        }

        const EventSnapshot* FindInstance(dmMDNS::EventType type, const char* instance_name) const
        {
            for (uint32_t i = 0; i < m_Events.size(); ++i)
            {
                const EventSnapshot& e = m_Events[i];
                if (e.m_Type == type && e.m_InstanceName == instance_name)
                    return &e;
            }
            return 0;
        }

        const EventSnapshot* FindLastInstance(dmMDNS::EventType type, const char* instance_name) const
        {
            for (int32_t i = (int32_t) m_Events.size() - 1; i >= 0; --i)
            {
                const EventSnapshot& e = m_Events[(uint32_t) i];
                if (e.m_Type == type && e.m_InstanceName == instance_name)
                    return &e;
            }
            return 0;
        }
    };

    struct RawDnsRecord
    {
        RawDnsRecord()
        : m_Type(0)
        , m_Class(0)
        , m_Ttl(0)
        , m_Rdlength(0)
        , m_Port(0)
        , m_HasAddress(false)
        {
            memset(m_Address, 0, sizeof(m_Address));
        }

        std::string m_Name;
        uint16_t    m_Type;
        uint16_t    m_Class;
        uint32_t    m_Ttl;
        uint16_t    m_Rdlength;
        std::string m_PtrName;
        uint16_t    m_Port;
        std::string m_Target;
        std::map<std::string, std::string> m_Txt;
        bool        m_HasAddress;
        uint8_t     m_Address[4];
    };

    struct RawDnsQuestion
    {
        std::string m_Name;
        uint16_t    m_Type;
        uint16_t    m_Class;
    };

    struct RawDnsPacket
    {
        bool                      m_IsResponse;
        std::vector<RawDnsQuestion> m_Questions;
        std::vector<RawDnsRecord> m_Records;
    };

    struct MulticastCapture
    {
        MulticastCapture()
        : m_Socket(dmSocket::INVALID_SOCKET_HANDLE)
        {
        }

        ~MulticastCapture()
        {
            if (m_Socket != dmSocket::INVALID_SOCKET_HANDLE)
                dmSocket::Delete(m_Socket);
        }

        dmSocket::Socket m_Socket;
    };

    struct ScopedSocketHandle
    {
        ScopedSocketHandle()
        : m_Socket(dmSocket::INVALID_SOCKET_HANDLE)
        {
        }

        ~ScopedSocketHandle()
        {
            if (m_Socket != dmSocket::INVALID_SOCKET_HANDLE)
                dmSocket::Delete(m_Socket);
        }

        dmSocket::Socket m_Socket;
    };

    static void Pump(dmMDNS::HMDNS mdns, dmMDNS::HBrowser browser, uint32_t iterations, uint32_t sleep_us)
    {
        for (uint32_t i = 0; i < iterations; ++i)
        {
            if (mdns)
                dmMDNS::Update(mdns);
            if (browser)
                dmMDNS::UpdateBrowser(browser);
            dmTime::Sleep(sleep_us);
        }
    }

    static bool WaitForEvent(EventLog& event_log, dmMDNS::EventType event_type, const char* instance_name, dmMDNS::HMDNS mdns, dmMDNS::HBrowser browser, uint32_t timeout_ms)
    {
        const uint64_t deadline = dmTime::GetMonotonicTime() + (uint64_t) timeout_ms * 1000ULL;
        while (dmTime::GetMonotonicTime() < deadline)
        {
            if (event_log.HasInstance(event_type, instance_name))
            {
                return true;
            }
            Pump(mdns, browser, 1, 5 * 1000);
        }
        return event_log.HasInstance(event_type, instance_name);
    }

    static bool WaitForEventCount(EventLog& event_log, dmMDNS::EventType event_type, const char* instance_name, uint32_t count, dmMDNS::HMDNS mdns, dmMDNS::HBrowser browser, uint32_t timeout_ms)
    {
        const uint64_t deadline = dmTime::GetMonotonicTime() + (uint64_t) timeout_ms * 1000ULL;
        while (dmTime::GetMonotonicTime() < deadline)
        {
            if (event_log.CountInstance(event_type, instance_name) >= count)
            {
                return true;
            }
            Pump(mdns, browser, 1, 5 * 1000);
        }
        return event_log.CountInstance(event_type, instance_name) >= count;
    }

    static bool WaitForFlag(int32_atomic_t* flag, uint32_t iterations, uint32_t sleep_us)
    {
        for (uint32_t i = 0; i < iterations; ++i)
        {
            if (dmAtomicGet32(flag))
                return true;
            dmTime::Sleep(sleep_us);
        }
        return false;
    }

    static void MakeUniqueName(char* buffer, size_t buffer_size, const char* prefix, uint64_t nonce)
    {
        snprintf(buffer, buffer_size, "%s-%llu", prefix, (unsigned long long) nonce);
    }

    static void MakeUniqueServiceType(char* buffer, size_t buffer_size, uint64_t nonce)
    {
        snprintf(buffer, buffer_size, "_mdnstest%llu._tcp", (unsigned long long) (nonce % 100000000ULL));
    }

    static void BuildLocalName(const char* value, char* out, uint32_t out_size)
    {
        dmStrlCpy(out, value, out_size);
        const uint32_t len = (uint32_t) strlen(out);
        if (len < 6 || dmStrCaseCmp(out + len - 6, ".local") != 0)
            dmStrlCat(out, ".local", out_size);
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

    static void BuildFullServiceName(const char* instance_name, const char* service_type_local, char* out, uint32_t out_size)
    {
        char escaped_instance[256];
        EscapeDnsText(instance_name, escaped_instance, sizeof(escaped_instance));
        dmStrlCpy(out, escaped_instance, out_size);
        if (out[0] && out[strlen(out) - 1] != '.')
            dmStrlCat(out, ".", out_size);
        dmStrlCat(out, service_type_local, out_size);
    }

    static uint16_t NameWireLength(const char* name)
    {
        uint16_t size = 1;
        const char* cursor = name;
        while (*cursor)
        {
            uint16_t label_len = 0;
            while (*cursor && *cursor != '.')
            {
                char decoded = 0;
                if (!DecodeEscapedChar(&cursor, &decoded))
                    return 0;
                ++label_len;
            }
            size += 1 + label_len;
            if (*cursor == '.')
                ++cursor;
        }
        return size;
    }

    static bool GetExpectedAdvertisedAddress(dmSocket::Address* out)
    {
        dmSocket::IfAddr interfaces[32];
        uint32_t interface_count = 0;
        dmSocket::GetIfAddresses(interfaces, DM_ARRAY_SIZE(interfaces), &interface_count);
        for (uint32_t i = 0; i < interface_count; ++i)
        {
            if (interfaces[i].m_Address.m_family != dmSocket::DOMAIN_IPV4)
                continue;
            if (dmSocket::Empty(interfaces[i].m_Address))
                continue;
            if ((interfaces[i].m_Flags & dmSocket::FLAGS_UP) == 0)
                continue;

            *out = interfaces[i].m_Address;
            return true;
        }

        return dmSocket::GetLocalAddress(out) == dmSocket::RESULT_OK && out->m_family == dmSocket::DOMAIN_IPV4;
    }

    static void WriteU16(std::vector<uint8_t>* buffer, uint16_t value)
    {
        buffer->push_back((uint8_t) ((value >> 8) & 0xff));
        buffer->push_back((uint8_t) (value & 0xff));
    }

    static void WriteU32(std::vector<uint8_t>* buffer, uint32_t value)
    {
        buffer->push_back((uint8_t) ((value >> 24) & 0xff));
        buffer->push_back((uint8_t) ((value >> 16) & 0xff));
        buffer->push_back((uint8_t) ((value >> 8) & 0xff));
        buffer->push_back((uint8_t) (value & 0xff));
    }

    static void WriteName(std::vector<uint8_t>* buffer, const char* name)
    {
        const char* cursor = name;
        while (*cursor)
        {
            char label[63];
            uint32_t label_len = 0;
            while (*cursor && *cursor != '.')
            {
                ASSERT_TRUE(label_len < sizeof(label));
                ASSERT_TRUE(DecodeEscapedChar(&cursor, &label[label_len]));
                ++label_len;
            }
            buffer->push_back((uint8_t) label_len);
            buffer->insert(buffer->end(), label, label + label_len);
            if (*cursor == '.')
                ++cursor;
        }
        buffer->push_back(0);
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

    static bool ReadName(const uint8_t* data, uint32_t size, uint32_t* offset, std::string* out)
    {
        uint32_t pos = *offset;
        bool jumped = false;
        uint32_t jump_count = 0;
        out->clear();

        while (true)
        {
            if (pos >= size)
                return false;

            const uint8_t len = data[pos];
            if ((len & 0xC0) == 0xC0)
            {
                if (pos + 1 >= size)
                    return false;

                const uint16_t pointer = (uint16_t) (((len & 0x3F) << 8) | data[pos + 1]);
                if (pointer >= size)
                    return false;

                if (!jumped)
                {
                    *offset = pos + 2;
                    jumped = true;
                }

                pos = pointer;
                if (++jump_count > 16)
                    return false;
                continue;
            }

            ++pos;
            if (len == 0)
            {
                if (!jumped)
                    *offset = pos;
                return true;
            }

            if (len > 63 || pos + len > size)
                return false;

            if (!out->empty())
                out->append(".");
            for (uint32_t i = 0; i < len; ++i)
            {
                const char c = (char) data[pos + i];
                if (c == '.' || c == '\\')
                    out->append("\\");
                out->push_back(c);
            }
            pos += len;
        }
    }

    static void BuildQueryPacket(const char* qname, uint16_t qtype, std::vector<uint8_t>* buffer)
    {
        buffer->clear();
        WriteU16(buffer, 0);
        WriteU16(buffer, 0);
        WriteU16(buffer, 1);
        WriteU16(buffer, 0);
        WriteU16(buffer, 0);
        WriteU16(buffer, 0);
        WriteName(buffer, qname);
        WriteU16(buffer, qtype);
        WriteU16(buffer, DNS_CLASS_IN);
    }

    struct RawDnsResponseRecord
    {
        const char*             m_Name;
        uint16_t                m_Type;
        uint16_t                m_Class;
        uint32_t                m_Ttl;
        const char*             m_PtrName;
        uint16_t                m_Port;
        const char*             m_Target;
        const dmMDNS::TxtEntry* m_Txt;
        uint32_t                m_TxtCount;
        const uint8_t*          m_Address;
    };

    static void PatchU16(std::vector<uint8_t>* buffer, uint32_t offset, uint16_t value)
    {
        (*buffer)[offset + 0] = (uint8_t) ((value >> 8) & 0xff);
        (*buffer)[offset + 1] = (uint8_t) (value & 0xff);
    }

    static void WriteNameCompressed(std::vector<uint8_t>* buffer, const char* name, std::map<std::string, uint16_t>* suffix_offsets)
    {
        const bool enable_compression = suffix_offsets != 0;
        const char* cursor = name;
        while (*cursor)
        {
            if (enable_compression)
            {
                std::map<std::string, uint16_t>::iterator it = suffix_offsets->find(cursor);
                if (it != suffix_offsets->end())
                {
                    const uint16_t pointer = it->second;
                    buffer->push_back((uint8_t) (0xC0 | ((pointer >> 8) & 0x3F)));
                    buffer->push_back((uint8_t) (pointer & 0xFF));
                    return;
                }

                (*suffix_offsets)[cursor] = (uint16_t) buffer->size();
            }

            char label[63];
            uint32_t label_len = 0;
            while (*cursor && *cursor != '.')
            {
                ASSERT_TRUE(label_len < sizeof(label));
                ASSERT_TRUE(DecodeEscapedChar(&cursor, &label[label_len]));
                ++label_len;
            }
            buffer->push_back((uint8_t) label_len);
            buffer->insert(buffer->end(), label, label + label_len);
            if (*cursor == '.')
                ++cursor;
        }

        buffer->push_back(0);
    }

    static void BuildResponsePacket(const RawDnsResponseRecord* records, uint32_t record_count, bool compress_names, std::vector<uint8_t>* buffer)
    {
        buffer->clear();
        WriteU16(buffer, 0);
        WriteU16(buffer, DNS_FLAG_RESPONSE);
        WriteU16(buffer, 0);
        WriteU16(buffer, (uint16_t) record_count);
        WriteU16(buffer, 0);
        WriteU16(buffer, 0);

        std::map<std::string, uint16_t> suffix_offsets;
        std::map<std::string, uint16_t>* offsets = compress_names ? &suffix_offsets : 0;

        for (uint32_t i = 0; i < record_count; ++i)
        {
            const RawDnsResponseRecord& record = records[i];
            WriteNameCompressed(buffer, record.m_Name, offsets);
            WriteU16(buffer, record.m_Type);
            WriteU16(buffer, record.m_Class);
            WriteU32(buffer, record.m_Ttl);

            const uint32_t rdlength_offset = (uint32_t) buffer->size();
            WriteU16(buffer, 0);
            const uint32_t rdata_start = (uint32_t) buffer->size();

            switch (record.m_Type)
            {
                case DNS_TYPE_PTR:
                {
                    WriteNameCompressed(buffer, record.m_PtrName, offsets);
                    break;
                }
                case DNS_TYPE_SRV:
                {
                    WriteU16(buffer, 0);
                    WriteU16(buffer, 0);
                    WriteU16(buffer, record.m_Port);
                    WriteNameCompressed(buffer, record.m_Target, offsets);
                    break;
                }
                case DNS_TYPE_TXT:
                {
                    for (uint32_t txt_i = 0; txt_i < record.m_TxtCount; ++txt_i)
                    {
                        char kv[320];
                        dmSnPrintf(kv, sizeof(kv), "%s=%s",
                                   record.m_Txt[txt_i].m_Key,
                                   record.m_Txt[txt_i].m_Value ? record.m_Txt[txt_i].m_Value : "");
                        const uint32_t len = (uint32_t) strlen(kv);
                        buffer->push_back((uint8_t) len);
                        buffer->insert(buffer->end(), kv, kv + len);
                    }

                    if (record.m_TxtCount == 0)
                    {
                        buffer->push_back(0);
                    }
                    break;
                }
                case DNS_TYPE_A:
                {
                    buffer->insert(buffer->end(), record.m_Address, record.m_Address + 4);
                    break;
                }
            }

            PatchU16(buffer, rdlength_offset, (uint16_t) ((uint32_t) buffer->size() - rdata_start));
        }
    }

    static bool ParseDnsPacket(const uint8_t* data, uint32_t size, RawDnsPacket* packet)
    {
        packet->m_IsResponse = false;
        packet->m_Questions.clear();
        packet->m_Records.clear();

        if (size < 12)
            return false;

        uint32_t offset = 0;
        uint16_t id = 0;
        uint16_t flags = 0;
        uint16_t qdcount = 0;
        uint16_t ancount = 0;
        uint16_t nscount = 0;
        uint16_t arcount = 0;
        if (!ReadU16(data, size, &offset, &id)
            || !ReadU16(data, size, &offset, &flags)
            || !ReadU16(data, size, &offset, &qdcount)
            || !ReadU16(data, size, &offset, &ancount)
            || !ReadU16(data, size, &offset, &nscount)
            || !ReadU16(data, size, &offset, &arcount))
        {
            return false;
        }

        packet->m_IsResponse = (flags & DNS_FLAG_RESPONSE) != 0;
        for (uint16_t i = 0; i < qdcount; ++i)
        {
            RawDnsQuestion question;
            if (!ReadName(data, size, &offset, &question.m_Name)
                || !ReadU16(data, size, &offset, &question.m_Type)
                || !ReadU16(data, size, &offset, &question.m_Class))
            {
                return false;
            }
            packet->m_Questions.push_back(question);
        }

        const uint32_t records = (uint32_t) ancount + (uint32_t) nscount + (uint32_t) arcount;
        for (uint32_t i = 0; i < records; ++i)
        {
            RawDnsRecord record;
            if (!ReadName(data, size, &offset, &record.m_Name)
                || !ReadU16(data, size, &offset, &record.m_Type)
                || !ReadU16(data, size, &offset, &record.m_Class)
                || !ReadU32(data, size, &offset, &record.m_Ttl)
                || !ReadU16(data, size, &offset, &record.m_Rdlength))
            {
                return false;
            }

            if (offset + record.m_Rdlength > size)
                return false;

            const uint32_t rdata_offset = offset;
            switch (record.m_Type)
            {
                case DNS_TYPE_PTR:
                {
                    uint32_t tmp = offset;
                    if (!ReadName(data, size, &tmp, &record.m_PtrName) || tmp != rdata_offset + record.m_Rdlength)
                        return false;
                    break;
                }
                case DNS_TYPE_SRV:
                {
                    uint16_t priority = 0;
                    uint16_t weight = 0;
                    uint32_t tmp = offset;
                    if (!ReadU16(data, size, &tmp, &priority)
                        || !ReadU16(data, size, &tmp, &weight)
                        || !ReadU16(data, size, &tmp, &record.m_Port)
                        || !ReadName(data, size, &tmp, &record.m_Target)
                        || tmp != rdata_offset + record.m_Rdlength)
                    {
                        return false;
                    }
                    break;
                }
                case DNS_TYPE_TXT:
                {
                    uint32_t txt_offset = offset;
                    while (txt_offset < rdata_offset + record.m_Rdlength)
                    {
                        const uint8_t len = data[txt_offset++];
                        if (txt_offset + len > rdata_offset + record.m_Rdlength)
                            return false;

                        std::string entry((const char*) data + txt_offset, (size_t) len);
                        size_t eq = entry.find('=');
                        if (eq == std::string::npos)
                            record.m_Txt[entry] = "";
                        else
                            record.m_Txt[entry.substr(0, eq)] = entry.substr(eq + 1);
                        txt_offset += len;
                    }
                    break;
                }
                case DNS_TYPE_A:
                {
                    if (record.m_Rdlength != 4)
                        return false;
                    memcpy(record.m_Address, data + rdata_offset, sizeof(record.m_Address));
                    record.m_HasAddress = true;
                    break;
                }
                default:
                    break;
            }

            packet->m_Records.push_back(record);
            offset += record.m_Rdlength;
        }

        return true;
    }

    static bool HasQuestion(const RawDnsPacket& packet, const char* name, uint16_t type, uint16_t klass)
    {
        for (uint32_t i = 0; i < packet.m_Questions.size(); ++i)
        {
            const RawDnsQuestion& question = packet.m_Questions[i];
            if (question.m_Type == type
                && question.m_Class == klass
                && dmStrCaseCmp(question.m_Name.c_str(), name) == 0)
            {
                return true;
            }
        }
        return false;
    }

    static const RawDnsRecord* FindRecord(const RawDnsPacket& packet, uint16_t type, const char* name)
    {
        for (uint32_t i = 0; i < packet.m_Records.size(); ++i)
        {
            const RawDnsRecord& record = packet.m_Records[i];
            if (record.m_Type == type && dmStrCaseCmp(record.m_Name.c_str(), name) == 0)
                return &record;
        }
        return 0;
    }

    static uint32_t CountRecordType(const RawDnsPacket& packet, uint16_t type)
    {
        uint32_t count = 0;
        for (uint32_t i = 0; i < packet.m_Records.size(); ++i)
        {
            if (packet.m_Records[i].m_Type == type)
                ++count;
        }
        return count;
    }

    static bool AllRecordsHaveTtl(const RawDnsPacket& packet, uint32_t ttl)
    {
        if (packet.m_Records.empty())
            return false;

        for (uint32_t i = 0; i < packet.m_Records.size(); ++i)
        {
            if (packet.m_Records[i].m_Ttl != ttl)
                return false;
        }
        return true;
    }

    static bool PacketMatchesNames(const RawDnsPacket& packet, const std::vector<std::string>& names)
    {
        for (uint32_t i = 0; i < packet.m_Records.size(); ++i)
        {
            for (uint32_t j = 0; j < names.size(); ++j)
            {
                if (dmStrCaseCmp(packet.m_Records[i].m_Name.c_str(), names[j].c_str()) == 0)
                    return true;
            }
        }
        return false;
    }

    static bool SetupMulticastCapture(MulticastCapture* capture)
    {
        dmSocket::Socket socket = dmSocket::INVALID_SOCKET_HANDLE;
        if (dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_DGRAM, dmSocket::PROTOCOL_UDP, &socket) != dmSocket::RESULT_OK)
            return false;

        if (dmSocket::SetReuseAddress(socket, true) != dmSocket::RESULT_OK
            || dmSocket::Bind(socket, dmSocket::AddressFromIPString("0.0.0.0"), MDNS_PORT) != dmSocket::RESULT_OK
            || dmSocket::SetBlocking(socket, false) != dmSocket::RESULT_OK)
        {
            dmSocket::Delete(socket);
            return false;
        }

        const dmSocket::Address multicast_address = dmSocket::AddressFromIPString(MDNS_MULTICAST_IPV4);
        bool joined = dmSocket::AddMembership(socket, multicast_address, dmSocket::AddressFromIPString("0.0.0.0"), 255) == dmSocket::RESULT_OK;

        dmSocket::Address local_address;
        if (dmSocket::GetLocalAddress(&local_address) == dmSocket::RESULT_OK && local_address.m_family == dmSocket::DOMAIN_IPV4)
        {
            joined = dmSocket::AddMembership(socket, multicast_address, local_address, 255) == dmSocket::RESULT_OK || joined;
        }

        if (!joined)
        {
            dmSocket::Delete(socket);
            return false;
        }

        capture->m_Socket = socket;
        return true;
    }

    static bool SetupSendSocket(ScopedSocketHandle* socket)
    {
        return dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_DGRAM, dmSocket::PROTOCOL_UDP, &socket->m_Socket) == dmSocket::RESULT_OK;
    }

    static bool SendPacketToAddress(dmSocket::Socket socket, const uint8_t* data, uint32_t size, const char* address, uint16_t port)
    {
        int sent = 0;
        return dmSocket::SendTo(socket, data, (int) size, &sent, dmSocket::AddressFromIPString(address), port) == dmSocket::RESULT_OK
               && sent == (int) size;
    }

    static void DrainSocket(dmSocket::Socket socket, uint32_t duration_ms)
    {
        const uint64_t deadline = dmTime::GetMonotonicTime() + (uint64_t) duration_ms * 1000ULL;
        uint8_t buffer[1500];
        while (dmTime::GetMonotonicTime() < deadline)
        {
            dmSocket::Address from_address;
            uint16_t from_port = 0;
            int received = 0;
            dmSocket::Result r = dmSocket::ReceiveFrom(socket, buffer, sizeof(buffer), &received, &from_address, &from_port);
            if (r == dmSocket::RESULT_WOULDBLOCK)
            {
                dmTime::Sleep(5 * 1000);
                continue;
            }
            if (r != dmSocket::RESULT_OK)
                return;
        }
    }

    static bool TryReceiveMatchingResponse(dmSocket::Socket socket, const std::vector<std::string>& names, RawDnsPacket* packet)
    {
        uint8_t buffer[1500];
        for (;;)
        {
            dmSocket::Address from_address;
            uint16_t from_port = 0;
            int received = 0;
            dmSocket::Result r = dmSocket::ReceiveFrom(socket, buffer, sizeof(buffer), &received, &from_address, &from_port);
            if (r == dmSocket::RESULT_WOULDBLOCK)
                return false;
            if (r != dmSocket::RESULT_OK || received <= 0)
                return false;

            RawDnsPacket candidate;
            if (ParseDnsPacket(buffer, (uint32_t) received, &candidate)
                && candidate.m_IsResponse
                && PacketMatchesNames(candidate, names))
            {
                *packet = candidate;
                return true;
            }
        }
    }

    static bool TryReceiveMatchingQuestion(dmSocket::Socket socket, const char* qname, uint16_t qtype, RawDnsPacket* packet)
    {
        uint8_t buffer[1500];
        for (;;)
        {
            dmSocket::Address from_address;
            uint16_t from_port = 0;
            int received = 0;
            dmSocket::Result r = dmSocket::ReceiveFrom(socket, buffer, sizeof(buffer), &received, &from_address, &from_port);
            if (r == dmSocket::RESULT_WOULDBLOCK)
                return false;
            if (r != dmSocket::RESULT_OK || received <= 0)
                return false;

            RawDnsPacket candidate;
            if (ParseDnsPacket(buffer, (uint32_t) received, &candidate)
                && !candidate.m_IsResponse
                && HasQuestion(candidate, qname, qtype, DNS_CLASS_IN))
            {
                *packet = candidate;
                return true;
            }
        }
    }

    static bool WaitForMatchingResponse(dmMDNS::HMDNS mdns, dmSocket::Socket socket, const std::vector<std::string>& names, RawDnsPacket* packet, uint32_t timeout_ms)
    {
        const uint64_t deadline = dmTime::GetMonotonicTime() + (uint64_t) timeout_ms * 1000ULL;
        while (dmTime::GetMonotonicTime() < deadline)
        {
            if (mdns)
                dmMDNS::Update(mdns);
            if (TryReceiveMatchingResponse(socket, names, packet))
                return true;
            dmTime::Sleep(5 * 1000);
        }
        return false;
    }

    static bool WaitForMatchingQuestion(dmMDNS::HBrowser browser, dmSocket::Socket socket, const char* qname, uint16_t qtype, RawDnsPacket* packet, uint32_t timeout_ms)
    {
        const uint64_t deadline = dmTime::GetMonotonicTime() + (uint64_t) timeout_ms * 1000ULL;
        while (dmTime::GetMonotonicTime() < deadline)
        {
            if (browser)
                dmMDNS::UpdateBrowser(browser);
            if (TryReceiveMatchingQuestion(socket, qname, qtype, packet))
                return true;
            dmTime::Sleep(5 * 1000);
        }
        return false;
    }

    static bool SendQueryAndCapture(dmMDNS::HMDNS mdns, dmSocket::Socket socket, const char* qname, uint16_t qtype, const std::vector<std::string>& names, RawDnsPacket* packet, uint32_t timeout_ms)
    {
        std::vector<uint8_t> query;
        BuildQueryPacket(qname, qtype, &query);

        // mDNS traffic is asynchronous and prior announcements may still be queued
        // on the capture socket. Drain them so this helper only observes replies
        // caused by the query we are about to send.
        DrainSocket(socket, 10);

        int sent = 0;
        if (dmSocket::SendTo(socket, &query[0], (int) query.size(), &sent, dmSocket::AddressFromIPString(MDNS_MULTICAST_IPV4), MDNS_PORT) != dmSocket::RESULT_OK
            || sent != (int) query.size())
        {
            return false;
        }

        return WaitForMatchingResponse(mdns, socket, names, packet, timeout_ms);
    }

    static bool QueryProducesNoMatchingResponse(dmMDNS::HMDNS mdns, dmSocket::Socket socket, const char* qname, uint16_t qtype, const std::vector<std::string>& names, uint32_t timeout_ms)
    {
        RawDnsPacket packet;
        return !SendQueryAndCapture(mdns, socket, qname, qtype, names, &packet, timeout_ms);
    }

    static bool SendPacketAndCaptureResponse(dmMDNS::HMDNS mdns, dmSocket::Socket send_socket, dmSocket::Socket capture_socket, const uint8_t* data, uint32_t size, const std::vector<std::string>& names, RawDnsPacket* packet, uint32_t timeout_ms)
    {
        if (!SendPacketToAddress(send_socket, data, size, MDNS_MULTICAST_IPV4, MDNS_PORT))
            return false;
        return WaitForMatchingResponse(mdns, capture_socket, names, packet, timeout_ms);
    }

    static bool PacketProducesNoMatchingResponse(dmMDNS::HMDNS mdns, dmSocket::Socket send_socket, dmSocket::Socket capture_socket, const uint8_t* data, uint32_t size, const std::vector<std::string>& names, uint32_t timeout_ms)
    {
        RawDnsPacket packet;
        return !SendPacketAndCaptureResponse(mdns, send_socket, capture_socket, data, size, names, &packet, timeout_ms);
    }

    struct TcpServerContext
    {
        TcpServerContext()
        {
            m_Port = 0;
            m_Result = dmSocket::RESULT_OK;
            dmAtomicStore32(&m_Listening, 0);
            dmAtomicStore32(&m_Accepted, 0);
            dmAtomicStore32(&m_Completed, 0);
            dmAtomicStore32(&m_Stop, 0);
        }

        uint16_t        m_Port;
        dmSocket::Result m_Result;
        int32_atomic_t  m_Listening;
        int32_atomic_t  m_Accepted;
        int32_atomic_t  m_Completed;
        int32_atomic_t  m_Stop;
    };

    struct ScopedMdnsTestResources
    {
        ScopedMdnsTestResources()
        : m_Mdns(0)
        , m_Browser(0)
        , m_ClientSocket(dmSocket::INVALID_SOCKET_HANDLE)
        , m_ServerThread(0)
        , m_HasServerThread(false)
        , m_ServerCtx(0)
        {
        }

        ~ScopedMdnsTestResources()
        {
            if (m_ServerCtx)
                dmAtomicStore32(&m_ServerCtx->m_Stop, 1);
            if (m_HasServerThread)
                dmThread::Join(m_ServerThread);
            if (m_ClientSocket != dmSocket::INVALID_SOCKET_HANDLE)
                dmSocket::Delete(m_ClientSocket);
            if (m_Browser != 0)
                dmMDNS::DeleteBrowser(m_Browser);
            if (m_Mdns != 0)
                dmMDNS::Delete(m_Mdns);
        }

        dmMDNS::HMDNS m_Mdns;
        dmMDNS::HBrowser m_Browser;
        dmSocket::Socket m_ClientSocket;
        dmThread::Thread m_ServerThread;
        bool m_HasServerThread;
        TcpServerContext* m_ServerCtx;
    };

    static void TcpServerThread(void* ctx_ptr)
    {
        TcpServerContext* ctx = (TcpServerContext*) ctx_ptr;

        dmSocket::Socket server_socket = dmSocket::INVALID_SOCKET_HANDLE;
        dmSocket::Socket client_socket = dmSocket::INVALID_SOCKET_HANDLE;
        dmSocket::Address bind_address;
        dmSocket::Address server_name;
        dmSocket::Result r = dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &server_socket);
        if (r != dmSocket::RESULT_OK)
        {
            ctx->m_Result = r;
            dmAtomicStore32(&ctx->m_Completed, 1);
            return;
        }

        r = dmSocket::SetReuseAddress(server_socket, true);
        if (r != dmSocket::RESULT_OK)
            goto bail;

        r = dmSocket::GetHostByName(DM_UNIVERSAL_BIND_ADDRESS_IPV4, &bind_address, true, false);
        if (r != dmSocket::RESULT_OK)
            goto bail;

        r = dmSocket::Bind(server_socket, bind_address, 0);
        if (r != dmSocket::RESULT_OK)
            goto bail;

        r = dmSocket::Listen(server_socket, 8);
        if (r != dmSocket::RESULT_OK)
            goto bail;

        r = dmSocket::GetName(server_socket, &server_name, &ctx->m_Port);
        if (r != dmSocket::RESULT_OK)
            goto bail;

        r = dmSocket::SetBlocking(server_socket, false);
        if (r != dmSocket::RESULT_OK)
            goto bail;

        dmAtomicStore32(&ctx->m_Listening, 1);

        for (uint32_t i = 0; i < 500 && !dmAtomicGet32(&ctx->m_Stop); ++i)
        {
            dmSocket::Address client_address;
            r = dmSocket::Accept(server_socket, &client_address, &client_socket);
            if (r == dmSocket::RESULT_WOULDBLOCK)
            {
                dmTime::Sleep(10 * 1000);
                continue;
            }
            if (r != dmSocket::RESULT_OK)
                goto bail;

            dmAtomicStore32(&ctx->m_Accepted, 1);

            char request[4];
            int read = 0;
            r = dmSocket::Receive(client_socket, request, sizeof(request), &read);
            if (r != dmSocket::RESULT_OK || read != (int) sizeof(request) || memcmp(request, "PING", 4) != 0)
            {
                ctx->m_Result = (r == dmSocket::RESULT_OK) ? dmSocket::RESULT_UNKNOWN : r;
                goto bail;
            }

            int written = 0;
            r = dmSocket::Send(client_socket, "PONG", 4, &written);
            if (r != dmSocket::RESULT_OK || written != 4)
            {
                ctx->m_Result = (r == dmSocket::RESULT_OK) ? dmSocket::RESULT_UNKNOWN : r;
                goto bail;
            }

            ctx->m_Result = dmSocket::RESULT_OK;
            goto bail;
        }

        ctx->m_Result = dmSocket::RESULT_TIMEDOUT;

bail:
        if (client_socket != dmSocket::INVALID_SOCKET_HANDLE)
            dmSocket::Delete(client_socket);
        if (server_socket != dmSocket::INVALID_SOCKET_HANDLE)
            dmSocket::Delete(server_socket);

        dmAtomicStore32(&ctx->m_Completed, 1);
    }
}

// Verifies API-level validation and service registration lifecycle rules.
// This locks down the contract before any wire-level behavior is exercised.
TEST(MDNS, RegisterLifecycle)
{
    dmMDNS::HMDNS mdns = 0;

    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::New(0, 0));
    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::New(0, &mdns));

    dmMDNS::Params invalid_params;
    invalid_params.m_UseIPv4 = 0;
    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::New(&invalid_params, &mdns));

    dmMDNS::Params params;
    params.m_AnnounceInterval = 1;
    params.m_Ttl = 2;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::New(&params, &mdns));
    ASSERT_NE((dmMDNS::HMDNS) 0, mdns);

    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::RegisterService(mdns, 0));
    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::DeregisterService(mdns, 0));

    char long_instance_name[80];
    memset(long_instance_name, 'a', sizeof(long_instance_name) - 1);
    long_instance_name[sizeof(long_instance_name) - 1] = 0;

    dmMDNS::ServiceDesc invalid_service;
    memset(&invalid_service, 0, sizeof(invalid_service));
    invalid_service.m_Id = "mdns-invalid-01";
    invalid_service.m_InstanceName = long_instance_name;
    invalid_service.m_ServiceType = SERVICE_TYPE;
    invalid_service.m_Port = 18001;
    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::RegisterService(mdns, &invalid_service));

    char long_host[80];
    memset(long_host, 'b', sizeof(long_host) - 1);
    long_host[sizeof(long_host) - 1] = 0;

    dmMDNS::ServiceDesc invalid_host_service;
    memset(&invalid_host_service, 0, sizeof(invalid_host_service));
    invalid_host_service.m_Id = "mdns-invalid-host-01";
    invalid_host_service.m_InstanceName = "invalid-host";
    invalid_host_service.m_ServiceType = SERVICE_TYPE;
    invalid_host_service.m_Host = long_host;
    invalid_host_service.m_Port = 18002;
    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::RegisterService(mdns, &invalid_host_service));

    dmMDNS::ServiceDesc missing_service_type_desc;
    memset(&missing_service_type_desc, 0, sizeof(missing_service_type_desc));
    missing_service_type_desc.m_Id = "mdns-missing-service-type-01";
    missing_service_type_desc.m_InstanceName = "missing-service-type";
    missing_service_type_desc.m_Port = 18003;
    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::RegisterService(mdns, &missing_service_type_desc));

    dmMDNS::TxtEntry too_many_entries[17];
    for (uint32_t i = 0; i < 17; ++i)
    {
        too_many_entries[i].m_Key = "k";
        too_many_entries[i].m_Value = "v";
    }

    dmMDNS::ServiceDesc too_many_txt_service;
    memset(&too_many_txt_service, 0, sizeof(too_many_txt_service));
    too_many_txt_service.m_Id = "mdns-too-many-txt-01";
    too_many_txt_service.m_InstanceName = "too-many-txt";
    too_many_txt_service.m_ServiceType = SERVICE_TYPE;
    too_many_txt_service.m_Port = 18004;
    too_many_txt_service.m_Txt = too_many_entries;
    too_many_txt_service.m_TxtCount = 17;
    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::RegisterService(mdns, &too_many_txt_service));

    char oversized_value[300];
    memset(oversized_value, 'x', sizeof(oversized_value) - 1);
    oversized_value[sizeof(oversized_value) - 1] = 0;

    dmMDNS::TxtEntry oversized_txt_entry[] =
    {
        {"oversized", oversized_value},
    };

    dmMDNS::ServiceDesc oversized_txt_service;
    memset(&oversized_txt_service, 0, sizeof(oversized_txt_service));
    oversized_txt_service.m_Id = "mdns-oversized-txt-01";
    oversized_txt_service.m_InstanceName = "oversized-txt";
    oversized_txt_service.m_ServiceType = SERVICE_TYPE;
    oversized_txt_service.m_Port = 18005;
    oversized_txt_service.m_Txt = oversized_txt_entry;
    oversized_txt_service.m_TxtCount = 1;
    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::RegisterService(mdns, &oversized_txt_service));

    dmMDNS::TxtEntry txt_entries[] =
    {
        {"id", "mdns-lifecycle-01"},
        {"name", "lifecycle-target"},
        {"log_port", "18001"},
    };

    dmMDNS::ServiceDesc service;
    memset(&service, 0, sizeof(service));
    service.m_Id = "mdns-lifecycle-01";
    service.m_InstanceName = "lifecycle-target";
    service.m_ServiceType = SERVICE_TYPE;
    service.m_Port = 18001;
    service.m_Txt = txt_entries;
    service.m_TxtCount = sizeof(txt_entries) / sizeof(txt_entries[0]);
    service.m_Ttl = 2;

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::RegisterService(mdns, &service));
    ASSERT_EQ(dmMDNS::RESULT_ALREADY_REGISTERED, dmMDNS::RegisterService(mdns, &service));
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::DeregisterService(mdns, service.m_Id));
    ASSERT_EQ(dmMDNS::RESULT_NOT_REGISTERED, dmMDNS::DeregisterService(mdns, service.m_Id));

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::Delete(mdns));
}

// Verifies native advertiser/browser discovery and explicit removal via deregistration.
// This is the baseline end-to-end lifecycle test for the C++ stack.
TEST(MDNS, ResolveAndRemove)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    EventLog event_log;
    ScopedMdnsTestResources cleanup;

    const uint64_t nonce = dmTime::GetMonotonicTime();
    char service_id[128];
    char instance_name[128];
    char advertised_id[128];
    MakeUniqueName(service_id, sizeof(service_id), "mdns-resolve", nonce);
    MakeUniqueName(instance_name, sizeof(instance_name), "resolve-target", nonce);
    MakeUniqueName(advertised_id, sizeof(advertised_id), "mdns-resolve-txt", nonce);

    dmMDNS::Params params;
    params.m_AnnounceInterval = 1;
    params.m_Ttl = 2;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::New(&params, &cleanup.m_Mdns));

    dmMDNS::BrowserParams browser_params;
    browser_params.m_ServiceType = SERVICE_TYPE;
    browser_params.m_Callback = EventLog::Callback;
    browser_params.m_Context = &event_log;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::NewBrowser(&browser_params, &cleanup.m_Browser));

    dmMDNS::TxtEntry txt_entries[] =
    {
        {"id", advertised_id},
        {"name", instance_name},
        {"log_port", "19001"},
        {"schema", "1"},
    };

    dmMDNS::ServiceDesc service;
    memset(&service, 0, sizeof(service));
    service.m_Id = service_id;
    service.m_InstanceName = instance_name;
    service.m_ServiceType = SERVICE_TYPE;
    service.m_Port = 19001;
    service.m_Txt = txt_entries;
    service.m_TxtCount = sizeof(txt_entries) / sizeof(txt_entries[0]);
    service.m_Ttl = 2;

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::RegisterService(cleanup.m_Mdns, &service));
    ASSERT_TRUE(WaitForEvent(event_log, dmMDNS::EVENT_RESOLVED, service.m_InstanceName, cleanup.m_Mdns, cleanup.m_Browser, 15000));

    const EventSnapshot* resolved = event_log.FindInstance(dmMDNS::EVENT_RESOLVED, service.m_InstanceName);
    ASSERT_NE((const EventSnapshot*) 0, resolved);
    ASSERT_EQ(service.m_Port, resolved->m_Port);
    ASSERT_TRUE(!resolved->m_Host.empty());
    ASSERT_EQ(std::string(instance_name), resolved->m_InstanceName);
    ASSERT_EQ(std::string(advertised_id), resolved->m_Id);
    std::map<std::string, std::string>::const_iterator log_port_it = resolved->m_Txt.find("log_port");
    ASSERT_TRUE(log_port_it != resolved->m_Txt.end());
    ASSERT_EQ(std::string("19001"), log_port_it->second);
    std::map<std::string, std::string>::const_iterator schema_it = resolved->m_Txt.find("schema");
    ASSERT_TRUE(schema_it != resolved->m_Txt.end());
    ASSERT_EQ(std::string("1"), schema_it->second);

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::DeregisterService(cleanup.m_Mdns, service.m_Id));
    ASSERT_TRUE(WaitForEvent(event_log, dmMDNS::EVENT_REMOVED, service.m_InstanceName, cleanup.m_Mdns, cleanup.m_Browser, 15000));
}

// Verifies a DNS-SD instance label containing dots is escaped on the wire and unescaped in browser callbacks.
TEST(MDNS, ResolveInstanceNameWithDots)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    EventLog event_log;
    ScopedMdnsTestResources cleanup;

    const uint64_t nonce = dmTime::GetMonotonicTime();
    char service_id[128];
    char advertised_id[128];
    MakeUniqueName(service_id, sizeof(service_id), "mdns-dot-instance", nonce);
    MakeUniqueName(advertised_id, sizeof(advertised_id), "mdns-dot-id", nonce);

    const char* instance_name = "resolve.with.dot";

    dmMDNS::Params params;
    params.m_AnnounceInterval = 1;
    params.m_Ttl = 2;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::New(&params, &cleanup.m_Mdns));

    dmMDNS::BrowserParams browser_params;
    browser_params.m_ServiceType = SERVICE_TYPE;
    browser_params.m_Callback = EventLog::Callback;
    browser_params.m_Context = &event_log;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::NewBrowser(&browser_params, &cleanup.m_Browser));

    dmMDNS::TxtEntry txt_entries[] =
    {
        {"id", advertised_id},
        {"name", instance_name},
        {"log_port", "19011"},
    };

    dmMDNS::ServiceDesc service;
    memset(&service, 0, sizeof(service));
    service.m_Id = service_id;
    service.m_InstanceName = instance_name;
    service.m_ServiceType = SERVICE_TYPE;
    service.m_Port = 19011;
    service.m_Txt = txt_entries;
    service.m_TxtCount = DM_ARRAY_SIZE(txt_entries);
    service.m_Ttl = 2;

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::RegisterService(cleanup.m_Mdns, &service));
    ASSERT_TRUE(WaitForEvent(event_log, dmMDNS::EVENT_RESOLVED, instance_name, cleanup.m_Mdns, cleanup.m_Browser, 15000));

    const EventSnapshot* resolved = event_log.FindInstance(dmMDNS::EVENT_RESOLVED, instance_name);
    ASSERT_NE((const EventSnapshot*) 0, resolved);
    ASSERT_EQ(std::string(instance_name), resolved->m_InstanceName);
    ASSERT_EQ(std::string(advertised_id), resolved->m_Id);
    std::map<std::string, std::string>::const_iterator name_it = resolved->m_Txt.find("name");
    ASSERT_TRUE(name_it != resolved->m_Txt.end());
    ASSERT_EQ(std::string(instance_name), name_it->second);
}

// Verifies that a resolved mDNS service yields a host/port that is actually usable.
// This catches regressions where discovery succeeds but the advertised endpoint is wrong.
TEST(MDNS, ResolveAndConnect)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    TcpServerContext server_ctx;
    ScopedMdnsTestResources cleanup;
    cleanup.m_ServerCtx = &server_ctx;
    cleanup.m_ServerThread = dmThread::New(TcpServerThread, 0x80000, &server_ctx, "mdns-tcp-server");
    ASSERT_TRUE(cleanup.m_ServerThread != (dmThread::Thread) 0);
    cleanup.m_HasServerThread = true;
    ASSERT_TRUE(WaitForFlag(&server_ctx.m_Listening, 300, 10 * 1000));
    ASSERT_TRUE(server_ctx.m_Port != 0);

    EventLog event_log;

    const uint64_t nonce = dmTime::GetMonotonicTime();
    char service_id[128];
    char instance_name[128];
    char advertised_id[128];
    MakeUniqueName(service_id, sizeof(service_id), "mdns-connect", nonce);
    MakeUniqueName(instance_name, sizeof(instance_name), "connect-target", nonce);
    MakeUniqueName(advertised_id, sizeof(advertised_id), "mdns-connect-txt", nonce);

    dmMDNS::Params params;
    params.m_AnnounceInterval = 1;
    params.m_Ttl = 2;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::New(&params, &cleanup.m_Mdns));

    dmMDNS::BrowserParams browser_params;
    browser_params.m_ServiceType = SERVICE_TYPE;
    browser_params.m_Callback = EventLog::Callback;
    browser_params.m_Context = &event_log;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::NewBrowser(&browser_params, &cleanup.m_Browser));

    dmMDNS::TxtEntry txt_entries[] =
    {
        {"id", advertised_id},
        {"name", instance_name},
        {"log_port", "19999"},
    };

    dmMDNS::ServiceDesc service;
    memset(&service, 0, sizeof(service));
    service.m_Id = service_id;
    service.m_InstanceName = instance_name;
    service.m_ServiceType = SERVICE_TYPE;
    service.m_Port = server_ctx.m_Port;
    service.m_Txt = txt_entries;
    service.m_TxtCount = sizeof(txt_entries) / sizeof(txt_entries[0]);
    service.m_Ttl = 2;

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::RegisterService(cleanup.m_Mdns, &service));

    for (uint32_t i = 0; i < 300 && !event_log.HasInstance(dmMDNS::EVENT_RESOLVED, service.m_InstanceName); ++i)
    {
        Pump(cleanup.m_Mdns, cleanup.m_Browser, 1, 10 * 1000);
    }

    const EventSnapshot* resolved = event_log.FindInstance(dmMDNS::EVENT_RESOLVED, service.m_InstanceName);
    ASSERT_NE((const EventSnapshot*) 0, resolved);
    ASSERT_TRUE(!resolved->m_Host.empty());
    ASSERT_EQ(std::string(instance_name), resolved->m_InstanceName);
    ASSERT_EQ(std::string(advertised_id), resolved->m_Id);

    dmSocket::Address address = dmSocket::AddressFromIPString(resolved->m_Host.c_str());
    if (address.m_family != dmSocket::DOMAIN_IPV4)
    {
        ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::GetHostByName(resolved->m_Host.c_str(), &address, true, false));
    }

    ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &cleanup.m_ClientSocket));
    ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::Connect(cleanup.m_ClientSocket, address, resolved->m_Port));

    int written = 0;
    ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::Send(cleanup.m_ClientSocket, "PING", 4, &written));
    ASSERT_EQ(4, written);

    char response[4] = {0};
    int read = 0;
    ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::Receive(cleanup.m_ClientSocket, response, sizeof(response), &read));
    ASSERT_EQ(4, read);
    ASSERT_TRUE(memcmp(response, "PONG", 4) == 0);

    ASSERT_TRUE(dmAtomicGet32(&server_ctx.m_Accepted) == 1);
    ASSERT_EQ(dmSocket::RESULT_OK, server_ctx.m_Result);
}

// Verifies the browser emits the expected PTR query for the requested service type.
// This protects the outgoing query wire shape that discovery depends on.
TEST(MDNS, BrowserBuildsExpectedQuery)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    ScopedMdnsTestResources cleanup;
    MulticastCapture capture;
    ScopedSocketHandle sender;
    ASSERT_TRUE(SetupMulticastCapture(&capture));
    ASSERT_TRUE(SetupSendSocket(&sender));

    const uint64_t nonce = dmTime::GetMonotonicTime();
    char service_type[64];
    char service_type_local[128];
    MakeUniqueServiceType(service_type, sizeof(service_type), nonce);
    BuildLocalName(service_type, service_type_local, sizeof(service_type_local));

    dmMDNS::BrowserParams browser_params;
    browser_params.m_ServiceType = service_type;
    browser_params.m_Callback = 0;
    browser_params.m_Context = 0;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::NewBrowser(&browser_params, &cleanup.m_Browser));
    DrainSocket(capture.m_Socket, 100);

    RawDnsPacket packet;
    ASSERT_TRUE(WaitForMatchingQuestion(cleanup.m_Browser, capture.m_Socket, service_type_local, DNS_TYPE_PTR, &packet, 2000));
    ASSERT_TRUE(!packet.m_IsResponse);
    ASSERT_EQ(1U, packet.m_Questions.size());
    ASSERT_EQ(0U, packet.m_Records.size());
    ASSERT_TRUE(HasQuestion(packet, service_type_local, DNS_TYPE_PTR, DNS_CLASS_IN));
}

// Verifies follow-up browse queries carry known PTR answers so responders can suppress duplicates.
TEST(MDNS, BrowserBuildsKnownAnswerQueryAfterDiscovery)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    EventLog event_log;
    ScopedMdnsTestResources cleanup;
    MulticastCapture capture;
    ScopedSocketHandle sender;
    ASSERT_TRUE(SetupMulticastCapture(&capture));
    ASSERT_TRUE(SetupSendSocket(&sender));

    const uint64_t nonce = dmTime::GetMonotonicTime();
    char service_type[64];
    char service_type_local[128];
    char instance_name[128];
    char full_service_name[256];
    char host_name[128];
    char host_local[256];
    char service_id[128];
    MakeUniqueServiceType(service_type, sizeof(service_type), nonce);
    MakeUniqueName(instance_name, sizeof(instance_name), "known-answer", nonce);
    MakeUniqueName(host_name, sizeof(host_name), "known-answer-host", nonce);
    MakeUniqueName(service_id, sizeof(service_id), "known-answer-id", nonce);
    BuildLocalName(service_type, service_type_local, sizeof(service_type_local));
    BuildFullServiceName(instance_name, service_type_local, full_service_name, sizeof(full_service_name));
    BuildLocalName(host_name, host_local, sizeof(host_local));

    dmMDNS::BrowserParams browser_params;
    browser_params.m_ServiceType = service_type;
    browser_params.m_Callback = EventLog::Callback;
    browser_params.m_Context = &event_log;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::NewBrowser(&browser_params, &cleanup.m_Browser));

    RawDnsPacket initial_query;
    ASSERT_TRUE(WaitForMatchingQuestion(cleanup.m_Browser, capture.m_Socket, service_type_local, DNS_TYPE_PTR, &initial_query, 2000));
    DrainSocket(capture.m_Socket, 100);

    dmMDNS::TxtEntry txt_entries[] =
    {
        {"id", service_id},
        {"name", instance_name},
        {"schema", "1"},
    };

    const uint8_t address[] = {127, 0, 0, 1};
    RawDnsResponseRecord records[] =
    {
        {service_type_local, DNS_TYPE_PTR, DNS_CLASS_IN, 10, full_service_name, 0, 0, 0, 0, 0},
        {full_service_name, DNS_TYPE_SRV, (uint16_t) (DNS_CLASS_IN | 0x8000), 10, 0, 19010, host_local, 0, 0, 0},
        {full_service_name, DNS_TYPE_TXT, (uint16_t) (DNS_CLASS_IN | 0x8000), 10, 0, 0, 0, txt_entries, DM_ARRAY_SIZE(txt_entries), 0},
        {host_local, DNS_TYPE_A, (uint16_t) (DNS_CLASS_IN | 0x8000), 10, 0, 0, 0, 0, 0, address},
    };

    std::vector<uint8_t> response;
    BuildResponsePacket(records, DM_ARRAY_SIZE(records), false, &response);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &response[0], (uint32_t) response.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_RESOLVED, instance_name, 1, 0, cleanup.m_Browser, 2000));
    DrainSocket(capture.m_Socket, 100);

    RawDnsPacket packet;
    ASSERT_TRUE(WaitForMatchingQuestion(cleanup.m_Browser, capture.m_Socket, service_type_local, DNS_TYPE_PTR, &packet, 4500));
    ASSERT_TRUE(!packet.m_IsResponse);
    ASSERT_EQ(1U, packet.m_Questions.size());
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_PTR));
    const RawDnsRecord* ptr_record = FindRecord(packet, DNS_TYPE_PTR, service_type_local);
    ASSERT_NE((const RawDnsRecord*) 0, ptr_record);
    ASSERT_EQ(std::string(full_service_name), ptr_record->m_PtrName);
    ASSERT_TRUE(ptr_record->m_Ttl > 0);
}

// Verifies the advertiser returns the expected PTR/SRV/TXT/A response set for valid queries.
// This locks down the native wire contract, including record payloads, classes, TTLs, and negative cases.
TEST(MDNS, QueryResponses)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    ScopedMdnsTestResources cleanup;
    MulticastCapture capture;
    ScopedSocketHandle sender;
    ASSERT_TRUE(SetupMulticastCapture(&capture));
    ASSERT_TRUE(SetupSendSocket(&sender));

    dmMDNS::Params params;
    params.m_AnnounceInterval = 60;
    params.m_Ttl = 7;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::New(&params, &cleanup.m_Mdns));

    dmMDNS::TxtEntry txt_entries[] =
    {
        {"id", "mdns-query-01"},
        {"name", "query-target"},
        {"log_port", "20001"},
    };

    dmMDNS::ServiceDesc service;
    memset(&service, 0, sizeof(service));
    service.m_Id = "mdns-query-01";
    service.m_InstanceName = "query-target";
    service.m_ServiceType = SERVICE_TYPE;
    service.m_Host = "query-host";
    service.m_Port = 20001;
    service.m_Txt = txt_entries;
    service.m_TxtCount = sizeof(txt_entries) / sizeof(txt_entries[0]);
    service.m_Ttl = params.m_Ttl;

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::RegisterService(cleanup.m_Mdns, &service));
    DrainSocket(capture.m_Socket, 300);
    dmMDNS::Update(cleanup.m_Mdns);
    DrainSocket(capture.m_Socket, 300);

    char service_type_local[128];
    char full_service_name[256];
    char host_local[256];
    BuildLocalName(service.m_ServiceType, service_type_local, sizeof(service_type_local));
    BuildFullServiceName(service.m_InstanceName, service_type_local, full_service_name, sizeof(full_service_name));
    BuildLocalName(service.m_Host, host_local, sizeof(host_local));

    std::vector<std::string> response_names;
    response_names.push_back(service_type_local);
    response_names.push_back(full_service_name);
    response_names.push_back(host_local);

    RawDnsPacket packet;
    ASSERT_TRUE(WaitForMatchingResponse(cleanup.m_Mdns, capture.m_Socket, response_names, &packet, 3000));
    // The responder sends a startup announcement burst after probing. Wait it out
    // so the explicit query/response checks below run against steady-state traffic.
    dmTime::Sleep(1200 * 1000);
    dmMDNS::Update(cleanup.m_Mdns);
    DrainSocket(capture.m_Socket, 100);
    ASSERT_TRUE(SendQueryAndCapture(cleanup.m_Mdns, capture.m_Socket, service_type_local, DNS_TYPE_PTR, response_names, &packet, 2000));
    ASSERT_EQ(4U, packet.m_Records.size());
    ASSERT_TRUE(packet.m_IsResponse);
    ASSERT_EQ(0U, packet.m_Questions.size());
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_PTR));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_SRV));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_TXT));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_A));
    ASSERT_TRUE(AllRecordsHaveTtl(packet, service.m_Ttl));

    const RawDnsRecord* ptr_record = FindRecord(packet, DNS_TYPE_PTR, service_type_local);
    ASSERT_NE((const RawDnsRecord*) 0, ptr_record);
    ASSERT_EQ(DNS_CLASS_IN, ptr_record->m_Class);
    ASSERT_EQ(std::string(full_service_name), ptr_record->m_PtrName);
    ASSERT_EQ(NameWireLength(full_service_name), ptr_record->m_Rdlength);

    const RawDnsRecord* srv_record = FindRecord(packet, DNS_TYPE_SRV, full_service_name);
    ASSERT_NE((const RawDnsRecord*) 0, srv_record);
    ASSERT_EQ((uint16_t) (DNS_CLASS_IN | 0x8000), srv_record->m_Class);
    ASSERT_EQ(service.m_Port, srv_record->m_Port);
    ASSERT_EQ(std::string(host_local), srv_record->m_Target);

    const RawDnsRecord* txt_record = FindRecord(packet, DNS_TYPE_TXT, full_service_name);
    ASSERT_NE((const RawDnsRecord*) 0, txt_record);
    ASSERT_EQ((uint16_t) (DNS_CLASS_IN | 0x8000), txt_record->m_Class);
    std::map<std::string, std::string>::const_iterator id_it = txt_record->m_Txt.find("id");
    std::map<std::string, std::string>::const_iterator name_it = txt_record->m_Txt.find("name");
    std::map<std::string, std::string>::const_iterator log_port_it = txt_record->m_Txt.find("log_port");
    ASSERT_TRUE(id_it != txt_record->m_Txt.end());
    ASSERT_TRUE(name_it != txt_record->m_Txt.end());
    ASSERT_TRUE(log_port_it != txt_record->m_Txt.end());
    ASSERT_EQ(std::string(service.m_Id), id_it->second);
    ASSERT_EQ(std::string(service.m_InstanceName), name_it->second);
    ASSERT_EQ(std::string("20001"), log_port_it->second);

    const RawDnsRecord* a_record = FindRecord(packet, DNS_TYPE_A, host_local);
    ASSERT_NE((const RawDnsRecord*) 0, a_record);
    ASSERT_EQ((uint16_t) (DNS_CLASS_IN | 0x8000), a_record->m_Class);
    ASSERT_TRUE(a_record->m_HasAddress);
    dmSocket::Address expected_address;
    ASSERT_TRUE(GetExpectedAdvertisedAddress(&expected_address));
    ASSERT_TRUE(memcmp(a_record->m_Address, dmSocket::IPv4(&expected_address), sizeof(a_record->m_Address)) == 0);

    DrainSocket(capture.m_Socket, 100);
    ASSERT_TRUE(SendQueryAndCapture(cleanup.m_Mdns, capture.m_Socket, full_service_name, DNS_TYPE_ANY, response_names, &packet, 2000));
    ASSERT_EQ(4U, packet.m_Records.size());
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_PTR));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_SRV));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_TXT));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_A));
    ASSERT_TRUE(AllRecordsHaveTtl(packet, service.m_Ttl));

    DrainSocket(capture.m_Socket, 100);
    ASSERT_TRUE(SendQueryAndCapture(cleanup.m_Mdns, capture.m_Socket, host_local, DNS_TYPE_A, response_names, &packet, 2000));
    ASSERT_EQ(4U, packet.m_Records.size());
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_PTR));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_SRV));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_TXT));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_A));
    ASSERT_TRUE(AllRecordsHaveTtl(packet, service.m_Ttl));

    DrainSocket(capture.m_Socket, 100);
    ASSERT_TRUE(QueryProducesNoMatchingResponse(cleanup.m_Mdns, capture.m_Socket, "_not-defold._tcp.local", DNS_TYPE_PTR, response_names, 500));
    ASSERT_TRUE(QueryProducesNoMatchingResponse(cleanup.m_Mdns, capture.m_Socket, "wrong.query-target._defoldtest._tcp.local", DNS_TYPE_ANY, response_names, 500));
    ASSERT_TRUE(QueryProducesNoMatchingResponse(cleanup.m_Mdns, capture.m_Socket, "wrong-query-host.local", DNS_TYPE_A, response_names, 500));
}

// Verifies malformed incoming queries are ignored without breaking later valid responses.
// This protects the advertiser against truncated or cyclic DNS traffic on the multicast socket.
TEST(MDNS, IgnoresMalformedQueries)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    ScopedMdnsTestResources cleanup;
    MulticastCapture capture;
    ScopedSocketHandle sender;
    ASSERT_TRUE(SetupMulticastCapture(&capture));
    ASSERT_TRUE(SetupSendSocket(&sender));

    dmMDNS::Params params;
    params.m_AnnounceInterval = 60;
    params.m_Ttl = 7;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::New(&params, &cleanup.m_Mdns));

    dmMDNS::TxtEntry txt_entries[] =
    {
        {"id", "mdns-malformed-query-01"},
        {"name", "malformed-query-target"},
        {"log_port", "20003"},
    };

    dmMDNS::ServiceDesc service;
    memset(&service, 0, sizeof(service));
    service.m_Id = "mdns-malformed-query-01";
    service.m_InstanceName = "malformed-query-target";
    service.m_ServiceType = SERVICE_TYPE;
    service.m_Host = "malformed-query-host";
    service.m_Port = 20003;
    service.m_Txt = txt_entries;
    service.m_TxtCount = sizeof(txt_entries) / sizeof(txt_entries[0]);
    service.m_Ttl = params.m_Ttl;

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::RegisterService(cleanup.m_Mdns, &service));
    DrainSocket(capture.m_Socket, 300);
    dmMDNS::Update(cleanup.m_Mdns);
    DrainSocket(capture.m_Socket, 300);

    char service_type_local[128];
    char full_service_name[256];
    char host_local[256];
    BuildLocalName(service.m_ServiceType, service_type_local, sizeof(service_type_local));
    BuildFullServiceName(service.m_InstanceName, service_type_local, full_service_name, sizeof(full_service_name));
    BuildLocalName(service.m_Host, host_local, sizeof(host_local));

    std::vector<std::string> response_names;
    response_names.push_back(service_type_local);
    response_names.push_back(full_service_name);
    response_names.push_back(host_local);

    RawDnsPacket packet;
    ASSERT_TRUE(WaitForMatchingResponse(cleanup.m_Mdns, capture.m_Socket, response_names, &packet, 3000));
    // Let the startup announcement burst finish before asserting that malformed
    // packets do not trigger additional responses.
    dmTime::Sleep(1200 * 1000);
    dmMDNS::Update(cleanup.m_Mdns);
    DrainSocket(capture.m_Socket, 100);

    std::vector<uint8_t> truncated_query;
    BuildQueryPacket(service_type_local, DNS_TYPE_PTR, &truncated_query);
    truncated_query.resize(truncated_query.size() - 1);
    DrainSocket(capture.m_Socket, 100);
    ASSERT_TRUE(PacketProducesNoMatchingResponse(cleanup.m_Mdns, sender.m_Socket, capture.m_Socket, &truncated_query[0], (uint32_t) truncated_query.size(), response_names, 500));

    std::vector<uint8_t> pointer_loop_query;
    WriteU16(&pointer_loop_query, 0);
    WriteU16(&pointer_loop_query, 0);
    WriteU16(&pointer_loop_query, 1);
    WriteU16(&pointer_loop_query, 0);
    WriteU16(&pointer_loop_query, 0);
    WriteU16(&pointer_loop_query, 0);
    pointer_loop_query.push_back(0xC0);
    pointer_loop_query.push_back(0x0C);
    WriteU16(&pointer_loop_query, DNS_TYPE_PTR);
    WriteU16(&pointer_loop_query, DNS_CLASS_IN);
    DrainSocket(capture.m_Socket, 100);
    ASSERT_TRUE(PacketProducesNoMatchingResponse(cleanup.m_Mdns, sender.m_Socket, capture.m_Socket, &pointer_loop_query[0], (uint32_t) pointer_loop_query.size(), response_names, 500));

    std::vector<uint8_t> truncated_question_query;
    BuildQueryPacket(service_type_local, DNS_TYPE_PTR, &truncated_question_query);
    truncated_question_query.resize(12 + NameWireLength(service_type_local) + 2);
    DrainSocket(capture.m_Socket, 100);
    ASSERT_TRUE(PacketProducesNoMatchingResponse(cleanup.m_Mdns, sender.m_Socket, capture.m_Socket, &truncated_question_query[0], (uint32_t) truncated_question_query.size(), response_names, 500));

    ASSERT_TRUE(SendQueryAndCapture(cleanup.m_Mdns, capture.m_Socket, service_type_local, DNS_TYPE_PTR, response_names, &packet, 2000));
    ASSERT_EQ(4U, packet.m_Records.size());
}

// Verifies deregister/delete send zero-TTL records for the full advertised record set.
// Browsers rely on these explicit deannouncements to remove stale services immediately.
TEST(MDNS, ZeroTtlAnnouncements)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    MulticastCapture capture;
    ASSERT_TRUE(SetupMulticastCapture(&capture));

    dmMDNS::Params params;
    params.m_AnnounceInterval = 60;
    params.m_Ttl = 9;

    dmMDNS::HMDNS mdns = 0;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::New(&params, &mdns));

    dmMDNS::TxtEntry txt_entries[] =
    {
        {"id", "mdns-zero-ttl-01"},
        {"name", "zero-ttl"},
        {"log_port", "20002"},
    };

    dmMDNS::ServiceDesc service;
    memset(&service, 0, sizeof(service));
    service.m_Id = "mdns-zero-ttl-01";
    service.m_InstanceName = "zero-ttl";
    service.m_ServiceType = SERVICE_TYPE;
    service.m_Host = "zero-ttl-host";
    service.m_Port = 20002;
    service.m_Txt = txt_entries;
    service.m_TxtCount = sizeof(txt_entries) / sizeof(txt_entries[0]);
    service.m_Ttl = params.m_Ttl;

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::RegisterService(mdns, &service));
    DrainSocket(capture.m_Socket, 100);

    char service_type_local[128];
    char full_service_name[256];
    char host_local[256];
    BuildLocalName(service.m_ServiceType, service_type_local, sizeof(service_type_local));
    BuildFullServiceName(service.m_InstanceName, service_type_local, full_service_name, sizeof(full_service_name));
    BuildLocalName(service.m_Host, host_local, sizeof(host_local));

    std::vector<std::string> response_names;
    response_names.push_back(service_type_local);
    response_names.push_back(full_service_name);
    response_names.push_back(host_local);

    RawDnsPacket packet;
    ASSERT_TRUE(WaitForMatchingResponse(mdns, capture.m_Socket, response_names, &packet, 3000));
    DrainSocket(capture.m_Socket, 100);
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::DeregisterService(mdns, service.m_Id));
    ASSERT_TRUE(WaitForMatchingResponse(0, capture.m_Socket, response_names, &packet, 1000));
    ASSERT_TRUE(AllRecordsHaveTtl(packet, 0));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_PTR));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_SRV));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_TXT));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_A));

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::RegisterService(mdns, &service));
    ASSERT_TRUE(WaitForMatchingResponse(mdns, capture.m_Socket, response_names, &packet, 3000));
    DrainSocket(capture.m_Socket, 100);
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::Delete(mdns));
    mdns = 0;
    ASSERT_TRUE(WaitForMatchingResponse(0, capture.m_Socket, response_names, &packet, 1000));
    ASSERT_TRUE(AllRecordsHaveTtl(packet, 0));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_PTR));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_SRV));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_TXT));
    ASSERT_EQ(1U, CountRecordType(packet, DNS_TYPE_A));
}

// Verifies the browser parses compressed DNS names and still resolves a complete service.
// This ensures interoperability with responders that use standard DNS name compression.
TEST(MDNS, BrowserParsesCompressedResponses)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    EventLog event_log;
    ScopedMdnsTestResources cleanup;
    ScopedSocketHandle sender;
    ASSERT_TRUE(SetupSendSocket(&sender));

    const uint64_t nonce = dmTime::GetMonotonicTime();
    char service_type[64];
    char service_type_local[128];
    char instance_name[128];
    char full_service_name[256];
    char host_name[128];
    char host_local[256];
    MakeUniqueServiceType(service_type, sizeof(service_type), nonce);
    MakeUniqueName(instance_name, sizeof(instance_name), "compressed-target", nonce);
    MakeUniqueName(host_name, sizeof(host_name), "compressed-host", nonce);
    BuildLocalName(service_type, service_type_local, sizeof(service_type_local));
    BuildFullServiceName(instance_name, service_type_local, full_service_name, sizeof(full_service_name));
    BuildLocalName(host_name, host_local, sizeof(host_local));

    dmMDNS::BrowserParams browser_params;
    browser_params.m_ServiceType = service_type;
    browser_params.m_Callback = EventLog::Callback;
    browser_params.m_Context = &event_log;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::NewBrowser(&browser_params, &cleanup.m_Browser));

    std::vector<uint8_t> query;
    BuildQueryPacket(service_type_local, DNS_TYPE_PTR, &query);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &query[0], (uint32_t) query.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    Pump(0, cleanup.m_Browser, 5, 10 * 1000);
    ASSERT_TRUE(event_log.m_Events.empty());

    dmMDNS::TxtEntry txt_entries[] =
    {
        {"name", instance_name},
        {"schema", "1"},
    };

    const uint8_t address[] = {127, 0, 0, 55};
    RawDnsResponseRecord ptr_record[] =
    {
        {service_type_local, DNS_TYPE_PTR, DNS_CLASS_IN, 120, full_service_name, 0, 0, 0, 0, 0},
    };
    RawDnsResponseRecord compressed_records[] =
    {
        {full_service_name, DNS_TYPE_SRV, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 21001, host_local, 0, 0, 0},
        {full_service_name, DNS_TYPE_TXT, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, txt_entries, 2, 0},
        {host_local, DNS_TYPE_A, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, 0, 0, address},
    };

    std::vector<uint8_t> response;
    BuildResponsePacket(ptr_record, 1, false, &response);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &response[0], (uint32_t) response.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_ADDED, instance_name, 1, 0, cleanup.m_Browser, 2000));

    BuildResponsePacket(compressed_records, sizeof(compressed_records) / sizeof(compressed_records[0]), true, &response);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &response[0], (uint32_t) response.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_RESOLVED, instance_name, 1, 0, cleanup.m_Browser, 2000));

    const EventSnapshot* resolved = event_log.FindInstance(dmMDNS::EVENT_RESOLVED, instance_name);
    ASSERT_NE((const EventSnapshot*) 0, resolved);
    ASSERT_EQ(std::string(full_service_name), resolved->m_Id);
    ASSERT_EQ(std::string(instance_name), resolved->m_InstanceName);
    ASSERT_EQ(std::string("127.0.0.55"), resolved->m_Host);
    ASSERT_EQ(21001, resolved->m_Port);
    ASSERT_EQ(1U, event_log.CountInstance(dmMDNS::EVENT_ADDED, instance_name));
    ASSERT_EQ(1U, event_log.CountInstance(dmMDNS::EVENT_RESOLVED, instance_name));
    ASSERT_EQ(0U, event_log.CountInstance(dmMDNS::EVENT_REMOVED, instance_name));
}

// Verifies the browser assembles service state across packets and handles malformed, zero-TTL, and expiry paths.
// This exercises the native browser state machine instead of only single-packet happy paths.
TEST(MDNS, BrowserTracksSplitResponsesAndExpiry)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    EventLog event_log;
    ScopedMdnsTestResources cleanup;
    ScopedSocketHandle sender;
    ASSERT_TRUE(SetupSendSocket(&sender));

    const uint64_t nonce = dmTime::GetMonotonicTime();
    char service_type[64];
    char service_type_local[128];
    char instance_name[128];
    char advertised_id[128];
    char full_service_name[256];
    char host_name[128];
    char host_local[256];
    MakeUniqueServiceType(service_type, sizeof(service_type), nonce);
    MakeUniqueName(instance_name, sizeof(instance_name), "split-target", nonce);
    MakeUniqueName(advertised_id, sizeof(advertised_id), "split-id", nonce);
    MakeUniqueName(host_name, sizeof(host_name), "split-host", nonce);
    BuildLocalName(service_type, service_type_local, sizeof(service_type_local));
    BuildFullServiceName(instance_name, service_type_local, full_service_name, sizeof(full_service_name));
    BuildLocalName(host_name, host_local, sizeof(host_local));

    dmMDNS::BrowserParams browser_params;
    browser_params.m_ServiceType = service_type;
    browser_params.m_Callback = EventLog::Callback;
    browser_params.m_Context = &event_log;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::NewBrowser(&browser_params, &cleanup.m_Browser));

    std::vector<uint8_t> query;
    BuildQueryPacket(service_type_local, DNS_TYPE_PTR, &query);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &query[0], (uint32_t) query.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    Pump(0, cleanup.m_Browser, 5, 10 * 1000);
    ASSERT_TRUE(event_log.m_Events.empty());

    const uint8_t address[] = {127, 0, 0, 56};
    dmMDNS::TxtEntry txt_entries[] =
    {
        {"id", advertised_id},
        {"schema", "1"},
    };

    RawDnsResponseRecord ptr_record[] =
    {
        {service_type_local, DNS_TYPE_PTR, DNS_CLASS_IN, 120, full_service_name, 0, 0, 0, 0, 0},
    };
    RawDnsResponseRecord srv_record[] =
    {
        {full_service_name, DNS_TYPE_SRV, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 22001, host_local, 0, 0, 0},
    };
    RawDnsResponseRecord txt_record[] =
    {
        {full_service_name, DNS_TYPE_TXT, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, txt_entries, 2, 0},
    };
    RawDnsResponseRecord a_record[] =
    {
        {host_local, DNS_TYPE_A, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, 0, 0, address},
    };

    std::vector<uint8_t> packet;
    BuildResponsePacket(ptr_record, 1, false, &packet);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &packet[0], (uint32_t) packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_ADDED, instance_name, 1, 0, cleanup.m_Browser, 2000));
    Pump(0, cleanup.m_Browser, 3, 10 * 1000);
    ASSERT_EQ(0U, event_log.CountInstance(dmMDNS::EVENT_RESOLVED, instance_name));

    BuildResponsePacket(srv_record, 1, false, &packet);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &packet[0], (uint32_t) packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    Pump(0, cleanup.m_Browser, 3, 10 * 1000);
    ASSERT_EQ(0U, event_log.CountInstance(dmMDNS::EVENT_RESOLVED, instance_name));

    BuildResponsePacket(txt_record, 1, false, &packet);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &packet[0], (uint32_t) packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    Pump(0, cleanup.m_Browser, 3, 10 * 1000);
    ASSERT_EQ(0U, event_log.CountInstance(dmMDNS::EVENT_RESOLVED, instance_name));

    BuildResponsePacket(a_record, 1, false, &packet);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &packet[0], (uint32_t) packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_RESOLVED, instance_name, 1, 0, cleanup.m_Browser, 2000));

    const EventSnapshot* resolved = event_log.FindInstance(dmMDNS::EVENT_RESOLVED, instance_name);
    ASSERT_NE((const EventSnapshot*) 0, resolved);
    ASSERT_EQ(std::string(advertised_id), resolved->m_Id);
    ASSERT_EQ(std::string("127.0.0.56"), resolved->m_Host);
    ASSERT_EQ(22001, resolved->m_Port);

    std::vector<uint8_t> truncated_packet;
    BuildResponsePacket(a_record, 1, false, &truncated_packet);
    truncated_packet.resize(truncated_packet.size() - 2);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &truncated_packet[0], (uint32_t) truncated_packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));

    std::vector<uint8_t> bad_rdlength_packet;
    BuildResponsePacket(a_record, 1, false, &bad_rdlength_packet);
    PatchU16(&bad_rdlength_packet, (uint32_t) bad_rdlength_packet.size() - 6, 16);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &bad_rdlength_packet[0], (uint32_t) bad_rdlength_packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));

    std::vector<uint8_t> pointer_loop_packet;
    WriteU16(&pointer_loop_packet, 0);
    WriteU16(&pointer_loop_packet, DNS_FLAG_RESPONSE);
    WriteU16(&pointer_loop_packet, 0);
    WriteU16(&pointer_loop_packet, 1);
    WriteU16(&pointer_loop_packet, 0);
    WriteU16(&pointer_loop_packet, 0);
    pointer_loop_packet.push_back(0xC0);
    pointer_loop_packet.push_back(0x0C);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &pointer_loop_packet[0], (uint32_t) pointer_loop_packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));

    Pump(0, cleanup.m_Browser, 10, 10 * 1000);
    ASSERT_EQ(1U, event_log.CountInstance(dmMDNS::EVENT_RESOLVED, instance_name));
    ASSERT_EQ(0U, event_log.CountInstance(dmMDNS::EVENT_REMOVED, instance_name));

    RawDnsResponseRecord srv_remove_record[] =
    {
        {full_service_name, DNS_TYPE_SRV, (uint16_t) (DNS_CLASS_IN | 0x8000), 0, 0, 22001, host_local, 0, 0, 0},
    };
    BuildResponsePacket(srv_remove_record, 1, false, &packet);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &packet[0], (uint32_t) packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_REMOVED, instance_name, 1, 0, cleanup.m_Browser, 2000));

    BuildResponsePacket(srv_record, 1, false, &packet);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &packet[0], (uint32_t) packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_RESOLVED, instance_name, 2, 0, cleanup.m_Browser, 2000));

    RawDnsResponseRecord txt_remove_record[] =
    {
        {full_service_name, DNS_TYPE_TXT, (uint16_t) (DNS_CLASS_IN | 0x8000), 0, 0, 0, 0, txt_entries, 2, 0},
    };
    BuildResponsePacket(txt_remove_record, 1, false, &packet);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &packet[0], (uint32_t) packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_REMOVED, instance_name, 2, 0, cleanup.m_Browser, 2000));

    BuildResponsePacket(txt_record, 1, false, &packet);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &packet[0], (uint32_t) packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_RESOLVED, instance_name, 3, 0, cleanup.m_Browser, 2000));

    RawDnsResponseRecord short_lived_a_record[] =
    {
        {host_local, DNS_TYPE_A, (uint16_t) (DNS_CLASS_IN | 0x8000), 1, 0, 0, 0, 0, 0, address},
    };
    BuildResponsePacket(short_lived_a_record, 1, false, &packet);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &packet[0], (uint32_t) packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    Pump(0, cleanup.m_Browser, 10, 10 * 1000);
    ASSERT_EQ(2U, event_log.CountInstance(dmMDNS::EVENT_REMOVED, instance_name));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_REMOVED, instance_name, 3, 0, cleanup.m_Browser, 2500));
}

// Verifies PTR, SRV, and TXT records each expire by time and remove the service when required.
// This covers timed-expiry behavior separately from explicit zero-TTL deannounce handling.
TEST(MDNS, BrowserExpiresPtrSrvTxtRecords)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    EventLog event_log;
    ScopedMdnsTestResources cleanup;
    ScopedSocketHandle sender;
    ASSERT_TRUE(SetupSendSocket(&sender));

    const uint64_t nonce = dmTime::GetMonotonicTime();
    char service_type[64];
    char service_type_local[128];
    MakeUniqueServiceType(service_type, sizeof(service_type), nonce);
    BuildLocalName(service_type, service_type_local, sizeof(service_type_local));

    dmMDNS::BrowserParams browser_params;
    browser_params.m_ServiceType = service_type;
    browser_params.m_Callback = EventLog::Callback;
    browser_params.m_Context = &event_log;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::NewBrowser(&browser_params, &cleanup.m_Browser));

    std::vector<uint8_t> query;
    BuildQueryPacket(service_type_local, DNS_TYPE_PTR, &query);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &query[0], (uint32_t) query.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    Pump(0, cleanup.m_Browser, 5, 10 * 1000);

    char ptr_instance[128];
    char srv_instance[128];
    char txt_instance[128];
    char ptr_id[128];
    char srv_id[128];
    char txt_id[128];
    char ptr_full_service_name[256];
    char srv_full_service_name[256];
    char txt_full_service_name[256];
    char ptr_host_name[128];
    char srv_host_name[128];
    char txt_host_name[128];
    char ptr_host_local[256];
    char srv_host_local[256];
    char txt_host_local[256];
    MakeUniqueName(ptr_instance, sizeof(ptr_instance), "expiry-ptr", nonce);
    MakeUniqueName(srv_instance, sizeof(srv_instance), "expiry-srv", nonce);
    MakeUniqueName(txt_instance, sizeof(txt_instance), "expiry-txt", nonce);
    MakeUniqueName(ptr_id, sizeof(ptr_id), "expiry-ptr-id", nonce);
    MakeUniqueName(srv_id, sizeof(srv_id), "expiry-srv-id", nonce);
    MakeUniqueName(txt_id, sizeof(txt_id), "expiry-txt-id", nonce);
    MakeUniqueName(ptr_host_name, sizeof(ptr_host_name), "expiry-ptr-host", nonce);
    MakeUniqueName(srv_host_name, sizeof(srv_host_name), "expiry-srv-host", nonce);
    MakeUniqueName(txt_host_name, sizeof(txt_host_name), "expiry-txt-host", nonce);
    BuildFullServiceName(ptr_instance, service_type_local, ptr_full_service_name, sizeof(ptr_full_service_name));
    BuildFullServiceName(srv_instance, service_type_local, srv_full_service_name, sizeof(srv_full_service_name));
    BuildFullServiceName(txt_instance, service_type_local, txt_full_service_name, sizeof(txt_full_service_name));
    BuildLocalName(ptr_host_name, ptr_host_local, sizeof(ptr_host_local));
    BuildLocalName(srv_host_name, srv_host_local, sizeof(srv_host_local));
    BuildLocalName(txt_host_name, txt_host_local, sizeof(txt_host_local));

    const uint8_t ptr_address[] = {127, 0, 0, 81};
    const uint8_t srv_address[] = {127, 0, 0, 82};
    const uint8_t txt_address[] = {127, 0, 0, 83};
    dmMDNS::TxtEntry ptr_txt[] = {{"id", ptr_id}, {"schema", "1"}};
    dmMDNS::TxtEntry srv_txt[] = {{"id", srv_id}, {"schema", "1"}};
    dmMDNS::TxtEntry txt_txt[] = {{"id", txt_id}, {"schema", "1"}};

    RawDnsResponseRecord records[] =
    {
        {service_type_local, DNS_TYPE_PTR, DNS_CLASS_IN, 1, ptr_full_service_name, 0, 0, 0, 0, 0},
        {ptr_full_service_name, DNS_TYPE_SRV, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 23001, ptr_host_local, 0, 0, 0},
        {ptr_full_service_name, DNS_TYPE_TXT, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, ptr_txt, DM_ARRAY_SIZE(ptr_txt), 0},
        {ptr_host_local, DNS_TYPE_A, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, 0, 0, ptr_address},
        {service_type_local, DNS_TYPE_PTR, DNS_CLASS_IN, 120, srv_full_service_name, 0, 0, 0, 0, 0},
        {srv_full_service_name, DNS_TYPE_SRV, (uint16_t) (DNS_CLASS_IN | 0x8000), 1, 0, 23002, srv_host_local, 0, 0, 0},
        {srv_full_service_name, DNS_TYPE_TXT, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, srv_txt, DM_ARRAY_SIZE(srv_txt), 0},
        {srv_host_local, DNS_TYPE_A, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, 0, 0, srv_address},
        {service_type_local, DNS_TYPE_PTR, DNS_CLASS_IN, 120, txt_full_service_name, 0, 0, 0, 0, 0},
        {txt_full_service_name, DNS_TYPE_SRV, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 23003, txt_host_local, 0, 0, 0},
        {txt_full_service_name, DNS_TYPE_TXT, (uint16_t) (DNS_CLASS_IN | 0x8000), 1, 0, 0, 0, txt_txt, DM_ARRAY_SIZE(txt_txt), 0},
        {txt_host_local, DNS_TYPE_A, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, 0, 0, txt_address},
    };

    std::vector<uint8_t> packet;
    BuildResponsePacket(records, DM_ARRAY_SIZE(records), false, &packet);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &packet[0], (uint32_t) packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_RESOLVED, ptr_instance, 1, 0, cleanup.m_Browser, 2000));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_RESOLVED, srv_instance, 1, 0, cleanup.m_Browser, 2000));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_RESOLVED, txt_instance, 1, 0, cleanup.m_Browser, 2000));

    dmTime::Sleep(1100 * 1000);
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_REMOVED, ptr_instance, 1, 0, cleanup.m_Browser, 2000));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_REMOVED, srv_instance, 1, 0, cleanup.m_Browser, 2000));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_REMOVED, txt_instance, 1, 0, cleanup.m_Browser, 2000));
}

// Verifies duplicate and conflicting records across multiple services do not corrupt browser state.
// Multicast traffic is noisy, so the browser must avoid duplicate events and converge on valid data.
TEST(MDNS, BrowserHandlesDuplicateAndConflictingRecords)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    EventLog event_log;
    ScopedMdnsTestResources cleanup;
    ScopedSocketHandle sender;
    ASSERT_TRUE(SetupSendSocket(&sender));

    const uint64_t nonce = dmTime::GetMonotonicTime();
    char service_type[64];
    char service_type_local[128];
    MakeUniqueServiceType(service_type, sizeof(service_type), nonce);
    BuildLocalName(service_type, service_type_local, sizeof(service_type_local));

    dmMDNS::BrowserParams browser_params;
    browser_params.m_ServiceType = service_type;
    browser_params.m_Callback = EventLog::Callback;
    browser_params.m_Context = &event_log;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::NewBrowser(&browser_params, &cleanup.m_Browser));

    std::vector<uint8_t> query;
    BuildQueryPacket(service_type_local, DNS_TYPE_PTR, &query);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &query[0], (uint32_t) query.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    Pump(0, cleanup.m_Browser, 5, 10 * 1000);

    char instance_a[128];
    char instance_b[128];
    char id_a[128];
    char id_b[128];
    char full_service_name_a[256];
    char full_service_name_b[256];
    char host_name_a[128];
    char host_name_b[128];
    char host_local_a[256];
    char host_local_b[256];
    MakeUniqueName(instance_a, sizeof(instance_a), "duplicate-target-a", nonce);
    MakeUniqueName(instance_b, sizeof(instance_b), "duplicate-target-b", nonce);
    MakeUniqueName(id_a, sizeof(id_a), "duplicate-id-a", nonce);
    MakeUniqueName(id_b, sizeof(id_b), "duplicate-id-b", nonce);
    MakeUniqueName(host_name_a, sizeof(host_name_a), "duplicate-host-a", nonce);
    MakeUniqueName(host_name_b, sizeof(host_name_b), "duplicate-host-b", nonce);
    BuildFullServiceName(instance_a, service_type_local, full_service_name_a, sizeof(full_service_name_a));
    BuildFullServiceName(instance_b, service_type_local, full_service_name_b, sizeof(full_service_name_b));
    BuildLocalName(host_name_a, host_local_a, sizeof(host_local_a));
    BuildLocalName(host_name_b, host_local_b, sizeof(host_local_b));

    const uint8_t address_a_1[] = {127, 0, 0, 91};
    const uint8_t address_a_2[] = {127, 0, 0, 92};
    const uint8_t address_b[] = {127, 0, 0, 93};
    dmMDNS::TxtEntry txt_a[] = {{"id", id_a}, {"schema", "1"}};
    dmMDNS::TxtEntry txt_b[] = {{"id", id_b}, {"schema", "1"}};

    RawDnsResponseRecord initial_records[] =
    {
        {service_type_local, DNS_TYPE_PTR, DNS_CLASS_IN, 120, full_service_name_a, 0, 0, 0, 0, 0},
        {service_type_local, DNS_TYPE_PTR, DNS_CLASS_IN, 120, full_service_name_a, 0, 0, 0, 0, 0},
        {full_service_name_a, DNS_TYPE_SRV, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 24001, host_local_a, 0, 0, 0},
        {full_service_name_a, DNS_TYPE_TXT, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, txt_a, DM_ARRAY_SIZE(txt_a), 0},
        {host_local_a, DNS_TYPE_A, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, 0, 0, address_a_1},
        {host_local_a, DNS_TYPE_A, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, 0, 0, address_a_2},
        {service_type_local, DNS_TYPE_PTR, DNS_CLASS_IN, 120, full_service_name_b, 0, 0, 0, 0, 0},
        {full_service_name_b, DNS_TYPE_SRV, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 24002, host_local_b, 0, 0, 0},
        {full_service_name_b, DNS_TYPE_TXT, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, txt_b, DM_ARRAY_SIZE(txt_b), 0},
        {host_local_b, DNS_TYPE_A, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 0, 0, 0, 0, address_b},
    };

    std::vector<uint8_t> packet;
    BuildResponsePacket(initial_records, DM_ARRAY_SIZE(initial_records), false, &packet);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &packet[0], (uint32_t) packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_RESOLVED, instance_a, 1, 0, cleanup.m_Browser, 2000));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_RESOLVED, instance_b, 1, 0, cleanup.m_Browser, 2000));
    ASSERT_EQ(1U, event_log.CountInstance(dmMDNS::EVENT_ADDED, instance_a));
    ASSERT_EQ(1U, event_log.CountInstance(dmMDNS::EVENT_RESOLVED, instance_a));
    ASSERT_EQ(0U, event_log.CountInstance(dmMDNS::EVENT_REMOVED, instance_a));
    ASSERT_EQ(1U, event_log.CountInstance(dmMDNS::EVENT_ADDED, instance_b));
    ASSERT_EQ(1U, event_log.CountInstance(dmMDNS::EVENT_RESOLVED, instance_b));

    const EventSnapshot* first_resolved_a = event_log.FindInstance(dmMDNS::EVENT_RESOLVED, instance_a);
    ASSERT_NE((const EventSnapshot*) 0, first_resolved_a);
    ASSERT_EQ(std::string("127.0.0.91"), first_resolved_a->m_Host);

    RawDnsResponseRecord srv_remove_record[] =
    {
        {full_service_name_a, DNS_TYPE_SRV, (uint16_t) (DNS_CLASS_IN | 0x8000), 0, 0, 24001, host_local_a, 0, 0, 0},
    };
    BuildResponsePacket(srv_remove_record, DM_ARRAY_SIZE(srv_remove_record), false, &packet);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &packet[0], (uint32_t) packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_REMOVED, instance_a, 1, 0, cleanup.m_Browser, 2000));

    RawDnsResponseRecord srv_restore_record[] =
    {
        {full_service_name_a, DNS_TYPE_SRV, (uint16_t) (DNS_CLASS_IN | 0x8000), 120, 0, 24001, host_local_a, 0, 0, 0},
    };
    BuildResponsePacket(srv_restore_record, DM_ARRAY_SIZE(srv_restore_record), false, &packet);
    ASSERT_TRUE(SendPacketToAddress(sender.m_Socket, &packet[0], (uint32_t) packet.size(), MDNS_MULTICAST_IPV4, MDNS_PORT));
    ASSERT_TRUE(WaitForEventCount(event_log, dmMDNS::EVENT_RESOLVED, instance_a, 2, 0, cleanup.m_Browser, 2000));

    const EventSnapshot* latest_resolved_a = event_log.FindLastInstance(dmMDNS::EVENT_RESOLVED, instance_a);
    ASSERT_NE((const EventSnapshot*) 0, latest_resolved_a);
    ASSERT_EQ(std::string("127.0.0.92"), latest_resolved_a->m_Host);
    ASSERT_EQ(std::string(id_a), latest_resolved_a->m_Id);
    ASSERT_EQ(1U, event_log.CountInstance(dmMDNS::EVENT_ADDED, instance_a));
    ASSERT_EQ(2U, event_log.CountInstance(dmMDNS::EVENT_RESOLVED, instance_a));
    ASSERT_EQ(1U, event_log.CountInstance(dmMDNS::EVENT_REMOVED, instance_a));
    ASSERT_EQ(0U, event_log.CountInstance(dmMDNS::EVENT_REMOVED, instance_b));
}

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    dmSocket::Finalize();
    return ret;
}
