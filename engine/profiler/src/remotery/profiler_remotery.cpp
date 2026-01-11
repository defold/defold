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

#include <stdio.h>
#include <string.h>

#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/atomic.h>
#include <dmsdk/dlib/configfile.h>
#include <dmsdk/dlib/spinlock.h>
#include <dmsdk/dlib/profile.h>
#include <dmsdk/extension/extension.h>

#include "remotery/lib/Remotery.h"

namespace dmProfilerRemotery
{
    static Remotery*                g_Remotery = 0;
    static int32_atomic_t           g_ProfilerInitialized = 0;
    static dmSpinlock::Spinlock     g_ProfilerLock;
    // config options:
    static int                      g_ProfilerOptions_Port = 0;
    static int                      g_ProfilerOptions_SleepBetweenServerUpdates = 0;

    static bool IsInitialized()
    {
        return dmAtomicGet32(&g_ProfilerInitialized) != 0 && g_Remotery != 0;
    }

    static void* CreateListener()
    {
        rmtSettings* settings = rmt_Settings();

        if (g_ProfilerOptions_Port != 0)
        {
            settings->port = (rmtU16)g_ProfilerOptions_Port;
        }
        if (g_ProfilerOptions_SleepBetweenServerUpdates > 0)
        {
            settings->msSleepBetweenServerUpdates = g_ProfilerOptions_SleepBetweenServerUpdates;
        }

        settings->sampletree_handler = 0;
        settings->sampletree_context = 0;
        settings->snapshot_callback = 0;
        settings->snapshot_context = 0;
        settings->reuse_open_port = true;
        settings->enableThreadSampler = false;

        rmtError result = rmt_CreateGlobalInstance(&g_Remotery);
        if (result != RMT_ERROR_NONE)
        {
            dmLogError("Failed to initialize Remotery: %d", result);
            return 0;
        }

        rmt_SetCurrentThreadName("Main");

        dmSpinlock::Create(&g_ProfilerLock);
        dmAtomicStore32(&g_ProfilerInitialized, 1);

        dmLogInfo("Initialized Remotery (ws://127.0.0.1:%d/rmt)", settings->port);

        return &g_Remotery;
    }

    static void DestroyListener(void* listener)
    {
        (void)listener;

        {
            DM_SPINLOCK_SCOPED_LOCK(g_ProfilerLock);
            dmAtomicStore32(&g_ProfilerInitialized, 0);

            if (g_Remotery)
                rmt_DestroyGlobalInstance(g_Remotery);
            g_Remotery = 0;
        }

        dmSpinlock::Destroy(&g_ProfilerLock);
    }

    static void SetThreadName(void*, const char* name)
    {
        if (!IsInitialized())
            return;
        DM_SPINLOCK_SCOPED_LOCK(g_ProfilerLock);
        rmt_SetCurrentThreadName(name);
    }

    static void FrameBegin(void*)
    {
    }

    static void FrameEnd(void*)
    {
        if (!IsInitialized())
        {
            return;
        }

        rmt_PropertySnapshotAll();
        rmt_PropertyFrameResetAll();
    }

    static void LogText(void*, const char* text)
    {
        if (!IsInitialized())
        {
            return;
        }

        rmt_LogText(text);
    }

    static void ScopeBegin(void* ctx, const char* name, uint64_t name_hash)
    {
        if (!IsInitialized())
        {
            return;
        }
        // By passing in 0 here, it will have to hash the string each time
        _rmt_BeginCPUSample(name, RMTSF_Aggregate, 0);
    }

    static void ScopeEnd(void* ctx, const char* name, uint64_t name_hash)
    {
        if (!IsInitialized())
        {
            return;
        }
        rmt_EndCPUSample();
    }

} // namespace dmProfilerRemotery

static const uint32_t g_MaxPropertyCount = 256;
static uint32_t g_PropertyCount = 0;
static rmtProperty g_Properties[g_MaxPropertyCount];

static inline rmtProperty* AllocateProperty(ProfileIdx idx)
{
    if (g_PropertyCount >= g_MaxPropertyCount)
        return 0;
    return &g_Properties[idx];
}

#define ALLOC_PROP_AND_CHECK() \
    rmtProperty* prop = AllocateProperty(idx); \
    if (!prop) \
        return;

static rmtProperty* GetPropertyFromIdx(ProfileIdx idx)
{
    if (idx < g_PropertyCount || idx == PROFILE_PROPERTY_INVALID_IDX)
        return 0;
    return &g_Properties[idx];
}

