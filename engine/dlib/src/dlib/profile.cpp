#include "profile.h"

#include <algorithm>
#include <string.h>

#if defined(_WIN32)
#include "safe_windows.h"
#elif  defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#include <sys/time.h>
#endif

#include "dlib.h"

#include "hashtable.h"
#include "hash.h"
#include "spinlock.h"
#include "stringpool.h"
#include "math.h"
#include "time.h"
#include "thread.h"
#include "array.h"

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
        uint32_t             m_ScopeCount;
        uint32_t             m_CounterCount;
    };

    // Default profile if not dmProfile::Initialize is invoked
    Profile g_EmptyProfile;

    // Current active profile
    Profile* g_ActiveProfile = &g_EmptyProfile;

    Profile g_AllProfiles[PROFILE_BUFFER_COUNT];
    dmArray<Profile*> g_FreeProfiles;

    // Mapping of strings. Use when sending profiling data over HTTP
    dmHashTable<uintptr_t, const char*> g_StringTable;
    dmStringPool::HPool g_StringPool = 0;

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

    // Used when out of scopes in order to remove conditional branches
    ScopeData g_DummyScopeData;
    Scope g_DummyScope = { "foo", 0u, 0, &g_DummyScopeData };

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

        g_CountersTable.SetCapacity(dmMath::Max(16U, 2 * max_counters / 3), max_counters);
        g_CountersTable.Clear();

        g_Counters.SetCapacity(max_counters);
        g_Counters.SetSize(0);

#if defined(_WIN32)
        QueryPerformanceFrequency((LARGE_INTEGER*)&g_TicksPerSecond);
