#include <signal.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <unistd.h>
#include <asm/sigcontext.h>
#include <sys/system_properties.h>
#include "crash.h"
#include "crash_private.h"

namespace dmCrash
{
    static const int SIGNAL_MAX = 64;
    static struct sigaction old_signal[SIGNAL_MAX];

    // -------------------
    // libunwind.so
    // --------------------

    typedef int (*unwind_backtrace_fn)(void** buffer, int size);
    typedef long unw_word_t;

    struct unw_cursor_t
    {
        unw_word_t buf[4096];
    };

    struct unw_context_t
    {
        unw_word_t reg[16];
    };

    typedef int (*unw_init_local_t)(unw_cursor_t *, unw_context_t *);
    typedef int (*unw_step_t)(unw_cursor_t *);
    typedef int (*unw_get_reg_t)(unw_cursor_t *, int regnum, unw_word_t *);

    static unw_init_local_t unw_init_local = 0;
    static unw_step_t unw_step = 0;
    static unw_get_reg_t unw_get_reg = 0;
    static unw_cursor_t s_cursor;

    // -------------------
    // libcorkscrew.so
    // --------------------

    struct backtrace_frame_t
    {
        uintptr_t absolute_pc;
        uintptr_t stack_top;
        size_t stack_size;
    } s_CorkscrewFrames[AppState::PTRS_MAX];

    typedef void *(*acquire_my_map_info_list_fn)();
    typedef void (*release_my_map_info_list_fn)(void *map);
    typedef ssize_t (*unwind_backtrace_signal_arch_fn)(void *siginfo, void *sigcontext, void *map_info_list, backtrace_frame_t *backtrace, size_t ignore_depth, size_t max_depth);

    static acquire_my_map_info_list_fn acquire_my_map_info_list = 0;
    static release_my_map_info_list_fn release_my_map_info_list = 0;
    static unwind_backtrace_signal_arch_fn unwind_backtrace_signal_arch = 0;

    struct ucontext {
        uint32_t uc_flags;
        struct ucontext* uc_link;
        stack_t uc_stack;
        sigcontext uc_mcontext;
    };

    void OnCrash(int signo, siginfo_t const *si, const ucontext *uc)
    {
        fflush(stdout);
        fflush(stderr);

        if (unw_init_local)
        {
            // Found out by manual inspection of libunwind source code
            //
            // libcorkscrew has the convenient function _signal_arch where it can step
            // into the stack frames from where the signal was triggered. Here, just
            // build a context with information taken from the signal info.
            unw_context_t ctx;
            ctx.reg[0] = uc->uc_mcontext.arm_r0;
            ctx.reg[1] = uc->uc_mcontext.arm_r1;
            ctx.reg[2] = uc->uc_mcontext.arm_r2;
            ctx.reg[3] = uc->uc_mcontext.arm_r3;
            ctx.reg[4] = uc->uc_mcontext.arm_r4;
            ctx.reg[5] = uc->uc_mcontext.arm_r5;
            ctx.reg[6] = uc->uc_mcontext.arm_r6;
            ctx.reg[7] = uc->uc_mcontext.arm_r7;
            ctx.reg[8] = uc->uc_mcontext.arm_r8;
            ctx.reg[9] = uc->uc_mcontext.arm_r9;
            ctx.reg[10] = uc->uc_mcontext.arm_r10;
            ctx.reg[11] = uc->uc_mcontext.arm_fp;
            ctx.reg[12] = uc->uc_mcontext.arm_ip;
            ctx.reg[13] = uc->uc_mcontext.arm_sp;
            ctx.reg[14] = uc->uc_mcontext.arm_lr;
            ctx.reg[15] = uc->uc_mcontext.arm_pc;

            unw_init_local(&s_cursor, &ctx);
            g_AppState.m_PtrCount = 0;
            while (g_AppState.m_PtrCount < AppState::PTRS_MAX)
            {
                // Reg number, see the 'white lie' in libunwind-arm.h in libunwind. Simulating passing
                // in a request for the program counter. Bit confusing, but works.
                unw_get_reg(&s_cursor, 14, (unw_word_t*) &g_AppState.m_Ptr[g_AppState.m_PtrCount++]);
                if (!unw_step(&s_cursor))
                    break;
            }
        }
        else if (unwind_backtrace_signal_arch)
        {
            void *map = acquire_my_map_info_list();
            ssize_t count = unwind_backtrace_signal_arch((void*)si, (void*)uc, map, &s_CorkscrewFrames[0], 0, AppState::PTRS_MAX);
            if (count > 0)
            {
                for (ssize_t i=0;i<count;i++)
                {
                    g_AppState.m_Ptr[i] = (void*)s_CorkscrewFrames[i].absolute_pc;
                }
                g_AppState.m_PtrCount = count;
            }
            release_my_map_info_list(map);
        }
        else
        {
            // Should never get here, but in case no unwinding lib could be loaded,
            // write program counter. (Maybe link register too?)
            g_AppState.m_Ptr[0] = (void*)uc->uc_mcontext.arm_pc;
            g_AppState.m_PtrCount = 1;
        }

        g_AppState.m_Signum = signo;

        uint32_t offset = 0;
        for (int i = 0;i < dmMath::Min(g_AppState.m_PtrCount, AppState::PTRS_MAX); ++i)
        {
            // Write each pointer on a separate line, much like backtrace_symbols_fd.
            char stacktrace[128] = { 0 };
            int written = snprintf(stacktrace, sizeof(stacktrace), "#%d %p\n", i, g_AppState.m_Ptr[i]);
            if (written > 0 && written < sizeof(stacktrace))
            {
                uint32_t stacktrace_length = strnlen(stacktrace, sizeof(stacktrace));
                if (offset + stacktrace_length < dmCrash::AppState::EXTRA_MAX - 1)
                {
                    memcpy(g_AppState.m_Extra + offset, stacktrace, stacktrace_length);
                    offset += stacktrace_length;
                }
                else
                {
                    dmLogError("Not enough space to write entire stacktrace!");
                }
            }
            else
            {
                dmLogError("Buffer too short (%d) to write stacktrace (%d)!", sizeof(stacktrace), written);
            }
        }

        WriteCrash(g_FilePath, &g_AppState);
    }

