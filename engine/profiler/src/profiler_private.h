// Copyright 2020-2026 The Defold Foundation
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

#ifndef DM_PROFILER_PRIVATE_H
#define DM_PROFILER_PRIVATE_H

#include <script/script.h>
#include <dlib/profile.h>

#define SAMPLE_CPU_INTERVAL 0.25

namespace dmProfiler
{
    // Opaque pointer to a sample
    typedef void* HSample;

    // Opaque pointer to a property
    typedef void* HProperty;

    // Opaque pointer to a counter
    typedef void* HCounter;

    typedef void (*FSampleTreeCallback)(void* ctx, const char* thread_name, HSample root);

    typedef void (*FPropertyTreeCallback)(void* ctx, HProperty root);


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

    uint32_t                PropertyGetNameHash(HProperty property);
    const char*             PropertyGetName(HProperty property);
    const char*             PropertyGetDesc(HProperty property);
    ProfilePropertyType     PropertyGetType(HProperty property);
    ProfilePropertyValue    PropertyGetValue(HProperty property);

    // Note that the callback might come from a different thread!
    void SetSampleTreeCallback(void* ctx, FSampleTreeCallback callback);

    // Note that the callback might come from a different thread!
    void SetPropertyTreeCallback(void* ctx, FPropertyTreeCallback callback);
}

namespace dmProfilerExt
{
    /**
     * Call to sample CPU usage in intevals. (Needed for Linux, Android and Win32 platforms.)
     */
    void SampleCpuUsage();

    /**
     * Get current memory usage in bytes (resident/working set) for the process, as reported by OS.
     */
    uint64_t GetMemoryUsage();

    /**
     * Get current CPU usage for process, as reported by OS.
     */
    double GetCpuUsage();

    /**
     * Call update in platforms implementations to collect platform specific data.
     */
    void UpdatePlatformProfiler();
}

#endif // #ifndef DM_PROFILER_PRIVATE_H
