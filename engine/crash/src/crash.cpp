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

#include <dlib/dlib.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "crash.h"
#include "crash_private.h"

namespace dmCrash
{
    AppState g_AppState;
    AppState g_PreviousAppState;

    // The default path is where the dumps are stored before the application configures a different path.
    static char g_FilePathDefault[AppState::FILEPATH_MAX];
    static bool g_CrashDumpEnabled = true;

    void SetExtraInfoCallback(FCallstackExtraInfoCallback cbk);
    char g_FilePath[AppState::FILEPATH_MAX];

    bool IsInitialized()
    {
        return g_AppState.m_EngineVersion[0] != 0x0;
    }

    void Init(const char *version, const char *hash)
    {
        memset(g_FilePath, 0x0, sizeof(g_FilePath));
        memset(&g_AppState, 0x0, sizeof(g_AppState));

        // Construct a file path with the app name 'Defold' until it is modified by the application;
        // this also means crash files can be stored in two different places. At the default path or in the
        // application-specific location. So need to look for both when loading.
        dmSys::Result r = dmSys::GetApplicationSupportPath(DMSYS_APPLICATION_NAME, g_FilePathDefault, sizeof(g_FilePathDefault));
        if (r == dmSys::RESULT_OK)
        {
            dmStrlCat(g_FilePathDefault, "/", sizeof(g_FilePathDefault));
            dmStrlCat(g_FilePathDefault, "_crash", sizeof(g_FilePathDefault));

            dmStrlCpy(g_FilePath, g_FilePathDefault, sizeof(g_FilePath));

            dmSys::SystemInfo info;
            dmSys::GetSystemInfo(&info);

            #define COPY_FIELD(x) dmStrlCpy(g_AppState.x, info.x, sizeof(g_AppState.x))
            COPY_FIELD(m_DeviceModel);
            COPY_FIELD(m_Manufacturer);
            COPY_FIELD(m_SystemName);
            COPY_FIELD(m_SystemVersion);
            COPY_FIELD(m_Language);
            COPY_FIELD(m_DeviceLanguage);
            COPY_FIELD(m_Territory);
            #undef COPY_FIELD

            dmStrlCpy(g_AppState.m_EngineVersion, version, sizeof(g_AppState.m_EngineVersion));
            dmStrlCpy(g_AppState.m_EngineHash, hash, sizeof(g_AppState.m_EngineHash));

            SetLoadAddrs(&g_AppState);

            SetCrashFilename(g_FilePath);

            InstallHandler();
            EnableHandler(g_CrashDumpEnabled);
        }
    }

    bool IsEnabled()
    {
        return g_CrashDumpEnabled;
    }

    void SetEnabled(bool enable)
    {
        g_CrashDumpEnabled = enable;
        EnableHandler(enable);
    }

    void SetExtraInfoCallback(FCallstackExtraInfoCallback cbk, void* ctx)
    {
        HandlerSetExtraInfoCallback(cbk, ctx);
    }

    void SetFilePath(const char *filepath)
    {
        dmStrlCpy(g_FilePath, filepath, sizeof(g_FilePath));

        SetCrashFilename(g_FilePath);
    }

    const char* GetFilePath()
    {
        return g_FilePath;
    }

    AppState* GetAppState()
    {
        return &g_AppState;
    }

    Result SetUserField(uint32_t index, const char *value)
    {
        if (index < AppState::USERDATA_SLOTS)
        {
            dmStrlCpy(g_AppState.m_UserData[index], value, sizeof(g_AppState.m_UserData[index]));
            return RESULT_OK;
        }

        return RESULT_INVALID_PARAM;
    }

    static HDump LoadDumpV2(AppState* state, FILE* f)
    {
        size_t nread = fread(state, 1, sizeof(AppStateV2), f);
        if (nread != sizeof(AppStateV2))
        {
            dmLogError("%s: Crashdump is incomplete", __FUNCTION__);
            return 0;
        }

        // Fixup data from V2 to Latest

        return 1;
    }

    static HDump LoadDumpV3(AppState* state, FILE* f)
    {
        if (fread(state, 1, sizeof(AppStateV3), f) != sizeof(AppStateV3))
        {
            dmLogError("%s: Crashdump is incomplete", __FUNCTION__);
            return 0;
        }
        return 1;
    }

    static HDump LoadPrevious(FILE* f)
    {
        AppStateHeader header;
        memset(&header, 0x0, sizeof(AppStateHeader));

        if (fread(&header, 1, sizeof(AppStateHeader), f) == sizeof(AppStateHeader))
        {
            memset(&g_PreviousAppState, 0, sizeof(AppState));

            HDump result = 0;
            if (header.m_Version == AppStateV3::VERSION && header.m_StructSize == sizeof(AppStateV3))
            {
                result = LoadDumpV3(&g_PreviousAppState, f);
            }
            else if (header.m_Version == AppStateV2::VERSION && header.m_StructSize == sizeof(AppStateV2))
            {
                result = LoadDumpV2(&g_PreviousAppState, f);
            }
            else
            {
                dmLogError("Crashdump version or format does not match: Crash version: %d.%d  Tool Version: %d.%d", header.m_Version, (int)header.m_StructSize, AppState::VERSION, (int)sizeof(AppState));
            }

            return result;
        }
        else
        {
            dmLogError("Crashdump does not contain a valid header.");
        }

        return 0;
    }

