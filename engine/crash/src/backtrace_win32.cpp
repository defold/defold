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

        HANDLE process = ::GetCurrentProcess();

        ::SymSetOptions(SYMOPT_DEBUG);
        ::SymInitialize(process, 0, TRUE);

        // The API only accepts 62 or less
        uint32_t max = dmMath::Min(AppState::PTRS_MAX, (uint32_t)62);
        g_AppState.m_PtrCount = CaptureStackBackTrace(0, max, &g_AppState.m_Ptr[0], 0);

        // Get a nicer printout as well
        const int name_length = 1024;


        char symbolbuffer[sizeof(SYMBOL_INFO) + name_length * sizeof(char)*2];
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbolbuffer;
        symbol->MaxNameLen = name_length;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

        DWORD displacement;
        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        uint32_t offset = 0;
        for (uint32_t i = 0; i < g_AppState.m_PtrCount; ++i)
        {
            DWORD64 address = (DWORD64)(g_AppState.m_Ptr[i]);

            const char* symbolname = "<unknown symbol>";
            DWORD64 symboladdress = address;

            DWORD64 symoffset = 0;

            if (::SymFromAddr(process, address, &symoffset, symbol))
            {
                symbolname = symbol->Name;
                symboladdress = symbol->Address;
            }

            const char* filename = "<unknown>";
            int line_number = 0;
            if (::SymGetLineFromAddr64(process, address, &displacement, &line))
            {
                filename = line.FileName;
                line_number = line.LineNumber;
            }

            if (offset < (dmCrash::AppState::EXTRA_MAX - 1))
            {
                uint32_t size_left = dmCrash::AppState::EXTRA_MAX - offset;
                offset += DM_SNPRINTF(&g_AppState.m_Extra[offset], size_left, "%d 0x%0llX %s %s:%d\n", i, symboladdress, symbolname, filename, line_number);
            }
        }
        g_AppState.m_Extra[dmCrash::AppState::EXTRA_MAX - 1] = 0;

        ::SymCleanup(process);

        WriteCrash(g_FilePath, &g_AppState);

        printf("\n%s\n", g_AppState.m_Extra);
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
