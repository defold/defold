#include <algorithm>
#include <string.h>
#include "profile.h"
#include "hashtable.h"
#include "hash.h"
#include "spinlock.h"

#include "math.h"

namespace dmProfile
{
    dmArray<Sample> g_Samples;
    dmArray<Scope> g_Scopes;
    dmArray<Counter> g_Counters;
    uint32_t g_Depth = 0;
    uint32_t g_BeginTime = 0;
    uint64_t g_TicksPerSecond = 1000000;
    float g_FrameTime = 0.0f;
    float g_MaxFrameTime = 0.0f;
    uint32_t g_MaxFrameTimeCounter = 0;
    bool g_OutOfScopes = false;
    bool g_OutOfSamples = false;
    bool g_OutOfCounters = false;
    bool g_MaxDepthReached = false;
    bool g_IsInitialized = false;
    dmSpinlock::lock_t g_CounterLock;

    void Initialize(uint32_t max_scopes, uint32_t max_samples, uint32_t max_counters)
    {
        g_Scopes.SetCapacity(max_scopes);
        g_Scopes.SetSize(0);
        g_Samples.SetCapacity(max_samples);
        g_Counters.SetCapacity(max_counters);
#if defined(_WIN32)
        QueryPerformanceFrequency((LARGE_INTEGER *) &g_TicksPerSecond);
#endif
        dmSpinlock::Init(&g_CounterLock);
        g_IsInitialized = true;
    }

    void Finalize()
    {
        g_Samples.SetCapacity(0);
        g_IsInitialized = false;
    }

    void Begin()
    {
        uint32_t n = g_Scopes.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            g_Scopes[i].m_Elapsed = 0;
            g_Scopes[i].m_Count = 0;
        }

        n = g_Counters.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            g_Counters[i].m_Counter = 0;
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
        g_OutOfCounters = false;
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

    static inline bool CounterPred(const Counter& c1, const Counter& c2)
    {
        return c1.m_NameHash < c2.m_NameHash;
    }

    static Counter* FindCounter(uint32_t name_hash)
    {
        if (g_Counters.Size() == 0) return 0;

        Counter* start = &g_Counters[0];
        Counter* end = &g_Counters[0] + g_Counters.Size();
        Counter tmp;
        tmp.m_NameHash = name_hash;
        Counter*c = std::lower_bound(start, end, tmp, &CounterPred);
        if (c != end && c->m_NameHash == name_hash)
            return c;
        else
            return 0;
    }

    void AddCounter(const char* name, uint32_t amount)
    {
        uint32_t name_hash = dmHashString32(name);
        AddCounterHash(name, name_hash, amount);
    }

    void AddCounterHash(const char* name, uint32_t name_hash, uint32_t amount)
    {
        // Make sure that the spin-lock below is initialized
        if (!g_IsInitialized)
            return;

        dmSpinlock::Lock(&g_CounterLock);
        Counter* counter = FindCounter(name_hash);

        if (!counter)
        {
            if (g_Counters.Full())
            {
                g_OutOfCounters = true;
                dmSpinlock::Unlock(&g_CounterLock);
                return;
            }

            Counter c;
            c.m_Name = name;
            c.m_NameHash = dmHashString32(name);
            c.m_Counter = 0;
            g_Counters.Push(c);

            Counter* start = &g_Counters[0];
            Counter* end = &g_Counters[0] + g_Counters.Size();
            std::sort(start, end, &CounterPred);
            counter = FindCounter(name_hash);
        }

        dmSpinlock::Unlock(&g_CounterLock);
        dmAtomicAdd32(&counter->m_Counter, amount);
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

    void IterateCounters(void* context, void (*call_back)(void* context, const Counter* counter))
    {
        uint32_t n = g_Counters.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            call_back(context, &g_Counters[i]);
        }
    }
}

