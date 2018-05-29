#ifndef DM_PROFILE_H
#define DM_PROFILE_H

#include "atomic.h"

#include <stdint.h>

#if defined(_WIN32)
#include "safe_windows.h"
#elif  defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#include <sys/time.h>
#endif

#define DM_PROFILE_PASTE(x, y) x ## y
#define DM_PROFILE_PASTE2(x, y) DM_PROFILE_PASTE(x, y)

/**
 * Profile macro.
 * scope_name is the scope name. Must be a literal
 * name is the sample name. An arbitrary constant string and *must* be valid during the life-time
 * of the application. If not, use dmProfile::Internalize()
 */
#define DM_PROFILE(scope_name, name)
#undef DM_PROFILE

/**
 * Profile counter macro
 * name is the counter name
 * amount is the amount (integer) to add to the specific counter.
 */
#define DM_COUNTER(name, amount)
#undef DM_COUNTER

/**
 * Profile counter macro. Fast version of #DM_COUNTER
 * name is the counter name
 * name_hash is the prehashed name
 * amount is the amount (integer) to add to the specific counter.
 */
#define DM_COUNTER_HASH(name, name_hash, amount)
#undef DM_COUNTER_HASH

#if defined(NDEBUG)
    #define DM_PROFILE(scope_name, name)
    #define DM_COUNTER(name, amount)
    #define DM_COUNTER_HASH(name, name_hash, amount)
