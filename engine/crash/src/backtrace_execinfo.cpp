// Copyright 2020-2026 The Defold Foundation
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

#include <execinfo.h>
#include <signal.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/dstrings.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "crash.h"
#include "crash_private.h"

namespace dmCrash
{
    static const int SIGNAL_MAX = 64;
    static bool g_CrashDumpEnabled = true;
    static FCallstackExtraInfoCallback  g_CrashExtraInfoCallback = 0;
    static void*                        g_CrashExtraInfoCallbackCtx = 0;

    // This array contains the default behavior for each signal.
    static struct sigaction sigdfl[SIGNAL_MAX];

    void EnableHandler(bool enable)
    {
        g_CrashDumpEnabled = enable;
    }

    void HandlerSetExtraInfoCallback(FCallstackExtraInfoCallback cbk, void* ctx)
    {
        g_CrashExtraInfoCallback = cbk;
        g_CrashExtraInfoCallbackCtx = ctx;
    }

    void OnCrash(int signo)
    {
        if (!g_CrashDumpEnabled)
            return;

        fflush(stdout);
        fflush(stderr);

        AppState* state = GetAppState();

        state->m_PtrCount = backtrace(state->m_Ptr, AppState::PTRS_MAX);
        state->m_Signum = signo;

        char** stacktrace = backtrace_symbols(state->m_Ptr, state->m_PtrCount);
        uint32_t offset = 0;
        for (uint32_t i = 0; i < state->m_PtrCount; ++i)
        {
            // Write each symbol on a separate line, just like
            // backgrace_symbols_fd would do.
            uint32_t stacktrace_length = strnlen(stacktrace[i], dmCrash::AppState::EXTRA_MAX - 1);
            if ((offset + stacktrace_length) < (dmCrash::AppState::EXTRA_MAX - 1))
            {
                memcpy(state->m_Extra + offset, stacktrace[i], stacktrace_length);
                state->m_Extra[offset + stacktrace_length] = '\n';
                offset += stacktrace_length + 1;
            }
            else
            {
                dmLogError("Not enough space to write entire stacktrace!");
                break;
            }
        }

        free(stacktrace);

        if (g_CrashExtraInfoCallback)
        {
            int extra_len = strlen(state->m_Extra);
            g_CrashExtraInfoCallback(g_CrashExtraInfoCallbackCtx, state->m_Extra + extra_len, dmCrash::AppState::EXTRA_MAX - extra_len - 1);
        }

        WriteCrash(GetFilePath(), state);

        LogCallstack(state->m_Extra);
    }

    void WriteDump()
    {
        OnCrash(0xDEAD);
    }

    static void Handler(const int signum, siginfo_t *const si, void *const sc)
    {
        // The previous (default) behavior is restored for the signal.
        // Unless this is done first thing in the signal handler we'll
        // be stuck in a signal-handler loop forever.
        sigaction(signum, &sigdfl[signum], NULL);
        OnCrash(signum);
    }

    void InstallOnSignal(int signum)
    {
        assert(signum >= 0 && signum < SIGNAL_MAX);

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = Handler;
        sa.sa_flags = SA_SIGINFO;

        // The current (default) behavior is stored in sigdfl.
        sigaction(signum, &sa, &sigdfl[signum]);
    }

    void SetCrashFilename(const char*)
    {
    }

    void PlatformPurge()
    {
    }

    void InstallHandler()
    {
        InstallOnSignal(SIGSEGV);
        InstallOnSignal(SIGBUS);
        InstallOnSignal(SIGTRAP);
        InstallOnSignal(SIGILL);
        InstallOnSignal(SIGABRT);
    }
}
