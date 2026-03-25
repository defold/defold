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

#include <link.h>
#include <dlib/align.h>
#include <dlib/dstrings.h>
#include "crash_private.h"

#define NT_GNU_BUILD_ID 3

namespace dmCrash
{
    static const char* hexdigit = "0123456789ABCDEF";

    static bool  IsBuildID(ElfW(Nhdr) * note)
    {
        return (note->n_type == NT_GNU_BUILD_ID &&
                note->n_descsz != 0 &&
                note->n_namesz == 4 &&
                memcmp((char*)(note + 1), "GNU", 4) == 0);
    }

    static void CopyBuildIDNote(char* id, struct dl_phdr_info* info, uint32_t id_len)
    {
        for (size_t i = 0; i < info->dlpi_phnum; i++)
        {
            if (info->dlpi_phdr[i].p_type != PT_NOTE)
            {
                continue;
            }
            uintptr_t note_addr = (uintptr_t)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
            uintptr_t notes_end = note_addr + (uintptr_t)info->dlpi_phdr[i].p_memsz;
            while (note_addr < notes_end)
            {
                ElfW(Nhdr)* note = (ElfW(Nhdr)*)(note_addr);
                if (IsBuildID(note))
                {
                    uint8_t* build_id = (uint8_t*)(note+1) + DM_ALIGN(note->n_namesz, 4);
                    uint32_t len = (uint32_t)(note->n_descsz);
                    if (len >= id_len / 2)
                    {
                        len = id_len / 2 - 1;
                    }
                    for (uint32_t i = 0; i < len; i++)
                    {
                        uint8_t b = build_id[i];
                        id[i * 2] = hexdigit[b >> 4];
                        id[i * 2 + 1] = hexdigit[b & 0xf];
                    }
                    id[len*2] = 0;
                    return;
                }
                note_addr += (sizeof(*note) +
                         DM_ALIGN(note->n_namesz, 4) +
                         DM_ALIGN(note->n_descsz, 4));
            }
        }
    }

    static int EachModule(struct dl_phdr_info* info, size_t size, void* data)
    {
        AppState* state = (AppState*)data;
        uint32_t  count = state->m_ModuleCount;
        if (count == AppState::MODULES_MAX)
        {
            return 0;
        }
        state->m_ModuleCount = count + 1;
        dmStrlCpy(state->m_ModuleName[count], info->dlpi_name, AppState::MODULE_NAME_SIZE);
        if (state->m_ModuleName[count][0] == 0)
        {
            // The main program has an empty name.
            dmStrlCpy(state->m_ModuleName[count], "dmengine", AppState::MODULE_NAME_SIZE);
        }

        uintptr_t min_addr = UINTPTR_MAX;
        uintptr_t max_addr = 0;
        for (ElfW(Half) i = 0; i < info->dlpi_phnum; i++)
        {
            const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
            if (ph->p_type != PT_LOAD)
                continue;
            uintptr_t seg_start = (uintptr_t)(info->dlpi_addr + ph->p_vaddr);
            uintptr_t seg_end = seg_start + (uintptr_t)(ph->p_memsz);
            if (seg_start != 0 && seg_start < min_addr)
                min_addr = seg_start;
            if (seg_end > max_addr)
                max_addr = seg_end;
        }
        if (info->dlpi_addr != 0)
            state->m_ModuleAddr[count] = (void*)info->dlpi_addr;
        else
            state->m_ModuleAddr[count] = (void*)min_addr;
        state->m_ModuleSize[count] = (max_addr > min_addr) ? (max_addr - min_addr) : 0;
        CopyBuildIDNote(state->m_ModuleBuildID[count], info, AppState::MODULE_BUILD_ID_SIZE);

        return 0;
    }

    void SetLoadAddrs(AppState* state)
    {
        dl_iterate_phdr(EachModule, (void*)state);
    }
} // namespace dmCrash
