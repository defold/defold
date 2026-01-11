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

#include "dlib/array.h"
#include "dlib/atomic.h"
#include "dlib/hash.h"
#include "dlib/hashtable.h"
#include "dlib/log.h"
#include "dlib/math.h"
#include "dlib/mutex.h"
#include "dlib/thread.h"
#include "dlib/time.h"
#include <dlib/profile.h>

#include <assert.h>
#include <stdio.h>

struct Property
{
    const char*             m_Name;
    const char*             m_Description;
    uint32_t                m_NameHash;
    uint32_t                m_Flags;
    ProfileIdx              m_Parent;
    ProfileIdx              m_Sibling;
    ProfileIdx              m_FirstChild;
    ProfilePropertyValue    m_DefaultValue;
    ProfilePropertyType     m_Type;
};

struct PropertyData
{
    ProfilePropertyValue    m_Value;
    uint8_t                 m_Used : 1;
};

struct Sample
{
    Sample*  m_Parent;
    Sample*  m_Sibling;
    Sample*  m_FirstChild;
    Sample*  m_LastChild;
    uint64_t m_Start;          // Start time for first sample (in microseconds)
    uint64_t m_TempStart;      // Start time for sub sample
    uint64_t m_Length;         // Elapsed time in microseconds
    uint64_t m_LengthChildren; // Elapsed time in children in microseconds
    uint32_t m_NameHash;       // faster access to name hash
    uint32_t m_CallCount;      // Number of times this scope was called
};

///////////////////////////////////////////////////////////////////////////////////////////////

// Properties are initialized at global scope, so we put them in a preallocated static array
// to avoid dynamic allocations or other setup issues
static int32_atomic_t g_PropertyInitialized = 0;
static const uint32_t g_MaxPropertyCount = 256;
static const uint32_t g_MaxSampleCount = 4096;
static Property       g_Properties[g_MaxPropertyCount];
static PropertyData   g_PropertyData[g_MaxPropertyCount];

// At global scope as they're set _before_ the profiler is started
static dmProfiler::FSampleTreeCallback   g_SampleTreeCallback = 0;
static void*                             g_SampleTreeCallbackCtx = 0;
static dmProfiler::FPropertyTreeCallback g_PropertyTreeCallback = 0;
static void*                             g_PropertyTreeCallbackCtx = 0;

// Synchronization
static dmThread::TlsKey       g_TlsKey = dmThread::AllocTls();
static int32_atomic_t         g_ThreadCount = 0;
static int32_atomic_t         g_ProfileInitialized = 0;
static dmMutex::HMutex        g_Lock = 0;

static struct ProfileContext* g_ProfileContext = 0;

///////////////////////////////////////////////////////////////////////////////////////////////
// UTILS

static inline uint32_t HashString32(const char* name)
{
    return dmHashBufferNoReverse32(name, strlen(name));
}

