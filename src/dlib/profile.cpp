#include <string.h>
#include "profile.h"

#include "math.h"

namespace dmProfile
{
    dmArray<Sample> g_Samples;
    dmArray<Scope> g_Scopes;
    uint32_t g_Depth = 0;
    uint32_t g_BeginTime = 0;
    uint64_t g_TicksPerSecond = 1000000;
    float g_FrameTime = 0.0f;
    float g_MaxFrameTime = 0.0f;
    uint32_t g_MaxFrameTimeCounter = 0;
    bool g_OutOfScopes = false;
    bool g_OutOfSamples = false;
    bool g_MaxDepthReached = false;

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
            g_Scopes[i].m_Count = 0;
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
        g_FrameTime = 0.0f;
        g_OutOfScopes = false;
        g_OutOfSamples = false;
        g_MaxDepthReached = false;
    }

    void End()
    {
        g_Depth = 0xffffffff;
        if (g_Scopes.Size() > 0)
        {
            g_FrameTime = g_Scopes[0].m_Elapsed/(0.001f * g_TicksPerSecond);
            for (uint32_t i = 1; i < g_Scopes.Size(); ++i)
            {
                float time = g_Scopes[i].m_Elapsed/(0.001f * g_TicksPerSecond);
                g_FrameTime = dmMath::Select(g_FrameTime - time, g_FrameTime, time);
            }
            ++g_MaxFrameTimeCounter;
            if (g_MaxFrameTimeCounter > 60 || g_FrameTime > g_MaxFrameTime)
            {
                g_MaxFrameTimeCounter = 0;
                g_MaxFrameTime = g_FrameTime;
            }
        }
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
            s.m_Count = 0;
            s.m_Entered = 0;
            g_Scopes.Push(s);
            return &g_Scopes[i];
        }
    }

    float GetFrameTime()
    {
        return g_FrameTime;
    }

    float GetMaxFrameTime()
    {
        return g_MaxFrameTime;
    }

    uint64_t GetTicksPerSecond()
    {
        return g_TicksPerSecond;
    }

    bool IsMaxDepthReached()
    {
        return g_MaxDepthReached;
    }

    bool IsOutOfScopes()
    {
        return g_OutOfScopes;
    }

    bool IsOutOfSamples()
    {
        return g_OutOfSamples;
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

