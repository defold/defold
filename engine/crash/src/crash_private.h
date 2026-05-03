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

#ifndef __CRASH_PRIVATE_H__
#define __CRASH_PRIVATE_H__

#include <stdint.h>
#include <string.h>
#include "crash.h"

namespace dmCrash
{
    struct AppStateHeader
    {
        uint32_t m_Version;
        uint32_t m_StructSize;

        AppStateHeader()
        {
            memset(this, 0x0, sizeof(*this));
        }
    };

    struct AppStateV2
    {
        static const uint32_t VERSION          = 2;
        static const uint32_t MODULES_MAX      = 128;
        static const uint32_t MODULE_NAME_SIZE = 64;
        static const uint32_t PTRS_MAX         = 64;
        static const uint32_t USERDATA_SLOTS   = 32;
        static const uint32_t USERDATA_SIZE    = 256;
        static const uint32_t EXTRA_MAX        = 32768;
        static const uint32_t FILEPATH_MAX     = 1024;

        // Version of app (defold)
        char m_EngineVersion[32];
        char m_EngineHash[128];

        // Mirrors SystemInfo from dlib
        char m_DeviceModel[32];
        char m_Manufacturer[32];
        char m_SystemName[32];
        char m_SystemVersion[32];
        char m_Language[8];
        char m_DeviceLanguage[16];
        char m_Territory[8];
        char m_AndroidBuildFingerprint[128];

        // Version 2 fields
        char m_UserData[USERDATA_SLOTS][USERDATA_SIZE];
        char m_ModuleName[MODULES_MAX][MODULE_NAME_SIZE];
        void* m_ModuleAddr[MODULES_MAX];
        uint32_t m_Signum;
        uint32_t m_PtrCount;
        void* m_Ptr[PTRS_MAX];
        char m_Extra[EXTRA_MAX];

        AppStateV2()
        {
            memset(this, 0x0, sizeof(*this));
        }
    };

    struct AppStateV3
    {
        static const uint32_t VERSION          = 3;
        static const uint32_t MODULES_MAX      = 128;
        static const uint32_t MODULE_NAME_SIZE = 64;
        static const uint32_t PTRS_MAX         = 64;
        static const uint32_t USERDATA_SLOTS   = 32;
        static const uint32_t USERDATA_SIZE    = 256;
        static const uint32_t EXTRA_MAX        = 32768;
        static const uint32_t FILEPATH_MAX     = 1024;

        // Version of app (defold)
        char m_EngineVersion[32];
        char m_EngineHash[128];

        // Mirrors SystemInfo from dlib
        char m_DeviceModel[32];
        char m_Manufacturer[32];
        char m_SystemName[32];
        char m_SystemVersion[32];
        char m_Language[8];
        char m_DeviceLanguage[16];
        char m_Territory[8];
        char m_AndroidBuildFingerprint[128];

        // V2
        char m_UserData[USERDATA_SLOTS][USERDATA_SIZE];
        char m_ModuleName[MODULES_MAX][MODULE_NAME_SIZE];
        void* m_ModuleAddr[MODULES_MAX];
        uint32_t m_Signum;
        uint32_t m_PtrCount;
        void* m_Ptr[PTRS_MAX];
        char m_Extra[EXTRA_MAX];

        // V3
        uint32_t            m_ModuleSize[MODULES_MAX];
        uint8_t             m_PtrModuleIndex[PTRS_MAX]; // Index into the list of modules
        uint32_t            m_ModuleCount;

        AppStateV3()
        {
            memset(this, 0x0, sizeof(*this));
        }
    };

    typedef AppStateV3 AppState;

    void InstallHandler();
    void EnableHandler(bool enable);
    void WriteCrash(const char* file_name, AppState* data);
    void SetLoadAddrs(AppState* state);
    void SetCrashFilename(const char* file_name);
    void PlatformPurge();
    void HandlerSetExtraInfoCallback(FCallstackExtraInfoCallback cbk, void* ctx);

    void LogCallstack(char* extras); // split the extras into separate lines and logs them

    const char* GetFilePath();
    AppState*   GetAppState();
}

#endif
