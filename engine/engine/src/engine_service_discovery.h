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

#ifndef DM_ENGINE_SERVICE_DISCOVERY
#define DM_ENGINE_SERVICE_DISCOVERY

#include <stdint.h>

namespace dmEngineService
{
    struct DiscoveryTxtEntry
    {
        const char* m_Key;
        const char* m_Value;
    };

    typedef struct DiscoveryService* HDiscoveryService;

    HDiscoveryService DiscoveryServiceNew(const char* service_id, const char* instance_name, uint16_t port, const DiscoveryTxtEntry* txt_entries, uint32_t txt_count);
    void DiscoveryServiceDelete(HDiscoveryService discovery_service);
    void DiscoveryServiceUpdate(HDiscoveryService discovery_service);
}

#endif // DM_ENGINE_SERVICE_DISCOVERY
