#include "crash.h"
#include "crash_private.h"
#include <dlib/math.h>

#include <windows.h>

namespace dmCrash
{

    void OnCrash()
    {
        // The API only accepts 62 or less
        uint32_t max = dmMath::Min(AppState::PTRS_MAX, 62);
        g_AppState.m_PtrCount = CaptureStackBackTrace(0, max, &g_AppState.m_Ptr[0], 0);
        WriteCrash(g_FilePath, &g_AppState);
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
