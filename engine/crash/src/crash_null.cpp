#include <stdio.h>
#include "crash.h"
#include "crash_private.h"

namespace dmCrash
{
    // A bit silly that we need these for a null implementation...
    AppState g_AppState;
    AppState g_PreviousAppState;
    char g_FilePath[AppState::FILEPATH_MAX];


    bool IsInitialized()
    {
        return false;
    }

    void Init(const char* version, const char* hash)
    {
        (void)version;
        (void)hash;
    }

    void SetFilePath(const char* filepath)
    {
        (void)filepath;
    }

    Result SetUserField(uint32_t index, const char* value)
    {
        (void)index;
        (void)value;
        return RESULT_OK;
    }

    HDump LoadPreviousPath(const char* where)
    {
        (void)where;
        return 0;
    }

    HDump LoadPrevious()
    {
        return 0;
    }

    void Release(HDump dump)
    {
        (void)dump;
    }

    AppState* Check(HDump dump)
    {
        (void)dump;
        return 0;
    }

    bool IsValidHandle(HDump dump)
    {
        (void)dump;
        return false;
    }

    const char* GetExtraData(HDump dump)
    {
        (void)dump;
        return 0;
    }

    int GetSignum(HDump dump)
    {
        (void)dump;
        return 0;
    }

    const char* GetSysField(HDump dump, SysField fld)
    {
        (void)dump;
        (void)fld;
        return 0;
    }

    const char* GetUserField(HDump dump, uint32_t index)
    {
        (void)dump;
        (void)index;
        return 0;
    }

    void Purge()
    {
    }

    uint32_t GetBacktraceAddrCount(HDump dump)
    {
        (void)dump;
        return 0;
    }

    void* GetBacktraceAddr(HDump dump, uint32_t index)
    {
        (void)dump;
        (void)index;
        return 0;
    }

    const char* GetModuleName(HDump dump, uint32_t index)
    {
        (void)dump;
        (void)index;
        return 0;
    }

    void* GetModuleAddr(HDump dump, uint32_t index)
    {
        (void)dump;
        (void)index;
        return 0;
    }
}

