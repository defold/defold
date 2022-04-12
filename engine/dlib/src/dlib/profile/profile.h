// Copyright 2020-2022 The Defold Foundation
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

#ifndef DM_PROFILE_H
#define DM_PROFILE_H

#include <stdint.h>

#define DM_PROFILE_PASTE(x, y) x ## y
#define DM_PROFILE_PASTE2(x, y) DM_PROFILE_PASTE(x, y)

/**
 * Profiler string internalize macro
 * name is the string to internalize
 * returns the internalized string pointer or zero if profiling is disabled
 */
#define DM_INTERNALIZE(name)
#undef DM_INTERNALIZE

/**
 * Profile macro.
 * scope_name is the scope name. Must be a literal
 * name is the sample name. Must be literal, to use non-literal name use DM_PROFILE_DYN
 */
#define DM_PROFILE(scope_name, name)
#undef DM_PROFILE

/**
 * Profile macro.
 * scope_name is the scope name. Must be a literal
 * name is the sample name. Can be non-literal, use DM_PROFILE if you know it is a literal
 * name_hash is the hash of the sample name obtained via dmProfile::GetNameHash()
 */
#define DM_PROFILE_DYN(scope_name, name, name_hash)
#undef DM_PROFILE_DYN

/**
 * Profile counter macro
 * name is the counter name. Must be a literal
 * amount is the amount (integer) to add to the specific counter.
 */
#define DM_COUNTER(name, amount)
#undef DM_COUNTER

/**
 * Profile counter macro for non-literal strings, caller must provide hash
 * counter_index is counter index from AllocateCounter.
 * amount is the amount (integer) to add to the specific counter.
 */
#define DM_COUNTER_DYN(counter_index, amount)
#undef DM_COUNTER_DYN

#if defined(NDEBUG)
    //#define DM_INTERNALIZE(name) 0
    #define _DM_PROFILE_SCOPE(scope_instance_name, name)
    #define DM_PROFILE(scope_name, name)
    #define DM_PROFILE_DYN(scope_name, name)
    #define DM_COUNTER(name, amount)
    #define DM_COUNTER_DYN(counter_index, amount)
#else
    // #define DM_INTERNALIZE(name) \
    //     (dmProfile::IsInitialized() ? dmProfile::Internalize(name, (uint32_t)strlen(name), dmProfile::GetNameHash(name, (uint32_t)strlen(name))) : 0)

    // #define _DM_PROFILE_SCOPE(scope_index, name, name_hash) \
    //     dmProfile::ProfileScope DM_PROFILE_PASTE2(profile_scope, __LINE__)(scope_index, name, name_hash);

    #define _DM_PROFILE_SCOPE(name, name_hash_ptr) \
        dmProfile::ProfileScope DM_PROFILE_PASTE2(profile_scope, __LINE__)(name, name_hash_ptr);

    #define DM_PROFILE(scope_name, name) \
        static uint32_t DM_PROFILE_PASTE2(hash, __LINE__)        = 0; \
        _DM_PROFILE_SCOPE(name, & DM_PROFILE_PASTE2(hash, __LINE__))

    // #define DM_PROFILE_DYN(scope_name, name) \
    //     static uint32_t DM_PROFILE_PASTE2(scope_index, __LINE__) = dmProfile::IsInitialized() ? dmProfile::AllocateScope(#scope_name) : 0xffffffffu; \
    //     _DM_PROFILE_SCOPE(name, 0)

    #define DM_PROFILE_DYN(scope_name, name) \
        _DM_PROFILE_SCOPE(name, 0)

    // #define DM_COUNTER(name, amount) \
    //     static uint32_t DM_PROFILE_PASTE2(counter_index, __LINE__) = dmProfile::IsInitialized() ? dmProfile::AllocateCounter(name) : 0xffffffffu; \
    //     if (DM_PROFILE_PASTE2(counter_index, __LINE__) != 0xffffffffu) { \
    //         dmProfile::AddCounterIndex(DM_PROFILE_PASTE2(counter_index, __LINE__), amount);\
    //     }

    // #define DM_COUNTER_DYN(counter_index, amount) \
    //     if (counter_index != 0xffffffffu) { \
    //         dmProfile::AddCounterIndex(counter_index, amount); \
    //     }


    #define DM_COUNTER(name, amount)
    #define DM_COUNTER_DYN(counter_index, amount)

#endif

namespace dmProfile
{
    /// Profile snapshot handle
    typedef void* HProfile;

    // Opaque pointer to a sample
    typedef void* HSample;

    // Opaque pointer to a counter
    typedef void* HCounter;

    struct Options
    {
        uint32_t m_Port;
    };

    /**
     * Initialize profiler
     * @param max_scopes Maximum scopes
     * @param max_samples Maximum samples
     * @param max_counters Maximum counters
     */
    void Initialize(const Options* options);

    /**
     * Finalize profiler
     */
    void Finalize();

    typedef void (*FSampleTreeCallback)(void* ctx, const char* thread_name, HSample root);

    // Note that the callback might come from a different thread!
    void SetSampleTreeCallback(void* ctx, FSampleTreeCallback callback);

    /**
     * Begin profiling, eg start of frame
     * @note NULL is returned if profiling is disabled
     * @return A snapshot of the current profile. Must be release by #Release after processed. It's valid to keep the profile snapshot throughout a "frame".
     */
    HProfile BeginFrame();

    /**
     * Release profile returned by #Begin
     * @param profile Profile to release
     */
    void EndFrame(HProfile profile);

    void SetThreadName(const char* name);

    /**
     * Get ticks per second
     * @return Ticks per second
     */
    uint64_t GetTicksPerSecond();

    /**
     * Add #amount to counter with #name
     * @param name Counter name
     * @param amount Amount to add
     */
    void AddCounter(const char* name, uint32_t amount);

    bool IsInitialized();

    /**
     * Logs a text to the profiler
     * @return Ticks per second
     */
    void LogText(const char* text);

    // *******************************************************************

    // Sample iterator

    struct SampleIterator
    {
    // public
        HSample m_Sample;
        SampleIterator();
    // private
        void* m_IteratorImpl;
        ~SampleIterator();
    };

    SampleIterator  IterateChildren(HSample sample);
    bool            IterateNext(SampleIterator* iter);

    // Sample accessors

    const char*     SampleGetName(HSample sample);
    uint64_t        SampleGetStart(HSample sample);
    uint64_t        SampleGetTime(HSample sample);      // in ticks per second
    uint64_t        SampleGetSelfTime(HSample sample);  // in ticks per second
    uint32_t        SampleGetCallCount(HSample sample);
    uint32_t        SampleGetColor(HSample sample);

    // *******************************************************************

    /// Internal, do not use.
    struct ProfileScope
    {
        inline ProfileScope(const char* name, uint32_t* name_hash)
        {
            StartScope(name, name_hash);
        }

        inline ~ProfileScope()
        {
            EndScope();
        }

        void StartScope(const char* name, uint32_t* name_hash);
        void EndScope();
    };

} // namespace dmProfile

#endif
