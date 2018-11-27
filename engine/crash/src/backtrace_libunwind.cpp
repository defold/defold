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

    void OnCrash(int signo)
    {
        fflush(stdout);
        fflush(stderr);

        g_AppState.m_Signum = signo;
        g_AppState.m_PtrCount = 0;

        unw_context_t context;
        unw_getcontext(&context);
        unw_cursor_t cursor;
        unw_init_local(&cursor, &context);

        uint32_t offset_extra = 0; // How much we've written to the extra field
        while (unw_step(&cursor) > 0 && g_AppState.m_PtrCount < AppState::PTRS_MAX)
        {
            unw_word_t offset, pc;
            unw_get_reg(&cursor, UNW_REG_IP, &pc);
            if (pc == 0)
            {
                break;
            }
            g_AppState.m_Ptr[g_AppState.m_PtrCount] = (void*)(uintptr_t)pc;
            g_AppState.m_PtrCount++;

            printf("0x%llx:", pc);

            char sym[256];
            if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
                printf(" (%s+0x%llx)\n", sym, offset);
            }
            else
            {
                strcpy(sym, "<unknown>");
            }

            int sym_len = strlen(sym);
            if ((offset_extra + sym_len) < (dmCrash::AppState::EXTRA_MAX - 1))
            {
                memcpy(g_AppState.m_Extra + offset_extra, sym, sym_len);
                g_AppState.m_Extra[offset_extra + sym_len] = '\n';
                offset_extra += sym_len + 1;
            }
            else
            {
                dmLogWarning("Not enough space to write entire stacktrace!");
                break;
            }
        }

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