    HDump LoadPreviousPath(const char* path)
    {
        HDump ret = 0;
        FILE* fhandle = fopen(path, "rb");
        if (fhandle)
        {
            ret = LoadPrevious(fhandle);
            fclose(fhandle);
        }

        return ret;
    }

    // Load previous dump (if exists)
    HDump LoadPrevious()
    {
        HDump dump = LoadPreviousPath(g_FilePathDefault);
        if (dump != 0)
        {
            return dump;
        }

        return LoadPreviousPath(g_FilePath);
    }

    void Release(HDump dump)
    {
        if (dump == 1)
        {
            memset(&g_PreviousAppState, 0x0, sizeof(AppState));
        }
    }

    static AppState* Check(HDump dump)
    {
        if (dump == 0)
            return 0;
        return &g_PreviousAppState;
    }

    #define CHECK_STATE_RET(_STATE, _RET) \
        AppState* state = Check(dump); \
        if (!state) return (_RET);

    bool IsValidHandle(HDump dump)
    {
        CHECK_STATE_RET(dump, 0);
        return state != 0;
    }

    const char* GetExtraData(HDump dump)
    {
        CHECK_STATE_RET(dump, 0);
        return state->m_Extra;
    }

    int GetSignum(HDump dump)
    {
        CHECK_STATE_RET(dump, 0);
        return state->m_Signum;
    }

    const char* GetSysField(HDump dump, SysField fld)
    {
        CHECK_STATE_RET(dump, 0);

        // Cannot trust anything in the dump so terminate before returning.
        #define TERM_RET(x) x[sizeof(x)-1] = 0; return x;

        switch (fld)
        {
            case SYSFIELD_ENGINE_VERSION:            TERM_RET(state->m_EngineVersion);
            case SYSFIELD_ENGINE_HASH:               TERM_RET(state->m_EngineHash);
            case SYSFIELD_DEVICE_MODEL:              TERM_RET(state->m_DeviceModel);
            case SYSFIELD_MANUFACTURER:              TERM_RET(state->m_Manufacturer);
            case SYSFIELD_SYSTEM_NAME:               TERM_RET(state->m_SystemName);
            case SYSFIELD_SYSTEM_VERSION:            TERM_RET(state->m_SystemVersion);
            case SYSFIELD_LANGUAGE:                  TERM_RET(state->m_Language);
            case SYSFIELD_DEVICE_LANGUAGE:           TERM_RET(state->m_DeviceLanguage);
            case SYSFIELD_TERRITORY:                 TERM_RET(state->m_Territory);
            case SYSFIELD_ANDROID_BUILD_FINGERPRINT: TERM_RET(state->m_AndroidBuildFingerprint);
            default:                                 return 0;
        }
        return 0;
    }

    const char* GetUserField(HDump dump, uint32_t index)
    {
        CHECK_STATE_RET(dump, 0);
        if (index < AppState::USERDATA_SLOTS)
        {
            state->m_UserData[index][AppState::USERDATA_SIZE - 1] = 0;
            return state->m_UserData[index];
        }
        return 0;
    }

    void Purge()
    {
        dmSys::Unlink(g_FilePath);
        dmSys::Unlink(g_FilePathDefault);

        PlatformPurge();
    }

    uint32_t GetBacktraceAddrCount(HDump dump)
    {
        CHECK_STATE_RET(dump, 0);
        return dmMath::Min(AppState::PTRS_MAX, state->m_PtrCount);
    }

    void* GetBacktraceAddr(HDump dump, uint32_t index)
    {
        CHECK_STATE_RET(dump, 0);
        if (index < dmMath::Min(AppState::PTRS_MAX, state->m_PtrCount))
        {
            return state->m_Ptr[index];
        }
        return 0;
    }

    uint32_t GetBacktraceModuleIndex(HDump dump, uint32_t index)
    {
        CHECK_STATE_RET(dump, 0);
        if (index < dmMath::Min(AppState::PTRS_MAX, state->m_PtrCount))
        {
            return state->m_PtrModuleIndex[index];
        }
        return 0;
    }

    const char *GetModuleName(HDump dump, uint32_t index)
    {
        CHECK_STATE_RET(dump, 0);
        if (index < AppState::MODULES_MAX)
        {
            if (state->m_ModuleName[index][0])
            {
                char* field = state->m_ModuleName[index];
                field[AppState::MODULE_NAME_SIZE - 1] = 0;
                return field;
            }
        }
        return 0;
    }

    void* GetModuleAddr(HDump dump, uint32_t index)
    {
        CHECK_STATE_RET(dump, 0);
        if (index < AppState::MODULES_MAX)
        {
            return state->m_ModuleAddr[index];
        }
        return 0;
    }

    uint32_t GetModuleSize(HDump dump, uint32_t index)
    {
        CHECK_STATE_RET(dump, 0);
        if (index < AppState::MODULES_MAX)
        {
            return state->m_ModuleSize[index];
        }
        return 0;
    }

    void LogCallstack(char* extras)
    {
        bool is_debug_mode = dLib::IsDebugMode();
        dLib::SetDebugMode(true);
        dmLogError("CALL STACK:\n\n");

        char* p = extras;
        char* end = extras + strlen(extras);
        while( p < end )
        {
            char* lineend = strchr(p, '\n');
            if (!lineend)
                lineend = strchr(p, '\r');
            if (lineend && lineend < end)
                *lineend = 0;

            dmLogError("%s", p);

            if (!lineend)
                break;

            p = lineend+1;
        }

        dmLogError("\n");
        dLib::SetDebugMode(is_debug_mode);
    }

}
