#include "jni_util.h"
#include <dlib/dstrings.h>

#include <memory.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
    #define UNW_LOCAL_ONLY
    #include <libunwind.h>
    #include <stdio.h>

#elif defined(_WIN32)
    #include <Windows.h>
    #include <Dbghelp.h>

#else
    #include <execinfo.h>
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

EXCEPTION_POINTERS* g_ExceptionPointers = 0;

LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* ptr)
{
    g_ExceptionPointers = ptr;
    g_UserCallback(0xDEAD, g_UserContext);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void SetHandlers()
{
    g_signal_handlers[0] = signal(SIGILL, DefaultSignalHandler);
    g_signal_handlers[1] = signal(SIGABRT, DefaultSignalHandler);
    g_signal_handlers[2] = signal(SIGFPE, DefaultSignalHandler);
    g_signal_handlers[3] = signal(SIGSEGV, DefaultSignalHandler);

    ::SetUnhandledExceptionFilter(ExceptionHandler);
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
    if (strstr(message, "SIGFPE"))    raise(SIGFPE);
    if (strstr(message, "SIGSEGV"))   raise(SIGSEGV);
#if !defined(_WIN32)
    if (strstr(message, "SIGPIPE"))   raise(SIGPIPE);
    if (strstr(message, "SIGBUS"))    raise(SIGBUS);
#endif

    printf("No signal found in string: %s!\n", message);
}


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

#elif defined(_WIN32)


static uint32_t GetCallstackPointers(EXCEPTION_POINTERS* pep, void** ptrs, uint32_t num_ptrs)
{
    if (!pep)
    {
        // The API only accepts 62 or less
        uint32_t max = num_ptrs > 62 ? 62 : num_ptrs;
        num_ptrs = CaptureStackBackTrace(0, max, ptrs, 0);
    } else
    {
        HANDLE process = ::GetCurrentProcess();
        HANDLE thread = ::GetCurrentThread();
        uint32_t count = 0;

        // Initialize stack walking.
        STACKFRAME64 stack_frame;
        memset(&stack_frame, 0, sizeof(stack_frame));
        #if defined(_WIN64)
            int machine_type = IMAGE_FILE_MACHINE_AMD64;
            stack_frame.AddrPC.Offset = pep->ContextRecord->Rip;
            stack_frame.AddrFrame.Offset = pep->ContextRecord->Rbp;
            stack_frame.AddrStack.Offset = pep->ContextRecord->Rsp;
        #else
            int machine_type = IMAGE_FILE_MACHINE_I386;
            stack_frame.AddrPC.Offset = pep->ContextRecord->Eip;
            stack_frame.AddrFrame.Offset = pep->ContextRecord->Ebp;
            stack_frame.AddrStack.Offset = pep->ContextRecord->Esp;
        #endif
        stack_frame.AddrPC.Mode = AddrModeFlat;
        stack_frame.AddrFrame.Mode = AddrModeFlat;
        stack_frame.AddrStack.Mode = AddrModeFlat;
        while (StackWalk64(machine_type,
            process,
            thread,
            &stack_frame,
            pep->ContextRecord,
            NULL,
            &SymFunctionTableAccess64,
            &SymGetModuleBase64,
            NULL) &&
            count < num_ptrs) {
            ptrs[count++] = reinterpret_cast<void*>(stack_frame.AddrPC.Offset);
        }
        num_ptrs = count;
    }
    return num_ptrs;
}

HMODULE GetCurrentModule()
{   // NB: XP+ solution!
    HMODULE hModule = NULL;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)GetCurrentModule, &hModule);
    return hModule;
}

char* GenerateCallstack()
{
    int output_size = 0;
    int output_cursor = 0;
    char* output = 0;

    HANDLE process = ::GetCurrentProcess();

    ::SymSetOptions(SYMOPT_DEBUG);
    BOOL initialized = ::SymInitialize(process, 0, TRUE);

    if (!initialized)
    {
        APPEND_STRING("No symbols available (Failed to initialize: %d)\n", GetLastError());
    }

    void* stack[62];
    uint32_t count = GetCallstackPointers(g_ExceptionPointers, stack, sizeof(stack)/sizeof(stack[0]));

    // Get a nicer printout as well
    const int name_length = 1024;
    char symbolbuffer[sizeof(SYMBOL_INFO) + name_length * sizeof(char)*2];
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbolbuffer;
    symbol->MaxNameLen = name_length;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    DWORD displacement;
    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    for(uint32_t c = 0; c < count; c++)
    {
        DWORD64 address = (DWORD64)stack[c];
        APPEND_STRING("    %d: %p: ", c, address);

        const char* symbolname = "<unknown symbol>";
        DWORD64 symboladdress = address;

        DWORD64 symoffset = 0;

        if (::SymFromAddr(process, address, &symoffset, symbol))
        {
            symbolname = symbol->Name;
            symboladdress = symbol->Address;
        }

        const char* filename = "<unknown>";
        int line_number = 0;
        if (::SymGetLineFromAddr64(process, address, &displacement, &line))
        {
            filename = line.FileName;
            line_number = line.LineNumber;
        }

        APPEND_STRING("%s(%d): %s\n", filename, line_number, symbolname);
    }

    ::SymCleanup(process);

    APPEND_STRING("    # <- native");
    return output;
}

#else

char* GenerateCallstack()
{
    int output_size = 0;
    int output_cursor = 0;
    char* output = 0;

    void* buffer[64];
    int num_pointers = backtrace(buffer, sizeof(buffer)/sizeof(buffer[0]));

    char** stacktrace = backtrace_symbols(buffer, num_pointers);
    for (uint32_t i = 0; i < num_pointers; ++i)
    {
        APPEND_STRING("    %p: %s\n", buffer[i], stacktrace[i]);
    }

    free(stacktrace);

    APPEND_STRING("    # <- native");
    return output;
}

#endif

#undef APPEND_STRING

} // namespace
