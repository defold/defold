#include <signal.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/dstrings.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
extern "C" {
    #define UNW_LOCAL_ONLY
    #include <libunwind.h>
}

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

#ifdef ANDROID
        int umw_map_ret = unw_map_local_create();
#endif

        unw_context_t context;
        unw_getcontext(&context);
        unw_cursor_t cursor;
        unw_init_local(&cursor, &context);

        uint32_t stack_index = 0;
        uint32_t offset_extra = 0; // How much we've written to the extra field
        while (unw_step(&cursor) > 0 && g_AppState.m_PtrCount < AppState::PTRS_MAX)
        {
            unw_word_t offset, pc = 0;
            unw_get_reg(&cursor, UNW_REG_IP, &pc);
            if (pc == 0)
            {
                break;
            }

            // Store stack pointers as absolute values.
            // They can be turned into offsets using `crash.get_modules`, but we also store
            // them as offsets (if available) inside the extras field for convenience.
            g_AppState.m_Ptr[g_AppState.m_PtrCount] = (void*)(uintptr_t)pc;
            g_AppState.m_PtrCount++;

            // Figure out the relative pc within the dylib
            // Currently only available on Android due to unw_map_* functions being
            // Android specific.
            bool proc_path_found = false;
            const char* proc_path = NULL;
            bool proc_path_truncated = false;
#ifdef ANDROID
            unw_map_t proc_map_item;
            if (umw_map_ret == 0)
            {
                unw_map_cursor_t proc_map_cursor;
                unw_map_local_cursor_get(&proc_map_cursor);
                while (unw_map_cursor_get_next(&proc_map_cursor, &proc_map_item) > 0) {
                    if (pc >= proc_map_item.start && pc < proc_map_item.end) {
                        proc_path_found = true;
                        proc_path = proc_map_item.path;
                        pc -= proc_map_item.start;
                        break;
                    }
                }
            }
#else
            void* best_match_addr = 0x0;
            const char* best_match_name = 0x0;
            for (uint32_t i=0; i < AppState::MODULES_MAX; i++)
            {
                void* addr = g_AppState.m_ModuleAddr[i];
                if (!addr)
                {
                    break;
                }
                if ((void*)pc >= addr) {
                    best_match_addr = addr;
                    best_match_name = g_AppState.m_ModuleName[i];
                }
            }
            if (best_match_addr && best_match_name) {
                proc_path_found = true;
                proc_path = best_match_name;
                pc -= (unw_word_t)best_match_addr;
            }
#endif

            // If path is too long, truncate it to the last 32 chars where the "<lib>.so" would be visible
            if (proc_path_found) {
                int proc_path_len = strlen(proc_path);
                if (proc_path_len > 32) {
                    proc_path = proc_path + (proc_path_len-32);
                    proc_path_truncated = true;
                }
            }

            // Try to get symbol name and offset inside procedure.
            // NOTE: unw_get_proc_name should return 0 on success, but on OSX implementation
            // of libunwind it does not! Instead we first write a \0 as first char in sym,
            // and check if it was overwritten after the function call to determine of the
            // symbol was found.
            char sym[256];
            sym[0] = 0;
            unw_get_proc_name(&cursor, sym, sizeof(sym), &offset);
            bool found_sym = (sym[0] != 0);

            // Store extra data;
            // frame index, pc (offset) in hex, [library name], [symbol name], [offset inside procedure in decimal]
            // We try to keep a fairly "standard" formatting based on what FF write.
            char extra[256];
            snprintf(extra, sizeof(extra),
                "#%d pc %p %s%s %s+%u",
                stack_index++,
                (void*)(uintptr_t)pc,
                proc_path_truncated ? "..." : "",
                proc_path_found && proc_path ? proc_path : "",
                found_sym ? sym : "<unknown>",
                found_sym ? (uint32_t)offset : 0);

            int extra_len = strlen(extra);
            if ((offset_extra + extra_len) < (dmCrash::AppState::EXTRA_MAX - 1))
            {
                memcpy(g_AppState.m_Extra + offset_extra, extra, extra_len);
                g_AppState.m_Extra[offset_extra + extra_len] = '\n';
                offset_extra += extra_len + 1;
            }
            else
            {
                dmLogWarning("Not enough space to write entire stacktrace!");
                break;
            }
        }

#ifdef ANDROID
        unw_map_local_destroy();
#endif

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
