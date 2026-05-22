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

#ifndef DM_CRASH_BACKTRACE_SIGNAL_POSIX_H
#define DM_CRASH_BACKTRACE_SIGNAL_POSIX_H

#include <signal.h>
#include <string.h>
#if defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace dmCrash
{
    static const int SIGNAL_MAX = 64;

    typedef void (*FSignalAction)(int, siginfo_t*, void*);

    static inline bool IsValidSignal(int signum)
    {
        return signum > 0 && signum < SIGNAL_MAX;
    }

    static inline void ResetToDefaultSignalHandler(int signum)
    {
        if (!IsValidSignal(signum))
        {
            return;
        }

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = SIG_DFL;
        sa.sa_flags = 0;
        sigaction(signum, &sa, 0);
    }

    static inline void RestoreSignalHandler(int signum, struct sigaction* old_signals)
    {
        if (!IsValidSignal(signum) || !old_signals)
        {
            return;
        }

        sigaction(signum, &old_signals[signum], 0);
    }

    static inline bool ChainSignalToPreviousHandler(int signum, siginfo_t* si, void* sc, struct sigaction* old_signals, FSignalAction current_handler)
    {
        if (!IsValidSignal(signum) || !old_signals)
        {
            return false;
        }

        struct sigaction* old_signal = &old_signals[signum];
        if ((old_signal->sa_flags & SA_SIGINFO) &&
            old_signal->sa_sigaction &&
            old_signal->sa_sigaction != (FSignalAction)SIG_DFL &&
            old_signal->sa_sigaction != (FSignalAction)SIG_IGN &&
            old_signal->sa_sigaction != current_handler)
        {
            RestoreSignalHandler(signum, old_signals);
            old_signal->sa_sigaction(signum, si, sc);
            return true;
        }
        else if (old_signal->sa_handler != SIG_DFL && old_signal->sa_handler != SIG_IGN && old_signal->sa_handler)
        {
            RestoreSignalHandler(signum, old_signals);
            old_signal->sa_handler(signum);
            return true;
        }

        return false;
    }

    static inline bool WasPreviousSignalIgnored(int signum, struct sigaction* old_signals)
    {
        return IsValidSignal(signum) && old_signals && old_signals[signum].sa_handler == SIG_IGN;
    }

    static inline void RaiseDefaultSignalHandler(int signum, const siginfo_t* si)
    {
        if (!IsValidSignal(signum))
        {
            return;
        }

#if defined(__linux__)
        if (si && si->si_signo == signum)
        {
            int result = syscall(SYS_rt_tgsigqueueinfo,
                                 getpid(),
                                 syscall(SYS_gettid),
                                 signum,
                                 si);
            if (result == 0)
            {
                return;
            }
        }
#endif

        raise(signum);
    }

    static inline void ChainSignalOrRaiseDefault(int signum, siginfo_t* si, void* sc, struct sigaction* old_signals, FSignalAction current_handler)
    {
        if (ChainSignalToPreviousHandler(signum, si, sc, old_signals, current_handler))
        {
            return;
        }

        if (WasPreviousSignalIgnored(signum, old_signals))
        {
            RestoreSignalHandler(signum, old_signals);
            return;
        }

        RaiseDefaultSignalHandler(signum, si);
    }

    static inline bool InstallSignalHandler(int signum, FSignalAction handler, struct sigaction* old_signals)
    {
        if (!IsValidSignal(signum) || !handler || !old_signals)
        {
            return false;
        }

        struct sigaction current;
        memset(&current, 0, sizeof(current));
        if (sigaction(signum, 0, &current) == 0 &&
            (current.sa_flags & SA_SIGINFO) &&
            current.sa_sigaction == handler)
        {
            return true;
        }

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = handler;
        sa.sa_flags = SA_SIGINFO;

        return sigaction(signum, &sa, &old_signals[signum]) == 0;
    }
}

#endif
