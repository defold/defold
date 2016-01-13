#ifndef __CRASH_PRIVATE_H__
#define __CRASH_PRIVATE_H__

#include <stdint.h>
#include <string.h>

namespace dmCrash
{
    // Version 1 of the dump file format
    struct AppStateV1
    {
        static const uint32_t MODULES_MAX = 128;
        static const uint32_t MODULE_NAME_SIZE = 64;
        static const uint32_t PTRS_MAX = 64;
        static const uint32_t USERDATA_SLOTS = 32;
        static const uint32_t USERDATA_SIZE = 256;
        static const uint32_t EXTRA_MAX = 4096;

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

        char m_UserData[USERDATA_SLOTS][USERDATA_SIZE];
        char m_ModuleName[MODULES_MAX][MODULE_NAME_SIZE];
        void* m_ModuleAddr[MODULES_MAX];
        uint32_t m_Signum;
        uint32_t m_PtrCount;
        void* m_Ptr[PTRS_MAX];
        char m_Extra[EXTRA_MAX];

        AppStateV1()
        {
            memset(this, 0x00, sizeof(*this));
        }
    };

    typedef AppStateV1 AppState;

    void InstallHandler();
    void WriteCrash(const char* file_name, AppState *data, void (*fn)(int));
    void SetLoadAddrs(AppState *state);
    extern AppState g_AppState;


    static const uint32_t FILEPATH_MAX = 1024;
    extern char g_FilePath[FILEPATH_MAX];
}

#endif
