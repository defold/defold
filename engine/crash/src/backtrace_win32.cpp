// Copyright 2020-2024 The Defold Foundation
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

#include "crash.h"
#include "crash_private.h"
#include <dlib/dlib.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/sys.h>

#include <stdio.h>
#include <Windows.h>
#include <Dbghelp.h>
#include <signal.h>

namespace dmCrash
{
    static char g_MiniDumpPath[AppState::FILEPATH_MAX];
    static bool g_CrashDumpEnabled = true;
    static FCallstackExtraInfoCallback  g_CrashExtraInfoCallback = 0;
    static void*                        g_CrashExtraInfoCallbackCtx = 0;

    static void WriteMiniDump( const char* path, EXCEPTION_POINTERS* pep )
    {
        fflush(stdout);
        bool is_debug_mode = dLib::IsDebugMode();
        dLib::SetDebugMode(true);

        HANDLE hFile = CreateFileA( path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

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
                dmLogError("MiniDumpWriteDump failed. Error: %u \n", GetLastError() );
            }
            else
            {
                dmLogInfo("Successfully wrote MiniDump to file: %s", path);
            }

            CloseHandle( hFile );
        }
        else
        {
            dmLogError("CreateFile for MiniDump failed: %s. Error: %u \n", path, GetLastError() );
        }
        dLib::SetDebugMode(is_debug_mode);
    }

    void EnableHandler(bool enable)
    {
        g_CrashDumpEnabled = enable;
    }

    void HandlerSetExtraInfoCallback(FCallstackExtraInfoCallback cbk, void* ctx)
    {
        g_CrashExtraInfoCallback = cbk;
        g_CrashExtraInfoCallbackCtx = ctx;
    }

    static uint32_t GetCallstackPointers(EXCEPTION_POINTERS* pep, void** ptrs, uint32_t num_ptrs)
    {
        if (!pep)
        {
            // The API only accepts 62 or less
            uint32_t max = dmMath::Min(num_ptrs, (uint32_t)62);
            num_ptrs = CaptureStackBackTrace(0, max, ptrs, 0);
        } else
        {
            HANDLE process = ::GetCurrentProcess();
            HANDLE thread = ::GetCurrentThread();
            uint32_t count = 0;

            // Initialize stack walking.
            STACKFRAME64 stack_frame;
            memset(&stack_frame, 0, sizeof(stack_frame));
            #if defined(_WIN64)
                int machine_type = IMAGE_FILE_MACHINE_AMD64;
                stack_frame.AddrPC.Offset = pep->ContextRecord->Rip;
                stack_frame.AddrFrame.Offset = pep->ContextRecord->Rbp;
                stack_frame.AddrStack.Offset = pep->ContextRecord->Rsp;
            #else
                int machine_type = IMAGE_FILE_MACHINE_I386;
                stack_frame.AddrPC.Offset = pep->ContextRecord->Eip;
                stack_frame.AddrFrame.Offset = pep->ContextRecord->Ebp;
                stack_frame.AddrStack.Offset = pep->ContextRecord->Esp;
            #endif
            stack_frame.AddrPC.Mode = AddrModeFlat;
            stack_frame.AddrFrame.Mode = AddrModeFlat;
            stack_frame.AddrStack.Mode = AddrModeFlat;
            while (StackWalk64(machine_type,
                process,
                thread,
                &stack_frame,
                pep->ContextRecord,
                NULL,
                &SymFunctionTableAccess64,
                &SymGetModuleBase64,
                NULL) &&
                count < num_ptrs) {
                ptrs[count++] = reinterpret_cast<void*>(stack_frame.AddrPC.Offset);
            }
            num_ptrs = count;
        }
        return num_ptrs;
    }

    static void GenerateCallstack(EXCEPTION_POINTERS* pep)
    {
        if (!g_CrashDumpEnabled)
            return;

        fflush(stdout);
        fflush(stderr);

        HANDLE process = ::GetCurrentProcess();

        ::SymSetOptions(SYMOPT_DEBUG);
        ::SymInitialize(process, 0, TRUE);

        g_AppState.m_PtrCount = GetCallstackPointers(pep, &g_AppState.m_Ptr[0], AppState::PTRS_MAX);

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
                offset += dmSnPrintf(&g_AppState.m_Extra[offset], size_left, "%2d 0x%0llX %s %s:%d\n", i, symboladdress, symbolname, filename, line_number);
            }
        }
        g_AppState.m_Extra[dmCrash::AppState::EXTRA_MAX - 1] = 0;

        ::SymCleanup(process);

        if (g_CrashExtraInfoCallback)
        {
            int extra_len = strlen(g_AppState.m_Extra);
            g_CrashExtraInfoCallback(g_CrashExtraInfoCallbackCtx, g_AppState.m_Extra + extra_len, dmCrash::AppState::EXTRA_MAX - extra_len - 1);
        }

        WriteCrash(g_FilePath, &g_AppState);

        LogCallstack(g_AppState.m_Extra);
    }

    void WriteDump()
    {
        // The test write signum
        g_AppState.m_Signum = 0xDEAD;
        GenerateCallstack(0);
    }

    LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS *ptr)
    {
        g_AppState.m_Signum = 0xDEAD;
        GenerateCallstack(ptr);
        WriteMiniDump(g_MiniDumpPath, ptr);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    void SetCrashFilename(const char* filename)
    {
        dmStrlCpy(g_MiniDumpPath, filename, sizeof(g_MiniDumpPath));
        dmStrlCat(g_MiniDumpPath, ".dmp", sizeof(g_MiniDumpPath));
    }

    static void SignalHandler(int signum)
    {
        // The previous (default) behavior is restored for the signal.
        // Unless this is done first thing in the signal handler we'll
        // be stuck in a signal-handler loop forever.
        signal(signum, 0);
        GenerateCallstack(0);
    }

    static void InstallOnSignal(int signum)
    {
        signal(signum, SignalHandler);
    }

    void InstallHandler()
    {
        ::SetUnhandledExceptionFilter(ExceptionHandler);

        InstallOnSignal(SIGABRT);
        InstallOnSignal(SIGINT);
        InstallOnSignal(SIGTERM);
        InstallOnSignal(SIGSEGV);
    }

    void PlatformPurge()
    {
        dmSys::Unlink(g_MiniDumpPath);
    }
}
