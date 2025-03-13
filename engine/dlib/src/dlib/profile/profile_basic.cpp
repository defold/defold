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

#include "dlib/array.h"
#include "dlib/atomic.h"
#include "dlib/dlib.h" // IsDebugMode
#include "dlib/hash.h"
#include "dlib/hashtable.h"
#include "dlib/log.h"
#include "dlib/math.h"
#include "dlib/mutex.h"
#include "dlib/thread.h"
#include "dlib/time.h"

#include <assert.h>
#include <stdio.h> // vsnprintf
#include <stdarg.h> // va_start et al

struct Property
{
    const char*              m_Name;
    const char*              m_Description;
    uint32_t                 m_NameHash;
    uint32_t                 m_Flags;
    dmProfileIdx             m_Parent;
    dmProfileIdx             m_Sibling;
    dmProfileIdx             m_FirstChild;
    dmProfile::PropertyValue m_DefaultValue;
    dmProfile::PropertyType  m_Type;
};

struct PropertyData
{
    dmProfile::PropertyValue    m_Value;
    dmProfileIdx                m_Index:63;
    dmProfileIdx                m_Used:1;
};

struct Sample
{
    Sample*         m_Parent;
    Sample*         m_Sibling;
    Sample*         m_FirstChild;
    Sample*         m_LastChild;
    uint64_t        m_Start;            // Start time for first sample (in microseconds)
    uint64_t        m_TempStart;        // Start time for sub sample
    uint64_t        m_Length;           // Elapsed time in microseconds
    uint64_t        m_LengthChildren;   // Elapsed time in children in microseconds
    uint32_t        m_NameHash;         // faster access to name hash
    uint32_t        m_CallCount;        // Number of times this scope was called
};

///////////////////////////////////////////////////////////////////////////////////////////////

// Properties are initialized at global scope, so we put them in a preallocated static array
// to avoid dynamic allocations or other setup issues
static int32_atomic_t   g_PropertyInitialized = 0;
static const uint32_t   g_MaxPropertyCount = 256;
static const uint32_t   g_MaxSampleCount = 4096;
static uint32_t         g_PropertyCount = 1; // #0 is root!
static Property         g_Properties[g_MaxPropertyCount];

// At global scope as they're set _before_ the profiler is started
static dmProfile::FSampleTreeCallback      g_SampleTreeCallback = 0;
static void*                               g_SampleTreeCallbackCtx = 0;
static dmProfile::FPropertyTreeCallback    g_PropertyTreeCallback = 0;
static void*                               g_PropertyTreeCallbackCtx = 0;

// Synchronization
static dmThread::TlsKey         g_TlsKey = dmThread::AllocTls();
static int32_atomic_t           g_ThreadCount = 0;
static int32_atomic_t           g_ProfileInitialized = 0;
static dmMutex::HMutex          g_Lock = 0;

static struct ProfileContext*   g_ProfileContext = 0;

///////////////////////////////////////////////////////////////////////////////////////////////
// UTILS

static inline uint32_t HashString32(const char* name)
{
    return dmHashBufferNoReverse32(name, strlen(name));
}

static inline bool IsValidIndex(dmProfileIdx idx)
{
    return idx != DM_PROFILE_PROPERTY_INVALID_IDX;
}

