#include "jni_util.h"
#include <dlib/dstrings.h>

#include <memory.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <stdio.h>
#endif

namespace dmJNI
{

static void* g_UserContext = 0;
static void (*g_UserCallback)(int, void*) = 0;

#if defined(_WIN32) || defined(__CYGWIN__)
    typedef void (*FSignalHandler)(int);
    static FSignalHandler g_signal_handlers[4];
#else
    static struct sigaction g_signal_handlers[6];
#endif

static void DefaultSignalHandler(int sig) {
    g_UserCallback(sig, g_UserContext);
}

#if defined(_WIN32) || defined(__CYGWIN__)

static void SetHandlers()
{
    g_signal_handlers[0] = signal(SIGILL, DefaultSignalHandler);
    g_signal_handlers[1] = signal(SIGABRT, DefaultSignalHandler);
    g_signal_handlers[2] = signal(SIGFPE, DefaultSignalHandler);
    g_signal_handlers[3] = signal(SIGSEGV, DefaultSignalHandler);
}

static void UnsetHandlers()
{
    signal(SIGILL, g_signal_handlers[0]);
    signal(SIGABRT, g_signal_handlers[1]);
    signal(SIGFPE, g_signal_handlers[2]);
    signal(SIGSEGV, g_signal_handlers[3]);
}
#else

static void SetHandlers()
{
    #if !defined(_MSC_VER) && defined(__clang__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
    #endif
    struct sigaction handler;
    memset(&handler, 0, sizeof(struct sigaction));
    handler.sa_handler = DefaultSignalHandler;
    sigaction(SIGILL, &handler, &g_signal_handlers[0]);
    sigaction(SIGABRT, &handler, &g_signal_handlers[1]);
    sigaction(SIGBUS, &handler, &g_signal_handlers[2]);
    sigaction(SIGFPE, &handler, &g_signal_handlers[3]);
    sigaction(SIGSEGV, &handler, &g_signal_handlers[4]);
    sigaction(SIGPIPE, &handler, &g_signal_handlers[5]);
    #if !defined(_MSC_VER) && defined(__clang__)
        #pragma GCC diagnostic pop
    #endif
}
static void UnsetHandlers()
{
    sigaction(SIGILL, &g_signal_handlers[0], 0);
    sigaction(SIGABRT, &g_signal_handlers[1], 0);
    sigaction(SIGBUS, &g_signal_handlers[2], 0);
    sigaction(SIGFPE, &g_signal_handlers[3], 0);
    sigaction(SIGSEGV, &g_signal_handlers[4], 0);
    sigaction(SIGPIPE, &g_signal_handlers[5], 0);
}
#endif

static void DefaultJniSignalHandler(int sig, void* ctx)
{
    JavaVM* vm = (JavaVM*)ctx;
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return;
    }

    char* callstack = GenerateCallstack();

    uint32_t size = 128 + strlen(callstack)+1;
    char* message = (char*)malloc(size);

    uint32_t written = 0;
    written += dmSnPrintf(message+written, size, "Exception in Defold JNI code. Signal %d\n", sig);
    written += dmSnPrintf(message+written, size, "%s", callstack);
    env->ThrowNew(env->FindClass("java/lang/RuntimeException"), message);

    free((void*)callstack);
}

void EnableSignalHanders(void* ctx, void (*callback)(int signal, void* ctx))
{
    g_UserCallback = callback;
    g_UserContext = ctx;

    memset(&g_signal_handlers[0], 0, sizeof(g_signal_handlers));
    SetHandlers();
}

void EnableDefaultSignalHanders(JavaVM* vm)
{
    EnableSignalHanders((void*)vm, DefaultJniSignalHandler);
}

void DisableSignalHanders()
{
    UnsetHandlers();
    g_UserCallback = 0;
    g_UserContext = 0;
}


void TestSignalFromString(const char* message)
{
    if (strstr(message, "SIGILL"))    raise(SIGILL);
    if (strstr(message, "SIGABRT"))   raise(SIGABRT);
    if (strstr(message, "SIGBUS"))    raise(SIGBUS);
    if (strstr(message, "SIGFPE"))    raise(SIGFPE);
    if (strstr(message, "SIGSEGV"))   raise(SIGSEGV);
    if (strstr(message, "SIGPIPE"))   raise(SIGPIPE);
    printf("No signal found in string: %s!\n", message);
}

#ifdef __APPLE__

char* GenerateCallstack()
{
    // Call this function to get a backtrace.
    unw_cursor_t cursor;
    unw_context_t context;

    // Initialize cursor to current frame for local unwinding.
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    int output_size = 0;
    int output_cursor = 0;
    char* output = 0;

#define APPEND_STRING(...) \
    { \
        int result; \
        do { \
            result = dmSnPrintf(output + output_cursor, output_size - output_cursor, __VA_ARGS__); \
            if (result == -1) \
            { \
                output_size += 64; \
                output = (char*)realloc(output, output_size); \
            } else { \
                output_cursor += result; \
            } \
        } while(result == -1); \
    }

    // Unwind frames one by one, going up the frame stack.
    while (unw_step(&cursor) > 0) {
        unw_word_t offset, pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (pc == 0) {
            break;
        }

        APPEND_STRING("    0x%016lx:", pc);

        char sym[256];
        if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
            APPEND_STRING(" (%s+0x%lx)\n", sym, offset);
        }
        else {
            APPEND_STRING(" -- error: unable to obtain symbol name for this frame\n");
        }
    }

    APPEND_STRING("    # <- native");

    return output;
}

#else

char* dmGenerateCallstack()
{
    char* output = strdup("Callstack generation needs implementation for this platform!");
    return output;
}

#endif

} // namespace