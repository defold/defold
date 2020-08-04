// Copyright 2020 The Defold Foundation
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

#ifdef _WIN32

/*
 * NOTE: This function is private in ntdll and changed in window 2000
 * We assume version >= NT5, ie windows 2000.
 */
typedef unsigned long (__stdcall* NtAllocateUuidsProto)(void* time  /* 8 bytes */,
                                                        void* range /* 4 bytes */,
                                                        void* sequence /* 4 bytes */,
                                                        void* seed /* 6 bytes */);
#include "safe_windows.h"

#elif !defined(ANDROID) && !defined(__EMSCRIPTEN__)
#include <uuid/uuid.h>
#endif

#include <stdint.h>
#include <assert.h>
#include "log.h"
#include "uuid.h"

namespace dmUUID
{
#ifdef _WIN32
    void Generate(UUID* uuid)
    {
        static NtAllocateUuidsProto NtAllocateUuids = 0;
        if (!NtAllocateUuids)
        {
            // NOTE: We don't call FreeLibrary as we keep a reference to NtAllocateUuids
            // Yes, this is a potential leak but Loading/Closing the library
            // for every call doesn't seems to be reasonable either
            HMODULE ntdll = LoadLibrary("ntdll.dll");
            assert(ntdll);
            if (!ntdll)
            {
                dmLogFatal("Unable to load ntdll.dll (%x)", GetLastError());
                return;
            }
            NtAllocateUuids = (NtAllocateUuidsProto) GetProcAddress(ntdll, "NtAllocateUuids");
            assert(NtAllocateUuids);
            if (!NtAllocateUuids)
            {
                dmLogFatal("Unable to find function NtAllocateUuids (%x)", GetLastError());
                return;
            }
        }

        static unsigned char seed[6] = { '\0' };
        uint8_t* p = &uuid->m_UUID[0];
        NtAllocateUuids(p, p+8, p+12, &seed[0] );
    }

#elif defined(ANDROID) || defined(__EMSCRIPTEN__)
#include <assert.h>
    void Generate(UUID* uuid)
    {
        assert(false && "NOT SUPPORTED");
    }
#else
    void Generate(UUID* uuid)
    {
        uuid_generate(uuid->m_UUID);
    }
#endif
}
