#include <algorithm>
#include <string.h>
#include "profile.h"
#include "hashtable.h"
#include "hash.h"
#include "spinlock.h"
#include "math.h"
#include "time.h"
#include "thread.h"

namespace dmProfile
{
    const uint32_t PROFILE_BUFFER_COUNT = 3;

    dmArray<Scope> g_Scopes;

    dmHashTable32<uint32_t> g_CountersTable;
    dmArray<Counter> g_Counters;

    struct Profile
    {
        dmArray<Sample>      m_Samples;
        dmArray<CounterData> m_CountersData;
        dmArray<ScopeData>   m_ScopesData;
    };

    // Default profile if not dmProfile::Initialize is invoked
    Profile  g_EmptyProfile;

    // Current active profile
    Profile* g_ActiveProfile = &g_EmptyProfile;

    Profile g_AllProfiles[PROFILE_BUFFER_COUNT];
    dmArray<Profile*> g_FreeProfiles;

    uint32_t g_BeginTime = 0;
    uint64_t g_TicksPerSecond = 1000000;
    float g_FrameTime = 0.0f;
    float g_MaxFrameTime = 0.0f;
    uint32_t g_MaxFrameTimeCounter = 0;
    bool g_OutOfScopes = false;
    bool g_OutOfSamples = false;
    bool g_OutOfCounters = false;
    bool g_IsInitialized = false;
    bool g_Paused = false;
    dmSpinlock::lock_t g_ProfileLock;

    dmThread::TlsKey g_TlsKey = dmThread::AllocTls();
    int32_atomic_t g_ThreadCount = 0;

    struct InitSpinLocks
    {
        InitSpinLocks()
        {
            dmSpinlock::Init(&g_ProfileLock);
        }
    };

    InitSpinLocks g_InitSpinlocks;

    void Initialize(uint32_t max_scopes, uint32_t max_samples, uint32_t max_counters)
    {
        if (g_Scopes.Capacity() > 0 && g_Scopes.Capacity() != max_scopes)
        {
            dmLogError("Failed to initialize profiler. It's not valid change number of scopes.");
            assert(0);
            return;
        }

        g_Scopes.SetCapacity(max_scopes);
        g_Scopes.SetSize(0);

        g_FreeProfiles.SetCapacity(PROFILE_BUFFER_COUNT);
        g_FreeProfiles.SetSize(0); // Could be > 0 if Initialized is called again after Finalize

        for (uint32_t i = 0; i < PROFILE_BUFFER_COUNT; ++i)
        {
            Profile* p = &g_AllProfiles[i];

            p->m_Samples.SetCapacity(max_samples);
            p->m_Samples.SetSize(0); // Could be > 0 if Initialized is called again after Finalize

            p->m_CountersData.SetCapacity(max_counters);
            p->m_CountersData.SetSize(max_counters);

            p->m_ScopesData.SetCapacity(max_scopes);
            p->m_ScopesData.SetSize(max_scopes);

            g_FreeProfiles.Push(p);
        }

        g_ActiveProfile = g_FreeProfiles[0];
        g_FreeProfiles.EraseSwap(0);

        g_CountersTable.SetCapacity(dmMath::Max(16U,  2 * max_counters / 3), max_counters);
        g_CountersTable.Clear();

        g_Counters.SetCapacity(max_counters);
        g_Counters.SetSize(0);

#if defined(_WIN32)
        QueryPerformanceFrequency((LARGE_INTEGER *) &g_TicksPerSecond);
#endif

        g_IsInitialized = true;
    }

    void Finalize()
    {
        // NOTE: We do not clear g_Scopes here
        // Might be dangerous as we have static references to Scope* in functions due to DM_PROFILE
        // See Initialize. It's not even valid to change the number of scopes

        for (uint32_t i = 0; i < PROFILE_BUFFER_COUNT; ++i)
        {
            Profile*p = &g_AllProfiles[i];

            p->m_Samples.SetCapacity(0);
            p->m_CountersData.SetCapacity(0);
        }

        g_CountersTable.Clear();
        g_Counters.SetCapacity(0);

        g_ActiveProfile = &g_EmptyProfile;

        g_IsInitialized = false;
    }

