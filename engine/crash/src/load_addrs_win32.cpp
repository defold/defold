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

#include <dlib/log.h>
#include <windows.h>
#include <psapi.h>

namespace dmCrash
{
    void SetLoadAddrs(AppState* state)
    {
        HANDLE process = GetCurrentProcess();
        HMODULE mods[AppState::MODULES_MAX];
        DWORD needed;

        if (EnumProcessModules(process, mods, sizeof(mods), &needed))
        {
            uint32_t count = needed / sizeof(HANDLE);
            state->m_ModuleCount = count;
            for (uint32_t i=0;i!=count;i++)
            {
                if (!GetModuleFileNameExA(process, mods[i], state->m_ModuleName[i], AppState::MODULE_NAME_SIZE))
                {
                    dmLogWarning("Failed to read name of module");
                }
                MODULEINFO info;
                if (GetModuleInformation(process, mods[i], &info, sizeof(info)))
                {
                    state->m_ModuleAddr[i] = info.lpBaseOfDll;
                    state->m_ModuleSize[i] = info.SizeOfImage;
                }
            }
        }
        else
        {
            dmLogWarning("Failed to enumerate process modules");
        }
    }
}
