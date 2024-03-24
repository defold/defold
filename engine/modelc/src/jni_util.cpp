// Copyright 2020-2024 The Defold Foundation
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

#include "jni_util.h"
#include <dlib/array.h>
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

static dmArray<JNIEnv*> g_AllowedContexts;

static void* g_UserContext = 0;
static int (*g_UserCallback)(int, void*) = 0;

// ************************************************************************************

#if defined(_WIN32) || defined(__CYGWIN__)

typedef void (*FSignalHandler)(int);
static FSignalHandler   g_SignalHandlers[64];

static void DefaultSignalHandler(int sig) {
    int handled = g_UserCallback(sig, g_UserContext);
    if (!handled)
    {
        g_SignalHandlers[sig](sig);
    }
}

#else

typedef void ( *sigaction_handler_t )( int, siginfo_t *, void * );
static struct sigaction g_SignalHandlers[64];

static void DefaultSignalHandler(int sig, siginfo_t* info, void* arg) {
    int handled = g_UserCallback(sig, g_UserContext);
    if (!handled)
    {
        if ( ( g_SignalHandlers[sig].sa_sigaction != (sigaction_handler_t)SIG_IGN ) &&
             ( g_SignalHandlers[sig].sa_sigaction != (sigaction_handler_t)SIG_DFL ) )
        {
            g_SignalHandlers[sig].sa_sigaction(sig, info, arg);
        }
    }
}

#endif

// ************************************************************************************

#if defined(_WIN32) || defined(__CYGWIN__)

EXCEPTION_POINTERS* g_ExceptionPointers = 0;

LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* ptr)
{
    g_ExceptionPointers = ptr;
    g_UserCallback(0xDEAD, g_UserContext);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void SetHandlers()
{
    g_SignalHandlers[SIGILL] = signal(SIGILL, DefaultSignalHandler);
    g_SignalHandlers[SIGABRT] = signal(SIGABRT, DefaultSignalHandler);
    g_SignalHandlers[SIGFPE] = signal(SIGFPE, DefaultSignalHandler);
    g_SignalHandlers[SIGSEGV] = signal(SIGSEGV, DefaultSignalHandler);
}

static void UnsetHandlers()
{
    signal(SIGILL, g_SignalHandlers[SIGILL]);
    signal(SIGABRT, g_SignalHandlers[SIGABRT]);
    signal(SIGFPE, g_SignalHandlers[SIGFPE]);
    signal(SIGSEGV, g_SignalHandlers[SIGSEGV]);
}
#else

static void SetHandler(int sig, struct sigaction* old)
{
    #if !defined(_MSC_VER) && defined(__clang__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
    #endif

    struct sigaction handler;
    memset(&handler, 0, sizeof(struct sigaction));
    sigemptyset(&handler.sa_mask);
    handler.sa_sigaction = DefaultSignalHandler;
    handler.sa_flags = SA_SIGINFO;

    if (old)
    {
        memset(old, 0, sizeof(struct sigaction));
        sigemptyset(&old->sa_mask);
    }

    sigaction(sig, &handler, old);

    #if !defined(_MSC_VER) && defined(__clang__)
        #pragma GCC diagnostic pop
    #endif
}

static void SetHandlers()
{
    SetHandler(SIGILL, &g_SignalHandlers[SIGILL]);
    SetHandler(SIGABRT, &g_SignalHandlers[SIGABRT]);
    SetHandler(SIGFPE, &g_SignalHandlers[SIGFPE]);
    SetHandler(SIGSEGV, &g_SignalHandlers[SIGSEGV]);
    SetHandler(SIGPIPE, &g_SignalHandlers[SIGPIPE]);
}
static void UnsetHandlers()
{
    SetHandler(SIGILL, 0);
    SetHandler(SIGABRT, 0);
    SetHandler(SIGFPE, 0);
    SetHandler(SIGSEGV, 0);
    SetHandler(SIGPIPE, 0);
}
#endif

bool IsContextAdded(JNIEnv* env)
{
    for (uint32_t i = 0; i < g_AllowedContexts.Size(); ++i)
    {
        if (g_AllowedContexts[i] == env)
            return true;
    }
    return false;
}

void AddContext(JNIEnv* env)
{
    if (IsContextAdded(env))
        return;
    if (g_AllowedContexts.Full())
        g_AllowedContexts.OffsetCapacity(4);
    g_AllowedContexts.Push(env);
}

void RemoveContext(JNIEnv* env)
{
    for (uint32_t i = 0; i < g_AllowedContexts.Size(); ++i)
    {
        if (g_AllowedContexts[i] == env)
        {
            g_AllowedContexts.EraseSwap(i);
            return;
        }
    }
}

static int DefaultJniSignalHandler(int sig, void* ctx)
{
    JavaVM* vm = (JavaVM*)ctx;
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return 0;
    }

    if (!IsContextAdded(env))
    {
        return 0;
    }

    char message[1024];
    uint32_t written = 0;
    written += dmSnPrintf(message+written, sizeof(message), "Exception in Defold JNI code. Signal %d\n", sig);

    GenerateCallstack(message+written, sizeof(message)-written);

    env->ThrowNew(env->FindClass("java/lang/RuntimeException"), message);
    return 1;
}

void EnableSignalHandlers(void* ctx, int (*callback)(int signal, void* ctx))
{
    g_UserCallback = callback;
    g_UserContext = ctx;
    g_AllowedContexts.SetCapacity(4);

    memset(&g_SignalHandlers[0], 0, sizeof(g_SignalHandlers));
    SetHandlers();
}

void EnableDefaultSignalHandlers(JavaVM* vm)
{
    EnableSignalHandlers((void*)vm, DefaultJniSignalHandler);
}

void DisableSignalHandlers()
{
    UnsetHandlers();
    g_UserCallback = 0;
    g_UserContext = 0;
}

// For unit tests to make sure it still works
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
    if (output_cursor < output_size) \
    { \
        int result = dmSnPrintf(output + output_cursor, output_size - output_cursor, __VA_ARGS__); \
        if (result == -1) { \
            output_cursor = output_size; \
        } else { \
            output_cursor += result; \
        } \
    }


#ifdef __APPLE__

char* GenerateCallstack(char* output, uint32_t output_size)
{
    // Call this function to get a backtrace.
    unw_cursor_t cursor;
    unw_context_t context;

    // Initialize cursor to current frame for local unwinding.
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    int output_cursor = 0;

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

char* GenerateCallstack(char* output, uint32_t output_size)
{
    int output_cursor = 0;

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

char* GenerateCallstack(char* output, uint32_t output_size)
{
    int output_cursor = 0;

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