static inline uint32_t MakeColorFromHash(uint32_t hash)
{
    // Borrowed from Remotery.c

    // Hash integer line position to full hue
    float h = (float)hash / (float)0xFFFFFFFF;
    float r = dmMath::Clamp(fabsf(fmodf(h * 6 + 0, 6) - 3) - 1, 0.0f, 1.0f);
    float g = dmMath::Clamp(fabsf(fmodf(h * 6 + 4, 6) - 3) - 1, 0.0f, 1.0f);
    float b = dmMath::Clamp(fabsf(fmodf(h * 6 + 2, 6) - 3) - 1, 0.0f, 1.0f);

    // Cubic smooth
    r = r * r * (3 - 2 * r);
    g = g * g * (3 - 2 * g);
    b = b * b * (3 - 2 * b);

    // Lerp to HSV lightness a little
    float k = 0.4;
    r = r * k + (1 - k);
    g = g * k + (1 - k);
    b = b * k + (1 - k);

    uint8_t br = (uint8_t)255*dmMath::Clamp(r, 0.0f, 1.0f);
    uint8_t bg = (uint8_t)255*dmMath::Clamp(g, 0.0f, 1.0f);
    uint8_t bb = (uint8_t)255*dmMath::Clamp(b, 0.0f, 1.0f);
    uint8_t ba = 0xFF;

    return (ba<<24) | (br << 16) | (bg << 8) | (bb << 0);
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
    dmArray<PropertyData>      m_PropertyData; // 1:1 index mapping to g_Properties array
    dmHashTable32<ThreadData*> m_ThreadData;

    ProfileContext()
    {
        m_PropertyData.SetCapacity(g_MaxPropertyCount);
        m_PropertyData.SetSize(g_MaxPropertyCount);

        for (uint32_t i = 0; i < g_MaxPropertyCount; ++i)
        {
            m_PropertyData[i].m_Index = i;
            m_PropertyData[i].m_Value.m_U64 = 0;
        }
        m_PropertyData[0].m_Used = 1;
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
    const char* name = strdup(original);
    uint32_t name_hash = GetOrCacheNameHash(name, pname_hash);

    if (g_ProfileContext->m_Names.Full())
    {
        uint32_t cap = g_ProfileContext->m_Names.Capacity() + 64;
        g_ProfileContext->m_Names.SetCapacity((cap*2)/3, cap);
    }
    g_ProfileContext->m_Names.Put(name_hash, name);
    return name_hash;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// PROPERTIES

static Property*        GetPropertyFromIdx(dmProfileIdx idx);
static PropertyData*    GetPropertyDataFromIdx(dmProfileIdx idx);

// Invoked when first property is initialized
static void PropertyInitialize()
{
    if (dmAtomicIncrement32(&g_PropertyInitialized) != 0)
        return;

    memset(g_Properties, 0, sizeof(g_Properties));

    g_Lock = dmMutex::New();

    g_Properties[0].m_Name      = "Root";
    g_Properties[0].m_NameHash  = dmHashString32(g_Properties[0].m_Name);
    g_Properties[0].m_Type      = dmProfile::PROPERTY_TYPE_GROUP;
    g_Properties[0].m_Parent    = DM_PROFILE_PROPERTY_INVALID_IDX;
    g_Properties[0].m_FirstChild= DM_PROFILE_PROPERTY_INVALID_IDX;
    g_Properties[0].m_Sibling   = DM_PROFILE_PROPERTY_INVALID_IDX;
}

static void ResetProperties(ProfileContext* ctx)
{
    uint32_t n = ctx->m_PropertyData.Size();
    for (uint32_t i = 0; i < n; ++i)
    {
        PropertyData* data = &ctx->m_PropertyData[i];
        data->m_Used = 0;

        Property* prop = &g_Properties[i];
        if ((prop->m_Flags & PROFILE_PROPERTY_FRAME_RESET) == PROFILE_PROPERTY_FRAME_RESET)
        {
            data->m_Value = prop->m_DefaultValue;
        }
    }

    ctx->m_PropertyData[0].m_Used = 1;
}

static void DebugProperties(int indent, Property* prop)
{
    Property* parent = GetPropertyFromIdx(prop->m_Parent);

    for (int i = 0; i < indent; ++i)
        printf("  ");
    printf("Property: '%s'  parent: '%s'\n", prop->m_Name, parent?parent->m_Name:"-");

    dmProfileIdx idx = prop->m_FirstChild;
    while(IsValidIndex(idx))
    {
        Property* child = GetPropertyFromIdx(idx);
        assert(child != 0);
        DebugProperties(indent+1, child);

        idx = child->m_Sibling;
    }
}

static void DebugSamples(int indent, Sample* sample)
{
    for (int i = 0; i < indent; ++i)
        printf("  ");
    printf("'%s'  length: %.3f ms  #%u\n", GetNameFromHash(sample->m_NameHash), sample->m_Length/1000.0f, sample->m_CallCount);

    Sample* child = sample->m_FirstChild;
    while(child)
    {
        DebugSamples(indent+1, child);
        child = child->m_Sibling;
    }
}

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
        ctx->m_ThreadData.SetCapacity((cap*2)/3, cap);
    }
    ctx->m_ThreadData.Put(thread_id, td);
    return td;
}

///////////////////////////////////////////////////////////////////////////////////////////////

namespace dmProfile
{
    static const uint32_t INVALID_INDEX = 0xFFFFFFFF;

    void Initialize(const Options* options)
    {
        if (!dLib::IsDebugMode())
            return;

        assert(!IsInitialized());
        g_ProfileContext = new ProfileContext;

        dmAtomicIncrement32(&g_ProfileInitialized);

        SetThreadName("Main");
    }

    void Finalize()
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

    bool IsInitialized()
    {
        return IsProfileInitialized();
    }

    void SetThreadName(const char* name)
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

    void AddCounter(const char* name, uint32_t amount)
    {
        // Not supported currently.
        // Used by mem profiler to dynamically insert a property at runtime
    }