    static void CalculateScopeProfile(Profile* profile)
    {
        uint32_t n_samples = profile->m_Samples.Size();
        uint32_t n_scopes = g_Scopes.Size();

        for (int32_t thread_id = 0; thread_id < g_ThreadCount; ++thread_id)
        {
            for (uint32_t i = 0; i < n_scopes; ++i)
            {
                g_Scopes[i].m_Internal = 0;
            }

            for (uint32_t i = 0; i < n_samples; ++i)
            {
                Sample* sample = &profile->m_Samples[i];

                // Does this sample belong to current thread?
                if (sample->m_ThreadId != thread_id)
                    continue;

                Scope* scope = sample->m_Scope;

                if (scope->m_Internal == 0)
                {
                    // First sample for this scope found
                    scope->m_Internal = sample;
                }
                else
                {
                    // Check if sample is overlapping the last sample
                    // If overlapping ignore the sample. We are only interested in the
                    // total time spent in top scope
                    Sample* last_sample = (Sample*) scope->m_Internal;
                    uint32_t end_last = last_sample->m_Start + last_sample->m_Elapsed;
                    if (sample->m_Start >= last_sample->m_Start && sample->m_Start < end_last)
                    {
                        // New simple within, ignore
                    }
                    else
                    {
                        // Close the last scope and set new sample to current
                        ScopeData* scope_data = &profile->m_ScopesData[scope->m_Index];
                        scope_data->m_Elapsed += last_sample->m_Elapsed;
                        scope_data->m_Count++;
                        scope->m_Internal = sample;
                    }
                }
            }

            // Close non-closed scopes
            for (uint32_t i = 0; i < n_scopes; ++i)
            {
                Scope* scope = &g_Scopes[i];
                if (scope->m_Internal != 0)
                {
                    Sample* last_sample = (Sample*) scope->m_Internal;
                    // Does this sample belong to current thread?
                    if (last_sample->m_ThreadId != thread_id)
                        continue;

                    ScopeData* scope_data = &profile->m_ScopesData[scope->m_Index];
                    scope_data->m_Elapsed += last_sample->m_Elapsed;
                    scope_data->m_Count++;
                    scope->m_Internal = 0;
                }
            }


            // Frame-time is defined as the maximum scope in frame 0, ie main frame
            if (thread_id == 0)
            {
                if (g_Scopes.Size() > 0)
                {
                    g_FrameTime = profile->m_ScopesData[0].m_Elapsed / (0.001f * g_TicksPerSecond);
                    for (uint32_t i = 1; i < g_Scopes.Size(); ++i)
                    {
                        float time = profile->m_ScopesData[i].m_Elapsed / (0.001f * g_TicksPerSecond);
                        g_FrameTime = dmMath::Select(g_FrameTime - time, g_FrameTime, time);
                    }
                    ++g_MaxFrameTimeCounter;
                    if (g_MaxFrameTimeCounter > 60 || g_FrameTime > g_MaxFrameTime)
                    {
                        g_MaxFrameTimeCounter = 0;
                        g_MaxFrameTime = g_FrameTime;
                    }
                }
                else
                {
                    g_FrameTime = 0.0f;
                }
            }
        }
    }

    HProfile Begin()
    {
        if (!g_IsInitialized)
        {
            dmLogError("dmProfile is not initialized");
            return g_ActiveProfile;
        }

        dmSpinlock::Lock(&g_ProfileLock);

        CalculateScopeProfile(g_ActiveProfile);
        Profile* ret = g_ActiveProfile;

        int wait_count = 0;
        while (g_FreeProfiles.Size() == 0)
        {
            dmSpinlock::Unlock(&g_ProfileLock);
            dmTime::Sleep(1000 * 4);

            ++wait_count;
            if (wait_count % 100 == 0)
            {
                dmLogError("Waiting for a free profile...");
            }

            dmSpinlock::Lock(&g_ProfileLock);
        }

        Profile* profile = g_FreeProfiles[0];
        g_FreeProfiles.EraseSwap(0);
        g_ActiveProfile = profile;

        uint32_t n = g_Scopes.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            ScopeData* scope_data = &profile->m_ScopesData[i];
            scope_data->m_Elapsed = 0;
            scope_data->m_Count = 0;
            profile->m_ScopesData[i].m_Scope = &g_Scopes[i];
        }

        n = g_Counters.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            profile->m_CountersData[i].m_Counter = &g_Counters[i];
            profile->m_CountersData[i].m_Value = 0;
        }

        profile->m_Samples.SetSize(0);

#if defined(_WIN32)
        uint64_t pcnt;
        QueryPerformanceCounter((LARGE_INTEGER *) &pcnt);
        g_BeginTime = (uint32_t) pcnt;
#else
        timeval tv;
        gettimeofday(&tv, 0);
        g_BeginTime = tv.tv_sec * 1000000 + tv.tv_usec;
