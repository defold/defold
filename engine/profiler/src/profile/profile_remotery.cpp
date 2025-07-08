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

#include "profile.h"

#include <stdio.h> // vsnprintf
#include <stdarg.h> // va_start et al

#include "dlib/dlib.h"
#include "dlib/log.h"
#include "dlib/atomic.h"
#include "dlib/spinlock.h"

#include "dlib/hash.h"

#include "dmsdk/external/remotery/Remotery.h"

namespace dmProfile
{
    static Remotery*                g_Remotery = 0;
    static FSampleTreeCallback      g_SampleTreeCallback = 0;
    static void*                    g_SampleTreeCallbackCtx = 0;
    static FPropertyTreeCallback    g_PropertyTreeCallback = 0;
    static void*                    g_PropertyTreeCallbackCtx = 0;
    static int32_atomic_t           g_ProfilerInitialized = 0;
    static dmSpinlock::Spinlock     g_ProfilerLock;

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

    bool IsInitialized()
    {
        return dmAtomicGet32(&g_ProfilerInitialized) != 0 && g_Remotery != 0;
    }

    static void SampleTreeCallback(void* ctx, rmtSampleTree* sample_tree)
    {
        if (!IsInitialized())
            return;

        DM_SPINLOCK_SCOPED_LOCK(g_ProfilerLock);

        if (g_SampleTreeCallback)
        {
            const char* thread_name = rmt_SampleTreeGetThreadName(sample_tree);
            rmtSample* root = rmt_SampleTreeGetRootSample(sample_tree);

            g_SampleTreeCallback(g_SampleTreeCallbackCtx, thread_name, SampleToHandle(root));
        }
    }

    static void PropertyTreeCallback(void* ctx, rmtProperty* root)
    {
        if (!IsInitialized())
            return;

        DM_SPINLOCK_SCOPED_LOCK(g_ProfilerLock);

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

        dmSpinlock::Create(&g_ProfilerLock);
        dmAtomicStore32(&g_ProfilerInitialized, 1);

        dmLogInfo("Initialized Remotery (ws://127.0.0.1:%d/rmt)", settings->port);
    }

    void Finalize()
    {
        {
            DM_SPINLOCK_SCOPED_LOCK(g_ProfilerLock);
            dmAtomicStore32(&g_ProfilerInitialized, 0);

            g_SampleTreeCallback = 0;
            g_SampleTreeCallbackCtx = 0;
            g_PropertyTreeCallback = 0;
            g_PropertyTreeCallbackCtx = 0;

            if (g_Remotery)
                rmt_DestroyGlobalInstance(g_Remotery);
            g_Remotery = 0;
        }

        dmSpinlock::Destroy(&g_ProfilerLock);
    }

    HProfile BeginFrame()
    {
        if (!IsInitialized())
        {
            return 0;
        }

        return g_Remotery;
    }

    void SetThreadName(const char* name)
    {
        if (!IsInitialized())
            return;
        DM_SPINLOCK_SCOPED_LOCK(g_ProfilerLock);
        rmt_SetCurrentThreadName(name);
    }

    void EndFrame(HProfile profile)
    {
        if (!IsInitialized())
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
        if (!IsInitialized())
        {
            return;
        }

        char buffer[DM_PROFILE_TEXT_LENGTH];

        va_list lst;
        va_start(lst, format);
        int n = vsnprintf(buffer, DM_PROFILE_TEXT_LENGTH, format, lst);
        if (n < 0)
        {
            buffer[DM_PROFILE_TEXT_LENGTH-1] = '\0';
        }
        va_end(lst);

        rmt_LogText(buffer);
    }

    void SetSampleTreeCallback(void* ctx, FSampleTreeCallback callback)
    {
        // This function is called both before the profiler is initialized, and during
        if (IsInitialized())
        {
            dmSpinlock::Lock(&g_ProfilerLock);
        }

        g_SampleTreeCallback = callback;
        g_SampleTreeCallbackCtx = ctx;

        if (IsInitialized())
        {
            dmSpinlock::Unlock(&g_ProfilerLock);
        }
    }

    void SetPropertyTreeCallback(void* ctx, FPropertyTreeCallback callback)
    {
        // This function is called both before the profiler is initialized, and during
        if (IsInitialized())
        {
            dmSpinlock::Lock(&g_ProfilerLock);
        }

        g_PropertyTreeCallback = callback;
        g_PropertyTreeCallbackCtx = ctx;

        if (IsInitialized())
        {
            dmSpinlock::Unlock(&g_ProfilerLock);
        }
    }

    void ProfileScopeHelper::StartScope(const char* name, uint64_t* name_hash)
    {
        if (!IsInitialized()) {
            return;
        }
        if (name != 0)
        {
            if (name[0] == 0)
                name = "<empty>";
            _rmt_BeginCPUSample(name, RMTSF_Aggregate, (uint32_t*)name_hash);
        }
    }

    void ProfileScopeHelper::EndScope()
    {
        rmt_EndCPUSample();
    }

    void ScopeBegin(const char* name, uint64_t* name_hash)
    {
        if (!IsInitialized()) {
            return;
        }
        _rmt_BeginCPUSample(name, RMTSF_Aggregate, (uint32_t*)name_hash);
    }

