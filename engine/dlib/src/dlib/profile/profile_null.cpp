// Copyright 2020-2024 The Defold Foundation
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

#include "profile.h"

namespace dmProfile
{
    void Initialize(const Options* options)
    {
    }

    void Finalize()
    {
    }

    bool IsInitialized()
    {
        return false;
    }

    void SetThreadName(const char* name)
    {
    }

    HProfile BeginFrame()
    {
        return 0;
    }

    void EndFrame(HProfile profile)
    {
    }

    uint64_t GetTicksPerSecond()
    {
        return 1000000;
    }

    void AddCounter(const char* name, uint32_t amount)
    {

    }

    void LogText(const char* text, ...)
    {
    }

    void SetSampleTreeCallback(void* ctx, FSampleTreeCallback callback)
    {
    }

    void SetPropertyTreeCallback(void* ctx, FPropertyTreeCallback callback)
    {
    }

    void ProfileScope::StartScope(const char* name, uint64_t* name_hash)
    {
    }

    void ProfileScope::EndScope()
    {
    }

    void ScopeBegin(const char* name, uint64_t* name_hash)
    {
    }

    void ScopeEnd()
    {
    }

    // *******************************************************************

    SampleIterator::SampleIterator()
    : m_Sample(0)
    , m_IteratorImpl(0)
    {
    }

    SampleIterator::~SampleIterator()
    {
    }

    SampleIterator* SampleIterateChildren(HSample sample, SampleIterator* iter)
    {
        return iter;
    }

    bool SampleIterateNext(SampleIterator* iter)
    {
        return false;
    }

    uint32_t SampleGetNameHash(HSample sample)
    {
        return 0;
    }

    const char* SampleGetName(HSample sample)
    {
        return 0;
    }

    uint64_t SampleGetStart(HSample sample)
    {
        return 0;
    }

    uint64_t SampleGetTime(HSample sample)
    {
        return 0;
    }

    uint64_t SampleGetSelfTime(HSample sample)
    {
        return 0;
    }

    uint32_t SampleGetCallCount(HSample sample)
    {
        return 0;
    }

    uint32_t SampleGetColor(HSample sample)
    {
        return 0;
    }


    // *******************************************************************

    PropertyIterator::PropertyIterator()
    : m_Property(0)
    , m_IteratorImpl(0)
    {
    }

    PropertyIterator::~PropertyIterator()
    {
    }

    PropertyIterator* PropertyIterateChildren(HProperty property, PropertyIterator* iter)
    {
        return iter;
    }

    bool PropertyIterateNext(PropertyIterator* iter)
    {
        return false;
    }

    // Property accessors

    uint32_t PropertyGetNameHash(HProperty hproperty)
    {
        return 0;
    }

    const char* PropertyGetName(HProperty hproperty)
    {
        return 0;
    }

    const char* PropertyGetDesc(HProperty hproperty)
    {
        return 0;
    }

    PropertyType PropertyGetType(HProperty hproperty)
    {
        return PROPERTY_TYPE_GROUP;
    }

    PropertyValue PropertyGetValue(HProperty hproperty)
    {
        PropertyValue out = {};
        return out;
    }

    // *******************************************************************


} // namespace dmProfile

