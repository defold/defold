#include <signal.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/dstrings.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <libunwind.h>

#include "crash.h"
#include "crash_private.h"

namespace dmCrash
{
    static const int SIGNAL_MAX = 64;

    // This array contains the default behavior for each signal.
    static struct sigaction sigdfl[SIGNAL_MAX];

    void unw_backtrace() {
      unw_cursor_t cursor;
      unw_context_t context;

      // Initialize cursor to current frame for local unwinding.
      unw_getcontext(&context);
      unw_init_local(&cursor, &context);

      // Unwind frames one by one, going up the frame stack.
      while (unw_step(&cursor) > 0) {
        unw_word_t offset, pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (pc == 0) {
          break;
        }
        printf("0x%llx:", pc);

        char sym[256];
        if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
          printf(" (%s+0x%llx)\n", sym, offset);
        } else {
          printf(" -- error: unable to obtain symbol name for this frame\n");
        }
      }
    }

    void OnCrash(int signo)
    {
        fflush(stdout);
        fflush(stderr);

        g_AppState.m_Signum = signo;
        g_AppState.m_PtrCount = 0;

//         g_AppState.m_PtrCount = backtrace(g_AppState.m_Ptr, AppState::PTRS_MAX);
//         g_AppState.m_Signum = signo;

//         char** stacktrace = backtrace_symbols(g_AppState.m_Ptr, g_AppState.m_PtrCount);
//         uint32_t offset = 0;

// dmLogWarning("MAWE: BACKTRACE %d", g_AppState.m_PtrCount);

//         for (int i = 0; i < g_AppState.m_PtrCount; ++i)
//         {
//             // Write each symbol on a separate line, just like
//             // backgrace_symbols_fd would do.
//             uint32_t stacktrace_length = strnlen(stacktrace[i], dmCrash::AppState::EXTRA_MAX - 1);
//             if ((offset + stacktrace_length) < (dmCrash::AppState::EXTRA_MAX - 1))
//             {
// dmLogWarning("MAWE: %d  %s", i, stacktrace[i]);
//                 memcpy(g_AppState.m_Extra + offset, stacktrace[i], stacktrace_length);
//                 g_AppState.m_Extra[offset + stacktrace_length] = '\n';
//                 offset += stacktrace_length + 1;
//             }
//             else
//             {
//                 dmLogError("Not enough space to write entire stacktrace!");
//                 break;
//             }
//         }

        unw_backtrace();

        //free(stacktrace);
        WriteCrash(g_FilePath, &g_AppState);
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
