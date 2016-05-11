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

    // This array contains the default behavior for each signal.
    static struct sigaction sigdfl[SIGNAL_MAX];

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

        struct sigaction sa = { 0 };
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = Handler;
        sa.sa_flags = SA_SIGINFO;

        // The current (default) behavior is stored in sigdfl.
        sigaction(signum, &sa, &sigdfl[signum]);
    }

    static char stack_buffer[SIGSTKSZ];

    void InstallHandler()
    {
        InstallOnSignal(SIGSEGV);
        InstallOnSignal(SIGBUS);
        InstallOnSignal(SIGTRAP);
        InstallOnSignal(SIGILL);
    }
}
