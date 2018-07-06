#if defined(_WIN32)
#include "safe_windows.h"
#elif  defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#include <sys/time.h>
#endif

#include <algorithm>
#include <string.h>
#include "dlib.h"
#include "log.h"

#include "hashtable.h"
#include "hash.h"
#include "spinlock.h"
#include "stringpool.h"
#include "math.h"
#include "time.h"
#include "thread.h"
#include "dstrings.h"
#include "array.h"
#include "profile.h"

namespace dmProfile
{
    static const uint32_t PROFILE_BUFFER_COUNT = 3;

    static dmArray<Scope> g_Scopes;

    static dmHashTable32<uint32_t> g_CountersTable;
    static dmArray<Counter> g_Counters;

    struct Profile
    {
        dmArray<Sample>      m_Samples;
        dmArray<CounterData> m_CountersData;
        dmArray<ScopeData>   m_ScopesData;
        uint32_t             m_ScopeCount;
        uint32_t             m_CounterCount;
    };

    // Default profile if not dmProfile::Initialize is invoked
    static Profile  g_EmptyProfile;

    // Current active profile
    static Profile* g_ActiveProfile = &g_EmptyProfile;

    static Profile g_AllProfiles[PROFILE_BUFFER_COUNT];
    static dmArray<Profile*> g_FreeProfiles;

    // Mapping of strings. Use when sending profiling data over HTTP
    static dmHashTable<uintptr_t, const char*> g_StringTable;
    static dmStringPool::HPool g_StringPool = 0;

    static uint32_t g_BeginTime = 0;
    static uint64_t g_TicksPerSecond = 1000000;
    static float g_FrameTime = 0.0f;
    static float g_MaxFrameTime = 0.0f;
    static uint32_t g_MaxFrameTimeCounter = 0;
    static bool g_OutOfScopes = false;
    static bool g_OutOfSamples = false;
    static bool g_OutOfCounters = false;
    bool g_IsInitialized = false;
    static bool g_Paused = false;
    static dmSpinlock::lock_t g_ProfileLock;

    static dmThread::TlsKey g_TlsKey = dmThread::AllocTls();
    static uint32_t g_ThreadCount = 0;

    // Used when out of scopes in order to remove conditional branches
    static ScopeData g_DummyScopeData;
    static Scope g_DummyScope = { "foo", 0, &g_DummyScopeData };

    struct InitSpinLocks
    {
        InitSpinLocks()
        {
            dmSpinlock::Init(&g_ProfileLock);
        }
    };

    static InitSpinLocks g_InitSpinlocks;

    static void HttpHeader(void* user_data, const char* key, const char* value)
    {
    }

    void Initialize(uint32_t max_scopes, uint32_t max_samples, uint32_t max_counters)
    {
        if (!dLib::IsDebugMode())
            return;

        if (g_Scopes.Capacity() > 0 && g_Scopes.Capacity() != max_scopes)
        {
            dmLogError("Failed to initialize profiler. It's not valid change number of scopes.");
            assert(0);
            return;
        }

        g_StringTable.SetCapacity(1024, 1200); // Rather arbitrary...
        g_StringPool = dmStringPool::New();

        if (g_Scopes.Capacity() == 0)
        {
            // Only allocate first time and leave already allocated scopes as is
            // Scopes are static variables in functions
            g_Scopes.SetCapacity(max_scopes);
            g_Scopes.SetSize(0);
        }

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

            p->m_ScopeCount = 0;
            p->m_CounterCount = 0;

            g_FreeProfiles.Push(p);
        }

        g_ActiveProfile = g_FreeProfiles[0];
        g_FreeProfiles.EraseSwap(0);

