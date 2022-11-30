// Copyright 2020-2022 The Defold Foundation
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
#include <unwind.h>
#include <dlfcn.h>
#include "crash.h"
#include "crash_private.h"

namespace dmCrash
{
    static const int SIGNAL_MAX = 64;
    static const int FRAMES_MAX = 128;

    static bool g_CrashDumpEnabled = true;
    static FCallstackExtraInfoCallback  g_CrashExtraInfoCallback = 0;
    static void*                        g_CrashExtraInfoCallbackCtx = 0;


    void EnableHandler(bool enable)
    {
        g_CrashDumpEnabled = enable;
    }

    void HandlerSetExtraInfoCallback(FCallstackExtraInfoCallback cbk, void* ctx)
    {
        g_CrashExtraInfoCallback = cbk;
        g_CrashExtraInfoCallbackCtx = ctx;
    }

    typedef struct {
        int frame_num;
    } unwind_data;

    _Unwind_Reason_Code OnFrameEnter(struct _Unwind_Context *context, void *data)
    {
        if (!g_CrashDumpEnabled)
            return _URC_NO_REASON;

        unwind_data* u_data = (unwind_data *)data;
        u_data->frame_num++;
        bool is_debug_mode = dLib::IsDebugMode();
        dLib::SetDebugMode(true);
        const uintptr_t pc = _Unwind_GetIP(context);
        if (pc)
        {
            Dl_info dl_info;
            if (dladdr((void *) pc, &dl_info) && dl_info.dli_sname)
            {
                
                dmLogError("#%02d pc %012p  %s %s+%u", 
                    u_data->frame_num,
                    (void*)((intptr_t) pc - (intptr_t) dl_info.dli_fbase),
                    dl_info.dli_fname ? dl_info.dli_fname : "",
                    dl_info.dli_sname,
                    (void*)((intptr_t) pc - (intptr_t) dl_info.dli_saddr));
            }
            else
            {
                dmLogError("#%2d pc %12p  %s", u_data->frame_num, (void*)pc, dl_info.dli_fname ? dl_info.dli_fname : "");
            }
        }
        dLib::SetDebugMode(is_debug_mode);
        // WriteCrash(g_FilePath, &g_AppState);

        // bool is_debug_mode = dLib::IsDebugMode();
        // dLib::SetDebugMode(true);
        // dmLogError("CALL STACK:\n\n%s\n", g_AppState.m_Extra);
        // dLib::SetDebugMode(is_debug_mode);
        return u_data->frame_num >= FRAMES_MAX ? _URC_END_OF_STACK : _URC_NO_REASON;
    }

    void WriteDump()
    {
        //?
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
        // The default behavior is restored for the signal.
        // Unless this is done first thing in the signal handler we'll
        // be stuck in a signal-handler loop forever.
        ResetToDefaultHandler(signo);
        unwind_data data;
        data.frame_num = 0;
        _Unwind_Backtrace(OnFrameEnter, &data);
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
