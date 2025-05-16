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

extern "C" {
// Implementation in library_profile.js
bool dmProfileJSInit();
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
uint32_t dmProfileJSCreatePropertyGroup(uint32_t type, const char *name, const char *desc, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyBool(uint32_t type, const char *name, const char *desc, bool v, uint32_t flags, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyS32(uint32_t type, const char *name, const char *desc, int32_t v, uint32_t flags, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyU32(uint32_t type, const char *name, const char *desc, uint32_t v, uint32_t flags, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyF32(uint32_t type, const char *name, const char *desc, float v, uint32_t flags, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyS64(uint32_t type, const char *name, const char *desc, int64_t v, uint32_t flags, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyU64(uint32_t type, const char *name, const char *desc, uint64_t v, uint32_t flags, uint32_t parent_idx);
uint32_t dmProfileJSCreatePropertyF64(uint32_t type, const char *name, const char *desc, double v, uint32_t flags, uint32_t parent_idx);
bool dmProfileJSGetPropertyBool(uint32_t idx);
int32_t dmProfileJSGetPropertyS32(uint32_t idx);
uint32_t dmProfileJSGetPropertyU32(uint32_t idx);
float dmProfileJSGetPropertyF32(uint32_t idx);
int64_t dmProfileJSGetPropertyS64(uint32_t idx);
uint64_t dmProfileJSGetPropertyU64(uint32_t idx);
double dmProfileJSGetPropertyF64(uint32_t idx);
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

        if (!dmProfileJSInit())
        {
            dmLogError("Failed to initialize the profiler");
            return;
        }

        assert(!IsInitialized());
        g_Lock = dmMutex::New();
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

        if (g_PropertyTreeCallback)
            g_PropertyTreeCallback(g_PropertyTreeCallbackCtx, (void*)(uintptr_t)0);
        dmProfileJSEndFrame(GetThreadId());
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

    void ScopeBegin(const char* name, uint64_t* pname_hash)
    {
        CHECK_INITIALIZED();
        DM_MUTEX_SCOPED_LOCK(g_Lock);
        dmProfileJSBeginMark(GetThreadId(), name);
    }

    void ScopeEnd()
    {
        CHECK_INITIALIZED();
        DM_MUTEX_SCOPED_LOCK(g_Lock);
        const int32_t thread_id = GetThreadId();
        if (dmProfileJSEndMark(thread_id) && g_SampleTreeCallback)
        {
            // We've closed a top scope (there may be several) on this thread and are ready to present the info
            for (int id = dmProfileJSFirstChild(thread_id, 0); id != dmProfile::INVALID_INDEX; id = dmProfileJSNextSibling(thread_id, id))
                g_SampleTreeCallback(g_SampleTreeCallbackCtx, GetThreadName(thread_id), (void*)(uintptr_t)id);
            dmProfileJSReset(thread_id);
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
    SampleIterator::SampleIterator() : m_Sample(0), m_IteratorImpl((void*)(uintptr_t)dmProfile::INVALID_INDEX)
    {
    }

    SampleIterator::~SampleIterator()
    {
    }

    SampleIterator* SampleIterateChildren(HSample hsample, SampleIterator* iter)
    {
        iter->m_Sample = 0;
        iter->m_IteratorImpl = (void*)(uintptr_t)dmProfileJSFirstChild(GetThreadId(), (uintptr_t)hsample);
        return iter;
    }

    bool SampleIterateNext(SampleIterator* iter)
    {
        const dmProfileIdx current = (dmProfileIdx)(uintptr_t)iter->m_IteratorImpl;
        if (current == dmProfile::INVALID_INDEX)
            return false;
        iter->m_Sample = iter->m_IteratorImpl;
        iter->m_IteratorImpl = (void*)(uintptr_t)dmProfileJSNextSibling(GetThreadId(), current);
        return true;
    }

    uint32_t SampleGetNameHash(HSample hsample)
    {
        return dmHashString32(SampleGetName(hsample));
    }

    const char* SampleGetName(HSample hsample)
    {
        return dmProfileJSGetName(GetThreadId(), (uintptr_t)hsample);
    }

    uint64_t SampleGetStart(HSample hsample)
    {
        return dmProfileJSGetStart(GetThreadId(), (uintptr_t)hsample) * 1000;
    }

    uint64_t SampleGetTime(HSample hsample)
    {
        return dmProfileJSGetDuration(GetThreadId(), (uintptr_t)hsample) * 1000;
    }

    uint64_t SampleGetSelfTime(HSample hsample)
    {
        return dmProfileJSGetSelfDuration(GetThreadId(), (uintptr_t)hsample) * 1000;
    }

    uint32_t SampleGetCallCount(HSample hsample)
    {
        return dmProfileJSGetCallCount(GetThreadId(), (uintptr_t)hsample);
    }

    uint32_t SampleGetColor(HSample)
    {
        return 0;
    }

    // *******************************************************************
    PropertyIterator::PropertyIterator() : m_Property(0), m_IteratorImpl((void*)(uintptr_t)dmProfile::INVALID_INDEX)
    {
    }

    PropertyIterator::~PropertyIterator()
    {
    }

    PropertyIterator* PropertyIterateChildren(HProperty hproperty, PropertyIterator* iter)
    {
        iter->m_Property = 0;
        iter->m_IteratorImpl = (void*)(uintptr_t)dmProfileJSFirstProperty((uintptr_t)hproperty);
        return iter;
    }

    bool PropertyIterateNext(PropertyIterator* iter)
    {
        const dmProfileIdx current = (dmProfileIdx)(uintptr_t)iter->m_IteratorImpl;
        if (current == dmProfile::INVALID_INDEX)
            return false;
        iter->m_Property = iter->m_IteratorImpl;
        iter->m_IteratorImpl = (void*)(uintptr_t)dmProfileJSNextProperty(current);
        return true;
    }

    // Property accessors

    uint32_t PropertyGetNameHash(HProperty hproperty)
    {
        return dmHashString32(PropertyGetName(hproperty));
    }

    const char* PropertyGetName(HProperty hproperty)
    {
        return dmProfileJSGetPropertyName((uintptr_t)hproperty);
    }

    const char* PropertyGetDesc(HProperty hproperty)
    {
        return dmProfileJSGetPropertyDescription((uintptr_t)hproperty);
    }

    PropertyType PropertyGetType(HProperty hproperty)
    {
        return (PropertyType)dmProfileJSGetPropertyType((uintptr_t)hproperty);
    }

    PropertyValue PropertyGetValue(HProperty hproperty)
    {
        PropertyValue result;
        switch(PropertyGetType(hproperty))
        {
            case PROPERTY_TYPE_GROUP:
                break;
            case PROPERTY_TYPE_BOOL:
                result.m_Bool = dmProfileJSGetPropertyBool((uintptr_t)hproperty);
                break;
            case PROPERTY_TYPE_S32:
                result.m_S32 = dmProfileJSGetPropertyS32((uintptr_t)hproperty);
                break;
            case PROPERTY_TYPE_U32:
                result.m_U32 = dmProfileJSGetPropertyU32((uintptr_t)hproperty);
                break;
            case PROPERTY_TYPE_F32:
                result.m_F32 = dmProfileJSGetPropertyF32((uintptr_t)hproperty);
                break;
            case PROPERTY_TYPE_S64:
                result.m_S64 = dmProfileJSGetPropertyS64((uintptr_t)hproperty);
                break;
            case PROPERTY_TYPE_U64:
                result.m_U64 = dmProfileJSGetPropertyU64((uintptr_t)hproperty);
                break;
            case PROPERTY_TYPE_F64:
                result.m_F64 = dmProfileJSGetPropertyF64((uintptr_t)hproperty);
                break;
        }
        return result;
    }

    // *******************************************************************


} // namespace dmProfile

// *******************************************************************


dmProfileIdx dmProfileCreatePropertyGroup(const char* name, const char* desc, dmProfileIdx* (*parent)())
{
    return dmProfileJSCreatePropertyGroup(dmProfile::PROPERTY_TYPE_GROUP, name, desc, parent ? *parent() : dmProfile::INVALID_INDEX);
}

dmProfileIdx dmProfileCreatePropertyBool(const char* name, const char* desc, int value, uint32_t flags, dmProfileIdx* (*parent)())
{
    return dmProfileJSCreatePropertyBool(dmProfile::PROPERTY_TYPE_BOOL, name, desc, value, flags, parent ? *parent() : dmProfile::INVALID_INDEX);
}

dmProfileIdx dmProfileCreatePropertyS32(const char* name, const char* desc, int32_t value, uint32_t flags, dmProfileIdx* (*parent)())
{
    return dmProfileJSCreatePropertyS32(dmProfile::PROPERTY_TYPE_S32, name, desc, value, flags, parent ? *parent() : dmProfile::INVALID_INDEX);
}

dmProfileIdx dmProfileCreatePropertyU32(const char* name, const char* desc, uint32_t value, uint32_t flags, dmProfileIdx* (*parent)())
{
    return dmProfileJSCreatePropertyU32(dmProfile::PROPERTY_TYPE_U32, name, desc, value, flags, parent ? *parent() : dmProfile::INVALID_INDEX);
}

dmProfileIdx dmProfileCreatePropertyF32(const char* name, const char* desc, float value, uint32_t flags, dmProfileIdx* (*parent)())
{
    return dmProfileJSCreatePropertyF32(dmProfile::PROPERTY_TYPE_F32, name, desc, value, flags, parent ? *parent() : dmProfile::INVALID_INDEX);
}

dmProfileIdx dmProfileCreatePropertyS64(const char* name, const char* desc, int64_t value, uint32_t flags, dmProfileIdx* (*parent)())
{
    return dmProfileJSCreatePropertyS64(dmProfile::PROPERTY_TYPE_S64, name, desc, value, flags, parent ? *parent() : dmProfile::INVALID_INDEX);
}

dmProfileIdx dmProfileCreatePropertyU64(const char* name, const char* desc, uint64_t value, uint32_t flags, dmProfileIdx* (*parent)())
{
    return dmProfileJSCreatePropertyU64(dmProfile::PROPERTY_TYPE_U64, name, desc, value, flags, parent ? *parent() : dmProfile::INVALID_INDEX);
}

dmProfileIdx dmProfileCreatePropertyF64(const char* name, const char* desc, double value, uint32_t flags, dmProfileIdx* (*parent)())
{
    return dmProfileJSCreatePropertyF64(dmProfile::PROPERTY_TYPE_F64, name, desc, value, flags, parent ? *parent() : dmProfile::INVALID_INDEX);
}

void dmProfilePropertySetBool(dmProfileIdx idx, int v)
{
    dmProfileJSSetPropertyBool(idx, v);
}

void dmProfilePropertySetS32(dmProfileIdx idx, int32_t v)
{
    dmProfileJSSetPropertyS32(idx, v);
}

void dmProfilePropertySetU32(dmProfileIdx idx, uint32_t v)
{
    dmProfileJSSetPropertyU32(idx, v);
}

void dmProfilePropertySetF32(dmProfileIdx idx, float v)
{
    dmProfileJSSetPropertyF32(idx, v);
}

void dmProfilePropertySetS64(dmProfileIdx idx, int64_t v)
{
    dmProfileJSSetPropertyS64(idx, v);
}

void dmProfilePropertySetU64(dmProfileIdx idx, uint64_t v)
{
    dmProfileJSSetPropertyU64(idx, v);
}

void dmProfilePropertySetF64(dmProfileIdx idx, double v)
{
    dmProfileJSSetPropertyF64(idx, v);
}

void dmProfilePropertyAddS32(dmProfileIdx idx, int32_t v)
{
    dmProfileJSAddPropertyS32(idx, v);
}

void dmProfilePropertyAddU32(dmProfileIdx idx, uint32_t v)
{
    dmProfileJSAddPropertyU32(idx, v);
}

void dmProfilePropertyAddF32(dmProfileIdx idx, float v)
{
    dmProfileJSAddPropertyF32(idx, v);
}

void dmProfilePropertyAddS64(dmProfileIdx idx, int64_t v)
{
    dmProfileJSAddPropertyS64(idx, v);
}

void dmProfilePropertyAddU64(dmProfileIdx idx, uint64_t v)
{
    dmProfileJSAddPropertyU64(idx, v);
}

void dmProfilePropertyAddF64(dmProfileIdx idx, double v)
{
    dmProfileJSAddPropertyF64(idx, v);
}

void dmProfilePropertyReset(dmProfileIdx idx)
{
    dmProfileJSResetProperty(idx);
}
