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

#if !defined(DM_PLATFORM_IOS)

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/mdns.h>

#include "engine_service_discovery.h"

namespace dmEngineService
{
    static const char DISCOVERY_SERVICE_TYPE[] = "_defold._tcp";
    static const uint32_t DISCOVERY_ANNOUNCE_INTERVAL = 30;
    static const uint32_t DISCOVERY_TTL = 120;
    static const uint32_t DISCOVERY_HOST_LABEL_MAX_LENGTH = 63;

    struct DiscoveryService
    {
        DiscoveryService()
        : m_MDNS(0)
        {
            m_ServiceId[0] = 0;
        }

        dmMDNS::HMDNS m_MDNS;
        char          m_ServiceId[128];
    };

    static void BuildDiscoveryHostName(const char* service_id, char* out, uint32_t out_size)
    {
        if (out_size == 0)
            return;

        const char* source = (service_id && service_id[0]) ? service_id : "defold";
        uint32_t max_length = out_size - 1;
        if (max_length > DISCOVERY_HOST_LABEL_MAX_LENGTH)
            max_length = DISCOVERY_HOST_LABEL_MAX_LENGTH;

        uint32_t offset = 0;
        bool last_was_dash = false;
        while (*source && offset < max_length)
        {
            char c = *source++;
            if (c >= 'A' && c <= 'Z')
                c = (char) (c - 'A' + 'a');

            const bool is_alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
            if (is_alnum)
            {
                out[offset++] = c;
                last_was_dash = false;
            }
            else if (!last_was_dash && offset < max_length)
            {
                out[offset++] = '-';
                last_was_dash = true;
            }
        }

        while (offset > 0 && out[offset - 1] == '-')
            --offset;

        if (offset == 0)
        {
            dmStrlCpy(out, "defold", out_size);
            return;
        }

        if (out[0] == '-')
            out[0] = 'd';

        out[offset] = 0;
    }

    HDiscoveryService DiscoveryServiceNew(const char* service_id, const char* instance_name, uint16_t port, const DiscoveryTxtEntry* txt_entries, uint32_t txt_count)
    {
        if (service_id == 0 || instance_name == 0 || port == 0 || txt_entries == 0)
            return 0;

        dmMDNS::TxtEntry mdns_txt_entries[16];
        const uint32_t mdns_txt_entries_count = sizeof(mdns_txt_entries) / sizeof(mdns_txt_entries[0]);
        if (txt_count > mdns_txt_entries_count)
        {
            dmLogWarning("Unable to register mDNS service, too many TXT entries (%u)", txt_count);
            return 0;
        }

        for (uint32_t i = 0; i < txt_count; ++i)
        {
            mdns_txt_entries[i].m_Key = txt_entries[i].m_Key;
            mdns_txt_entries[i].m_Value = txt_entries[i].m_Value;
        }

        dmMDNS::Params mdns_params;
        mdns_params.m_AnnounceInterval = DISCOVERY_ANNOUNCE_INTERVAL;
        mdns_params.m_Ttl = DISCOVERY_TTL;

        dmMDNS::HMDNS mdns = 0;
        dmMDNS::Result mr = dmMDNS::New(&mdns_params, &mdns);
        if (mr != dmMDNS::RESULT_OK)
        {
            dmLogWarning("Unable to create mDNS service (%d)", mr);
            return 0;
        }

        dmMDNS::ServiceDesc mdns_desc;
        memset(&mdns_desc, 0, sizeof(mdns_desc));
        char host_name[64];
        BuildDiscoveryHostName(service_id, host_name, sizeof(host_name));

        mdns_desc.m_Id = service_id;
        mdns_desc.m_InstanceName = instance_name;
        mdns_desc.m_ServiceType = DISCOVERY_SERVICE_TYPE;
        mdns_desc.m_Host = host_name;
        mdns_desc.m_Port = port;
        mdns_desc.m_Txt = mdns_txt_entries;
        mdns_desc.m_TxtCount = txt_count;
        mdns_desc.m_Ttl = mdns_params.m_Ttl;

        mr = dmMDNS::RegisterService(mdns, &mdns_desc);
        if (mr != dmMDNS::RESULT_OK)
        {
            dmMDNS::Delete(mdns);
            dmLogWarning("Unable to register mDNS service (%d)", mr);
            return 0;
        }

        DiscoveryService* discovery_service = new DiscoveryService();
        discovery_service->m_MDNS = mdns;
        dmStrlCpy(discovery_service->m_ServiceId, service_id, sizeof(discovery_service->m_ServiceId));
        return discovery_service;
    }

    void DiscoveryServiceDelete(HDiscoveryService discovery_service)
    {
        if (discovery_service == 0)
            return;

        if (discovery_service->m_MDNS)
        {
            dmMDNS::DeregisterService(discovery_service->m_MDNS, discovery_service->m_ServiceId);
            dmMDNS::Delete(discovery_service->m_MDNS);
        }

        delete discovery_service;
    }

    void DiscoveryServiceUpdate(HDiscoveryService discovery_service)
    {
        if (discovery_service && discovery_service->m_MDNS)
        {
            dmMDNS::Update(discovery_service->m_MDNS);
        }
    }
}

#endif // !DM_PLATFORM_IOS
