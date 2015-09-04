#include <signal.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/dstrings.h>
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
    // Currently running app's crash data.
    AppState g_AppState;

    // Data from previously generated crashes.
    bool g_PreviousStateV1Loaded = false;
    AppStateV1 g_PreviousStateV1;
    char g_PreviousExtra[32768];

    static bool g_IsInitialized = false;

    // The default path is where the dumps are stored before the application configures a different path.
    static char g_FilePathDefault[FILEPATH_MAX];
    char g_FilePath[FILEPATH_MAX];
    static const char* g_FileName = "_crash";

    bool IsInitialized()
    {
        return g_IsInitialized;
    }

    void Init(const char *version, const char *hash)
    {
        memset(g_FilePath, 0x00, sizeof(g_FilePath));

        // Construct a file path with the app name 'Defold' until it is modified by the application;
        // this also means crash files can be stored in two different places. At the default path or in the
        // application-specific location. So need to look for both when loading.
        dmSys::Result r = dmSys::GetApplicationSupportPath("Defold", g_FilePathDefault, sizeof(g_FilePathDefault));
        if (r != dmSys::RESULT_OK)
        {
            return;
        }

        dmStrlCat(g_FilePathDefault, "/", sizeof(g_FilePathDefault));
        dmStrlCat(g_FilePathDefault, g_FileName, sizeof(g_FilePathDefault));

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

        InstallHandler();

        g_IsInitialized = true;
    }

    void SetFilePath(const char *fn)
    {
        dmStrlCpy(g_FilePath, fn, sizeof(g_FilePath));
    }

    Result SetUserField(uint32_t index, const char *value)
    {
        if (index < AppState::USERDATA_SLOTS)
        {
            dmStrlCpy(g_AppState.m_UserData[index], value, sizeof(g_AppState.m_UserData[0]));
            return RESULT_OK;
        }
        return RESULT_INVALID_PARAM;
    }

    HDump LoadPrevious(FILE *f)
    {
        struct hdr
        {
            uint32_t version;
            uint32_t struct_size;
        } h;

        int rd = fread(&h, 1, sizeof(hdr), f);
        if (rd != sizeof(hdr))
        {
            dmLogWarning("Dump file does not contain header");
            return 0;
        }

        memset(&g_PreviousStateV1, 0x00, sizeof(g_PreviousStateV1));
        if (h.version == 1 && h.struct_size == sizeof(g_PreviousStateV1))
        {
            if (fread(&g_PreviousStateV1, 1, sizeof(g_PreviousStateV1), f) != sizeof(g_PreviousStateV1))
            {
                dmLogWarning("Dump not complete");
                return 0;
            }

            // Bonus platform specifically formatted data that is just treated as a null terminated
            // text blob; therefore do not read all the way.
            memset(g_PreviousExtra, 0x0, sizeof(g_PreviousExtra));
            fread(&g_PreviousExtra, 1, sizeof(g_PreviousExtra)-1, f);
            g_PreviousStateV1Loaded = true;
            return 1; // See Check()
        }
        else
        {
            dmLogWarning("Version and size unknown %d %d", h.version, h.struct_size);
            return 0;
        }
    }

    void Release(HDump dump)
    {
        if (dump == 1)
        {
            g_PreviousStateV1Loaded = false;
        }
    }

    AppStateV1 *Check(HDump dump)
    {
        if (dump == 1 && g_PreviousStateV1Loaded)
        {
            return &g_PreviousStateV1;
        }
        return 0;
    }

    bool IsValidHandle(HDump dump)
    {
        return Check(dump) != 0;
    }

    const char* GetExtraData(HDump dump)
    {
        // Not stored in this struct, but check so argument is valid.
        AppStateV1 *v1 = Check(dump);
        return v1 ? g_PreviousExtra : 0;
    }

    int GetSignum(HDump dump)
    {
        AppStateV1 *v1 = Check(dump);
        return v1 ? v1->m_Signum : 0;
    }

    const char* GetSysField(HDump dump, SysField fld)
    {
        AppStateV1 *v1 = Check(dump);
        if (v1)
        {
            // Cannot trust anything in the dump so terminate before returning.
            #define TERM_RET(x) \
                x[sizeof(x)-1] = 0; \
                return x;

            switch (fld)
            {
                case SYSFIELD_ENGINE_VERSION:  TERM_RET(v1->m_EngineVersion);
                case SYSFIELD_ENGINE_HASH:     TERM_RET(v1->m_EngineHash);
                case SYSFIELD_DEVICE_MODEL:    TERM_RET(v1->m_DeviceModel);
                case SYSFIELD_MANUFACTURER:    TERM_RET(v1->m_Manufacturer);
                case SYSFIELD_SYSTEM_NAME:     TERM_RET(v1->m_SystemName);
                case SYSFIELD_SYSTEM_VERSION:  TERM_RET(v1->m_SystemVersion);
                case SYSFIELD_LANGUAGE:        TERM_RET(v1->m_Language);
                case SYSFIELD_DEVICE_LANGUAGE: TERM_RET(v1->m_DeviceLanguage);
                case SYSFIELD_TERRITORY:       TERM_RET(v1->m_Territory);
                case SYSFIELD_ANDROID_BUILD_FINGERPRINT: TERM_RET(v1->m_AndroidBuildFingerprint);
                default:
                    return 0;
            }

            #undef TERM_RET
        }
        return 0;
    }

    const char* GetUserField(HDump dump, uint32_t index)
    {
        AppStateV1 *v1 = Check(dump);
        if (v1)
        {
            if (index < AppStateV1::USERDATA_SLOTS)
            {
                v1->m_UserData[index][AppStateV1::USERDATA_SIZE-1] = 0;
                return v1->m_UserData[index];
            }
        }
        return 0;
    }

    HDump LoadPreviousFn(const char *where)
    {
        HDump ret = 0;
        FILE *f = fopen(where, "rb");
        if (f)
        {
            ret = LoadPrevious(f);
            fclose(f);
        }
        return ret;
    }

    // Load previous dump (if exists)
    HDump LoadPrevious()
    {
        HDump d;
        if ((d = LoadPreviousFn(g_FilePathDefault)) != 0)
        {
            return d;
        }
        if ((d = LoadPreviousFn(g_FilePath)) != 0)
        {
            return d;
        }
        return 0;
    }

    void Purge()
    {
        dmSys::Unlink(g_FilePath);
        dmSys::Unlink(g_FilePathDefault);
    }

    uint32_t GetBacktraceAddrCount(HDump dump)
    {
        AppStateV1 *v1 = Check(dump);
        if (v1)
        {
            return (v1->m_PtrCount < AppStateV1::PTRS_MAX) ? v1->m_PtrCount : AppStateV1::PTRS_MAX;
        }
        return 0;
    }

    void* GetBacktraceAddr(HDump dump, uint32_t index)
    {
        AppStateV1 *v1 = Check(dump);
        if (v1)
        {
            return v1->m_Ptr[index];
        }
        return 0;
    }

    const char *GetModuleName(HDump dump, uint32_t index)
    {
        if (index < AppStateV1::MODULES_MAX)
        {
            AppStateV1 *v1 = Check(dump);
            if (v1 && v1->m_ModuleName[index][0])
            {
                char *fld = v1->m_ModuleName[index];
                fld[AppStateV1::MODULE_NAME_SIZE-1] = 0;
                return fld;
            }
        }
        return 0;
    }

    void* GetModuleAddr(HDump dump, uint32_t index)
    {
        if (index < AppStateV1::MODULES_MAX)
        {
            AppStateV1 *v1 = Check(dump);
            if (v1)
            {
                return v1->m_ModuleAddr[index];
            }
        }
        return 0;
    }
}

