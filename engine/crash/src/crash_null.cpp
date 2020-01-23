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
    // A bit silly that we need these for a null implementation...
    AppState g_AppState;
    AppState g_PreviousAppState;
    static char g_FilePathDefault[AppState::FILEPATH_MAX];
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

    static HDump LoadPrevious(FILE *f)
    {
        (void)f;
        return 0;
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

