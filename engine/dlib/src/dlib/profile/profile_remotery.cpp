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

#include "profile.h"

#include <stdio.h> // vsnprintf
#include <stdarg.h> // va_start et al

#include "dlib/dlib.h"
#include "dlib/log.h"

#include "dlib/hash.h"

#include "dmsdk/external/remotery/Remotery.h"

namespace dmProfile
{
    static Remotery* g_Remotery = 0;
    static FSampleTreeCallback      g_SampleTreeCallback = 0;
    static void*                    g_SampleTreeCallbackCtx = 0;
    static FPropertyTreeCallback    g_PropertyTreeCallback = 0;
    static void*                    g_PropertyTreeCallbackCtx = 0;

    static inline rmtSample* SampleFromHandle(HSample sample)
    {
        return (rmtSample*)sample;
    }
    static inline HSample SampleToHandle(rmtSample* sample)
    {
        return (HSample)sample;
    }
    static inline rmtProperty* PropertyFromHandle(HProperty property)
    {
        return (rmtProperty*)property;
    }
    static inline HProperty PropertyToHandle(rmtProperty* property)
    {
        return (HProperty)property;
    }

    static void SampleTreeCallback(void* ctx, rmtSampleTree* sample_tree)
    {
        if (g_SampleTreeCallback)
        {
            const char* thread_name = rmt_SampleTreeGetThreadName(sample_tree);
            rmtSample* root = rmt_SampleTreeGetRootSample(sample_tree);

            g_SampleTreeCallback(g_SampleTreeCallbackCtx, thread_name, SampleToHandle(root));
        }
    }

    static void PropertyTreeCallback(void* ctx, rmtProperty* root)
    {
        if (g_PropertyTreeCallback)
        {
            g_PropertyTreeCallback(g_PropertyTreeCallbackCtx, PropertyToHandle(root));
        }
    }

    void Initialize(const Options* options)
    {
        if (!dLib::IsDebugMode())
            return;

        rmtSettings* settings = rmt_Settings();

        if (options)
        {
            if (options->m_Port != 0)
            {
                settings->port = (rmtU16)options->m_Port;
            }
            if (options->m_SleepBetweenServerUpdates > 0)
            {
                settings->msSleepBetweenServerUpdates = options->m_SleepBetweenServerUpdates;
            }
        }
        settings->sampletree_handler = SampleTreeCallback;
        settings->sampletree_context = 0;
        settings->snapshot_callback = PropertyTreeCallback;
        settings->snapshot_context = 0;
        settings->reuse_open_port = true;
        settings->enableThreadSampler = false;

        rmtError result = rmt_CreateGlobalInstance(&g_Remotery);
        if (result != RMT_ERROR_NONE)
        {
            dmLogError("Failed to initialize profile library %d", result);
            return;
        }

        rmt_SetCurrentThreadName("Main");

        dmLogInfo("Initialized Remotery (ws://127.0.0.1:%d/rmt)", settings->port);
    }

    void Finalize()
    {
        if (g_Remotery)
            rmt_DestroyGlobalInstance(g_Remotery);
        g_Remotery = 0;
    }

    bool IsInitialized()
    {
        return g_Remotery != 0;
    }

    HProfile BeginFrame()
    {
        if (!g_Remotery)
        {
            return 0;
        }

        return g_Remotery;
    }

    void SetThreadName(const char* name)
    {
        rmt_SetCurrentThreadName(name);
    }

    void EndFrame(HProfile profile)
    {
        if (!g_Remotery)
        {
            return;
        }

        if (!profile)
            return;

        rmt_PropertySnapshotAll();
        rmt_PropertyFrameResetAll();
    }

    void AddCounter(const char* name, uint32_t amount)
    {
        // Not supported currently.
        // Used by mem profiler to dynamically insert a property at runtime
    }

    uint64_t GetTicksPerSecond()
    {
        return 1000000;
    }

    void LogText(const char* format, ...)
    {
        char buffer[DM_PROFILE_TEXT_LENGTH];

        va_list lst;
        va_start(lst, format);
        int n = vsnprintf(buffer, DM_PROFILE_TEXT_LENGTH, format, lst);
        buffer[n < DM_PROFILE_TEXT_LENGTH ? n : (DM_PROFILE_TEXT_LENGTH-1)] = 0;
        va_end(lst);

        rmt_LogText(buffer);
    }

    void SetSampleTreeCallback(void* ctx, FSampleTreeCallback callback)
    {
        g_SampleTreeCallback = callback;
        g_SampleTreeCallbackCtx = ctx;
    }

    void SetPropertyTreeCallback(void* ctx, FPropertyTreeCallback callback)
    {
        g_PropertyTreeCallback = callback;
        g_PropertyTreeCallbackCtx = ctx;
    }

    void ProfileScope::StartScope(const char* name, uint64_t* name_hash)
    {
        if (g_Remotery == NULL) {
            return;
        }
        if (name != 0)
        {
            valid = 1;
            _rmt_BeginCPUSample(name, RMTSF_Aggregate, (uint32_t*)name_hash);
        }
    }

    void ProfileScope::EndScope()
    {
        if (valid)
        {
            rmt_EndCPUSample();
        }
    }

    void ScopeBegin(const char* name, uint64_t* name_hash)
    {
        if (g_Remotery == NULL) {
            return;
        }
        _rmt_BeginCPUSample(name, RMTSF_Aggregate, (uint32_t*)name_hash);
    }

    void ScopeEnd()
    {
        if (g_Remotery == NULL) {
            return;
        }
        rmt_EndCPUSample();
    }

    // *******************************************************************