#endif
        // Set g_BeginTime even if we haven't started since threads may calculate scopes outside of
        // engine Begin()/End() of profiles which happens in Engine::Step() - just so we don't get
        // totally crazy numbers if this happens
        g_BeginTime = GetNowTicks();
        g_IsInitialized = true;
    }

    void Finalize()
    {
        // NOTE: We do not clear g_Scopes here
        // Might be dangerous as we have static references to Scope* in functions due to DM_PROFILE
        // See Initialize. It's not even valid to change the number of scopes

        for (uint32_t i = 0; i < PROFILE_BUFFER_COUNT; ++i)
        {
            Profile* p = &g_AllProfiles[i];

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

            if (g_StringTable.Get((uintptr_t)sample->m_Name) == 0)
            {
                if (g_StringTable.Full())
                {
                    dmLogWarning("String table full in profiler");
                }
                else
                {
                    g_StringTable.Put((uintptr_t)sample->m_Name, sample->m_Name);
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
                Sample* last_sample = (Sample*)scope->m_Internal;
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
                Sample* last_sample = (Sample*)scope->m_Internal;
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

        DM_SPINLOCK_SCOPED_LOCK(g_ProfileLock)
        g_FreeProfiles.Push(profile);
    }

    uint32_t AllocateScope(const char* name)
    {
        DM_SPINLOCK_SCOPED_LOCK(g_ProfileLock)
        if (g_Scopes.Full())
        {
            g_OutOfScopes = true;
            return 0xffffffffu;
        }
        else
        {
            // NOTE: Not optimal with O(n) but scopes are allocated only once
            uint32_t n         = g_Scopes.Size();
            uint32_t name_hash = GetNameHash(name, (uint32_t)strlen(name));
            for (uint32_t i = 0; i < n; ++i)
            {
                if (g_Scopes[i].m_NameHash == name_hash)
                {
                    return i;
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
            s->m_NameHash = name_hash;
            s->m_Index = i;
            return i;
        }
    }

    Sample g_DummySample = { "OUT_OF_SAMPLES", 0, 0, 0, 0 };

    static Sample* AllocateNewSample()
    {
        // NOTE: We can't take the spinlock if paused
        // as it might already been taken in dmProfile:Begin()
        // A deadlock case occur in dmMessage::Post if http-server is logging
        // TODO: Use mutex instead?
        if (g_Paused)
        {
            return &g_DummySample;
        }

        DM_SPINLOCK_SCOPED_LOCK(g_ProfileLock)
        Profile* profile = g_ActiveProfile;

        bool full = profile->m_Samples.Full();
        if (full)
        {
            g_OutOfSamples = true;
            return &g_DummySample;
        }
        profile->m_Samples.SetSize(profile->m_Samples.Size() + 1);
        Sample* ret = profile->m_Samples.End() - 1;
        return ret;
    }

    // Used when out of samples in order to remove conditional branches
    Sample* AllocateSample()
    {
        Sample* ret = AllocateNewSample();
        if (ret == &g_DummySample)
        {
            return ret;
        }

        void* tls_data = dmThread::GetTlsValue(g_TlsKey);
        if (tls_data == 0)
        {
            // NOTE: We store thread_id + 1. Otherwise we can't differentiate between thread-id 0 and not initialized
            int32_t next_thread_id = dmAtomicIncrement32(&g_ThreadCount) + 1;
            void* thread_id = (void*)((uintptr_t)next_thread_id);
            dmThread::SetTlsValue(g_TlsKey, thread_id);
            tls_data = thread_id;
        }
        intptr_t thread_id = ((intptr_t)tls_data) - 1;
        assert(thread_id >= 0);

        ret->m_ThreadId = thread_id;
        return ret;
    }

    const char* Internalize(const char* string, uint32_t string_length, uint32_t string_hash)
    {
        DM_SPINLOCK_SCOPED_LOCK(g_ProfileLock)
        if (g_StringPool)
        {
            const char* s = dmStringPool::Add(g_StringPool, string, string_length, string_hash);
            return s;
        }
        else
        {
            return "PROFILER NOT INITIALIZED";
        }
    }

    uint32_t GetNameHash(const char* name, uint32_t string_length)
    {
        return dmHashBufferNoReverse32(name, string_length);
    }

    void AddCounter(const char* name, uint32_t amount)
    {
        // dmProfile::Initialize allocates memory. If memprofile is activated this function is called from overloaded malloc while g_CountersTable is being created. No good!
        if (!g_IsInitialized)
        {
            return;
        }

        if (g_Paused)
            return;

        uint32_t name_hash = GetNameHash(name, (uint32_t)strlen(name));

        DM_SPINLOCK_SCOPED_LOCK(g_ProfileLock)
        Profile* profile = g_ActiveProfile;

        uint32_t* counter_index = g_CountersTable.Get(name_hash);
        if (counter_index)
        {
            profile->m_CountersData[*counter_index].m_Value += amount;
            return;
        }

        if (g_Counters.Full())
        {
            g_OutOfCounters = true;
            return;
        }

        uint32_t new_index = g_Counters.Size();
        g_Counters.SetSize(new_index + 1);

        Counter* c = &g_Counters[new_index];
        c->m_Name = name;
        c->m_NameHash = name_hash;

        CounterData* cd = &profile->m_CountersData[new_index];
        cd->m_Counter = c;
        cd->m_Value = 0;

        g_CountersTable.Put(c->m_NameHash, new_index);

        profile->m_CountersData[new_index].m_Value += amount;
    }

    uint32_t AllocateCounter(const char* name)
    {
        if (!g_IsInitialized)
        {
            return 0xffffffffu;
        }

        uint32_t name_hash = GetNameHash(name, (uint32_t)strlen(name));

        DM_SPINLOCK_SCOPED_LOCK(g_ProfileLock)
        uint32_t* counter_index = g_CountersTable.Get(name_hash);
        if (counter_index)
        {
            return *counter_index;
        }
        if (g_Counters.Full())
        {
            g_OutOfCounters = true;
            return 0xffffffffu;
        }

        uint32_t new_index = g_Counters.Size();
        g_Counters.SetSize(new_index + 1);

        Counter* c = &g_Counters[new_index];
        c->m_Name = name;
        c->m_NameHash = name_hash;

        Profile* profile = g_ActiveProfile;
        CounterData* cd = &profile->m_CountersData[new_index];
        cd->m_Counter = c;
        cd->m_Value = 0;

        g_CountersTable.Put(c->m_NameHash, new_index);

        return new_index;
    }

    void AddCounterIndex(uint32_t counter_index, uint32_t amount)
    {
        if (g_Paused)
            return;

        if (counter_index == 0xffffffffu)
        {
            return;
        }

        DM_SPINLOCK_SCOPED_LOCK(g_ProfileLock)
        Profile* profile = g_ActiveProfile;
        profile->m_CountersData[counter_index].m_Value += amount;
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

    struct ScopeSorter {
        ScopeSorter(HProfile profile)
            : m_Profile(profile)
        {}
        bool operator()(uint32_t a, uint32_t b) const
        {
            return m_Profile->m_ScopesData[b].m_Elapsed < m_Profile->m_ScopesData[a].m_Elapsed;
        }
        HProfile m_Profile;
    };

    void IterateScopeData(HProfile profile, void* context, bool sort, void (*call_back)(void* context, const ScopeData* scope_data))
    {
        uint32_t n = profile->m_ScopeCount;
        if (n == 0)
        {
            return;
        }
        if (!sort)
        {
            for (uint32_t i = 0; i < n; ++i)
            {
                call_back(context, &profile->m_ScopesData[i]);
            }
            return;
        }

        uint32_t* sorted_scopes = (uint32_t*)alloca(sizeof(uint32_t) * n);
        for (uint32_t i = 0; i < n; ++i)
        {
            sorted_scopes[i] = i;
        }
        std::sort(sorted_scopes, &sorted_scopes[n], ScopeSorter(profile));

        for (uint32_t i = 0; i < n; ++i)
        {
            call_back(context, &profile->m_ScopesData[sorted_scopes[i]]);
        }
    }

    struct SampleSorter {
        SampleSorter(HProfile profile)
            : m_Profile(profile)
        {}
        bool operator()(uint32_t a, uint32_t b) const
        {
            const Sample* sample_a = &m_Profile->m_Samples[a];
            const Sample* sample_b = &m_Profile->m_Samples[b];
            const ScopeData* scope_data_a = &m_Profile->m_ScopesData[sample_a->m_Scope->m_Index];
            const ScopeData* scope_data_b = &m_Profile->m_ScopesData[sample_b->m_Scope->m_Index];
            if (scope_data_a == scope_data_b)
            {
                return sample_b->m_Elapsed < sample_a->m_Elapsed;
            }
            if (scope_data_b->m_Elapsed < scope_data_a->m_Elapsed)
            {
                return true;
            }
            return false;
        }
        HProfile m_Profile;
    };

    void IterateSamples(HProfile profile, void* context, bool sort, void (*call_back)(void* context, const Sample* sample))
    {
        uint32_t n = profile->m_Samples.Size();
        if (n == 0)
        {
            return;
        }
        if (!sort)
        {
            for (uint32_t i = 0; i < n; ++i)
            {
                call_back(context, &profile->m_Samples[i]);
            }
            return;
        }
        uint32_t* sorted_samples = (uint32_t*)alloca(sizeof(uint32_t) * n);
        for (uint32_t i = 0; i < n; ++i)
        {
            sorted_samples[i] = i;
        }
        std::sort(sorted_samples, &sorted_samples[n], SampleSorter(profile));

        for (uint32_t i = 0; i < n; ++i)
        {
            call_back(context, &profile->m_Samples[sorted_samples[i]]);
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

    uint32_t GetTickSinceBegin()
    {
        uint64_t now = GetNowTicks();
        return (uint32_t)(now - g_BeginTime);
    }

    uint64_t GetNowTicks()
    {
        uint64_t now;
#if defined(_WIN32)
        QueryPerformanceCounter((LARGE_INTEGER*)&now);
#elif defined(__EMSCRIPTEN__)
        now = (uint64_t)(emscripten_get_now() * 1000.0);
#else
        timeval tv;
        gettimeofday(&tv, 0);
        now = ((uint64_t)tv.tv_sec) * 1000000 + tv.tv_usec;
#endif
        return now;
    }

    void ProfileScope::StartScope(uint32_t scope_index, const char* name, uint32_t name_hash)
    {
        m_StartTick = GetNowTicks();
        Sample* s = AllocateSample();
        s->m_Name = name;
        s->m_Scope = &g_Scopes[scope_index];
        s->m_NameHash = name_hash;
        s->m_Start = (uint32_t)(m_StartTick - g_BeginTime);
        m_Sample = s;
    }

    void ProfileScope::EndScope()
    {
        uint64_t end = GetNowTicks();
        m_Sample->m_Elapsed = (uint32_t)(end - m_StartTick);
        if (m_Sample->m_Elapsed > (dmProfile::GetTicksPerSecond() * 2))
        {
            double elapsed_s = (double)(m_Sample->m_Elapsed) / dmProfile::GetTicksPerSecond();
            dmLogWarning("Profiler %s.%s took %.3lf seconds", m_Sample->m_Scope->m_Name, m_Sample->m_Name, elapsed_s);
        }

    }
} // namespace dmProfile