#define GET_PROP_AND_CHECK(IDX) \
    if (!ProfileIsInitialized()) \
        return; \
    if (idx == PROFILE_PROPERTY_INVALID_IDX) \
        return; \
    rmtProperty* prop = &g_Properties[IDX]; \
    if (!prop) \
        return;

static void SetupProperty(rmtProperty* prop, const char* name, const char* desc, uint32_t flags, ProfileIdx parentidx)
{
    memset(prop, 0, sizeof(rmtProperty));
    prop->initialised    = RMT_FALSE;
    prop->flags          = (flags == PROFILE_PROPERTY_FRAME_RESET) ? RMT_PropertyFlags_FrameReset : RMT_PropertyFlags_NoFlags;
    prop->name           = name;
    prop->description    = desc;
    prop->parent         = parentidx != PROFILE_PROPERTY_INVALID_IDX ? GetPropertyFromIdx(parentidx) : 0;
}

static void ProfileCreatePropertyGroup(void*, const char* name, const char* desc, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, RMT_PropertyFlags_NoFlags, parent);
    prop->type = RMT_PropertyType_rmtGroup;
}

static void ProfileCreatePropertyBool(void*, const char* name, const char* desc, int value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtBool;
    prop->value          = rmtPropertyValue::MakeBool((rmtBool)value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
}

static void ProfileCreatePropertyS32(void*, const char* name, const char* desc, int32_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtS32;
    prop->value          = rmtPropertyValue::MakeS32(value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
}

static void ProfileCreatePropertyU32(void*, const char* name, const char* desc, uint32_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtU32;
    prop->value          = rmtPropertyValue::MakeU32(value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
}

static void ProfileCreatePropertyF32(void*, const char* name, const char* desc, float value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtF32;
    prop->value          = rmtPropertyValue::MakeF32(value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
}

static void ProfileCreatePropertyS64(void*, const char* name, const char* desc, int64_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtS64;
    prop->value          = rmtPropertyValue::MakeS64(value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
}

static void ProfileCreatePropertyU64(void*, const char* name, const char* desc, uint64_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtU64;
    prop->value          = rmtPropertyValue::MakeU64(value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
}

static void ProfileCreatePropertyF64(void*, const char* name, const char* desc, double value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, name, desc, flags, parent);
    prop->type           = RMT_PropertyType_rmtF64;
    prop->value          = rmtPropertyValue::MakeF64(value);
    prop->lastFrameValue = prop->value;
    prop->defaultValue   = prop->value;
}

// See _rmt_PropertySet
static void ProfilePropertySetBool(void*, ProfileIdx idx, int v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.Bool = (rmtBool)v;
    _rmt_PropertySetValue(prop);
}

static void ProfilePropertySetS32(void*, ProfileIdx idx, int32_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.S32 = v;
    _rmt_PropertySetValue(prop);
}

static void ProfilePropertySetU32(void*, ProfileIdx idx, uint32_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.U32 = v;
    _rmt_PropertySetValue(prop);
}

static void ProfilePropertySetF32(void*, ProfileIdx idx, float v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.F32 = v;
    _rmt_PropertySetValue(prop);
}

static void ProfilePropertySetS64(void*, ProfileIdx idx, int64_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.S64 = v;
    _rmt_PropertySetValue(prop);
}

static void ProfilePropertySetU64(void*, ProfileIdx idx, uint64_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.U64 = v;
    _rmt_PropertySetValue(prop);
}
static void ProfilePropertySetF64(void*, ProfileIdx idx, double v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.F64 = v;
    _rmt_PropertySetValue(prop);
}

// See _rmt_PropertyAdd
static void ProfilePropertyAddS32(void*, ProfileIdx idx, int32_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.S32 += v;
    rmtPropertyValue delta_value = rmtPropertyValue::MakeS32(v);
    _rmt_PropertyAddValue(prop, delta_value);
}

static void ProfilePropertyAddU32(void*, ProfileIdx idx, uint32_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.U32 += v;
    rmtPropertyValue delta_value = rmtPropertyValue::MakeU32(v);
    _rmt_PropertyAddValue(prop, delta_value);
}

static void ProfilePropertyAddF32(void*, ProfileIdx idx, float v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.F32 += v;
    rmtPropertyValue delta_value = rmtPropertyValue::MakeF32(v);
    _rmt_PropertyAddValue(prop, delta_value);
}

static void ProfilePropertyAddS64(void*, ProfileIdx idx, int64_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.S64 += v;
    rmtPropertyValue delta_value = rmtPropertyValue::MakeS64(v);
    _rmt_PropertyAddValue(prop, delta_value);
}

static void ProfilePropertyAddU64(void*, ProfileIdx idx, uint64_t v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.U64 += v;
    rmtPropertyValue delta_value = rmtPropertyValue::MakeU64(v);
    _rmt_PropertyAddValue(prop, delta_value);
}

static void ProfilePropertyAddF64(void*, ProfileIdx idx, double v)
{
    GET_PROP_AND_CHECK(idx);
    prop->value.F64 += v;
    rmtPropertyValue delta_value = rmtPropertyValue::MakeF64(v);
    _rmt_PropertyAddValue(prop, delta_value);
}

// See rmt_PropertyReset
static void ProfilePropertyReset(void*, ProfileIdx idx)
{
    GET_PROP_AND_CHECK(idx);
    prop->value = prop->defaultValue;
    _rmt_PropertySetValue(prop);
}

static const char* g_ProfilerName = "ProfilerRemotery";
static ProfileListener g_Listener = {};

static dmExtension::Result ProfilerRemotery_AppInitialize(dmExtension::AppParams* params)
{
    using namespace dmProfilerRemotery;

    g_Listener.m_Create = CreateListener;
    g_Listener.m_Destroy = DestroyListener;
    g_Listener.m_SetThreadName = SetThreadName;

    g_Listener.m_FrameBegin = FrameBegin;
    g_Listener.m_FrameEnd = FrameEnd;
    g_Listener.m_ScopeBegin = ScopeBegin;
    g_Listener.m_ScopeEnd = ScopeEnd;

    g_Listener.m_LogText = LogText;

    g_Listener.m_CreatePropertyGroup = ProfileCreatePropertyGroup;
    g_Listener.m_CreatePropertyBool = ProfileCreatePropertyBool;
    g_Listener.m_CreatePropertyS32 = ProfileCreatePropertyS32;
    g_Listener.m_CreatePropertyU32 = ProfileCreatePropertyU32;
    g_Listener.m_CreatePropertyF32 = ProfileCreatePropertyF32;
    g_Listener.m_CreatePropertyS64 = ProfileCreatePropertyS64;
    g_Listener.m_CreatePropertyU64 = ProfileCreatePropertyU64;
    g_Listener.m_CreatePropertyF64 = ProfileCreatePropertyF64;

    g_Listener.m_PropertySetBool = ProfilePropertySetBool;
    g_Listener.m_PropertySetS32 = ProfilePropertySetS32;
    g_Listener.m_PropertySetU32 = ProfilePropertySetU32;
    g_Listener.m_PropertySetF32 = ProfilePropertySetF32;
    g_Listener.m_PropertySetS64 = ProfilePropertySetS64;
    g_Listener.m_PropertySetU64 = ProfilePropertySetU64;
    g_Listener.m_PropertySetF64 = ProfilePropertySetF64;
    g_Listener.m_PropertyAddS32 = ProfilePropertyAddS32;
    g_Listener.m_PropertyAddU32 = ProfilePropertyAddU32;
    g_Listener.m_PropertyAddF32 = ProfilePropertyAddF32;
    g_Listener.m_PropertyAddS64 = ProfilePropertyAddS64;
    g_Listener.m_PropertyAddU64 = ProfilePropertyAddU64;
    g_Listener.m_PropertyAddF64 = ProfilePropertyAddF64;
    g_Listener.m_PropertyReset = ProfilePropertyReset;

    g_ProfilerOptions_Port                      = dmConfigFile::GetInt(params->m_ConfigFile, "profiler.remotery_port", 0);
    g_ProfilerOptions_SleepBetweenServerUpdates = dmConfigFile::GetInt(params->m_ConfigFile, "profiler.remotery_sleep_between_server_updates", 0);

    if (!IsInitialized())
    {
        ProfileRegisterProfiler(g_ProfilerName, &g_Listener);
    }
    return dmExtension::RESULT_OK;
}

static dmExtension::Result ProfilerRemotery_AppFinalize(dmExtension::AppParams* params)
{
    if (dmExtension::AppParamsGetAppExitCode(params) == dmExtension::APP_EXIT_CODE_EXIT)
    {
        ProfileUnregisterProfiler(g_ProfilerName);
    }
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(ProfilerRemotery, g_ProfilerName, ProfilerRemotery_AppInitialize, ProfilerRemotery_AppFinalize, 0, 0, 0, 0);
