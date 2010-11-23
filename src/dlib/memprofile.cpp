#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "profile.h"
#include "memprofile.h"

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

    bool IsEnabled()
    {
        return g_IsEnabled;
    }

    void GetStats(Stats* stats)
    {
        *stats = g_Stats;
    }
}

// Internal function
extern "C"
{
dmMemProfile::InternalData dmMemProfileInternalData() __attribute__((visibility("default")));

dmMemProfile::InternalData dmMemProfileInternalData()
{
    dmMemProfile::InternalData ret;
    ret.m_Stats = &dmMemProfile::g_Stats;
    ret.m_IsEnabled = &dmMemProfile::g_IsEnabled;
    ret.m_AddCounter = dmProfile::AddCounter;
    return ret;
}
}

#endif

#ifdef DM_LIBMEMPROFILE

// Not available on WIN32 - yet.
#ifndef _MSC_VER

#include <stdlib.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>
#include "profile.h"

#ifdef __MACH__
#include <malloc/malloc.h>
#endif

// Code belonging to libdlib_profile. Not part of libdlib.

namespace dmMemProfile
{
    void __attribute__ ((constructor)) Init();
    void __attribute__ ((destructor)) Cleanup();

    dmMemProfileParams g_MemProfileParams;

    Stats* g_ExtStats = 0;
    void (*g_AddCounter)(const char*, uint32_t);

    void Init(void)
    {
        char buf[256];
        sprintf(buf, "dmMemProfile loaded\n");
        write(2, buf, strlen(buf));

        dmInitMemProfile init = (dmInitMemProfile) dlsym(RTLD_DEFAULT, "dmInitMemProfile");

        dmMemProfile::InternalData (*GetInternalData)() = (dmMemProfile::InternalData (*)()) dlsym(RTLD_DEFAULT, "dmMemProfileInternalData");
        if (!GetInternalData)
        {
            sprintf(buf, "ERROR: dmMemProfileInternalData not found.\n");
            write(2, buf, strlen(buf));
            exit(1);
        }

        dmMemProfile::InternalData data = GetInternalData();
        *data.m_IsEnabled = true;

        g_ExtStats = data.m_Stats;
        g_AddCounter = data.m_AddCounter;

        if (init)
        {
            sprintf(buf, "dmInitMemProfile function found. Initializing.\n");
            write(2, buf, strlen(buf));
            memset(&g_MemProfileParams, 0, sizeof(g_MemProfileParams));
            init(&g_MemProfileParams);
            if (!g_MemProfileParams.m_BeginFrame)
            {
                sprintf(buf, "WARNING: dmMemProfileParams.m_BeginFrame not set.\n");
                write(2, buf, strlen(buf));
            }
        }
        else
        {
            sprintf(buf, "WARNING: No dmInitMemProfile function found. Frame based profiling not possible.\n");
            write(2, buf, strlen(buf));
        }
    }

    void Cleanup()
    {
    }
}

extern "C"
void *malloc(size_t size)
{
    char buf[256];

    static void *(*mallocp)(size_t size) = 0;
    char *error;

    // get address of libc malloc
    if (!mallocp)
    {
        mallocp = (void* (*)(size_t)) dlsym(RTLD_NEXT, "malloc");
        if ((error = dlerror()) != NULL)
        {
            write(2, error, strlen(error));
            exit(1);
        }
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

        if (dmMemProfile::g_ExtStats)
        {
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_TotalAllocated, (uint32_t) usable_size);
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_TotalActive, (uint32_t) usable_size);
            dmAtomicAdd32(&dmMemProfile::g_ExtStats->m_AllocationCount, 1U);

            dmMemProfile::g_AddCounter("Memory.Allocations", 1U);
            dmMemProfile::g_AddCounter("Memory.Amount", usable_size);
        }
    }
    return ptr;
}

void free(void *ptr)
{
    static void (*freep)(void *) = 0;
    char *error;

    // get address of libc free
    if (!freep)
    {
        freep = (void(*)(void*)) dlsym(RTLD_NEXT, "free");
        if ((error = dlerror()) != NULL)
        {
            write(2, error, strlen(error));
            exit(1);
        }
    }

    if (dmMemProfile::g_ExtStats)
    {
        size_t usable_size = 0;
#if defined(__linux__)
        usable_size = malloc_usable_size(ptr);
#elif defined(__MACH__)
        usable_size = malloc_size(ptr);
#else
#error "Unsupported platform"
#endif

        dmAtomicSub32(&dmMemProfile::g_ExtStats->m_TotalActive, (uint32_t) usable_size);
    }

    freep(ptr);
}

void* operator new(size_t size)
{
    return malloc(size);
}

void operator delete(void* ptr)
{
    free(ptr);
}

void* operator new[](size_t size)
{
    return malloc(size);
}

void operator delete[](void* ptr)
{
    free(ptr);
}

#endif // _MSC_VER

#endif
