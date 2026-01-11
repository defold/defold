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

#include <dmsdk/extension/extension.h>
#include "../profiler_private.h"

#include <dlib/array.h>
#include <dlib/atomic.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/thread.h>
#include <dlib/time.h>

#include <assert.h>
#include <stdio.h>
#include <string.h> // memset

extern "C" {
// Implementation in library_profile.js
bool dmProfileJSInit(uint8_t enablePerformanceTimeline);
void dmProfileJSReset(uint32_t thread_id);
void dmProfileJSEndFrame(uint32_t thread_id);
uint32_t dmProfileJSBeginMark(uint32_t thread_id, const char *name);
bool dmProfileJSEndMark(uint32_t thread_id);
uint32_t dmProfileJSFirstChild(uint32_t thread_id, uint32_t idx);
uint32_t dmProfileJSNextSibling(uint32_t thread_id, uint32_t idx);
const char *dmProfileJSGetName(uint32_t thread_id, uint32_t idx);
double dmProfileJSGetStart(uint32_t thread_id, uint32_t idx);
double dmProfileJSGetDuration(uint32_t thread_id, uint32_t idx);
double dmProfileJSGetSelfDuration(uint32_t thread_id, uint32_t idx);
uint32_t dmProfileJSGetCallCount(uint32_t thread_id, uint32_t idx);
uint32_t dmProfileJSFirstProperty(uint32_t idx);
uint32_t dmProfileJSNextProperty(uint32_t idx);
const char * dmProfileJSGetPropertyName(int32_t idx);
const char *dmProfileJSGetPropertyDescription(uint32_t idx);
uint32_t dmProfileJSGetPropertyType(uint32_t idx);
uint32_t dmProfileJSGetPropertyFlags(uint32_t idx);
uint32_t dmProfileJSCreatePropertyGroup(uint32_t type, const char *name, const char *desc, uint32_t idx, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyBool(uint32_t type, const char *name, const char *desc, bool v, uint32_t flags, uint32_t idx, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyS32(uint32_t type, const char *name, const char *desc, int32_t v, uint32_t flags, uint32_t idx, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyU32(uint32_t type, const char *name, const char *desc, uint32_t v, uint32_t flags, uint32_t idx, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyF32(uint32_t type, const char *name, const char *desc, float v, uint32_t flags, uint32_t idx, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyS64(uint32_t type, const char *name, const char *desc, int64_t v, uint32_t flags, uint32_t idx, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyU64(uint32_t type, const char *name, const char *desc, uint64_t v, uint32_t flags, uint32_t idx, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyF64(uint32_t type, const char *name, const char *desc, double v, uint32_t flags, uint32_t idx, uint32_t parent_idx);
void dmProfileJSSetPropertyBool(uint32_t idx, bool v);
void dmProfileJSSetPropertyS32(uint32_t idx, int32_t v);
void dmProfileJSSetPropertyU32(uint32_t idx, uint32_t v);
void dmProfileJSSetPropertyF32(uint32_t idx, float v);
void dmProfileJSSetPropertyS64(uint32_t idx, int64_t v);
void dmProfileJSSetPropertyU64(uint32_t idx, uint64_t v);
void dmProfileJSSetPropertyF64(uint32_t idx, double v);
void dmProfileJSAddPropertyS32(uint32_t idx, uint32_t v);
void dmProfileJSAddPropertyU32(uint32_t idx, uint32_t v);
void dmProfileJSAddPropertyF32(uint32_t idx, float v);
void dmProfileJSAddPropertyS64(uint32_t idx, int64_t v);
void dmProfileJSAddPropertyU64(uint32_t idx, uint64_t v);
void dmProfileJSAddPropertyF64(uint32_t idx, double v);
void dmProfileJSResetProperty(uint32_t idx);
}

///////////////////////////////////////////////////////////////////////////////////////////////

// At global scope as they're set _before_ the profiler is started

// Synchronization
static dmThread::TlsKey         g_TlsKey = dmThread::AllocTls();
static int32_atomic_t           g_ThreadCount = 0;
static int32_atomic_t           g_ProfileInitialized = 0;
static dmMutex::HMutex          g_Lock = 0;

static struct ProfileContext*   g_ProfileContext = 0;
// config options:
static int                      g_ProfilerOptions_PerformanceTimelineEnabled = 0;

///////////////////////////////////////////////////////////////////////////////////////////////
// ThreadData

// We use one instance of ThreadData per thread, keeping track of it's sample scopes
struct ThreadData
{
    int32_t     m_ThreadId;

    ThreadData(int32_t thread_id) : m_ThreadId(thread_id)
    {
        memset(this, 0, sizeof(*this));
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////
// PROFILE CONTEXT

static void DeleteThreadDataFn(void* context, const uint32_t* key, ThreadData** td)
{
    delete *td;
}

static void DeleteStringFn(void* context, const uint32_t* key, const char** str)
{
    free((void*)*str);
}

struct ProfileContext
{
    dmHashTable32<const char*> m_Names;
    dmHashTable32<const char*> m_ThreadNames;
    dmHashTable32<ThreadData*> m_ThreadData;

    ProfileContext()
    {
    }

    ~ProfileContext()
    {
        m_ThreadData.Iterate(DeleteThreadDataFn, (void*)0);
        m_ThreadData.Clear();

        m_Names.Iterate(DeleteStringFn, (void*)0);
        m_Names.Clear();

        m_ThreadNames.Iterate(DeleteStringFn, (void*)0);
        m_ThreadNames.Clear();
    }
};

static bool IsProfileInitialized()
{
    return dmAtomicGet32(&g_ProfileInitialized) != 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////

#define CHECK_INITIALIZED() \
    if (!IsProfileInitialized()) \
        return;

#define CHECK_INITIALIZED_RETVAL(RETVAL) \
    if (!IsProfileInitialized()) \
        return RETVAL;

///////////////////////////////////////////////////////////////////////////////////////////////
// Threads

static int32_t GetThreadId()
{
    void* tls_data = (ThreadData*)dmThread::GetTlsValue(g_TlsKey);
    if (tls_data == 0)
    {
        // NOTE: We store thread_id + 1. Otherwise we can't differentiate between thread-id 0 and not initialized
        int32_t next_thread_id = dmAtomicIncrement32(&g_ThreadCount) + 1;
        tls_data = (void*)((uintptr_t)next_thread_id);
        dmThread::SetTlsValue(g_TlsKey, tls_data);
    }

    intptr_t thread_id = ((intptr_t)tls_data) - 1;
    assert(thread_id >= 0);
    return (int32_t)thread_id;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static void* CreateListener()
{
    if (!dmProfileJSInit(g_ProfilerOptions_PerformanceTimelineEnabled ? 1 : 0))
    {
        dmLogError("Failed to initialize the profiler");
        return 0;
    }

    assert(!IsProfileInitialized());
    g_Lock = dmMutex::New();
    g_ProfileContext = new ProfileContext;

    dmAtomicIncrement32(&g_ProfileInitialized);

    return g_ProfileContext;
}

static void DestroyListener(void*)
{
    CHECK_INITIALIZED();
    {
        DM_MUTEX_SCOPED_LOCK(g_Lock);
        dmAtomicDecrement32(&g_ProfileInitialized);

        delete g_ProfileContext;
        g_ProfileContext = 0;
    }
    dmMutex::Delete(g_Lock);
    g_Lock = 0;
}

static void SetThreadName(void*, const char* name)
{
    CHECK_INITIALIZED();
    DM_MUTEX_SCOPED_LOCK(g_Lock);
    if (g_ProfileContext->m_ThreadNames.Full())
    {
        uint32_t cap = g_ProfileContext->m_ThreadNames.Capacity() + 32;
        g_ProfileContext->m_ThreadNames.SetCapacity((cap*2)/3, cap);
    }
    g_ProfileContext->m_ThreadNames.Put(GetThreadId(), strdup(name));
}

static void FrameBegin(void*)
{
}

static void FrameEnd(void*)
{
    CHECK_INITIALIZED();
    DM_MUTEX_SCOPED_LOCK(g_Lock);

    dmProfileJSEndFrame(GetThreadId());
}

static void ScopeBegin(void*, const char* name, uint64_t name_hash)
{
    (void)name_hash;
    CHECK_INITIALIZED();
    DM_MUTEX_SCOPED_LOCK(g_Lock);
    dmProfileJSBeginMark(GetThreadId(), name);
}

static void ScopeEnd(void*, const char* name, uint64_t name_hash)
{
    (void)name_hash;
    CHECK_INITIALIZED();
    DM_MUTEX_SCOPED_LOCK(g_Lock);
    const int32_t thread_id = GetThreadId();

    if (dmProfileJSEndMark(thread_id)) {
        dmProfileJSReset(thread_id);
    }
}

// *******************************************************************

static void ProfileCreatePropertyGroup(void*, const char* name, const char* desc, ProfileIdx idx, ProfileIdx parent)
{
    dmProfileJSCreatePropertyGroup(PROFILE_PROPERTY_TYPE_GROUP, name, desc, idx, parent);
}

static void ProfileCreatePropertyBool(void*, const char* name, const char* desc, int value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    dmProfileJSCreatePropertyBool(PROFILE_PROPERTY_TYPE_BOOL, name, desc, value, flags, idx, parent);
}

static void ProfileCreatePropertyS32(void*, const char* name, const char* desc, int32_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    dmProfileJSCreatePropertyS32(PROFILE_PROPERTY_TYPE_S32, name, desc, value, flags, idx, parent);
}

static void ProfileCreatePropertyU32(void*, const char* name, const char* desc, uint32_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    dmProfileJSCreatePropertyU32(PROFILE_PROPERTY_TYPE_U32, name, desc, value, flags, idx, parent);
}

static void ProfileCreatePropertyF32(void*, const char* name, const char* desc, float value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    dmProfileJSCreatePropertyF32(PROFILE_PROPERTY_TYPE_F32, name, desc, value, flags, idx, parent);
}

static void ProfileCreatePropertyS64(void*, const char* name, const char* desc, int64_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    dmProfileJSCreatePropertyS64(PROFILE_PROPERTY_TYPE_S64, name, desc, value, flags, idx, parent);
}

static void ProfileCreatePropertyU64(void*, const char* name, const char* desc, uint64_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    dmProfileJSCreatePropertyU64(PROFILE_PROPERTY_TYPE_U64, name, desc, value, flags, idx, parent);
}

static void ProfileCreatePropertyF64(void*, const char* name, const char* desc, double value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    dmProfileJSCreatePropertyF64(PROFILE_PROPERTY_TYPE_F64, name, desc, value, flags, idx, parent);
}

static void ProfilePropertySetBool(void*, ProfileIdx idx, int v)
{
    dmProfileJSSetPropertyBool(idx, v);
}

static void ProfilePropertySetS32(void*, ProfileIdx idx, int32_t v)
{
    dmProfileJSSetPropertyS32(idx, v);
}

static void ProfilePropertySetU32(void*, ProfileIdx idx, uint32_t v)
{
    dmProfileJSSetPropertyU32(idx, v);
}

static void ProfilePropertySetF32(void*, ProfileIdx idx, float v)
{
    dmProfileJSSetPropertyF32(idx, v);
}

static void ProfilePropertySetS64(void*, ProfileIdx idx, int64_t v)
{
    dmProfileJSSetPropertyS64(idx, v);
}

static void ProfilePropertySetU64(void*, ProfileIdx idx, uint64_t v)
{
    dmProfileJSSetPropertyU64(idx, v);
}

static void ProfilePropertySetF64(void*, ProfileIdx idx, double v)
{
    dmProfileJSSetPropertyF64(idx, v);
}

static void ProfilePropertyAddS32(void*, ProfileIdx idx, int32_t v)
{
    dmProfileJSAddPropertyS32(idx, v);
}

static void ProfilePropertyAddU32(void*, ProfileIdx idx, uint32_t v)
{
    dmProfileJSAddPropertyU32(idx, v);
}

static void ProfilePropertyAddF32(void*, ProfileIdx idx, float v)
{
    dmProfileJSAddPropertyF32(idx, v);
}

static void ProfilePropertyAddS64(void*, ProfileIdx idx, int64_t v)
{
    dmProfileJSAddPropertyS64(idx, v);
}

static void ProfilePropertyAddU64(void*, ProfileIdx idx, uint64_t v)
{
    dmProfileJSAddPropertyU64(idx, v);
}

static void ProfilePropertyAddF64(void*, ProfileIdx idx, double v)
{
    dmProfileJSAddPropertyF64(idx, v);
}

static void ProfilePropertyReset(void*, ProfileIdx idx)
{
    dmProfileJSResetProperty(idx);
}

static const char* g_ProfilerName = "ProfilerJS";
static ProfileListener g_Listener = {};

static dmExtension::Result ProfilerJS_AppInitialize(dmExtension::AppParams* params)
{
    const char* perf_timeline_enabled_key = "profiler.performance_timeline_enabled";
    g_ProfilerOptions_PerformanceTimelineEnabled = dmConfigFile::GetInt(params->m_ConfigFile, perf_timeline_enabled_key, 0);
    if (!g_ProfilerOptions_PerformanceTimelineEnabled)
    {
        dmLogInfo("Skipped %s profiler due to setting %s", g_ProfilerName, perf_timeline_enabled_key);
        return dmExtension::RESULT_OK;
    }

    g_Listener.m_Create = CreateListener;
    g_Listener.m_Destroy = DestroyListener;
    g_Listener.m_SetThreadName = SetThreadName;

    g_Listener.m_FrameBegin = FrameBegin;
    g_Listener.m_FrameEnd = FrameEnd;
    g_Listener.m_ScopeBegin = ScopeBegin;
    g_Listener.m_ScopeEnd = ScopeEnd;

    g_Listener.m_LogText = 0;

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
    return dmExtension::RESULT_OK;
}

static dmExtension::Result ProfilerJS_AppFinalize(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(ProfilerJS, g_ProfilerName, ProfilerJS_AppInitialize, ProfilerJS_AppFinalize, 0, 0, 0, 0);
