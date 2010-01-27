#include <string.h>
#include "profile.h"

namespace dmProfile
{
    dmArray<Sample> g_Samples;
    dmArray<Scope> g_Scopes;
    uint32_t g_Depth = 0;
    uint32_t g_BeginTime = 0;
    uint64_t g_TicksPerSecond = 1000000;

    void Initialize(uint32_t max_scopes, uint32_t max_samples)
    {
        g_Scopes.SetCapacity(max_scopes);
        g_Scopes.SetSize(0);
        g_Samples.SetCapacity(max_samples);
#if defined(_WIN32)
        QueryPerformanceFrequency((LARGE_INTEGER *) &g_TicksPerSecond);
#endif
    }

    void Finalize()
    {
        g_Samples.SetCapacity(0);
    }

    void Begin()
    {
        uint32_t n = g_Scopes.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            g_Scopes[i].m_Elapsed = 0;
            g_Scopes[i].m_Samples = 0;
        }

        g_Samples.SetSize(0);
        g_Depth = 0;

#if defined(_WIN32)
        uint64_t pcnt;
        QueryPerformanceCounter((LARGE_INTEGER *) &pcnt);
        g_BeginTime = (uint32_t) pcnt;
#else
        timeval tv;
        gettimeofday(&tv, 0);
        g_BeginTime = tv.tv_sec * 1000000 + tv.tv_usec;
#endif
    }

    void End()
    {
        g_Depth = 0xffffffff;
    }

    Scope* AllocateScope(const char* name)
    {
        if (g_Scopes.Full())
        {
            return 0;
        }
        else
        {
            // NOTE: Not optimal with O(n) but scopes are allocated only once
            uint32_t n = g_Scopes.Size();
            for (uint32_t i = 0; i < n; ++i)
            {
                if (strcmp(name, g_Scopes[i].m_Name) == 0)
                    return &g_Scopes[i];
            }

            uint32_t i = g_Scopes.Size();
            Scope s;
            s.m_Name = name;
            s.m_Elapsed = 0;
            s.m_Index = i;
            g_Scopes.Push(s);
            return &g_Scopes[i];
        }
    }

    uint64_t GetTicksPerSecond()
    {
        return g_TicksPerSecond;
    }

    void IterateScopes(void* context, void (*call_back)(void* context, const Scope* sample))
    {
        uint32_t n = g_Scopes.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            call_back(context, &g_Scopes[i]);
        }
    }

    void IterateSamples(void* context, void (*call_back)(void* context, const Sample* sample))
    {
        uint32_t n = g_Samples.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            call_back(context, &g_Samples[i]);
        }
    }
}

