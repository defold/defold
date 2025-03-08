// Copyright 2020-2025 The Defold Foundation
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

#include <dmsdk/dlib/profile.h>

namespace dmProfile
{
    // Opaque pointer to a sample
    typedef void* HSample;

    // Opaque pointer to a property
    typedef void* HProperty;

    // Opaque pointer to a counter
    typedef void* HCounter;

    struct Options
    {
        uint32_t m_Port;
        uint32_t m_SleepBetweenServerUpdates;
    };

    /**
     * Initialize profiler
     * @param options [type:dmProfile::Options] The profiler options
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

    void ScopeBegin(const char* name, uint64_t* name_hash);
    void ScopeEnd();

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

    uint32_t        SampleGetNameHash(HSample sample);
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

    uint32_t        PropertyGetNameHash(HProperty property);
    const char*     PropertyGetName(HProperty property);
    const char*     PropertyGetDesc(HProperty property);
    PropertyType    PropertyGetType(HProperty property);
    PropertyValue   PropertyGetValue(HProperty property);

    // *******************************************************************

} // namespace dmProfile

#endif
