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

#include <assert.h>
#include <dlib/log.h>
#include <dlib/dlib.h>
#include <signal.h>
#include <stdio.h>
#include <unwind.h>
#include <dlfcn.h>
#include "crash.h"
#include "crash_private.h"

namespace dmCrash
{
    static const int SIGNAL_MAX = 64;

    static bool g_CrashDumpEnabled = true;
    static FCallstackExtraInfoCallback  g_CrashExtraInfoCallback = 0;
    static void*                        g_CrashExtraInfoCallbackCtx = 0;

    struct unwind_data {
      uint32_t offset_extra;
      uint32_t stack_index;
    };

    void EnableHandler(bool enable)
    {
        g_CrashDumpEnabled = enable;
    }

    void HandlerSetExtraInfoCallback(FCallstackExtraInfoCallback cbk, void* ctx)
    {
        g_CrashExtraInfoCallback = cbk;
        g_CrashExtraInfoCallbackCtx = ctx;
    }

    _Unwind_Reason_Code OnFrameEnter(struct _Unwind_Context *context, void *data)
    {
        unwind_data * unwindData = (unwind_data *) data;
        const uintptr_t pc = _Unwind_GetIP(context);

        AppState* state = GetAppState();
        if (pc)
        {
            state->m_Ptr[state->m_PtrCount] = (void*)(uintptr_t)pc;
            state->m_PtrCount++;
            Dl_info dl_info;
            int result = dladdr((void *) pc, &dl_info);
            if (result) {
                const char* proc_path = dl_info.dli_fname;
                bool proc_path_truncated = false;
                if (proc_path) {
                    int proc_path_len = strlen(proc_path);
                    if (proc_path_len > 32) {
                        proc_path = proc_path + (proc_path_len-32);
                        proc_path_truncated = true;
                    }
                }
                char extra[256];
                snprintf(extra, sizeof(extra),
                    "#%02d pc %012p %s%s %s+%u",
                    unwindData->stack_index++,
                    (void*)((uintptr_t)pc - (intptr_t) dl_info.dli_fbase),
                    proc_path_truncated ? "..." : "",
                    proc_path ? proc_path : "",
                    dl_info.dli_sname ? dl_info.dli_sname : "<unknown>",
                    dl_info.dli_saddr != NULL ? (void*)((intptr_t) pc - (intptr_t) dl_info.dli_saddr) : 0);
                
                int extra_len = strlen(extra);
                if ((unwindData->offset_extra + extra_len) < (dmCrash::AppState::EXTRA_MAX - 1))
                {
                    memcpy(state->m_Extra + unwindData->offset_extra, extra, extra_len);
                    state->m_Extra[unwindData->offset_extra + extra_len] = '\n';
                    unwindData->offset_extra += extra_len + 1;
                }
                else
                {
                    dmLogWarning("Not enough space to write entire stacktrace!");
                }
            }
        }
        return state->m_PtrCount >= AppState::PTRS_MAX ? _URC_END_OF_STACK : _URC_NO_REASON;
    }

    static void ResetToDefaultHandler(const int signum)
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = SIG_DFL;
        sa.sa_flags = 0;
        sigaction(signum, &sa, NULL);
    }

    static void Handler(const int signo, siginfo_t* const si, void *const sc)
    {
        if (!g_CrashDumpEnabled)
            return;

        AppState* state = GetAppState();

        state->m_Signum = signo;
        state->m_PtrCount = 0;

        // The default behavior is restored for the signal.
        // Unless this is done first thing in the signal handler we'll
        // be stuck in a signal-handler loop forever.
        ResetToDefaultHandler(signo);

        unwind_data unwindData;
        unwindData.offset_extra = 0;
        unwindData.stack_index = 0;
        _Unwind_Backtrace(OnFrameEnter, &unwindData);

        if (g_CrashExtraInfoCallback)
        {
            int extra_len = strlen(state->m_Extra);
            g_CrashExtraInfoCallback(g_CrashExtraInfoCallbackCtx, state->m_Extra + extra_len, dmCrash::AppState::EXTRA_MAX - extra_len - 1);
        }

        WriteCrash(GetFilePath(), state);

        bool is_debug_mode = dLib::IsDebugMode();
        dLib::SetDebugMode(true);
        dmLogError("CALL STACK:\n\n%s\n", state->m_Extra);
        dLib::SetDebugMode(is_debug_mode);
    }

    void WriteDump()
    {
        siginfo_t empty;
        Handler(0xDEAD, &empty, 0);
    }

    void InstallOnSignal(int signum)
    {
        assert(signum >= 0 && signum < SIGNAL_MAX);

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = Handler;
        sa.sa_flags = SA_SIGINFO;
        
        sigaction(signum, &sa, NULL);
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
