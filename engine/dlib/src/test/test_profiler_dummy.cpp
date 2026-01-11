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

#include "test_profiler_dummy.h"

#include <assert.h>
#include <string.h>
#include <dlib/hash.h>
#include <dlib/math.h>
#include <dlib/time.h>
#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/profile.h>

DM_PROPERTY_EXTERN(prop_TestGroup1);
DM_PROPERTY_U32(prop_ChildToExtern1, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);
DM_PROPERTY_U32(prop_ChildToExtern2, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);

typedef ProfilerDummyProperty Property;

static ProfilerDummyContext*    g_Context = 0;
static const char*              g_ProfilerName = "ProfilerDummy";
static ProfileListener          g_Listener = {};


static void LogText(void* _ctx, const char* text)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    dmStrlCat(ctx->m_TextBuffer, text, sizeof(ctx->m_TextBuffer));
}

static void* CreateListener()
{
    g_Context = new ProfilerDummyContext;
    memset(g_Context, 0, sizeof(*g_Context));

    g_Context->m_CurrentSample = &g_Context->m_Samples[0];
    g_Context->m_NumSamples = 1;

    return (void*)g_Context;
}

static void DestroyListener(void* _ctx)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    delete ctx;
    g_Context = 0;
}

#define ALLOC_PROP_AND_CHECK(IDX)                                        \
    if (IDX >= (sizeof(ctx->m_Properties)/sizeof(ctx->m_Properties[0]))) \
        return;                                                          \
    Property* prop = &ctx->m_Properties[IDX];                            \
    if (!prop)                                                           \
        return

static inline bool IsValidIndex(ProfileIdx idx)
{
    return idx != PROFILE_PROPERTY_INVALID_IDX;
}

static Property* GetPropertyFromIdx(ProfilerDummyContext* ctx, ProfileIdx idx)
{
    if (!IsValidIndex(idx))
        return 0;
    return &ctx->m_Properties[idx];
}

static void SetupProperty(ProfilerDummyContext* ctx, Property* prop, ProfileIdx idx, const char* name, const char* desc, uint32_t flags, ProfileIdx parentidx)
{
    ctx->m_NumProperties++;
    prop->m_Name        = name;
    prop->m_Idx         = idx;
    prop->m_ParentIdx   = parentidx;
    prop->m_FirstChild  = PROFILE_PROPERTY_INVALID_IDX;
    prop->m_Sibling     = PROFILE_PROPERTY_INVALID_IDX;

    if (parentidx != PROFILE_PROPERTY_INVALID_IDX)
    {
        Property* parent = &ctx->m_Properties[parentidx];
        // Make sure that we create the properties in order!
        assert(parent->m_Idx != PROFILE_PROPERTY_INVALID_IDX);

        prop->m_Sibling = parent->m_FirstChild;
        parent->m_FirstChild = idx;
    }
}

static void ProfileCreatePropertyGroup(void* _ctx, const char* name, const char* desc, ProfileIdx idx, ProfileIdx parent)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(ctx, prop, idx, name, desc, 0, parent);
    prop->m_Type = PROFILE_PROPERTY_TYPE_GROUP;
}

static void ProfileCreatePropertyBool(void* _ctx, const char* name, const char* desc, int value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(ctx, prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_Bool = (bool)value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_BOOL;
}

static void ProfileCreatePropertyS32(void* _ctx, const char* name, const char* desc, int32_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(ctx, prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_S32 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_S32;
}

static void ProfileCreatePropertyU32(void* _ctx, const char* name, const char* desc, uint32_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(ctx, prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_U32 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_U32;
}

static void ProfileCreatePropertyF32(void* _ctx, const char* name, const char* desc, float value, uint32_t flags, ProfileIdx idx, ProfileIdx     parent)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(ctx, prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_F32 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_F32;
}

static void ProfileCreatePropertyS64(void* _ctx, const char* name, const char* desc, int64_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(ctx, prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_S64 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_S64;
}

static void ProfileCreatePropertyU64(void* _ctx, const char* name, const char* desc, uint64_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(ctx, prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_U64 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_U64;
}

static void ProfileCreatePropertyF64(void* _ctx, const char* name, const char* desc, double value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(ctx, prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_F64 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_F64;
}

static void ProfilePropertySetBool(void* _ctx, ProfileIdx idx, int v)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value.m_Bool = v;
}

static void ProfilePropertySetS32(void* _ctx, ProfileIdx idx, int32_t v)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value.m_S32 = v;
}

static void ProfilePropertySetU32(void* _ctx, ProfileIdx idx, uint32_t v)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value.m_U32 = v;
}

static void ProfilePropertySetF32(void* _ctx, ProfileIdx idx, float v)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value.m_F32 = v;
}

static void ProfilePropertySetS64(void* _ctx, ProfileIdx idx, int64_t v)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value.m_S64 = v;
}

static void ProfilePropertySetU64(void* _ctx, ProfileIdx idx, uint64_t v)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value.m_U64 = v;
}

