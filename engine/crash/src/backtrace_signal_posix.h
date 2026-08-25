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

namespace dmCrash
{
    static const int MAX_SIGNAL_COUNT = 64;

    typedef void (*FSignalAction)(int, siginfo_t*, void*);

    bool IsValidSignal(int signum);
    void ResetToDefaultSignalHandler(int signum);
    void RestoreSignalHandler(int signum, struct sigaction* previous_signal_actions);
    bool ChainSignalToPreviousHandler(int signum, siginfo_t* si, void* sc, struct sigaction* previous_signal_actions, FSignalAction current_handler);
    bool WasPreviousSignalIgnored(int signum, struct sigaction* previous_signal_actions);
    void RaiseDefaultSignalHandler(int signum, const siginfo_t* si);
    void ChainSignalOrRaiseDefault(int signum, siginfo_t* si, void* sc, struct sigaction* previous_signal_actions, FSignalAction current_handler);
    bool InstallSignalHandler(int signum, FSignalAction handler, struct sigaction* previous_signal_actions);
    bool BeginSignalHandler();
    void EndSignalHandler();
}

#endif
