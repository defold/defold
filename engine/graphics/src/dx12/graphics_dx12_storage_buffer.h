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

#ifndef DM_GRAPHICS_DX12_STORAGE_BUFFER_H
#define DM_GRAPHICS_DX12_STORAGE_BUFFER_H

#include "../graphics_private.h"

namespace dmGraphics
{
    // Native resource identity matters: DX12 buffer states cover the entire
    // resource, even when descriptors address disjoint byte ranges.
    struct DX12StorageBufferAccess
    {
        const void* m_Resource;
        uint8_t m_AccessFlags;
        uint8_t m_Set;
        uint8_t m_Binding;
    };

    // Legacy DX12 barriers cannot combine SRV read states with UAV write state.
    // The shader/root signature fixes each descriptor's type, so transitioning
    // an aliased buffer to UAV alone cannot make a readonly SRV binding valid.
    // Check the complete command before recording any transitions/descriptors.
    inline bool FindDX12StorageBufferAliasConflict(const DX12StorageBufferAccess* accesses, uint32_t count,
                                                   uint32_t& first, uint32_t& second)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!accesses[i].m_Resource)
                continue;
            // Missing access metadata is conservatively a UAV, as in binding.
            const bool read_only = accesses[i].m_AccessFlags == SHADER_RESOURCE_ACCESS_READ;
            for (uint32_t j = 0; j < i; ++j)
            {
                if (accesses[i].m_Resource == accesses[j].m_Resource &&
                    read_only != (accesses[j].m_AccessFlags == SHADER_RESOURCE_ACCESS_READ))
                {
                    first = j;
                    second = i;
                    return true;
                }
            }
        }
        return false;
    }
}

#endif