static void ProfilePropertySetF64(void* _ctx, ProfileIdx idx, double v)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value.m_F64 = v;
}

static void ProfilePropertyAddS32(void* _ctx, ProfileIdx idx, int32_t v)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value.m_S32 += v;
}

static void ProfilePropertyAddU32(void* _ctx, ProfileIdx idx, uint32_t v)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value.m_U32 += v;
}

static void ProfilePropertyAddF32(void* _ctx, ProfileIdx idx, float v)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value.m_F32 += v;
}

static void ProfilePropertyAddS64(void* _ctx, ProfileIdx idx, int64_t v)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value.m_S64 += v;
}

static void ProfilePropertyAddU64(void* _ctx, ProfileIdx idx, uint64_t v)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value.m_U64 += v;
}

static void ProfilePropertyAddF64(void* _ctx, ProfileIdx idx, double v)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value.m_F64 += v;
}

static void ProfilePropertyReset(void* _ctx, ProfileIdx idx)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;
    Property* prop = GetPropertyFromIdx(ctx, idx);
    prop->m_Value = prop->m_DefaultValue;
}

static void FrameBegin(void* ctx)
{
    (void)ctx;
}

static void FrameEnd(void* ctx)
{
    (void)ctx;

    memset(&g_Context->m_Samples[0], 0, sizeof(g_Context->m_Samples));

    g_Context->m_NumProperties = 0;
    g_Context->m_NumSamples = 1;
    g_Context->m_CurrentSample = &g_Context->m_Samples[0];
}

static ProfilerDummySample* AllocateSample(ProfilerDummyContext* ctx, uint64_t name_hash)
{
    ProfilerDummySample* parent = ctx->m_CurrentSample;

    // Aggregate samples:
    //      If there already is a child with this name, use that
    ProfilerDummySample* sample = parent->m_FirstChild;
    while (sample)
    {
        if (sample->m_NameHash == name_hash)
        {
            break;
        }
        sample = sample->m_Sibling;
    }

    if (!sample)
    {
        sample = &ctx->m_Samples[ctx->m_NumSamples++];
        memset(sample, 0, sizeof(*sample));

        sample->m_NameHash = name_hash;

        // link the sample into the tree
        ProfilerDummySample* parent = ctx->m_CurrentSample;
        sample->m_Parent = parent;
        if (parent->m_FirstChild == 0)
        {
            parent->m_FirstChild = sample;
            parent->m_LastChild = sample;
        }
        else
        {
            parent->m_LastChild->m_Sibling = sample;
            parent->m_LastChild = sample;
        }
    }

    sample->m_CallCount++;
    ctx->m_CurrentSample = sample;
    return sample;
}

static void ScopeBegin(void* ctx, const char* name, uint64_t name_hash)
{
    (void)ctx;

    uint64_t tstart = dmTime::GetMonotonicTime();

    if (name_hash == 0)
        name_hash = dmHashString64(name);

    ProfilerDummySample* sample = AllocateSample((ProfilerDummyContext*)ctx, name_hash); // Adds it to the thread data

    if (sample->m_CallCount > 1) // we want to preserve the real start of this sample
        sample->m_TempStart = tstart;
    else
        sample->m_Start = tstart;
}

static void ScopeEnd(void* _ctx, const char* name, uint64_t name_hash)
{
    ProfilerDummyContext* ctx = (ProfilerDummyContext*)_ctx;

    uint64_t    end = dmTime::GetMonotonicTime();

    ProfilerDummySample* sample = ctx->m_CurrentSample;

    uint64_t length = 0;
    if (sample->m_CallCount > 1)
        length = dmMath::Max(end - sample->m_TempStart, (uint64_t)0);
    else
        length = dmMath::Max(end - sample->m_Start, (uint64_t)0);

    sample->m_Length += (uint32_t)length;

    if (sample->m_Parent)
    {
        sample->m_Parent->m_LengthChildren += length;
    }

    ctx->m_CurrentSample = sample->m_Parent;
    assert(ctx->m_CurrentSample != 0);
}

void DummyProfilerRegister()
{
    g_Listener.m_Create = CreateListener;
    g_Listener.m_Destroy = DestroyListener;
    g_Listener.m_SetThreadName = 0;

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

    ProfileRegisterProfiler(g_ProfilerName, &g_Listener);
}

void DummyProfilerUnregister()
{
    ProfileUnregisterProfiler(g_ProfilerName);
}

ProfilerDummyContext* DummyProfilerGetContext()
{
    return g_Context;
}

// // Note that the callback might come from a different thread!
// void DummyProfilerSetSampleTreeCallback(void* ctx, FSampleTreeCallback callback)
// {

// }

// // Note that the callback might come from a different thread!
// void DummyProfilerSetPropertyTreeCallback(void* ctx, FPropertyTreeCallback callback)
// {

// }
