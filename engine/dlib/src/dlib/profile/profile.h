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

#include <dmsdk/external/remotery/Remotery.h> // Private, don't use this api directly!

#define DM_PROFILE_PASTE(x, y) x ## y
#define DM_PROFILE_PASTE2(x, y) DM_PROFILE_PASTE(x, y)

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

#if defined(NDEBUG) || defined(DM_PROFILE_NULL)
    #define DM_PROFILE_TEXT_LENGTH 1024
    #define DM_PROFILE_TEXT(format, ...)
    #define DM_PROFILE(scope_name, name)
    #define DM_PROFILE_DYN(scope_name, name)

    #define DM_COUNTER_DYN(counter_index, amount)

    #define DM_PROPERTY_EXTERN(name)                extern rmtProperty name;
    #define DM_PROPERTY_GROUP(name, desc, ...)
    #define DM_PROPERTY_BOOL(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_S32(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_U32(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_F32(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_S64(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_U64(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_F64(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_SET_BOOL(name, set_value)
    #define DM_PROPERTY_SET_S32(name, set_value)
    #define DM_PROPERTY_SET_U32(name, set_value)
    #define DM_PROPERTY_SET_F32(name, set_value)
    #define DM_PROPERTY_SET_S64(name, set_value)
    #define DM_PROPERTY_SET_U64(name, set_value)
    #define DM_PROPERTY_SET_F64(name, set_value)
    #define DM_PROPERTY_ADD_S32(name, add_value)
    #define DM_PROPERTY_ADD_U32(name, add_value)
    #define DM_PROPERTY_ADD_F32(name, add_value)
    #define DM_PROPERTY_ADD_S64(name, add_value)
    #define DM_PROPERTY_ADD_U64(name, add_value)
    #define DM_PROPERTY_ADD_F64(name, add_value)
    #define DM_PROPERTY_RESET(name)

#else
    #define _DM_PROFILE_SCOPE(name, name_hash_ptr) \
        dmProfile::ProfileScope DM_PROFILE_PASTE2(profile_scope, __LINE__)(name, name_hash_ptr);

    #define DM_PROFILE(scope_name, name) \
        static uint32_t DM_PROFILE_PASTE2(hash, __LINE__)        = 0; \
        _DM_PROFILE_SCOPE(name, & DM_PROFILE_PASTE2(hash, __LINE__))

    #define DM_PROFILE_DYN(scope_name, name) \
        _DM_PROFILE_SCOPE(name, 0)

    #define DM_PROFILE_TEXT_LENGTH 1024
    #define DM_PROFILE_TEXT(format, ...)              dmProfile::LogText(format, __VA_ARGS__)

    #define DM_COUNTER_DYN(counter_index, amount)

    // The profiler property api
    #define DM_PROPERTY_EXTERN(name)                                rmt_PropertyExtern(name)
    #define DM_PROPERTY_GROUP(name, desc, ...)                      rmt_PropertyDefine_Group(name, desc, __VA_ARGS__)
    #define DM_PROPERTY_BOOL(name, default_value, flag, desc, ...)  rmt_PropertyDefine_Bool(name, default_value, flag, desc, __VA_ARGS__)
    #define DM_PROPERTY_S32(name, default_value, flag, desc, ...)   rmt_PropertyDefine_S32(name, default_value, flag, desc, __VA_ARGS__)
    #define DM_PROPERTY_U32(name, default_value, flag, desc, ...)   rmt_PropertyDefine_U32(name, default_value, flag, desc, __VA_ARGS__)
    #define DM_PROPERTY_F32(name, default_value, flag, desc, ...)   rmt_PropertyDefine_F32(name, default_value, flag, desc, __VA_ARGS__)
    #define DM_PROPERTY_S64(name, default_value, flag, desc, ...)   rmt_PropertyDefine_S64(name, default_value, flag, desc, __VA_ARGS__)
    #define DM_PROPERTY_U64(name, default_value, flag, desc, ...)   rmt_PropertyDefine_U64(name, default_value, flag, desc, __VA_ARGS__)
    #define DM_PROPERTY_F64(name, default_value, flag, desc, ...)   rmt_PropertyDefine_F64(name, default_value, flag, desc, __VA_ARGS__)

    // Set properties to the given value
    #define DM_PROPERTY_SET_BOOL(name, set_value)   rmt_PropertySet_Bool(name, set_value)
    #define DM_PROPERTY_SET_S32(name, set_value)    rmt_PropertySet_S32(name, set_value)
    #define DM_PROPERTY_SET_U32(name, set_value)    rmt_PropertySet_U32(name, set_value)
    #define DM_PROPERTY_SET_F32(name, set_value)    rmt_PropertySet_F32(name, set_value)
    #define DM_PROPERTY_SET_S64(name, set_value)    rmt_PropertySet_S64(name, set_value)
    #define DM_PROPERTY_SET_U64(name, set_value)    rmt_PropertySet_U64(name, set_value)
    #define DM_PROPERTY_SET_F64(name, set_value)    rmt_PropertySet_F64(name, set_value)

    // Add the given value to properties
    #define DM_PROPERTY_ADD_S32(name, add_value)    rmt_PropertyAdd_S32(name, add_value)
    #define DM_PROPERTY_ADD_U32(name, add_value)    rmt_PropertyAdd_U32(name, add_value)
    #define DM_PROPERTY_ADD_F32(name, add_value)    rmt_PropertyAdd_F32(name, add_value)
    #define DM_PROPERTY_ADD_S64(name, add_value)    rmt_PropertyAdd_S64(name, add_value)
    #define DM_PROPERTY_ADD_U64(name, add_value)    rmt_PropertyAdd_U64(name, add_value)
    #define DM_PROPERTY_ADD_F64(name, add_value)    rmt_PropertyAdd_F64(name, add_value)

    #define DM_PROPERTY_RESET(name)                 rmt_PropertyReset(name)

