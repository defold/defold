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

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <windows.h>
#include <psapi.h>

namespace dmCrash
{
    static void CopyPdbGuid(char* id, HMODULE hModule)
    {
        IMAGE_DOS_HEADER*    dos = (IMAGE_DOS_HEADER*)hModule;
        IMAGE_NT_HEADERS*    nt = (IMAGE_NT_HEADERS*)((BYTE*)hModule + dos->e_lfanew);
        IMAGE_DATA_DIRECTORY debugDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];

        if (debugDir.VirtualAddress == 0 || debugDir.Size == 0)
        {
            return;
        }

        IMAGE_DEBUG_DIRECTORY* debugEntries = (IMAGE_DEBUG_DIRECTORY*)((BYTE*)hModule + debugDir.VirtualAddress);
        uint32_t numEntries = debugDir.Size / sizeof(IMAGE_DEBUG_DIRECTORY);

        for (uint32_t i = 0; i < numEntries; i++)
        {
            if (debugEntries[i].Type == IMAGE_DEBUG_TYPE_CODEVIEW)
            {
                CV_INFO_PDB70* cv = (CV_INFO_PDB70*)((BYTE*)hModule + debugEntries[i].AddressOfRawData);
                if (cv->CvSignature == 0x53445352) // "RSDS"
                {
                    dmSnPrintf(id, AppState::MODULE_BUILD_ID_SIZE,
                               "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                               cv->Signature.Data1, cv->Signature.Data2, cv->Signature.Data3
                               cv->Signature.Data4[0], cv->Signature.Data4[1],
                               cv->Signature.Data4[2], cv->Signature.Data4[3],
                               cv->Signature.Data4[4], cv->Signature.Data4[5],
                               cv->Signature.Data4[6], cv->Signature.Data4[7]);
                    return;
                }
            }
        }
    }

    void SetLoadAddrs(AppState* state)
    {
        HANDLE  process = GetCurrentProcess();
        HMODULE mods[AppState::MODULES_MAX];
        DWORD   needed;

        if (!EnumProcessModules(process, mods, sizeof(mods), &needed))
        {
            dmLogWarning("Failed to enumerate process modules");
            return;
        }

        uint32_t count = needed / sizeof(HANDLE);
        state->m_ModuleCount = count;
        for (uint32_t i = 0; i != count; i++)
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
            CopyPdbGuid(state->m_ModuleBuildID[i], mods[i]);
        }
    }
}
} // namespace dmCrash