    HProfile BeginFrame()
    {
        CHECK_INITIALIZED_RETVAL(0);
        DM_MUTEX_SCOPED_LOCK(g_Lock);

        return (HProfile)g_ProfileContext;
    }

    void EndFrame(HProfile profile)
    {
        (void)profile;
        CHECK_INITIALIZED();
        DM_MUTEX_SCOPED_LOCK(g_Lock);

        // For each top property
        if (g_PropertyTreeCallback)
            g_PropertyTreeCallback(g_PropertyTreeCallbackCtx, &g_ProfileContext->m_PropertyData[0]);

        ResetProperties(g_ProfileContext);
    }

    uint64_t GetTicksPerSecond()
    {
        // Since this profiler uses the dmTime::GetMonotonicTime() which returns microseconds
        return 1000000;
    }

    void LogText(const char* format, ...)
    {
        (void)format;

        // This function is called by the logging system, and is intended for passing the data on to
        // any connected devices collecting the profile data.
    }

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

    void ScopeBegin(const char* name, uint64_t* pname_hash)
    {
        CHECK_INITIALIZED();
        DM_MUTEX_SCOPED_LOCK(g_Lock);

        uint64_t tstart = dmTime::GetMonotonicTime();

        uint32_t name_hash = InternalizeName(name, (uint32_t*)pname_hash);

        ThreadData* td = GetOrCreateThreadData(g_ProfileContext, GetThreadId());
        Sample* sample = AllocateSample(td, name_hash); // Adds it to the thread data

        if (sample->m_CallCount > 1) // we want to preserve the real start of this sample
            sample->m_TempStart = tstart;
        else
            sample->m_Start = tstart;
    }

