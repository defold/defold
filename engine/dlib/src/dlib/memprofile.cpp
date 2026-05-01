// Copyright 2020-2026 The Defold Foundation
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

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <dlib/profile/profile.h>
#include <dlib/dstrings.h>

#include "memprofile.h"
#include "dlib.h"

// TODO: Reverse this if statement to set which platforms are actually supported!
#if !(defined(_MSC_VER) || defined(ANDROID) || defined(__EMSCRIPTEN__) || defined(DM_PLATFORM_VENDOR))

#include <stdlib.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include "shared_library.h"

#ifdef __MACH__
#include <malloc/malloc.h>


#ifdef DM_LIBMEMPROFILE

#include <mach/mach.h>
#include <mach-o/dyld_images.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static void GetBaseAddress(uint64_t* out_base, uint64_t* out_size)
{
    *out_base = 0;
    *out_size = 0;

    struct dyld_all_image_infos* infos = 0;
    {
        task_dyld_info_data_t dyld_info;
        mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;

        if (task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&dyld_info, &count) == KERN_SUCCESS)
        {
            infos = (struct dyld_all_image_infos *)dyld_info.all_image_info_addr;
        }
    }

    if (!infos)
    {
        return;
    }

    const dyld_image_info* last_info = &infos->infoArray[infos->infoArrayCount-1];
    // for (int i = 0; i < infos->infoArrayCount; ++i)
    // {
    //     const dyld_image_info* info = &infos->infoArray[i];
    //     printf("  module: %s  addr: %p\n", info->imageFilePath, (void*)info->imageLoadAddress);
    // }

    if (last_info)
    {
        *out_base = (uint64_t)last_info->imageLoadAddress;

        struct stat st;
        stat(last_info->imageFilePath, &st);
        *out_size = (uint64_t)st.st_size;
    }
}
#endif // DM_LIBMEMPROFILE

#else
#include <malloc.h>

#ifdef DM_LIBMEMPROFILE

static void GetBaseAddress(uint64_t* base, uint64_t* size)
{
}

#endif // DM_LIBMEMPROFILE

#endif

#endif

// Common
namespace dmMemProfile
{
    uint64_t g_BaseAddress = 0;
    uint64_t g_AppSize = 0;

    typedef ProfileIdx (*PropertyRegisterGroupFn)(const char* name, const char* desc, ProfileIdx* (*parentfn)());
    typedef ProfileIdx (*PropertyRegisterGroupU32Fn)(const char* name, const char* desc, uint32_t value, uint32_t flags, ProfileIdx* (*parentfn)());
    typedef void       (*PropertyAddU32Fn)(ProfileIdx idx, uint32_t value);

    struct InternalData
    {
        Stats* m_Stats;
        bool*  m_IsEnabled;
        PropertyRegisterGroupFn     m_PropertyRegisterGroup;
        PropertyRegisterGroupU32Fn  m_PropertyRegisterU32;
        PropertyAddU32Fn            m_PropertyAddU32;
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
#if defined(__MACH__) || defined(__linux__) && !defined(ANDROID) && !defined(__EMSCRIPTEN__)
        void (*init)(dmMemProfile::InternalData*) = (void (*)(dmMemProfile::InternalData*)) dlsym(RTLD_DEFAULT, "dmMemProfileInitializeLibrary");
        if (init)
        {
            dmMemProfile::InternalData data;
            data.m_Stats = &dmMemProfile::g_Stats;
            data.m_IsEnabled = &dmMemProfile::g_IsEnabled;
            data.m_PropertyRegisterGroup = ProfileRegisterPropertyGroup;
            data.m_PropertyRegisterU32 = ProfileRegisterPropertyU32;
            data.m_PropertyAddU32 = ProfilePropertyAddU32;

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
#if !(defined(_MSC_VER) || defined(ANDROID) || defined(__EMSCRIPTEN__))

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
    GetBaseAddress(&dmMemProfile::g_BaseAddress, &dmMemProfile::g_AppSize);
    printf("Base Address: %p sz: %u\n", (void*)(uintptr_t)dmMemProfile::g_BaseAddress, (uint32_t)dmMemProfile::g_AppSize);

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

    PropertyRegisterGroupFn     m_PropertyRegisterGroup;
    PropertyRegisterGroupU32Fn  m_PropertyRegisterU32;
    PropertyAddU32Fn            m_PropertyAddU32;

    ProfileIdx g_ProfileGroup = 0;
    ProfileIdx g_ProfileAllocations = 0;
    ProfileIdx g_ProfileAmount = 0;

    int g_TraceFile = -1;

    static ProfileIdx* GetProfileGroup()
    {
        return &g_ProfileGroup;
    }

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

        if (m_PropertyRegisterGroup)
        {
            g_ProfileGroup = m_PropertyRegisterGroup("Memory", "", 0);
            g_ProfileAllocations = m_PropertyRegisterU32("Allocations", "Num allocations", 0, 0, GetProfileGroup);
            g_ProfileAmount = m_PropertyRegisterU32("Amount", "Total amount (bytes)", 0, 0, GetProfileGroup);
        }

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

    void DumpBacktracePlatform(int file)
    {
        char buf[256];
        const int maxbt = 32;
        void* backt[maxbt];

        uintptr_t app_start = dmMemProfile::g_BaseAddress;
        uintptr_t app_end = app_start + dmMemProfile::g_AppSize;

        int n_bt = backtrace(backt, maxbt);

        for (int i = 0; i < n_bt; ++i)
        {
            uintptr_t addr = (uintptr_t)backt[i];
            if (addr >= app_start && addr < app_end)
            {
                addr -= app_start;
            }
            snprintf(buf, sizeof(buf), "%p ", (void*)addr);
            Log(file, buf);
        }
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
        snprintf(buf, sizeof(buf), "%c %p 0x%x ", type, ptr, (int32_t) size);
        Log(g_TraceFile, buf);

        DumpBacktracePlatform(g_TraceFile);

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

            if (dmMemProfile::m_PropertyAddU32)
            {
                dmMemProfile::m_PropertyAddU32(dmMemProfile::g_ProfileAllocations, 1U);
                dmMemProfile::m_PropertyAddU32(dmMemProfile::g_ProfileAmount, usable_size);
            }
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

            if (dmMemProfile::m_PropertyAddU32)
            {
                dmMemProfile::m_PropertyAddU32(dmMemProfile::g_ProfileAllocations, 1U);
                dmMemProfile::m_PropertyAddU32(dmMemProfile::g_ProfileAmount, usable_size);
            }
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

            if (dmMemProfile::m_PropertyAddU32)
            {
                dmMemProfile::m_PropertyAddU32(dmMemProfile::g_ProfileAllocations, 1U);
                dmMemProfile::m_PropertyAddU32(dmMemProfile::g_ProfileAmount, usable_size);
            }
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

            if (dmMemProfile::m_PropertyAddU32)
            {
                dmMemProfile::m_PropertyAddU32(dmMemProfile::g_ProfileAllocations, 1U);
                dmMemProfile::m_PropertyAddU32(dmMemProfile::g_ProfileAmount, usable_size);
            }
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

            if (dmMemProfile::m_PropertyAddU32)
            {
                dmMemProfile::m_PropertyAddU32(dmMemProfile::g_ProfileAllocations, 1U);
                dmMemProfile::m_PropertyAddU32(dmMemProfile::g_ProfileAmount, usable_size);
            }
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

#endif // SUPPORTED?

#endif
