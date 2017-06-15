#include <signal.h>
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
        dmSys::Result r = dmSys::GetApplicationSupportPath("Defold", g_FilePathDefault, sizeof(g_FilePathDefault));
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
        }
    }

    void SetFilePath(const char *filepath)
    {
        dmStrlCpy(g_FilePath, filepath, sizeof(g_FilePath));

        SetCrashFilename(g_FilePath);
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

    static HDump LoadPrevious(FILE *f)
    {
        AppStateHeader header;
        memset(&header, 0x0, sizeof(AppStateHeader));
        if (fread(&header, 1, sizeof(AppStateHeader), f) == sizeof(AppStateHeader))
        {
            memset(&g_PreviousAppState, 0x0, sizeof(AppState));
            if (header.version == AppState::VERSION && header.struct_size == sizeof(AppState))
            {
                if (fread(&g_PreviousAppState, 1, sizeof(AppState), f) == sizeof(AppState))
                {
                    return 1;
                }
                else
                {
                    dmLogError("Crashdump is incomplete.");
                }
            }
            else
            {
                dmLogWarning("Crashdump version or format does not match: Crash version: %d.%d  Tool Version: %d.%d", header.version, (int)header.struct_size, AppState::VERSION, (int)sizeof(AppState));
            }
        }
        else
        {
            dmLogError("Crashdump does not contain a valid header.");
        }

        return 0;
    }

    HDump LoadPreviousPath(const char *where)
    {
        HDump ret = 0;
        FILE* fhandle = fopen(where, "rb");
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
        HDump dump = 0;
        if ((dump = LoadPreviousPath(g_FilePathDefault)) != 0)
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

    AppState* Check(HDump dump)
    {
        if (dump == 1 && g_PreviousAppState.m_EngineVersion[0] != 0x0)
        {
            return &g_PreviousAppState;
        }

        return NULL;
    }

    bool IsValidHandle(HDump dump)
    {
        return Check(dump) != NULL;
    }

    const char* GetExtraData(HDump dump)
    {
        AppState* state = Check(dump);
        return state ? state->m_Extra : 0;
    }

    int GetSignum(HDump dump)
    {
        AppState* state = Check(dump);
        return state ? state->m_Signum : 0;
    }

    const char* GetSysField(HDump dump, SysField fld)
    {
        AppState* state = Check(dump);
        if (state != NULL)
        {
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

            #undef TERM_RET
        }

        return 0;
    }

    const char* GetUserField(HDump dump, uint32_t index)
    {
        AppState* state = Check(dump);
        if (state != NULL)
        {
            if (index < AppState::USERDATA_SLOTS)
            {
                state->m_UserData[index][AppState::USERDATA_SIZE - 1] = 0;
                return state->m_UserData[index];
            }
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
        AppState* state = Check(dump);
        if (state != NULL)
        {
            return dmMath::Min(AppState::PTRS_MAX, state->m_PtrCount);
        }

        return 0;
    }

    void* GetBacktraceAddr(HDump dump, uint32_t index)
    {
        AppState* state = Check(dump);
        if (state != NULL)
        {
            if (index < dmMath::Min(AppState::PTRS_MAX, state->m_PtrCount))
            {
                return state->m_Ptr[index];
            }
        }

        return 0;
    }

    const char *GetModuleName(HDump dump, uint32_t index)
    {
        if (index < AppState::MODULES_MAX)
        {
            AppState* state = Check(dump);
            if (state != NULL && state->m_ModuleName[index][0])
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
        if (index < AppState::MODULES_MAX)
        {
            AppState* state = Check(dump);
            if (state != NULL)
            {
                return state->m_ModuleAddr[index];
            }
        }

        return 0;
    }
}