    void ScopeEnd()
    {
        CHECK_INITIALIZED();
        DM_MUTEX_SCOPED_LOCK(g_Lock);

        uint64_t end = dmTime::GetMonotonicTime();

        ThreadData* td = GetOrCreateThreadData(g_ProfileContext, GetThreadId());
        Sample* sample = td->m_CurrentSample;

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

        td->m_CurrentSample = sample->m_Parent;
        assert(td->m_CurrentSample != 0);

        if (td->m_CurrentSample == &td->m_Root)
        {
            // We've closed a top scope (there may be several) on this thread and are ready to present the info
            if (g_SampleTreeCallback)
            {
                // TODO: ability to show the other threads
                // Note: the rest of the threads make their own calls to this callback
                int32_t thread_id = GetThreadId();
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

    void ProfileScopeHelper::StartScope(const char* name, uint64_t* pname_hash)
    {
        if (name[0] == 0)
            name = "<empty>";
        ScopeBegin(name, pname_hash);
    }

    void ProfileScopeHelper::EndScope()
    {
        ScopeEnd();
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
        Sample* sample = (Sample*)hsample;
        return MakeColorFromHash(sample->m_NameHash);
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
        PropertyData* data = (PropertyData*)(HPROP); \
        Property* prop = GetPropertyFromIdx(data->m_Index);

    PropertyIterator* PropertyIterateChildren(HProperty hproperty, PropertyIterator* iter)
    {
        CHECK_HPROPERTY(hproperty);
        (void)prop;
        iter->m_Property = 0;
        iter->m_IteratorImpl = (void*)(uintptr_t)INVALID_INDEX;
        if (data->m_Used)
        {
            iter->m_IteratorImpl = (void*)(uintptr_t)prop->m_FirstChild;
        }
        return iter;
    }

    bool PropertyIterateNext(PropertyIterator* iter)
    {
        dmProfileIdx idx = (dmProfileIdx)(uintptr_t)iter->m_IteratorImpl;
        if (!IsValidIndex(idx))
            return false;

        Property* prop = GetPropertyFromIdx(idx);
        PropertyData* data = GetPropertyDataFromIdx(idx);
        assert(data->m_Index == idx);
        iter->m_Property = data;
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

    PropertyType PropertyGetType(HProperty hproperty)
    {
        CHECK_HPROPERTY(hproperty)
        return prop->m_Type;
    }

    PropertyValue PropertyGetValue(HProperty hproperty)
    {
        CHECK_HPROPERTY(hproperty)
        (void)prop;
        return data->m_Value;
    }

    // *******************************************************************


} // namespace dmProfile

// *******************************************************************


static Property* AllocateProperty(dmProfileIdx* idx)
{
    DM_MUTEX_SCOPED_LOCK(g_Lock);
    if (g_PropertyCount >= g_MaxPropertyCount)
        return 0;
    *idx = g_PropertyCount++;
    return &g_Properties[*idx];
}

static Property* GetPropertyFromIdx(dmProfileIdx idx)
{
    if (!IsValidIndex(idx))
        return 0;
    DM_MUTEX_SCOPED_LOCK(g_Lock);
    return &g_Properties[idx];
}

static PropertyData* GetPropertyDataFromIdx(dmProfileIdx idx)
{
    if (!IsValidIndex(idx))
        return 0;
    if (idx >= g_ProfileContext->m_PropertyData.Size())
        return 0;
    return &g_ProfileContext->m_PropertyData[idx];
}


#define ALLOC_PROP_AND_CHECK() \
    PropertyInitialize(); \
    dmProfileIdx idx; \
    Property* prop = AllocateProperty(&idx); \
    if (!prop) \
        return DM_PROFILE_PROPERTY_INVALID_IDX;

#define GET_PROP_AND_CHECK(IDX) \
    if (!IsProfileInitialized()) \
        return; \
    DM_MUTEX_SCOPED_LOCK(g_Lock); \
    PropertyData* data = GetPropertyDataFromIdx(IDX); \
    if (!data) \
        return; \
    assert(data->m_Index == (IDX));

static void SetupProperty(Property* prop, dmProfileIdx idx, const char* name, const char* desc, uint32_t flags, dmProfileIdx* parentidx)
{
    memset(prop, 0, sizeof(Property));
    prop->m_Flags        = flags;
    prop->m_Name         = name;
    prop->m_NameHash     = dmHashString32(name);
    prop->m_Description  = desc;
    prop->m_FirstChild   = dmProfile::INVALID_INDEX;
    prop->m_Sibling      = dmProfile::INVALID_INDEX;
    prop->m_Parent       = parentidx ? *parentidx : 0;

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

dmProfileIdx dmProfileCreatePropertyGroup(const char* name, const char* desc, dmProfileIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, 0, parentidx);
    prop->m_Type = dmProfile::PROPERTY_TYPE_GROUP;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyBool(const char* name, const char* desc, int value, uint32_t flags, dmProfileIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_Bool = (bool)value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_BOOL;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyS32(const char* name, const char* desc, int32_t value, uint32_t flags, dmProfileIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_S32 = value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_S32;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyU32(const char* name, const char* desc, uint32_t value, uint32_t flags, dmProfileIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_U32 = value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_U32;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyF32(const char* name, const char* desc, float value, uint32_t flags, dmProfileIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_F32 = value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_F32;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyS64(const char* name, const char* desc, int64_t value, uint32_t flags, dmProfileIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_S64 = value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_S64;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyU64(const char* name, const char* desc, uint64_t value, uint32_t flags, dmProfileIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_U64 = value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_U64;
    return idx;
}

dmProfileIdx dmProfileCreatePropertyF64(const char* name, const char* desc, double value, uint32_t flags, dmProfileIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_F64 = value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_F64;
    return idx;
}

void dmProfilePropertySetBool(dmProfileIdx idx, int v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_Bool = v;
    data->m_Used = 1;
}

void dmProfilePropertySetS32(dmProfileIdx idx, int32_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_S32 = v;
    data->m_Used = 1;
}

void dmProfilePropertySetU32(dmProfileIdx idx, uint32_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_U32 = v;
    data->m_Used = 1;
}

void dmProfilePropertySetF32(dmProfileIdx idx, float v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_F32 = v;
    data->m_Used = 1;
}

void dmProfilePropertySetS64(dmProfileIdx idx, int64_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_S64 = v;
    data->m_Used = 1;
}

void dmProfilePropertySetU64(dmProfileIdx idx, uint64_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_U64 = v;
    data->m_Used = 1;
}

void dmProfilePropertySetF64(dmProfileIdx idx, double v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_F64 = v;
    data->m_Used = 1;
}

void dmProfilePropertyAddS32(dmProfileIdx idx, int32_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_S32 += v;
    data->m_Used = 1;
}

void dmProfilePropertyAddU32(dmProfileIdx idx, uint32_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_U32 += v;
    data->m_Used = 1;
}

void dmProfilePropertyAddF32(dmProfileIdx idx, float v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_F32 += v;
    data->m_Used = 1;
}

void dmProfilePropertyAddS64(dmProfileIdx idx, int64_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_S64 += v;
    data->m_Used = 1;
}

void dmProfilePropertyAddU64(dmProfileIdx idx, uint64_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_U64 += v;
    data->m_Used = 1;
}

void dmProfilePropertyAddF64(dmProfileIdx idx, double v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_F64 += v;
    data->m_Used = 1;
}

void dmProfilePropertyReset(dmProfileIdx idx)
{
    GET_PROP_AND_CHECK(idx);
    Property* prop = GetPropertyFromIdx(data->m_Index);
    data->m_Value = prop->m_DefaultValue;
    data->m_Used = 0;
}
