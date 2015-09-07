#include "crash.h"
#include "crash_private.h"

#include <windows.h>

namespace dmCrash
{

    void OnCrash()
    {
        uint32_t max = AppState::PTRS_MAX;

        // The API only accepts 62 or less
        if (max > 62)
        {
            max = 62;
        }

        g_AppState.m_PtrCount = CaptureStackBackTrace(0, max, &g_AppState.m_Ptr[0], 0);
        WriteCrash(g_FilePath, &g_AppState, 0);
    }
    
    void WriteDump()
    {
        // The test write signum
        g_AppState.m_Signum = 0xDEAD;
        OnCrash();
    }

    LONG WINAPI OnCrash(EXCEPTION_POINTERS *ptr)
    {
        WriteDump();
        return EXCEPTION_CONTINUE_SEARCH;
    }

    void InstallHandler()
    {
        ::SetUnhandledExceptionFilter(OnCrash);
    }
}
