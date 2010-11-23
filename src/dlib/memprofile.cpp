#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "profile.h"
#include "memprofile.h"

#ifndef _MSC_VER

#include <stdlib.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>
#include "profile.h"

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
#if defined(__MACH__) or defined(__linux__)
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

// Not available on WIN32 - yet.
#ifndef _MSC_VER

namespace dmMemProfile
{
    void InitializeLibrary(dmMemProfile::InternalData*);
}

extern "C"
void dmMemProfileInitializeLibrary(dmMemProfile::InternalData* internal_data)
{
    dmMemProfile::InitializeLibrary(internal_data);
}

namespace dmMemProfile
{
    void __attribute__ ((destructor)) FinalizeLibrary();

    dmMemProfileParams g_MemProfileParams;

    Stats* g_ExtStats = 0;
    void (*g_AddCounter)(const char*, uint32_t);

    void InitializeLibrary(dmMemProfile::InternalData* internal_data)
    {
        char buf[256];
        sprintf(buf, "dmMemProfile loaded\n");
        write(2, buf, strlen(buf));

        *internal_data->m_IsEnabled = true;
        g_ExtStats = internal_data->m_Stats;
        g_AddCounter = internal_data->m_AddCounter;
    }

    void FinalizeLibrary()
    {
    }
}

extern "C"
void *malloc(size_t size)
{
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