    void WriteDump()
    {
        // This is only a debugging functionality; need to use actual signal here since the stack unwinding
        // only works from signal handler.
        raise(SIGSEGV);
    }

    static void Handler(const int signo, siginfo_t* const si, void *const sc)
    {
        if (old_signal[signo].sa_sigaction)
        {
            old_signal[signo].sa_sigaction(signo, si, sc);
        }

        OnCrash(signo, si, (const ucontext*) sc);
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

    void SetCrashFilename(const char*)
    {
    }

    void PlatformPurge()
    {
    }

    void InstallHandler()
    {
        // Somewhat hacky to do this here, but this is a nice platform specific file and saves adding
        // additional layers of platform initialization.
        char fp[PROP_VALUE_MAX];
        __system_property_get("ro.build.fingerprint", fp);
        dmStrlCpy(g_AppState.m_AndroidBuildFingerprint, fp, sizeof(g_AppState.m_AndroidBuildFingerprint));

        void *lib = dlopen("libunwind.so", RTLD_LAZY | RTLD_LOCAL);
        if (lib)
        {
            // The libunwind usage here is quite a bit of hackery and assumptions
            // made by peering at the source code and generated dynamic libraries.
            unw_init_local = (unw_init_local_t) dlsym(lib, "_Uarm_init_local");
            unw_step = (unw_step_t) dlsym(lib, "_Uarm_step");
            unw_get_reg = (unw_get_reg_t) dlsym(lib, "_Uarm_get_reg");

            // clear handler if not complete.
            if (!unw_step || !unw_get_reg)
            {
                unw_init_local = 0;
            }
        }
        else
        {
            lib = dlopen("libcorkscrew.so", RTLD_LAZY | RTLD_LOCAL);
            if (lib)
            {
                acquire_my_map_info_list = (acquire_my_map_info_list_fn) dlsym(lib, "acquire_my_map_info_list");
                release_my_map_info_list = (release_my_map_info_list_fn) dlsym(lib, "release_my_map_info_list");
                unwind_backtrace_signal_arch = (unwind_backtrace_signal_arch_fn) dlsym(lib, "unwind_backtrace_signal_arch");

                if (!acquire_my_map_info_list || !release_my_map_info_list)
                {
                    unwind_backtrace_signal_arch = 0;
                }
            }
        }

        stack_t stack;
        memset(&stack, 0, sizeof(stack));
        stack.ss_size = SIGSTKSZ;
        stack.ss_sp = malloc(stack.ss_size);
        stack.ss_flags = 0;
        sigaltstack(&stack, NULL);

        InstallOnSignal(SIGSEGV);
        InstallOnSignal(SIGBUS);
        InstallOnSignal(SIGTRAP);
        InstallOnSignal(SIGILL);
    }
}