#endif

namespace dmProfile
{
    /// Profile snapshot handle
    typedef void* HProfile;

    // Opaque pointer to a sample
    typedef void* HSample;

    // Opaque pointer to a property
    typedef void* HProperty;

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

    typedef void (*FPropertyTreeCallback)(void* ctx, HProperty root);

    // Note that the callback might come from a different thread!
    void SetSampleTreeCallback(void* ctx, FSampleTreeCallback callback);

    // Note that the callback might come from a different thread!
    void SetPropertyTreeCallback(void* ctx, FPropertyTreeCallback callback);

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
     */
    void LogText(const char* text, ...);

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

    SampleIterator* SampleIterateChildren(HSample sample, SampleIterator* iter);
    bool            SampleIterateNext(SampleIterator* iter);

    // Sample accessors

    const char*     SampleGetName(HSample sample);
    uint64_t        SampleGetStart(HSample sample);
    uint64_t        SampleGetTime(HSample sample);      // in ticks per second
    uint64_t        SampleGetSelfTime(HSample sample);  // in ticks per second
    uint32_t        SampleGetCallCount(HSample sample);
    uint32_t        SampleGetColor(HSample sample);


    // *******************************************************************
    // Property iterator

    enum PropertyType
    {
        PROPERTY_TYPE_GROUP,
        PROPERTY_TYPE_BOOL,
        PROPERTY_TYPE_S32,
        PROPERTY_TYPE_U32,
        PROPERTY_TYPE_F32,
        PROPERTY_TYPE_S64,
        PROPERTY_TYPE_U64,
        PROPERTY_TYPE_F64,
    };

    union PropertyValue
    {
        bool        m_Bool;
        int32_t     m_S32;
        uint32_t    m_U32;
        float       m_F32;
        int64_t     m_S64;
        uint64_t    m_U64;
        double      m_F64;
    };

    struct PropertyIterator
    {
    // public
        HProperty m_Property;
        PropertyIterator();
    // private
        void* m_IteratorImpl;
        ~PropertyIterator();
    };

    PropertyIterator*   PropertyIterateChildren(HProperty property, PropertyIterator* iter);
    bool                PropertyIterateNext(PropertyIterator* iter);

    // Sample accessors

    const char*     PropertyGetName(HProperty property);
    const char*     PropertyGetDesc(HProperty property);
    uint64_t        PropertyGetNameHash(HProperty property);
    PropertyType    PropertyGetType(HProperty property);
    PropertyValue   PropertyGetValue(HProperty property);

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
