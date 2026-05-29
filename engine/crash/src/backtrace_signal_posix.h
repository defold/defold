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

#include "backtrace_signal_policy.h"

namespace dmCrash
{
    static const int MAX_SIGNAL_COUNT = 64;

    typedef void (*FSignalAction)(int, siginfo_t*, void*);

    static inline bool IsValidSignal(int signum)
    {
        return signum > 0 && signum < MAX_SIGNAL_COUNT;
    }

    static inline struct sigaction* GetPreviousSignalAction(int signum, struct sigaction* previous_signal_actions)
    {
        return IsValidSignal(signum) && previous_signal_actions ? &previous_signal_actions[signum] : 0;
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

    static inline void RestoreSignalHandler(int signum, struct sigaction* previous_signal_actions)
    {
        struct sigaction* previous_signal_action = GetPreviousSignalAction(signum, previous_signal_actions);
        if (!previous_signal_action)
        {
            return;
        }

        sigaction(signum, previous_signal_action, 0);
    }

    static inline bool ChainSignalToPreviousHandler(int signum, siginfo_t* si, void* sc, struct sigaction* previous_signal_actions, FSignalAction current_handler)
    {
        struct sigaction* previous_signal_action = GetPreviousSignalAction(signum, previous_signal_actions);
        if (!previous_signal_action)
        {
            return false;
        }

        if ((previous_signal_action->sa_flags & SA_SIGINFO) &&
            previous_signal_action->sa_sigaction &&
            previous_signal_action->sa_handler != SIG_DFL &&
            previous_signal_action->sa_handler != SIG_IGN &&
            previous_signal_action->sa_sigaction != current_handler)
        {
            RestoreSignalHandler(signum, previous_signal_actions);
            previous_signal_action->sa_sigaction(signum, si, sc);
            return true;
        }
        else if (!(previous_signal_action->sa_flags & SA_SIGINFO) &&
                 previous_signal_action->sa_handler != SIG_DFL &&
                 previous_signal_action->sa_handler != SIG_IGN &&
                 previous_signal_action->sa_handler)
        {
            RestoreSignalHandler(signum, previous_signal_actions);
            previous_signal_action->sa_handler(signum);
            return true;
        }

        return false;
    }

    static inline bool WasPreviousSignalIgnored(int signum, struct sigaction* previous_signal_actions)
    {
        struct sigaction* previous_signal_action = GetPreviousSignalAction(signum, previous_signal_actions);
        return previous_signal_action && previous_signal_action->sa_handler == SIG_IGN;
    }

    static inline void RaiseDefaultSignalHandler(int signum, const siginfo_t* si)
    {
        if (!IsValidSignal(signum))
        {
            return;
        }

        if (ShouldReturnToOriginalFault(signum, si))
        {
            return;
        }

        raise(signum);
    }

    static inline void ChainSignalOrRaiseDefault(int signum, siginfo_t* si, void* sc, struct sigaction* previous_signal_actions, FSignalAction current_handler)
    {
        if (ChainSignalToPreviousHandler(signum, si, sc, previous_signal_actions, current_handler))
        {
            return;
        }

        if (WasPreviousSignalIgnored(signum, previous_signal_actions))
        {
            RestoreSignalHandler(signum, previous_signal_actions);
            return;
        }

        RaiseDefaultSignalHandler(signum, si);
    }

    static inline bool InstallSignalHandler(int signum, FSignalAction handler, struct sigaction* previous_signal_actions)
    {
        struct sigaction* previous_signal_action = GetPreviousSignalAction(signum, previous_signal_actions);
        if (!handler || !previous_signal_action)
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

        return sigaction(signum, &sa, previous_signal_action) == 0;
    }
}

#endif