    SampleIterator::SampleIterator()
    : m_Sample(0)
    , m_IteratorImpl(0)
    {
    }

    SampleIterator::~SampleIterator()
    {
        delete (rmtSampleIterator*)m_IteratorImpl;
    }

    SampleIterator* SampleIterateChildren(HSample sample, SampleIterator* iter)
    {
        iter->m_Sample = 0;

        rmtSampleIterator* rmt_iter = new rmtSampleIterator;
        rmt_IterateChildren(rmt_iter, SampleFromHandle(sample));

        iter->m_IteratorImpl = (void*)rmt_iter;
        return iter;
    }

    bool SampleIterateNext(SampleIterator* iter)
    {
        rmtSampleIterator* rmt_iter = (rmtSampleIterator*)iter->m_IteratorImpl;
        bool result = rmt_IterateNext(rmt_iter);
        iter->m_Sample = SampleToHandle(rmt_iter->sample);
        return result;
    }

    uint32_t SampleGetNameHash(HSample sample)
    {
        return (uint32_t)rmt_SampleGetNameHash(SampleFromHandle(sample));
    }

    const char* SampleGetName(HSample sample)
    {
        return rmt_SampleGetName(SampleFromHandle(sample));
    }

    uint64_t SampleGetStart(HSample sample)
    {
        return rmt_SampleGetStart(SampleFromHandle(sample));
    }

    uint64_t SampleGetTime(HSample sample)
    {
        return rmt_SampleGetTime(SampleFromHandle(sample));
    }

    uint64_t SampleGetSelfTime(HSample sample)
    {
        return rmt_SampleGetSelfTime(SampleFromHandle(sample));
    }

    uint32_t SampleGetCallCount(HSample sample)
    {
        return rmt_SampleGetCallCount(SampleFromHandle(sample));
    }

    uint32_t SampleGetColor(HSample sample)
    {
        uint8_t r, g, b;
        rmt_SampleGetColour(SampleFromHandle(sample), &r, &g, &b);
        return r << 16 | g << 8 | b;
    }

    // *******************************************************************


    PropertyIterator::PropertyIterator()
    : m_Property(0)
    , m_IteratorImpl(0)
    {
    }

    PropertyIterator::~PropertyIterator()
    {
        delete (rmtPropertyIterator*)m_IteratorImpl;
    }

    PropertyIterator* PropertyIterateChildren(HProperty property, PropertyIterator* iter)
    {
        iter->m_Property = 0;

        rmtPropertyIterator* rmt_iter = new rmtPropertyIterator;
        rmt_PropertyIterateChildren(rmt_iter, PropertyFromHandle(property));

        iter->m_IteratorImpl = (void*)rmt_iter;
        return iter;
    }

    bool PropertyIterateNext(PropertyIterator* iter)
    {
        rmtPropertyIterator* rmt_iter = (rmtPropertyIterator*)iter->m_IteratorImpl;
        bool result = rmt_PropertyIterateNext(rmt_iter);
        iter->m_Property = PropertyToHandle(rmt_iter->property);
        return result;
    }

    // Property accessors

    uint32_t PropertyGetNameHash(HProperty hproperty)
    {
        rmtProperty* property = PropertyFromHandle(hproperty);
        return (uint32_t)property->nameHash;
    }

    const char* PropertyGetName(HProperty hproperty)
    {
        rmtProperty* property = PropertyFromHandle(hproperty);
        return property->name;
    }

    const char* PropertyGetDesc(HProperty hproperty)
    {
        rmtProperty* property = PropertyFromHandle(hproperty);
        return property->description;
    }

    PropertyType PropertyGetType(HProperty hproperty)
    {
        rmtProperty* property = PropertyFromHandle(hproperty);

        switch(property->type)
        {
        case RMT_PropertyType_rmtGroup: return PROPERTY_TYPE_GROUP;
        case RMT_PropertyType_rmtBool:  return PROPERTY_TYPE_BOOL;
        case RMT_PropertyType_rmtS32:   return PROPERTY_TYPE_S32;
        case RMT_PropertyType_rmtU32:   return PROPERTY_TYPE_U32;
        case RMT_PropertyType_rmtF32:   return PROPERTY_TYPE_F32;
        case RMT_PropertyType_rmtS64:   return PROPERTY_TYPE_S64;
        case RMT_PropertyType_rmtU64:   return PROPERTY_TYPE_U64;
        case RMT_PropertyType_rmtF64:   return PROPERTY_TYPE_F64;
        default:
            dmLogError("Unknown property type: %d", property->type);
            return PROPERTY_TYPE_GROUP;
        }
    }

    PropertyValue PropertyGetValue(HProperty hproperty)
    {
        PropertyValue out = {};
        rmtProperty* property = PropertyFromHandle(hproperty);

        switch(property->type)
        {
        case RMT_PropertyType_rmtBool:  out.m_Bool = property->value.Bool; break;
        case RMT_PropertyType_rmtS32:   out.m_S32 = property->value.S32; break;
        case RMT_PropertyType_rmtU32:   out.m_U32 = property->value.U32; break;
        case RMT_PropertyType_rmtF32:   out.m_F32 = property->value.F32; break;
        case RMT_PropertyType_rmtS64:   out.m_S64 = property->value.S64; break;
        case RMT_PropertyType_rmtU64:   out.m_U64 = property->value.U64; break;
        case RMT_PropertyType_rmtF64:   out.m_F64 = property->value.F64; break;
        case RMT_PropertyType_rmtGroup: break;
        default:
            dmLogError("Unknown property type: %d", property->type); break;
        }
        return out;
    }

    // *******************************************************************


} // namespace dmProfile
