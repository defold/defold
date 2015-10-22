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
    static struct sigaction old_signal[SIGNAL_MAX];

    void WriteExtra(int fd)
    {
        backtrace_symbols_fd(g_AppState.m_Ptr, g_AppState.m_PtrCount, fd);
    }

    void OnCrash(int signo)
    {
        g_AppState.m_PtrCount = backtrace(g_AppState.m_Ptr, AppState::PTRS_MAX);
        g_AppState.m_Signum = signo;
        WriteCrash(g_FilePath, &g_AppState, WriteExtra);
    }

    void WriteDump()
    {
        OnCrash(0xDEAD);
    }

    static void Handler(const int signo, siginfo_t *const si, void *const sc)
    {
        if (old_signal[signo].sa_sigaction)
        {
            old_signal[signo].sa_sigaction(signo, si, sc);
        }
        OnCrash(signo);
    }

    void InstallOnSignal(int signum)
    {
        assert(signum >= 0 && signum < SIGNAL_MAX);

        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = Handler;
        sa.sa_flags = SA_SIGINFO;
        sigaction(signum, &sa, &old_signal[signum]);
    }

    static char stack_buffer[SIGSTKSZ];

    void InstallHandler()
    {
        stack_t stack;
        memset(&stack, 0, sizeof(stack));
        stack.ss_size = sizeof(stack_buffer);
        stack.ss_sp = stack_buffer;
        stack.ss_flags = 0;
#if !defined(__TVOS__)
        sigaltstack(&stack, NULL);
#endif

        InstallOnSignal(SIGSEGV);
        InstallOnSignal(SIGBUS);
        InstallOnSignal(SIGTRAP);
        InstallOnSignal(SIGILL);
    }
}
