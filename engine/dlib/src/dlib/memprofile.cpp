#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "profile.h"
#include "memprofile.h"

#if !(defined(_MSC_VER) || defined(ANDROID) || defined(__EMSCRIPTEN__) || defined(__AVM2__))

#include <stdlib.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include "dlib.h"
#include "profile.h"
#include "shared_library.h"

#ifdef __MACH__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#endif

// Common
namespace dmMemProfile
{
    struct InternalData
    {
        Stats* m_Stats;
        bool*  m_IsEnabled;
        void (*m_AddCounter)(const char*, uint32_t);
    };
}


#ifndef DM_LIBMEMPROFILE

namespace dmMemProfile
{
    // Common code and data
    Stats g_Stats = {0};
    bool g_IsEnabled = false;

    void Initialize()
    {
        if (!dLib::IsDebugMode())
            return;
#if defined(__MACH__) || defined(__linux__) && !defined(ANDROID) && !defined(__EMSCRIPTEN__) && !defined(__AVM2__)
        void (*init)(dmMemProfile::InternalData*) = (void (*)(dmMemProfile::InternalData*)) dlsym(RTLD_DEFAULT, "dmMemProfileInitializeLibrary");
        if (init)
        {
            dmMemProfile::InternalData data;
            data.m_Stats = &dmMemProfile::g_Stats;
            data.m_IsEnabled = &dmMemProfile::g_IsEnabled;
            data.m_AddCounter = dmProfile::AddCounter;

            init(&data);
        }
#endif
    }

    void Finalize()
    {
    }

    bool IsEnabled()
    {
        return g_IsEnabled;
    }

    void GetStats(Stats* stats)
    {
        *stats = g_Stats;
    }
}

#endif

#ifdef DM_LIBMEMPROFILE

// Code belonging to libdlib_profile. Not part of libdlib.

// Not available on WIN32 or Android - yet.
#if !(defined(_MSC_VER) || defined(ANDROID) || defined(__EMSCRIPTEN__) || defined(__AVM2__))

#ifdef __linux__
static void *null_calloc(size_t /*count*/, size_t /*size*/)
{
    return 0;
}
#endif

static void *(*mallocp)(size_t size) = 0;
static void *(*callocp)(size_t count, size_t size) = 0;
static void *(*reallocp)(void*, size_t size) = 0;
static int (*posix_memalignp)(void **memptr, size_t alignment, size_t size) = 0;
static void (*freep)(void *) = 0;
#ifndef __MACH__
static void *(*memalignp)(size_t, size_t) = 0;
#endif

static void Log(int file, const char* str)
{
    ssize_t ret = write(file, str, strlen(str));
    (void)ret;
}

static void* LoadFunction(const char* name)
{
    void* fn = dlsym (RTLD_NEXT, name);
    char* error = 0;
    if ((error = dlerror()) != NULL)
    {
        Log(STDERR_FILENO, error);
        exit(1);
    }
    return fn;
}

static void CreateHooks(void)
{
#ifdef __linux__
    // dlsym uses calloc. "Known" hack to return NULL for the first allocation
    // http://code.google.com/p/chromium/issues/detail?id=28244
    // http://src.chromium.org/viewvc/chrome/trunk/src/base/process_util_linux.cc?r1=32953&r2=32952
    callocp = null_calloc;
#endif
    callocp = (void *(*) (size_t, size_t)) LoadFunction("calloc");
    mallocp = (void *(*) (size_t)) LoadFunction("malloc");
    reallocp = (void *(*) (void*, size_t)) LoadFunction("realloc");
    posix_memalignp = (int (*)(void **, size_t, size_t)) LoadFunction("posix_memalign");
    freep = (void (*) (void*)) LoadFunction("free");

#ifndef __MACH__
    memalignp = (void *(*)(size_t, size_t)) LoadFunction("memalign");
#endif
}

namespace dmMemProfile
{
    void InitializeLibrary(dmMemProfile::InternalData*);
}

extern "C"
DM_DLLEXPORT void dmMemProfileInitializeLibrary(dmMemProfile::InternalData* internal_data)
{
    dmMemProfile::InitializeLibrary(internal_data);
}

namespace dmMemProfile
{
    void __attribute__ ((destructor)) FinalizeLibrary();

    pthread_mutex_t* g_Mutex = 0;
    Stats* g_ExtStats = 0;
    void (*g_AddCounter)(const char*, uint32_t) = 0;

    int g_TraceFile = -1;