        /*
          Set up initial scope-data for the active profile as scope-data
          is used in CalculateScopeProfile before initial profile swap is performed.
          We now keep scopes over Initialize and Finalize.
          Before that change all scopes where reallocated and the scope-data
          was initialized at scope-allocation, see AllocateScope
         */
        uint32_t n = g_Scopes.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            ScopeData* scope_data = &g_ActiveProfile->m_ScopesData[i];
            scope_data->m_Elapsed = 0;
            scope_data->m_Count = 0;
            g_ActiveProfile->m_ScopesData[i].m_Scope = &g_Scopes[i];
        }

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

        g_StringTable.Clear();
        if (g_StringPool != 0)
            dmStringPool::Delete(g_StringPool);
        g_StringPool = 0;
        g_IsInitialized = false;
    }

    static void CalculateScopeProfileThread(Profile* profile, const uint32_t* key, uint8_t* value)
    {
        const uint32_t n_scopes = g_Scopes.Size();
        const uint32_t n_samples = profile->m_Samples.Size();
        const uint32_t thread_id = *key;

        for (uint32_t i = 0; i < n_scopes; ++i)
        {
            g_Scopes[i].m_Internal = 0;
        }

        g_DummyScope.m_Internal = 0;

        for (uint32_t i = 0; i < n_samples; ++i)
        {
            Sample* sample = &profile->m_Samples[i];

            if (g_StringTable.Get((uintptr_t) sample->m_Name) == 0)
            {
                if (g_StringTable.Full())
                {
                    dmLogWarning("String table full in profiler");
                }
                else
                {
                    g_StringTable.Put((uintptr_t) sample->m_Name, sample->m_Name);
                }
            }

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
                float millisPerTick = (float)(1000.0 / g_TicksPerSecond);
                g_FrameTime = profile->m_ScopesData[0].m_Elapsed * millisPerTick;
                for (uint32_t i = 1; i < g_Scopes.Size(); ++i)
                {
                    float time = profile->m_ScopesData[i].m_Elapsed * millisPerTick;
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

    static void CalculateScopeProfile(Profile* profile)
    {
        // First pass is to determine count of active threads
        const uint32_t table_size = 16;
        const uint32_t capacity = 64;
        typedef dmHashTable<uint32_t, uint8_t> ActiveThreadsT;
        char hash_buf[table_size * sizeof(uint32_t) + capacity * sizeof(ActiveThreadsT::Entry)];

        ActiveThreadsT active_threads(hash_buf, table_size, capacity);
        const uint32_t n_samples = profile->m_Samples.Size();
        for (uint32_t i = 0; i < n_samples; ++i)
        {
            Sample* sample = &profile->m_Samples[i];
            if (!active_threads.Get(sample->m_ThreadId))
            {
                if (active_threads.Full())
                {
                    dmLogError("Thread set exceeded in profiler!");
                    break;
                }
                active_threads.Put(sample->m_ThreadId, 1);
            }
        }

        active_threads.Iterate(&CalculateScopeProfileThread, profile);
    }

    static uint64_t GetNowTicks()
    {
        uint64_t now;
#if defined(_WIN32)
        QueryPerformanceCounter((LARGE_INTEGER *) &now);
#elif defined(__EMSCRIPTEN__)
        now = (uint64_t)(emscripten_get_now() * 1000.0);
#else
        timeval tv;
        gettimeofday(&tv, 0);
        now = tv.tv_sec * 1000000 + tv.tv_usec;
#endif
        return now;
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
        ret->m_ScopeCount = g_Scopes.Size();
        ret->m_CounterCount = g_Counters.Size();

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

        g_BeginTime = GetNowTicks();
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
        if (!g_IsInitialized)
        {
            return;
        }

        if (!profile)
            return;

        dmSpinlock::Lock(&g_ProfileLock);
        g_FreeProfiles.Push(profile);
        dmSpinlock::Unlock(&g_ProfileLock);
    }

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
        // NOTE: We can't take the spinlock if paused
        // as it might already been taken in dmProfile:Begin()
        // A deadlock case occur in dmMessage::Post if http-server is logging
        // TODO: Use mutex instead?
        if (g_Paused)
        {
            return &g_DummySample;
        }

        dmSpinlock::Lock(&g_ProfileLock);
        Profile* profile = g_ActiveProfile;

        bool full = profile->m_Samples.Full();
        if (full)
        {
            g_OutOfSamples = true;
            dmSpinlock::Unlock(&g_ProfileLock);
            return &g_DummySample;
        }
        else
        {
            void* tls_data = dmThread::GetTlsValue(g_TlsKey);
            if (tls_data == 0)
            {
                // NOTE: We store thread_id + 1. Otherwise we can't differentiate between thread-id 0 and not initialized
                int32_t thread_id = g_ThreadCount + 1;
                g_ThreadCount++;
                dmThread::SetTlsValue(g_TlsKey, (void*) thread_id);
                tls_data = (void*) thread_id;
            }
            intptr_t thread_id = ((intptr_t) tls_data) - 1;
            assert(thread_id >= 0);

            uint32_t size = profile->m_Samples.Size();
            profile->m_Samples.SetSize(size + 1);
            Sample* ret = &profile->m_Samples[size];
            ret->m_ThreadId = thread_id;
            dmSpinlock::Unlock(&g_ProfileLock);
            return ret;
        }
    }

    const char* Internalize(const char* string)
    {
        dmSpinlock::Lock(&g_ProfileLock);
        if (g_StringPool) {
            const char* s = dmStringPool::Add(g_StringPool, string);
            dmSpinlock::Unlock(&g_ProfileLock);
            return s;
        } else {
            dmSpinlock::Unlock(&g_ProfileLock);
            return "PROFILER NOT INITIALIZED";
        }
    }

    uint32_t GetNameHash(const char* name)
    {
        uintptr_t p = (uintptr_t)name;
        return (uint32_t)((p >> 2) * 0x9E3779B97F4A7C15llu);
    }

    static void AddCounterHash(const char* name, uint32_t name_hash, uint32_t amount)
    {
        // dmProfile::Initialize allocates memory. Is memprofile is activated this function is called from overloaded malloc while g_CountersTable is being created. No good!
        if (!g_IsInitialized || g_Paused)
            return;

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
            c->m_NameHash = name_hash;

            CounterData* cd = &profile->m_CountersData[new_index];
            cd->m_Counter = c;
            cd->m_Value = 0;

            g_CountersTable.Put(name_hash, new_index);

            counter_index = g_CountersTable.Get(name_hash);
        }

        profile->m_CountersData[*counter_index].m_Value += amount;
        dmSpinlock::Unlock(&g_ProfileLock);
    }

    void AddCounter(const char* name, uint32_t amount)
    {
        uint32_t name_hash = GetNameHash(name);
        AddCounterHash(name, name_hash, amount);
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

    void IterateStrings(HProfile profile, void* context, void (*call_back)(void* context, const uintptr_t* key, const char** value))
    {
        g_StringTable.Iterate(call_back, context);
    }

    void IterateScopes(HProfile profile, void* context, void (*call_back)(void* context, const Scope* scope_data))
    {
        uint32_t n = g_Scopes.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            Scope* scope = &g_Scopes[i];
            call_back(context, scope);
        }
    }

    void IterateScopeData(HProfile profile, void* context, void (*call_back)(void* context, const ScopeData* scope_data))
    {
        uint32_t n = profile->m_ScopeCount;
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

    void IterateCounters(HProfile profile, void* context, void (*call_back)(void* context, const Counter* counter))
    {
        uint32_t n = g_Counters.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            Counter* counter = &g_Counters[i];
            call_back(context, counter);
        }
    }

    void IterateCounterData(HProfile profile, void* context, void (*call_back)(void* context, const CounterData* counter))
    {
        uint32_t n = profile->m_CounterCount;
        for (uint32_t i = 0; i < n; ++i)
        {
            call_back(context, &profile->m_CountersData[i]);
        }
    }

    void ProfileScope::StartScope(Scope* scope, const char* name)
    {
        uint64_t start = GetNowTicks();
        Sample*s = AllocateSample();
        s->m_Name = name;
        s->m_Scope = scope;
        s->m_Start = (uint32_t)(start - g_BeginTime);
        m_Sample = s;
    }

    void ProfileScope::EndScope()
    {
        uint64_t end = GetNowTicks();
        m_Sample->m_Elapsed = (uint32_t)(end - g_BeginTime) - m_Sample->m_Start;
    }
}