#else
    #define DM_PROFILE(scope_name, name) \
        static dmProfile::Scope* DM_PROFILE_PASTE2(scope, __LINE__) = 0; \
        if (dmProfile::g_IsInitialized && DM_PROFILE_PASTE2(scope, __LINE__) == 0) \
        {\
            DM_PROFILE_PASTE2(scope, __LINE__) = dmProfile::AllocateScope(#scope_name);\
        }\
        dmProfile::ProfileScope DM_PROFILE_PASTE2(profile_scope, __LINE__)(DM_PROFILE_PASTE2(scope, __LINE__), name);\


    #define DM_COUNTER(name, amount) \
    dmProfile::AddCounter(name, amount);

    #define DM_COUNTER_HASH(name, name_hash, amount) \
    dmProfile::AddCounterHash(name, name_hash, amount);

#endif

namespace dmProfile
{
    /// Profile snapshot handle
    typedef struct Profile* HProfile;

    struct ScopeData;

    /**
     * Profile scope
     */
    struct Scope
    {
        /// Scope name
        const char* m_Name;
        /// Scope index, range [0, scopes-1]
        uint16_t    m_Index;
        /// Internal data
        void*       m_Internal;
    };

    /**
     * Scope data
     */
    struct ScopeData
    {
        /// The scope
        Scope*  m_Scope;
        /// Total time spent in scope (in ticks) summed over all threads
        int32_t m_Elapsed;
        /// Occurrences of this scope summed over all threads
        int32_t m_Count;
    };

    /**
     * Profile sample
     */
    struct Sample
    {
        /// Sample name
        const char* m_Name;
        /// Sampled within scope
        Scope*      m_Scope;
        /// Start time in ticks
        uint32_t    m_Start;
        /// Elapsed time in ticks
        uint32_t    m_Elapsed;
        /// Thread id this sample belongs to
        uint16_t    m_ThreadId;
        /// Padding to 64-bit align
        uint16_t    m_Pad;
        uint32_t    m_Pad2;
    };

    /**
     * Profile counter
     */
    struct Counter
    {
        /// Counter name
        const char*      m_Name;
        /// Counter name hash
        uint32_t         m_NameHash;
    };

    /**
     * Profile counter data
     */
    struct CounterData
    {
        /// The counter
        Counter*       m_Counter;
        /// Counter value
        int32_atomic_t m_Value;
    };

    /**
     * Initialize profiler
     * @param max_scopes Maximum scopes
     * @param max_samples Maximum samples
     * @param max_counters Maximum counters
     */
    void Initialize(uint32_t max_scopes, uint32_t max_samples, uint32_t max_counters);

    /**
     * Finalize profiler
     */
    void Finalize();

    /**
     * Begin profiling, eg start of frame
     * @note NULL is returned if profiling is disabled
     * @return A snapshot of the current profile. Must be release by #Release after processed. It's valid to keep the profile snapshot throughout a "frame".
     */
    HProfile Begin();

    /**
     * Pause profiling
     * @param pause True to pause. False to resume.
     */
    void Pause(bool pause);

    /**
     * Release profile returned by #Begin
     * @param profile Profile to release
     */
    void Release(HProfile profile);

    /**
     * Get ticks per second
     * @return Ticks per second
     */
    uint64_t GetTicksPerSecond();

    /**
     * Iterate over all registered strings
     * @param profile Profile snapshot to iterate over
     * @param context User context
     * @param call_back Call-back function pointer
     */
    void IterateStrings(HProfile profile, void* context, void (*call_back)(void* context, const uintptr_t* key, const char** value));

    /**
     * Iterate over all scopes
     * @param profile Profile snapshot to iterate over
     * @param context User context
     * @param call_back Call-back function pointer
     */
    void IterateScopes(HProfile profile, void* context, void (*call_back)(void* context, const Scope* scope_data));


    /**
     * Iterate over all scopes
     * @param profile Profile snapshot to iterate over
     * @param context User context
     * @param call_back Call-back function pointer
     */
    void IterateScopeData(HProfile profile, void* context, void (*call_back)(void* context, const ScopeData* scope_data));

    /**
     * Iterate over all samples
     * @param profile Profile snapshot to iterate over
     * @param context User context
     * @param call_back Call-back function pointer
     */
    void IterateSamples(HProfile profile, void* context, void (*call_back)(void* context, const Sample* sample));

    /**
     * Iterate over all counters
     * @param profile Profile snapshot to iterate over
     * @param context User context
     * @param call_back Call-back function pointer
     */
    void IterateCounters(HProfile profile, void* context, void (*call_back)(void* context, const Counter* counter));

    /**
     * Iterate over all counters
     * @param profile Profile snapshot to iterate over
     * @param context User context
     * @param call_back Call-back function pointer
     */
    void IterateCounterData(HProfile profile, void* context, void (*call_back)(void* context, const CounterData* counter));

    /**
     * Internal function
     * @param name
     * @return #Scope
     */
    Scope* AllocateScope(const char* name);

    /**
     * Internal function
     * @return #Sample
     */
    Sample* AllocateSample();

    /**
     * Create an internalized string. Use this function in DM_PROFILE if the
     * name isn't valid for the life-time of the application
     * @param string string to internalize
     * @return internalized string
     */
    const char* Internalize(const char* string);

    /**
     * Add #amount to counter with #name
     * @param name Counter name
     * @param amount Amount to add
     */
    void AddCounter(const char* name, uint32_t amount);

    /**
     * Add #amount to counter with prehashed name. Faster version of #AddCounter
     * @param name Counter name
     * @param name Counter name hash
     * @param amount Amount to add
     */
    void AddCounterHash(const char* name, uint32_t name_hash, uint32_t amount);

    /**
     * Get time for the frame total
     * @return Total frame time
     */
    float GetFrameTime();

    /**
     * Get time for the frame total during the last 60 frames
     * @return Total frame time
     */
    float GetMaxFrameTime();

    /**
     * Check of out of scope resources
     * @return True if out of scope resources
     */
    bool IsOutOfScopes();

    /**
     * Check of out of sample resources
     * @return True if out of sample resources
     */
    bool IsOutOfSamples();

    /// Internal, do not use.
    extern uint32_t g_BeginTime;

    /// Internal, do not use.
    extern bool g_IsInitialized;

    /// Internal, do not use.
    struct ProfileScope
    {
        Sample*     m_Sample;
        inline ProfileScope(Scope* scope, const char* name)
        {
            if (!g_IsInitialized)
            {
                return;
            }

            uint64_t start;
#if defined(_WIN32)
            QueryPerformanceCounter((LARGE_INTEGER *)&start);
#elif defined(__EMSCRIPTEN__)
            start = (uint64_t)(emscripten_get_now() * 1000.0);
#else
            timeval tv;
            gettimeofday(&tv, 0);
            start = tv.tv_sec * 1000000 + tv.tv_usec;
#endif
            Sample*s = AllocateSample();
            s->m_Name = name;
            s->m_Scope = scope;
            s->m_Start = (uint32_t)(start - g_BeginTime);
            m_Sample = s;
        }

        inline ~ProfileScope()
        {
            if (!g_IsInitialized)
            {
                return;
            }

            uint64_t end;
#if defined(_WIN32)
            QueryPerformanceCounter((LARGE_INTEGER *) &end);
#elif defined(__EMSCRIPTEN__)
            end = (uint64_t)(emscripten_get_now() * 1000.0);
#else
            timeval tv;
            gettimeofday(&tv, 0);
            end = tv.tv_sec * 1000000 + tv.tv_usec;
#endif
            m_Sample->m_Elapsed = (uint32_t)(end - g_BeginTime) - m_Sample->m_Start;
        }
    };

}

#endif