    void InitializeLibrary(dmMemProfile::InternalData* internal_data)
    {
        Log(STDERR_FILENO, "dmMemProfile loaded\n");

        pthread_mutexattr_t attr;
        int ret = pthread_mutexattr_init(&attr);
        assert(ret == 0);
        ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        assert(ret == 0);

        g_Mutex = new pthread_mutex_t;
        ret = pthread_mutex_init(g_Mutex, &attr);
        assert(ret == 0);
        ret = pthread_mutexattr_destroy(&attr);
        assert(ret == 0);

        *internal_data->m_IsEnabled = true;
        g_ExtStats = internal_data->m_Stats;
        g_AddCounter = internal_data->m_AddCounter;

        char* trace = getenv("DMMEMPROFILE_TRACE");
        if (trace && strlen(trace) > 0 && trace[0] != '0')
        {
            g_TraceFile = open("memprofile.trace", O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
            if (g_TraceFile == -1)
            {
                Log(STDERR_FILENO, "WARNING: Unable to open memprofile.trace\n");
            }
        }
    }

    void FinalizeLibrary()
    {
        int ret;
        ret = pthread_mutex_lock(dmMemProfile::g_Mutex);
        assert(ret == 0);

        if (g_TraceFile != -1)
            close(g_TraceFile);
        g_TraceFile = -1;

        ret = pthread_mutex_unlock(dmMemProfile::g_Mutex);
        assert(ret == 0);
        // NOTE: We don't destory the mutex here. DumpBacktrace can run *after* FinalizeLibrary
        // We leak a mutex delibrity here
    }

    void DumpBacktrace(char type, void* ptr, uint32_t size)
    {
        static int32_atomic_t call_depth = 0;

        if (!dmMemProfile::g_Mutex)
            return;

        int ret = pthread_mutex_lock(dmMemProfile::g_Mutex);
        assert(ret == 0);

        if (g_TraceFile == -1)
        {
            ret = pthread_mutex_unlock(dmMemProfile::g_Mutex);
            assert(ret == 0);
            return;
        }

        // Avoid recursive calls to DumpBacktrace. backtrace can allocate memory...
        if (dmAtomicIncrement32(&call_depth) > 0)
        {
            dmAtomicDecrement32(&call_depth);
            return;
        }

        char buf[256];
        const int maxbt = 32;
        void* backt[maxbt];

        int n_bt = backtrace(backt, maxbt);

        sprintf(buf, "%c %p 0x%x ", type, ptr, (int32_t) size);
        Log(g_TraceFile, buf);

        for (int i = 0; i < n_bt; ++i)
        {
            sprintf(buf, "%p ", backt[i]);
            Log(g_TraceFile, buf);
        }
        Log(g_TraceFile, "\n");

        dmAtomicDecrement32(&call_depth);
        ret = pthread_mutex_unlock(dmMemProfile::g_Mutex);
        assert(ret == 0);
    }
}

extern "C"
DM_DLLEXPORT void *malloc(size_t size)
{
    if (!mallocp)
    {
        CreateHooks();
    }

    void *ptr = mallocp(size);

    if (ptr)
    {
        size_t usable_size = 0;
#if defined(__linux__)
        usable_size = malloc_usable_size(ptr);
#elif defined(__MACH__)
        usable_size = malloc_size(ptr);
#else
#error "Unsupported platform"
#endif

        dmMemProfile::DumpBacktrace('M', ptr, usable_size);

        if (dmMemProfile::g_ExtStats)
        {
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_TotalAllocated, (uint32_t) usable_size);
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_TotalActive, (uint32_t) usable_size);
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_AllocationCount, 1U);

            dmMemProfile::g_AddCounter("Memory.Allocations", 1U);
            dmMemProfile::g_AddCounter("Memory.Amount", usable_size);
        }
    }
    else
    {
        dmMemProfile::DumpBacktrace('M', 0, 0);
    }
    return ptr;
}

#ifndef __MACH__
extern "C"
DM_DLLEXPORT void *memalign(size_t alignment, size_t size)
{
    if (!memalignp)
    {
        CreateHooks();
    }

    void *ptr = memalignp(alignment, size);

    if (ptr)
    {
        size_t usable_size = 0;
#if defined(__linux__)
        usable_size = malloc_usable_size(ptr);
#elif defined(__MACH__)
        usable_size = malloc_size(ptr);
#else
#error "Unsupported platform"
#endif

        dmMemProfile::DumpBacktrace('M', ptr, usable_size);

        if (dmMemProfile::g_ExtStats)
        {
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_TotalAllocated, (uint32_t) usable_size);
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_TotalActive, (uint32_t) usable_size);
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_AllocationCount, 1U);

            dmMemProfile::g_AddCounter("Memory.Allocations", 1U);
            dmMemProfile::g_AddCounter("Memory.Amount", usable_size);
        }
    }
    else
    {
        dmMemProfile::DumpBacktrace('M', 0, 0);
    }
    return ptr;
}
#endif

