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

#include "crash_private.h"

#include <mach-o/loader.h>
#include <mach-o/dyld.h>
#include <uuid/uuid.h>
#include <dlib/dstrings.h>

namespace dmCrash
{
    static void WalkMachCommands(AppState* state, uint32_t mod, const struct mach_header* hdr)
    {
        // Commands start after the header,
        // which is really a mach_header_64
        // on 64-bit machines.
        uintptr_t cmd_addr = (uintptr_t)hdr + sizeof(struct mach_header_64);
        struct load_command* cmd = (struct load_command*)cmd_addr;

        uintptr_t min_addr = UINTPTR_MAX;
        uintptr_t max_addr = 0;
        for (uint32_t i = 0; i < hdr->ncmds; i++, cmd_addr += cmd->cmdsize)
        {
            cmd = (struct load_command*)cmd_addr;
            if (cmd->cmd == LC_UUID)
            {
                struct uuid_command* uuid = (struct uuid_command*)cmd;
                uuid_unparse_upper(uuid->uuid, state->m_ModuleBuildID[mod]);
            }
            else if (cmd->cmd == LC_SEGMENT_64)
            {
                struct segment_command_64* seg = (struct segment_command_64*)cmd;
                if (strcmp(seg->segname, "__TEXT") == 0)
                {
                    state->m_ModuleSize[mod] = (uint32_t)seg->vmsize;
                }
            }
        }
    }

    void SetLoadAddrs(AppState* state)
    {
        // Structure is all 0 to start with, so just fill everything in.
        state->m_ModuleCount = 0;
        for (uint32_t i = 0; i != AppState::MODULES_MAX; i++)
        {
            const struct mach_header* hdr = _dyld_get_image_header(i);
            if (!hdr)
            {
                break;
            }

            const char* name = _dyld_get_image_name(i);
            const char* p = strrchr(name, '/');
            dmStrlCpy(state->m_ModuleName[i], p ? (p + 1) : name, sizeof(state->m_ModuleName));
            state->m_ModuleAddr[i] = (void*)hdr;
            WalkMachCommands(state, i, hdr);
            state->m_ModuleCount++;
        }
    }
} // namespace dmCrash
