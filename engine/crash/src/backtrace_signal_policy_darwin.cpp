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

#include "backtrace_signal_policy.h"

namespace dmCrash
{
    bool ShouldReturnToOriginalFault(int signum, const siginfo_t* si)
    {
        if (!si || si->si_signo != signum)
        {
            return false;
        }

        int code = si->si_code;
        switch (signum)
        {
        case SIGSEGV:
            return code == SEGV_MAPERR || code == SEGV_ACCERR;
        case SIGTRAP:
            // Breakpoint traps often resume after the trap instruction when the
            // handler returns, so raise it explicitly through the default path.
            return false;
        case SIGBUS:
            return code == BUS_ADRALN || code == BUS_ADRERR || code == BUS_OBJERR;
        case SIGILL:
            return code == ILL_ILLOPC || code == ILL_ILLTRP || code == ILL_PRVOPC ||
                   code == ILL_ILLOPN || code == ILL_ILLADR || code == ILL_PRVREG ||
                   code == ILL_COPROC || code == ILL_BADSTK;
        default:
            return false;
        }
    }
}