    void ScopeEnd()
    {
        if (!IsInitialized()) {
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

static const uint32_t g_MaxPropertyCount = 256;
static uint32_t g_PropertyCount = 0;
static rmtProperty g_Properties[g_MaxPropertyCount];

static rmtProperty* AllocateProperty(dmProfileIdx* idx)
{
    if (g_PropertyCount >= g_MaxPropertyCount)
        return 0;
    *idx = g_PropertyCount++;
    return &g_Properties[*idx];
}

#define ALLOC_PROP_AND_CHECK() \
    dmProfileIdx idx; \
    rmtProperty* prop = AllocateProperty(&idx); \
    if (!prop) \
        return DM_PROFILE_PROPERTY_INVALID_IDX;

static rmtProperty* GetPropertyFromIdx(dmProfileIdx idx)
{
    if (idx < g_PropertyCount || idx == DM_PROFILE_PROPERTY_INVALID_IDX)
        return 0;
    return &g_Properties[idx];
}

#define GET_PROP_AND_CHECK(IDX) \
    if (!ProfileIsInitialized()) \
        return; \
    if (idx == DM_PROFILE_PROPERTY_INVALID_IDX) \
        return; \
    rmtProperty* prop = &g_Properties[IDX]; \
    if (!prop) \
        return;

static void SetupProperty(rmtProperty* prop, const char* name, const char* desc, uint32_t flags, dmProfileIdx* (*parent)())
{
    memset(prop, 0, sizeof(rmtProperty));
    prop->initialised    = RMT_FALSE;
    prop->flags          = (flags == PROFILE_PROPERTY_FRAME_RESET) ? RMT_PropertyFlags_FrameReset : RMT_PropertyFlags_NoFlags;
    prop->name           = name;
    prop->description    = desc;
    prop->parent         = parent ? GetPropertyFromIdx(*parent()) : 0;
}

dmProfileIdx dmProfileCreatePropertyGroup(const char* name, const char* desc, dmProfileIdx* (*parent)())
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, RMT_PropertyFlags_NoFlags, parent);
    prop->type = RMT_PropertyType_rmtGroup;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyBool(const char* name, const char* desc, int value, uint32_t flags, dmProfileIdx* (*parent)())
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtBool;
    prop->value          = rmtPropertyValue::MakeBool((rmtBool)value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyS32(const char* name, const char* desc, int32_t value, uint32_t flags, dmProfileIdx* (*parent)())
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtS32;
    prop->value          = rmtPropertyValue::MakeS32(value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyU32(const char* name, const char* desc, uint32_t value, uint32_t flags, dmProfileIdx* (*parent)())
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtU32;
    prop->value          = rmtPropertyValue::MakeU32(value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyF32(const char* name, const char* desc, float value, uint32_t flags, dmProfileIdx* (*parent)())
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtF32;
    prop->value          = rmtPropertyValue::MakeF32(value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyS64(const char* name, const char* desc, int64_t value, uint32_t flags, dmProfileIdx* (*parent)())
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtS64;
    prop->value          = rmtPropertyValue::MakeS64(value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyU64(const char* name, const char* desc, uint64_t value, uint32_t flags, dmProfileIdx* (*parent)())
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtU64;
    prop->value          = rmtPropertyValue::MakeU64(value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyF64(const char* name, const char* desc, double value, uint32_t flags, dmProfileIdx* (*parent)())
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtF64;
    prop->value          = rmtPropertyValue::MakeF64(value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
    return idx;
}

// See _rmt_PropertySet
void dmProfilePropertySetBool(dmProfileIdx idx, int v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.Bool = (rmtBool)v;
    _rmt_PropertySetValue(prop);
}

void dmProfilePropertySetS32(dmProfileIdx idx, int32_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.S32 = v;
    _rmt_PropertySetValue(prop);
}

void dmProfilePropertySetU32(dmProfileIdx idx, uint32_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.U32 = v;
    _rmt_PropertySetValue(prop);
}

void dmProfilePropertySetF32(dmProfileIdx idx, float v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.F32 = v;
    _rmt_PropertySetValue(prop);
}

void dmProfilePropertySetS64(dmProfileIdx idx, int64_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.S64 = v;
    _rmt_PropertySetValue(prop);
}

void dmProfilePropertySetU64(dmProfileIdx idx, uint64_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.U64 = v;
    _rmt_PropertySetValue(prop);
}
void dmProfilePropertySetF64(dmProfileIdx idx, double v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.F64 = v;
    _rmt_PropertySetValue(prop);
}

// See _rmt_PropertyAdd
void dmProfilePropertyAddS32(dmProfileIdx idx, int32_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.S32 += v;
    rmtPropertyValue delta_value = rmtPropertyValue::MakeS32(v);
    _rmt_PropertyAddValue(prop, delta_value);
}

void dmProfilePropertyAddU32(dmProfileIdx idx, uint32_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.U32 += v;
    rmtPropertyValue delta_value = rmtPropertyValue::MakeU32(v);
    _rmt_PropertyAddValue(prop, delta_value);
}

void dmProfilePropertyAddF32(dmProfileIdx idx, float v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.F32 += v;
    rmtPropertyValue delta_value = rmtPropertyValue::MakeF32(v);
    _rmt_PropertyAddValue(prop, delta_value);
}

void dmProfilePropertyAddS64(dmProfileIdx idx, int64_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.S64 += v;
    rmtPropertyValue delta_value = rmtPropertyValue::MakeS64(v);
    _rmt_PropertyAddValue(prop, delta_value);
}

void dmProfilePropertyAddU64(dmProfileIdx idx, uint64_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.U64 += v;
    rmtPropertyValue delta_value = rmtPropertyValue::MakeU64(v);
    _rmt_PropertyAddValue(prop, delta_value);
}

void dmProfilePropertyAddF64(dmProfileIdx idx, double v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.F64 += v;
    rmtPropertyValue delta_value = rmtPropertyValue::MakeF64(v);
    _rmt_PropertyAddValue(prop, delta_value);
}

// See rmt_PropertyReset
void dmProfilePropertyReset(dmProfileIdx idx)
{
    GET_PROP_AND_CHECK(idx);
    prop->value = prop->defaultValue;
    _rmt_PropertySetValue(prop);
}