#endif
        g_OutOfScopes = false;
        g_OutOfSamples = false;
        g_OutOfCounters = false;

        dmSpinlock::Unlock(&g_ProfileLock);
        return ret;
    }

    void Pause(bool pause)
    {
        g_Paused = pause;
    }

    void Release(HProfile profile)
    {
        dmSpinlock::Lock(&g_ProfileLock);
        g_FreeProfiles.Push(profile);
        dmSpinlock::Unlock(&g_ProfileLock);
    }

    // Used when out of scopes in order to remove conditional branches
    ScopeData g_DummyScopeData;
    Scope g_DummyScope = { "foo", 0, &g_DummyScopeData };
    Scope* AllocateScope(const char* name)
    {
        dmSpinlock::Lock(&g_ProfileLock);
        if (g_Scopes.Full())
        {
            g_OutOfScopes = true;
            dmSpinlock::Unlock(&g_ProfileLock);
            return &g_DummyScope;
        }
        else
        {
            // NOTE: Not optimal with O(n) but scopes are allocated only once
            uint32_t n = g_Scopes.Size();
            for (uint32_t i = 0; i < n; ++i)
            {
                if (strcmp(name, g_Scopes[i].m_Name) == 0)
                {
                    dmSpinlock::Unlock(&g_ProfileLock);
                    return &g_Scopes[i];
                }
            }

            uint32_t i = g_Scopes.Size();
            g_Scopes.SetSize(i + 1);
            Scope* s = &g_Scopes[i];
            ScopeData* sd = &g_ActiveProfile->m_ScopesData[i];
            sd->m_Scope = s;
            sd->m_Elapsed = 0;
            sd->m_Count = 0;
            s->m_Name = name;
            s->m_Index = i;
            dmSpinlock::Unlock(&g_ProfileLock);
            return s;
        }
    }

    // Used when out of samples in order to remove conditional branches
    Sample g_DummySample = { "OUT_OF_SAMPLES", 0, 0, 0, 0 };
    Sample* AllocateSample()
    {
        dmSpinlock::Lock(&g_ProfileLock);
        Profile* profile = g_ActiveProfile;

        bool full = profile->m_Samples.Full();
        if (full || g_Paused)
        {
            g_OutOfSamples = full;
            dmSpinlock::Unlock(&g_ProfileLock);
            return &g_DummySample;
        }
        else
        {
            void* tls_data = dmThread::GetTlsValue(g_TlsKey);
            if (tls_data == 0)
            {
                // NOTE: We store thread_id + 1. Otherwise we can't differentiate between thread-id 0 and not initialized
                int32_t thread_id = dmAtomicIncrement32(&g_ThreadCount) + 1;
                dmThread::SetTlsValue(g_TlsKey, (void*) thread_id);
                tls_data = (void*) thread_id;
            }
            int32_t thread_id = ((int32_t) tls_data) - 1;
            assert(thread_id >= 0);

            uint32_t size = profile->m_Samples.Size();
            profile->m_Samples.SetSize(size + 1);
            Sample* ret = &profile->m_Samples[size];
            ret->m_ThreadId = thread_id;
            dmSpinlock::Unlock(&g_ProfileLock);
            return ret;
        }
    }

    static inline bool CounterPred(const Counter& c1, const Counter& c2)
    {
        return c1.m_NameHash < c2.m_NameHash;
    }

    void AddCounter(const char* name, uint32_t amount)
    {
        uint32_t name_hash = dmHashString32(name);
        AddCounterHash(name, name_hash, amount);
    }

    void AddCounterHash(const char* name, uint32_t name_hash, uint32_t amount)
    {
        dmSpinlock::Lock(&g_ProfileLock);
        Profile* profile = g_ActiveProfile;

        uint32_t* counter_index = g_CountersTable.Get(name_hash);

        if (!counter_index)
        {
            if (g_Counters.Full())
            {
                g_OutOfCounters = true;
                dmSpinlock::Unlock(&g_ProfileLock);
                return;
            }

            uint32_t new_index = g_Counters.Size();
            g_Counters.SetSize(new_index+1);

            Counter* c = &g_Counters[new_index];
            c->m_Name = name;
            c->m_NameHash = dmHashString32(name);

            CounterData* cd = &profile->m_CountersData[new_index];
            cd->m_Counter = c;
            cd->m_Value = 0;

            g_CountersTable.Put(c->m_NameHash, new_index);

            counter_index = g_CountersTable.Get(name_hash);
        }

        dmSpinlock::Unlock(&g_ProfileLock);
        dmAtomicAdd32(&profile->m_CountersData[*counter_index].m_Value, amount);
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

    bool IsOutOfScopes()
    {
        return g_OutOfScopes;
    }

    bool IsOutOfSamples()
    {
        return g_OutOfSamples;
    }

    void IterateScopes(HProfile profile, void* context, void (*call_back)(void* context, const ScopeData* scope_data))
    {
        uint32_t n = g_Scopes.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            call_back(context, &profile->m_ScopesData[i]);
        }
    }

    void IterateSamples(HProfile profile, void* context, void (*call_back)(void* context, const Sample* sample))
    {
        uint32_t n = profile->m_Samples.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            call_back(context, &profile->m_Samples[i]);
        }
    }

    void IterateCounters(HProfile profile, void* context, void (*call_back)(void* context, const CounterData* counter))
    {
        uint32_t n = g_Counters.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            call_back(context, &profile->m_CountersData[i]);
        }
    }
}

