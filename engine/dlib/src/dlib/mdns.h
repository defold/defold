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

#ifndef DM_MDNS
#define DM_MDNS

#include <stdint.h>
#include <string.h>

namespace dmMDNS
{
    typedef struct MDNS* HMDNS;
    typedef struct Browser* HBrowser;

    enum Result
    {
        RESULT_OK = 0,
        RESULT_NETWORK_ERROR = -1,
        RESULT_ALREADY_REGISTERED = -2,
        RESULT_NOT_REGISTERED = -3,
        RESULT_OUT_OF_RESOURCES = -4,
        RESULT_INVALID_ARGS = -5
    };

    struct TxtEntry
    {
        const char* m_Key;
        const char* m_Value;
    };

    struct ServiceDesc
    {
        const char*      m_Id;
        const char*      m_InstanceName;
        const char*      m_ServiceType; // e.g. "_defold._tcp"
        const char*      m_Host;        // optional, nullptr means auto
        uint16_t         m_Port;
        const TxtEntry*  m_Txt;
        uint32_t         m_TxtCount;
        uint32_t         m_Ttl;
    };

    struct Params
    {
        Params()
        {
            memset(this, 0, sizeof(*this));
            m_AnnounceInterval = 60;
            m_Ttl = 120;
            m_UseIPv4 = 1;
            m_UseIPv6 = 0;
        }

        uint32_t m_AnnounceInterval;
        uint32_t m_Ttl;
        uint32_t m_UseIPv4 : 1;
        uint32_t m_UseIPv6 : 1;
    };

    enum EventType
    {
        EVENT_ADDED = 0,
        EVENT_REMOVED = 1,
        EVENT_RESOLVED = 2
    };

    struct ServiceEvent
    {
        EventType       m_Type;
        const char*     m_Id;
        const char*     m_InstanceName;
        const char*     m_ServiceType;
        const char*     m_Host;
        uint16_t        m_Port;
        const TxtEntry* m_Txt;
        uint32_t        m_TxtCount;
    };

    typedef void (*ServiceCallback)(void* context, const ServiceEvent* event);

    struct BrowserParams
    {
        BrowserParams()
        {
            memset(this, 0, sizeof(*this));
        }

        const char*      m_ServiceType; // e.g. "_defold._tcp"
        ServiceCallback  m_Callback;
        void*            m_Context;
    };

    Result New(const Params* params, HMDNS* mdns);
    Result RegisterService(HMDNS mdns, const ServiceDesc* desc);
    Result DeregisterService(HMDNS mdns, const char* id);
    void Update(HMDNS mdns);
    Result Delete(HMDNS mdns);

    Result NewBrowser(const BrowserParams* params, HBrowser* browser);
    void UpdateBrowser(HBrowser browser);
    Result DeleteBrowser(HBrowser browser);
}

#endif // DM_MDNS