extern "C"
DM_DLLEXPORT int posix_memalign(void **memptr, size_t alignment, size_t size)
{
    if (!posix_memalignp)
    {
        CreateHooks();
    }

    int ret = posix_memalignp(memptr, alignment, size);

    if (ret == 0)
    {
        size_t usable_size = 0;
#if defined(__linux__)
        usable_size = malloc_usable_size(*memptr);
#elif defined(__MACH__)
        usable_size = malloc_size(*memptr);
#else
#error "Unsupported platform"
#endif

        dmMemProfile::DumpBacktrace('M', *memptr, usable_size);

        if (dmMemProfile::g_ExtStats)
        {
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_TotalAllocated, (uint32_t) usable_size);
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_TotalActive, (uint32_t) usable_size);
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_AllocationCount, 1U);

            dmMemProfile::g_AddCounter("Memory.Allocations", 1U);
            dmMemProfile::g_AddCounter("Memory.Amount", usable_size);
        }
    }
    else
    {
        dmMemProfile::DumpBacktrace('M', 0, 0);
    }
    return ret;
}

extern "C"
DM_DLLEXPORT void *calloc(size_t count, size_t size)
{
    if (!callocp)
    {
        CreateHooks();
    }

    void *ptr = callocp(count, size);

    if (ptr)
    {
        size_t usable_size = 0;
#if defined(__linux__)
        usable_size = malloc_usable_size(ptr);
#elif defined(__MACH__)
        usable_size = malloc_size(ptr);
#else
#error "Unsupported platform"
#endif

        dmMemProfile::DumpBacktrace('M', ptr, usable_size);

        if (dmMemProfile::g_ExtStats)
        {
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_TotalAllocated, (uint32_t) usable_size);
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_TotalActive, (uint32_t) usable_size);
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_AllocationCount, 1U);

            dmMemProfile::g_AddCounter("Memory.Allocations", 1U);
            dmMemProfile::g_AddCounter("Memory.Amount", usable_size);
        }
    }
    else
    {
        dmMemProfile::DumpBacktrace('M', 0, 0);
    }
    return ptr;
}

extern "C"
DM_DLLEXPORT void *realloc(void* ptr, size_t size)
{
    // This simplifies the code below a bit
    if (ptr == 0)
        return malloc(size);

    if (!reallocp)
    {
        CreateHooks();
    }

    size_t old_usable_size = 0;
#if defined(__linux__)
    old_usable_size = malloc_usable_size(ptr);
#elif defined(__MACH__)
    old_usable_size = malloc_size(ptr);
#else
#error "Unsupported platform"
#endif

    void* old_ptr = ptr;
    ptr = reallocp(ptr, size);
    if (ptr)
    {
        size_t usable_size = 0;
#if defined(__linux__)
        usable_size = malloc_usable_size(ptr);
#elif defined(__MACH__)
        usable_size = malloc_size(ptr);
#else
#error "Unsupported platform"
#endif

        dmMemProfile::DumpBacktrace('F', old_ptr, old_usable_size);
        dmMemProfile::DumpBacktrace('M', ptr, usable_size);

        if (dmMemProfile::g_ExtStats)
        {
            dmAtomicSub32(&dmMemProfile::g_ExtStats->m_TotalActive, (uint32_t) old_usable_size);

            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_TotalAllocated, (uint32_t) usable_size);
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_TotalActive, (uint32_t) usable_size);
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_AllocationCount, 1U);

            dmMemProfile::g_AddCounter("Memory.Allocations", 1U);
            dmMemProfile::g_AddCounter("Memory.Amount", usable_size);
        }
    }
    else
    {
        // Nothing happended
    }
    return ptr;
}

#ifdef __MACH__
extern "C"
DM_DLLEXPORT void* reallocf(void *ptr, size_t size)
{
    free(ptr);
    return malloc(size);
}
#endif

DM_DLLEXPORT void free(void *ptr)
{
    if (!freep)
    {
        CreateHooks();
    }

    size_t usable_size = 0;
#if defined(__linux__)
        usable_size = malloc_usable_size(ptr);
#elif defined(__MACH__)
        usable_size = malloc_size(ptr);
#else
#error "Unsupported platform"
#endif

    if (dmMemProfile::g_ExtStats)
    {
        dmAtomicSub32(&dmMemProfile::g_ExtStats->m_TotalActive, (uint32_t) usable_size);
    }

    if (ptr)
        dmMemProfile::DumpBacktrace('F', ptr, usable_size);
    freep(ptr);
}

DM_DLLEXPORT void* operator new(size_t size)
{
    return malloc(size);
}

DM_DLLEXPORT void operator delete(void* ptr)
{
    free(ptr);
}

DM_DLLEXPORT void* operator new[](size_t size)
{
    return malloc(size);
}

DM_DLLEXPORT void operator delete[](void* ptr)
{
    free(ptr);
}

#endif // _MSC_VER

#endif