static inline bool IsValidIndex(ProfileIdx idx)
{
    return idx != PROFILE_PROPERTY_INVALID_IDX;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// ThreadData

static void ResetThreadData(struct ThreadData* td);

// We use one instance of ThreadData per thread, keeping track of it's sample scopes
struct ThreadData
{
    Sample          m_Root;
    Sample*         m_CurrentSample;
    dmArray<Sample> m_SamplePool;
    int32_t         m_ThreadId;

    ThreadData(int32_t thread_id)
        : m_ThreadId(thread_id)
    {
        memset(this, 0, sizeof(*this));
        m_Root.m_NameHash = HashString32("Root");

        // mustn't reallocate as we use pointers directly into this array
        m_SamplePool.SetCapacity(g_MaxSampleCount);
        m_SamplePool.SetSize(0);

        ResetThreadData(this);
    }
};

static Sample* AllocateNewSample(ThreadData* td)
{
    static Sample g_DummySample = { 0 };

    if (td->m_SamplePool.Full())
    {
        return &g_DummySample;
    }

    td->m_SamplePool.SetSize(td->m_SamplePool.Size() + 1);
    return td->m_SamplePool.End() - 1;
}

static void ResetThreadData(ThreadData* td)
{
    td->m_SamplePool.SetSize(0);
    td->m_Root.m_FirstChild = 0;
    td->m_Root.m_LastChild = 0;
    td->m_CurrentSample = &td->m_Root;
}

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

static const char* GetNameFromHash(uint32_t name_hash)
{
    CHECK_INITIALIZED_RETVAL("null");
    DM_MUTEX_SCOPED_LOCK(g_Lock);
    const char** pname = g_ProfileContext->m_Names.Get(name_hash);
    return pname != 0 ? *pname : "null";
}

static uint32_t GetOrCacheNameHash(const char* name, uint32_t* pname_hash)
{
    if (pname_hash)
    {
        uint32_t name_hash = *pname_hash;
        if (!name_hash)
        {
            *pname_hash = HashString32(name);
        }
        return *pname_hash;
    }
    return HashString32(name); // No cache to store it in, so we have to recalculate each time
}

static uint32_t InternalizeName(const char* original, uint32_t* pname_hash)
{
    uint32_t name_hash = GetOrCacheNameHash(original, pname_hash);

    if (g_ProfileContext->m_Names.Full())
    {
        uint32_t cap = g_ProfileContext->m_Names.Capacity() + 64;
        g_ProfileContext->m_Names.SetCapacity((cap * 2) / 3, cap);
    }

    const char** existing = g_ProfileContext->m_Names.Get(name_hash);
    if (existing)
    {
        return name_hash;
    }

    const char* name = strdup(original);
    g_ProfileContext->m_Names.Put(name_hash, name);
    return name_hash;
}

///////////////////////////////////////
// PROPERTIES

static Property*     GetPropertyFromIdx(ProfileIdx idx);
static PropertyData* GetPropertyDataFromIdx(ProfileIdx idx);

// Invoked when first property is initialized
static void PropertyInitialize()
{
    if (dmAtomicIncrement32(&g_PropertyInitialized) != 0)
        return;

    memset(g_Properties, 0, sizeof(g_Properties));
    memset(g_PropertyData, 0, sizeof(g_PropertyData));

    g_Lock = dmMutex::New();

    g_Properties[0].m_Name = "Root";
    g_Properties[0].m_NameHash = dmHashString32(g_Properties[0].m_Name);
    g_Properties[0].m_Type = PROFILE_PROPERTY_TYPE_GROUP;
    g_Properties[0].m_Parent = PROFILE_PROPERTY_INVALID_IDX;
    g_Properties[0].m_FirstChild = PROFILE_PROPERTY_INVALID_IDX;
    g_Properties[0].m_Sibling = PROFILE_PROPERTY_INVALID_IDX;
    g_PropertyData[0].m_Used = 1; // used == 0, means we won't traverse it during display
}

static void ResetProperties(ProfileContext* ctx)
{
    for (uint32_t i = 0; i < g_MaxPropertyCount; ++i)
    {
        Property* prop = &g_Properties[i];
        PropertyData* data = &g_PropertyData[i];
        data->m_Used = prop->m_Type == PROFILE_PROPERTY_TYPE_GROUP ? 1 : 0;

        if ((prop->m_Flags & PROFILE_PROPERTY_FRAME_RESET) == PROFILE_PROPERTY_FRAME_RESET)
        {
            data->m_Value = prop->m_DefaultValue;
        }
    }
}

// static void DebugProperties(int indent, Property* prop)
// {
//     Property* parent = GetPropertyFromIdx(prop->m_Parent);

//     for (int i = 0; i < indent; ++i)
//         printf("  ");
//     printf("Property: '%s'  parent: '%s'\n", prop->m_Name, parent ? parent->m_Name : "-");

//     ProfileIdx   idx = prop->m_FirstChild;
//     while (IsValidIndex(idx))
//     {
//         Property* child = GetPropertyFromIdx(idx);
//         assert(child != 0);
//         DebugProperties(indent + 1, child);

//         idx = child->m_Sibling;
//     }
// }

// static void DebugSamples(int indent, Sample* sample)
// {
//     for (int i = 0; i < indent; ++i)
//         printf("  ");
//     printf("'%s'  length: %.3f ms  #%u\n", GetNameFromHash(sample->m_NameHash), sample->m_Length / 1000.0f, sample->m_CallCount);

//     Sample* child = sample->m_FirstChild;
//     while (child)
//     {
//         DebugSamples(indent + 1, child);
//         child = child->m_Sibling;
//     }
// }

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

static const char* GetThreadName(int32_t thread_id)
{
    CHECK_INITIALIZED_RETVAL("null");
    DM_MUTEX_SCOPED_LOCK(g_Lock);
    const char** pname = g_ProfileContext->m_ThreadNames.Get(thread_id);
    return pname != 0 ? *pname : "null";
}

static ThreadData* GetOrCreateThreadData(ProfileContext* ctx, int32_t thread_id)
{
    ThreadData** pdata = ctx->m_ThreadData.Get(thread_id);
    if (pdata)
        return *pdata;

    ThreadData* td = new ThreadData(thread_id);

    if (ctx->m_ThreadData.Full())
    {
        uint32_t cap = ctx->m_ThreadData.Capacity() + 16;
        ctx->m_ThreadData.SetCapacity((cap * 2) / 3, cap);
    }
    ctx->m_ThreadData.Put(thread_id, td);
    return td;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static Sample* AllocateSample(ThreadData* td, uint32_t name_hash)
{
    Sample* parent = td->m_CurrentSample;

    // Aggregate samples:
    //      If there already is a child with this name, use that
    Sample* sample = parent->m_FirstChild;
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
        sample = AllocateNewSample(td);
        memset(sample, 0, sizeof(Sample));

        sample->m_NameHash = name_hash;

        // link the sample into the tree
        Sample* parent = td->m_CurrentSample;
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
    td->m_CurrentSample = sample;
    return sample;
}

namespace dmProfiler
{
    void SetSampleTreeCallback(void* ctx, FSampleTreeCallback callback)
    {
        // NOTE: Called _before_ initialization
        g_SampleTreeCallback = callback;
        g_SampleTreeCallbackCtx = ctx;
    }

    void SetPropertyTreeCallback(void* ctx, FPropertyTreeCallback callback)
    {
        // NOTE: Called _before_ initialization
        g_PropertyTreeCallback = callback;
        g_PropertyTreeCallbackCtx = ctx;
    }

    SampleIterator::SampleIterator()
        : m_Sample(0)
        , m_IteratorImpl(0)
    {
    }

    SampleIterator::~SampleIterator()
    {
    }

    SampleIterator* SampleIterateChildren(HSample hsample, SampleIterator* iter)
    {
        Sample* sample = (Sample*)hsample;
        iter->m_Sample = 0;
        iter->m_IteratorImpl = (void*)sample->m_FirstChild;
        return iter;
    }

    bool SampleIterateNext(SampleIterator* iter)
    {
        Sample* sample = (Sample*)iter->m_IteratorImpl;
        if (!sample)
            return false;

        iter->m_Sample = sample;
        iter->m_IteratorImpl = sample->m_Sibling;
        return true;
    }

    uint32_t SampleGetNameHash(HSample hsample)
    {
        Sample* sample = (Sample*)hsample;
        return sample->m_NameHash;
    }

    const char* SampleGetName(HSample hsample)
    {
        Sample* sample = (Sample*)hsample;
        return GetNameFromHash(sample->m_NameHash);
    }

    uint64_t SampleGetStart(HSample hsample)
    {
        Sample* sample = (Sample*)hsample;
        return sample->m_Start;
    }

    uint64_t SampleGetTime(HSample hsample)
    {
        Sample* sample = (Sample*)hsample;
        return sample->m_Length;
    }

    uint64_t SampleGetSelfTime(HSample hsample)
    {
        Sample* sample = (Sample*)hsample;
        return dmMath::Max((uint64_t)0, sample->m_Length - sample->m_LengthChildren);
    }

    uint32_t SampleGetCallCount(HSample hsample)
    {
        Sample* sample = (Sample*)hsample;
        return sample->m_CallCount;
    }

    uint32_t SampleGetColor(HSample hsample)
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

#define CHECK_HPROPERTY(HPROP) \
    uint32_t        idx  = (uint32_t)(uintptr_t)HPROP; \
    PropertyData*   data = GetPropertyDataFromIdx(idx); \
    Property*       prop = GetPropertyFromIdx(idx); \
    (void)data; (void)prop;

    PropertyIterator* PropertyIterateChildren(HProperty hproperty, PropertyIterator* iter)
    {
        CHECK_HPROPERTY(hproperty);
        iter->m_Property = 0;
        iter->m_IteratorImpl = (void*)(uintptr_t)PROFILE_PROPERTY_INVALID_IDX;
        if (data->m_Used)
        {
            iter->m_IteratorImpl = (void*)(uintptr_t)prop->m_FirstChild;
        }
        return iter;
    }

    bool PropertyIterateNext(PropertyIterator* iter)
    {
        ProfileIdx idx = (ProfileIdx)(uintptr_t)iter->m_IteratorImpl;
        if (!IsValidIndex(idx))
            return false;

        Property* prop = GetPropertyFromIdx(idx);
        iter->m_Property = (void*)(uintptr_t)idx;
        iter->m_IteratorImpl = (void*)(uintptr_t)prop->m_Sibling;
        return true;
    }

    // Property accessors

    uint32_t PropertyGetNameHash(HProperty hproperty)
    {
        CHECK_HPROPERTY(hproperty)
        return prop->m_NameHash;
    }

    const char* PropertyGetName(HProperty hproperty)
    {
        CHECK_HPROPERTY(hproperty)
        return prop->m_Name;
    }

    const char* PropertyGetDesc(HProperty hproperty)
    {
        CHECK_HPROPERTY(hproperty)
        return prop->m_Description;
    }

    ProfilePropertyType PropertyGetType(HProperty hproperty)
    {
        CHECK_HPROPERTY(hproperty)
        return prop->m_Type;
    }

    ProfilePropertyValue PropertyGetValue(HProperty hproperty)
    {
        CHECK_HPROPERTY(hproperty)
        return data->m_Value;
    }

    // *******************************************************************

} // namespace dmProfiler

// ***************************************************************************************************************
// Listener api

static void SetThreadName(void* ctx, const char* name)
{
    (void)ctx;
    PropertyInitialize();
    CHECK_INITIALIZED();
    DM_MUTEX_SCOPED_LOCK(g_Lock);
    int32_t      thread_id = GetThreadId();
    const char** existing = g_ProfileContext->m_ThreadNames.Get(thread_id);
    if (existing)
    {
        free((void*)*existing);
    }

    if (g_ProfileContext->m_ThreadNames.Full())
    {
        uint32_t cap = g_ProfileContext->m_ThreadNames.Capacity() + 32;
        g_ProfileContext->m_ThreadNames.SetCapacity((cap * 2) / 3, cap);
    }
    g_ProfileContext->m_ThreadNames.Put(thread_id, strdup(name));
}

static void* CreateListener()
{
    assert(!IsProfileInitialized());
    g_ProfileContext = new ProfileContext;

    dmAtomicIncrement32(&g_ProfileInitialized);

    SetThreadName(g_ProfileContext, "Main");

    return (void*)g_ProfileContext;
}

static void DestroyListener(void* listener)
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

static void FrameBegin(void* ctx)
{
    (void)ctx;
    CHECK_INITIALIZED();
    DM_MUTEX_SCOPED_LOCK(g_Lock);
}

static void FrameEnd(void* ctx)
{
    (void)ctx;
    CHECK_INITIALIZED();
    DM_MUTEX_SCOPED_LOCK(g_Lock);

    // For each top property
    if (g_PropertyTreeCallback)
        g_PropertyTreeCallback(g_PropertyTreeCallbackCtx, 0); // 0 being the root index

    ResetProperties(g_ProfileContext);
}

static void ScopeBegin(void* ctx, const char* name, uint64_t name_hash)
{
    (void)ctx;
    CHECK_INITIALIZED();
    DM_MUTEX_SCOPED_LOCK(g_Lock);

    uint64_t    tstart = dmTime::GetMonotonicTime();

    InternalizeName(name, (uint32_t*)&name_hash);

    ThreadData* td = GetOrCreateThreadData(g_ProfileContext, GetThreadId());
    Sample*     sample = AllocateSample(td, name_hash); // Adds it to the thread data

    if (sample->m_CallCount > 1) // we want to preserve the real start of this sample
        sample->m_TempStart = tstart;
    else
        sample->m_Start = tstart;
}

static void ScopeEnd(void* ctx, const char* name, uint64_t name_hash)
{
    (void)ctx;
    CHECK_INITIALIZED();
    DM_MUTEX_SCOPED_LOCK(g_Lock);

    uint64_t    end = dmTime::GetMonotonicTime();

    ThreadData* td = GetOrCreateThreadData(g_ProfileContext, GetThreadId());
    Sample*     sample = td->m_CurrentSample;

    uint64_t    length = 0;
    if (sample->m_CallCount > 1)
        length = dmMath::Max(end - sample->m_TempStart, (uint64_t)0);
    else
        length = dmMath::Max(end - sample->m_Start, (uint64_t)0);

    sample->m_Length += (uint32_t)length;

    if (sample->m_Parent)
    {
        sample->m_Parent->m_LengthChildren += length;
    }

    td->m_CurrentSample = sample->m_Parent;
    assert(td->m_CurrentSample != 0);

    if (td->m_CurrentSample == &td->m_Root)
    {
        // We've closed a top scope (there may be several) on this thread and are ready to present the info
        if (g_SampleTreeCallback)
        {
            // TODO: ability to show the other threads
            // Note: the rest of the threads make their own calls to this callback
            int32_t     thread_id = GetThreadId();
            ThreadData* td = GetOrCreateThreadData(g_ProfileContext, thread_id);
            td->m_Root.m_Length = td->m_Root.m_LengthChildren;
            Sample* sibling = td->m_Root.m_FirstChild;
            while (sibling)
            {
                g_SampleTreeCallback(g_SampleTreeCallbackCtx, GetThreadName(thread_id), sibling);
                sibling = sibling->m_Sibling;
            }
        }

        ResetThreadData(td);
    }
}

// ***************************************************************************************************************
// Properties

static Property* AllocateProperty(ProfileIdx idx)
{
    DM_MUTEX_SCOPED_LOCK(g_Lock);
    if (idx >= g_MaxPropertyCount)
        return 0;
    return &g_Properties[idx];
}

static Property* GetPropertyFromIdx(ProfileIdx idx)
{
    if (!IsValidIndex(idx))
        return 0;
    DM_MUTEX_SCOPED_LOCK(g_Lock);
    return &g_Properties[idx];
}

static PropertyData* GetPropertyDataFromIdx(ProfileIdx idx)
{
    if (!IsValidIndex(idx))
        return 0;
    if (idx >= g_MaxPropertyCount)
        return 0;
    return &g_PropertyData[idx];
}

#define ALLOC_PROP_AND_CHECK() \
    PropertyInitialize(); \
    Property* prop = AllocateProperty(idx); \
    if (!prop) \
        return

#define GET_PROPDATA_AND_CHECK(IDX) \
    if (!IsProfileInitialized()) \
        return; \
    DM_MUTEX_SCOPED_LOCK(g_Lock); \
    PropertyData* data = GetPropertyDataFromIdx(IDX); \
    if (!data) \
        return;

static void SetupProperty(Property* prop, ProfileIdx idx, const char* name, const char* desc, uint32_t flags, ProfileIdx parentidx)
{
    memset(prop, 0, sizeof(Property));
    prop->m_Flags = flags;
    prop->m_Name = name;
    prop->m_NameHash = dmHashString32(name);
    prop->m_Description = desc;
    prop->m_FirstChild = PROFILE_PROPERTY_INVALID_IDX;
    prop->m_Sibling = PROFILE_PROPERTY_INVALID_IDX;
    prop->m_Parent = parentidx;

    // TODO: append them last so that they'll render in the same order they were registered
    Property* parent = GetPropertyFromIdx(prop->m_Parent);
    if (parent)
    {
        prop->m_Sibling = parent->m_FirstChild;
        parent->m_FirstChild = idx;
    }
    if (prop->m_Parent == 0)
    {
        assert(idx != 0);
    }
}

static void ProfileCreatePropertyGroup(void* ctx, const char* name, const char* desc, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, 0, parent);
    prop->m_Type = PROFILE_PROPERTY_TYPE_GROUP;
}

static void ProfileCreatePropertyBool(void* ctx, const char* name, const char* desc, int value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_Bool = (bool)value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_BOOL;
}

static void ProfileCreatePropertyS32(void* ctx, const char* name, const char* desc, int32_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_S32 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_S32;
}

static void ProfileCreatePropertyU32(void* ctx, const char* name, const char* desc, uint32_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_U32 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_U32;
}

static void ProfileCreatePropertyF32(void* ctx, const char* name, const char* desc, float value, uint32_t flags, ProfileIdx idx, ProfileIdx     parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_F32 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_F32;
}

static void ProfileCreatePropertyS64(void* ctx, const char* name, const char* desc, int64_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_S64 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_S64;
}

static void ProfileCreatePropertyU64(void* ctx, const char* name, const char* desc, uint64_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_U64 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_U64;
}

static void ProfileCreatePropertyF64(void* ctx, const char* name, const char* desc, double value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_F64 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_F64;
}

static void ProfilePropertySetBool(void* ctx, ProfileIdx idx, int v)
{
    (void)ctx;
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_Bool = v;
    data->m_Used = 1;
}

static void ProfilePropertySetS32(void* ctx, ProfileIdx idx, int32_t v)
{
    (void)ctx;
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_S32 = v;
    data->m_Used = 1;
}

static void ProfilePropertySetU32(void* ctx, ProfileIdx idx, uint32_t v)
{
    (void)ctx;
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_U32 = v;
    data->m_Used = 1;
}

static void ProfilePropertySetF32(void* ctx, ProfileIdx idx, float v)
{
    (void)ctx;
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_F32 = v;
    data->m_Used = 1;
}

static void ProfilePropertySetS64(void* ctx, ProfileIdx idx, int64_t v)
{
    (void)ctx;
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_S64 = v;
    data->m_Used = 1;
}

static void ProfilePropertySetU64(void* ctx, ProfileIdx idx, uint64_t v)
{
    (void)ctx;
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_U64 = v;
    data->m_Used = 1;
}

static void ProfilePropertySetF64(void* ctx, ProfileIdx idx, double v)
{
    (void)ctx;
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_F64 = v;
    data->m_Used = 1;
}

static void ProfilePropertyAddS32(void* ctx, ProfileIdx idx, int32_t v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_S32 += v;
    data->m_Used = 1;
}

static void ProfilePropertyAddU32(void* ctx, ProfileIdx idx, uint32_t v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_U32 += v;
    data->m_Used = 1;
}

static void ProfilePropertyAddF32(void* ctx, ProfileIdx idx, float v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_F32 += v;
    data->m_Used = 1;
}

static void ProfilePropertyAddS64(void* ctx, ProfileIdx idx, int64_t v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_S64 += v;
    data->m_Used = 1;
}

static void ProfilePropertyAddU64(void* ctx, ProfileIdx idx, uint64_t v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_U64 += v;
    data->m_Used = 1;
}

static void ProfilePropertyAddF64(void* ctx, ProfileIdx idx, double v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_F64 += v;
    data->m_Used = 1;
}

static void ProfilePropertyReset(void* ctx, ProfileIdx idx)
{
    (void)ctx;
    GET_PROPDATA_AND_CHECK(idx);
    Property* prop = GetPropertyFromIdx(idx);
    data->m_Value = prop->m_DefaultValue;
    data->m_Used = prop->m_Type == PROFILE_PROPERTY_TYPE_GROUP ? 1 : 0;
}

static const char* g_ProfilerName = "ProfilerBasic";
static ProfileListener g_Listener = {};

static dmExtension::Result ProfilerBasic_AppInitialize(dmExtension::AppParams* params)
{
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

    if (!IsProfileInitialized())
    {
        ProfileRegisterProfiler(g_ProfilerName, &g_Listener);
    }
    return dmExtension::RESULT_OK;
}

static dmExtension::Result ProfilerBasic_AppFinalize(dmExtension::AppParams* params)
{
    if (dmExtension::AppParamsGetAppExitCode(params) == dmExtension::APP_EXIT_CODE_EXIT)
    {
        ProfileUnregisterProfiler(g_ProfilerName);
    }
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(ProfilerBasic, g_ProfilerName, ProfilerBasic_AppInitialize, ProfilerBasic_AppFinalize, 0, 0, 0, 0);
