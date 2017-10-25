#include "crash.h"
#include "crash_private.h"
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/sys.h>

#include <stdio.h>
#include <Windows.h>
#include <Dbghelp.h>

namespace dmCrash
{
    static char g_MiniDumpPath[AppState::FILEPATH_MAX];

    static void WriteMiniDump( const char* path, EXCEPTION_POINTERS* pep )
    {
        // Open the file

        HANDLE hFile = CreateFile( path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

        if( ( hFile != NULL ) && ( hFile != INVALID_HANDLE_VALUE ) )
        {
            MINIDUMP_EXCEPTION_INFORMATION mdei;

            mdei.ThreadId           = GetCurrentThreadId();
            mdei.ExceptionPointers  = pep;
            mdei.ClientPointers     = FALSE;

            MINIDUMP_TYPE mdt       = MiniDumpNormal;

            BOOL rv = MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), hFile, mdt, (pep != 0) ? &mdei : 0, 0, 0 );

            if( !rv )
            {
                fprintf(stderr, "MiniDumpWriteDump failed. Error: %u \n", GetLastError() );
            }

            CloseHandle( hFile );
        }
        else
        {
            fprintf(stderr, "CreateFile failed: %s. Error: %u \n", path, GetLastError() );
        }
    }

    void OnCrash()
    {
        fflush(stdout);
        fflush(stderr);

        // The API only accepts 62 or less
        uint32_t max = dmMath::Min(AppState::PTRS_MAX, (uint32_t)62);
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
        WriteMiniDump(g_MiniDumpPath, ptr);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    void SetCrashFilename(const char* filename)
    {
        dmStrlCpy(g_MiniDumpPath, filename, sizeof(g_MiniDumpPath));
        dmStrlCat(g_MiniDumpPath, ".dmp", sizeof(g_MiniDumpPath));
    }

    void InstallHandler()
    {
        ::SetUnhandledExceptionFilter(OnCrash);
    }

    void PlatformPurge()
    {
        dmSys::Unlink(g_MiniDumpPath);
    }
}
