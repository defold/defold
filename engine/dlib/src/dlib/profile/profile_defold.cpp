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
#include "dlib/log.h"
#include "dlib/spinlock.h"
#include "dlib/static_assert.h"
#include "dlib/thread.h"
#include "dlib/time.h"

#include <assert.h>
#include <stdio.h> // vsnprintf
#include <stdarg.h> // va_start et al

// https://github.com/defold/defold/blob/1.2.189/engine/dlib/src/dlib/profile.cpp

struct Property
{
    const char*                 m_Name;
    const char*                 m_Description;
    uint32_t                    m_NameHash;
    uint32_t                    m_Flags;
    dmProfilePropertyIdx        m_Parent;
    dmProfilePropertyIdx        m_Sibling;
    dmProfilePropertyIdx        m_FirstChild;
    dmProfile::PropertyValue    m_DefaultValue;
    dmProfile::PropertyType     m_Type;
};

struct PropertyData
{
    dmProfilePropertyIdx        m_Index;
    dmProfile::PropertyValue    m_Value;
};

static bool             g_PropertyInitialized = false;
static const uint32_t   g_MaxPropertyCount = 256;
static uint32_t         g_PropertyCount = 1; // #0 is in valid!
static Property         g_Properties[g_MaxPropertyCount];

static void PropertyInitialize()
{
    if (g_PropertyInitialized)
        return;
    memset(g_Properties, 0, sizeof(g_Properties));
    g_PropertyInitialized = true;

    g_Properties[0].m_Name      = "Root";
    g_Properties[0].m_NameHash  = dmHashString32(g_Properties[0].m_Name);
    g_Properties[0].m_Type      = dmProfile::PROPERTY_TYPE_GROUP;
    g_Properties[0].m_Parent    = DM_PROFILE_PROPERTY_INVALID_IDX;
    g_Properties[0].m_FirstChild= DM_PROFILE_PROPERTY_INVALID_IDX;
    g_Properties[0].m_Sibling   = DM_PROFILE_PROPERTY_INVALID_IDX;
}

static Property* AllocateProperty(dmProfilePropertyIdx* idx)
{
    if (g_PropertyCount >= g_MaxPropertyCount)
        return 0;
    *idx = g_PropertyCount++;
    return &g_Properties[*idx];
}

static inline bool IsValidIndex(dmProfilePropertyIdx idx)
{
    return idx != DM_PROFILE_PROPERTY_INVALID_IDX;
}

static Property* GetPropertyFromIdx(dmProfilePropertyIdx idx)
{
    if (!IsValidIndex(idx))
        return 0;
    return &g_Properties[idx];
}

static PropertyData* GetFramePropertyFromIdx(dmProfilePropertyIdx idx);

namespace dmProfile
{
    #define CHECK_INITIALIZED() \
        if (!dmProfile::IsInitialized()) \
            return;

    #define CHECK_INITIALIZED_RETVAL(RETVAL) \
        if (!dmProfile::IsInitialized()) \
            return RETVAL;


    static const uint32_t INVALID_INDEX = 0xFFFFFFFF;

    struct Scope
    {
        const char* m_Name;
        uint32_t    m_NameHash;
        uint16_t    m_Index;    /// Scope index, range [0, scopes-1]
        void*       m_Internal;
    };

    struct ScopeData
    {
        Scope*   m_Scope;
        uint32_t m_Elapsed; /// Total time spent in scope (in ticks) summed over all threads
        uint32_t m_Count;   /// Occurrences of this scope summed over all threads
    };

    struct Sample
    {
        const char* m_Name;
        Scope*      m_Scope;    /// Sampled within scope
        uint32_t    m_Start;    /// Start time in ticks
        uint32_t    m_Elapsed;  /// Elapsed time in ticks
        uint32_t    m_NameHash; /// Sample name hash
        uint16_t    m_ThreadId; /// Thread id this sample belongs to
        uint16_t    :16;        /// Padding to 64-bit align
    };

    DM_STATIC_ASSERT(sizeof(Sample) == 32, Invalid_struct_size);

    struct Profile
    {
        dmArray<Sample>         m_Samples;
        dmArray<ScopeData>      m_ScopesData;
        dmArray<PropertyData>   m_PropertyData; // 1:1 index mapping to g_Properties array
        uint32_t                m_ScopeCount;
        //uint32_t                m_CounterCount;
    };

    struct ProfileContext
    {
        dmSpinlock::Spinlock    m_Lock;
        dmThread::TlsKey        m_TlsKey;
        int32_atomic_t          m_ThreadCount;

        uint64_t                m_BeginTime;

        Profile                 m_EmptyProfile;
        Profile*                m_ActiveProfile;

        dmArray<Profile*>       m_FreeProfiles;

        dmArray<Scope>          m_Scopes;

        ProfileContext()
        {
            m_TlsKey        = dmThread::AllocTls();
            m_ThreadCount   = 0;

            dmSpinlock::Create(&m_Lock);

            memset(&m_EmptyProfile, 0, sizeof(m_EmptyProfile));
            m_ActiveProfile = 0;//&m_EmptyProfile;

            // Setup profiles

            const uint32_t max_samples    = 8192;
            const uint32_t max_scopes     = 256;
            const uint32_t max_properties = g_MaxPropertyCount;

            // let's support only one profile at a time
            Profile* p = &m_EmptyProfile;

            p->m_Samples.SetCapacity(max_samples);

            p->m_PropertyData.SetCapacity(max_properties);
            p->m_PropertyData.SetSize(max_properties);

            p->m_ScopesData.SetCapacity(max_scopes);
            p->m_ScopesData.SetSize(max_scopes);

            p->m_Samples.SetSize(0); // Could be > 0 if Initialized is called again after Finalize

            //p->m_ScopeCount = 0;
            //p->m_CounterCount = 0;
        }

        ~ProfileContext()
        {
            dmSpinlock::Destroy(&m_Lock);
        }
    };

    static void DebugProperties(int indent, Property* prop)
    {
        Property* parent = GetPropertyFromIdx(prop->m_Parent);

        for (int i = 0; i < indent; ++i)
            printf("  ");
        printf("Property: '%s'  parent: '%s'\n", prop->m_Name, parent?parent->m_Name:"-");

        dmProfilePropertyIdx idx = prop->m_FirstChild;
        while(IsValidIndex(idx))
        {
            Property* child = GetPropertyFromIdx(idx);
            assert(child != 0);
            DebugProperties(indent+1, child);

            idx = child->m_Sibling;
        }

    }


    FSampleTreeCallback     g_SampleTreeCallback = 0;
    FPropertyTreeCallback   g_PropertyTreeCallback = 0;
    ProfileContext* g_ProfileContext = 0;
    ScopeData       g_DummyScopeData = { 0 };
    Scope           g_DummyScope = { "dummy", 0u, 0, &g_DummyScopeData };

    void Initialize(const Options* options)
    {
        if (!dLib::IsDebugMode())
            return;

        dmLogInfo("Defold Profiler");
        DebugProperties(0, &g_Properties[0]);

        assert(!IsInitialized());
        g_ProfileContext = new ProfileContext;

        dmSpinlock::Create(&g_ProfileContext->m_Lock);
    }

    void Finalize()
    {
        if (g_ProfileContext)
        {
            dmSpinlock::Destroy(&g_ProfileContext->m_Lock);
            delete g_ProfileContext;
        }
    }

    bool IsInitialized()
    {
        return g_ProfileContext != 0;
    }

    void SetThreadName(const char* name)
    {
        CHECK_INITIALIZED();
        // we're on the main thread here, so we'll respect the current name
        (void)name;
    }

    void AddCounter(const char* name, uint32_t amount)
    {
        // Not supported currently.
        // Used by mem profiler to dynamically insert a property at runtime
    }

    static void ResetProfile(ProfileContext* ctx, Profile* profile)
    {
        profile->m_Samples.SetSize(0);

        uint32_t n = ctx->m_Scopes.Size(); // instead of resetting max count every frame
        for (uint32_t i = 0; i < n; ++i)
        {
            ScopeData* scope_data = &profile->m_ScopesData[i];
            scope_data->m_Elapsed = 0;
            scope_data->m_Count = 0;
            profile->m_ScopesData[i].m_Scope = &ctx->m_Scopes[i];
        }

        n = g_PropertyCount;
        for (uint32_t i = 0; i < n; ++i)
        {
            profile->m_PropertyData[i].m_Index = i;
            profile->m_PropertyData[i].m_Value.m_U64 = 0;
        }
    }

    HProfile BeginFrame()
    {
        CHECK_INITIALIZED_RETVAL(0);
        DM_SPINLOCK_SCOPED_LOCK(g_ProfileContext->m_Lock);

        // let's support only one profile frame at a time for now
        assert(g_ProfileContext->m_ActiveProfile == 0);
        g_ProfileContext->m_ActiveProfile = &g_ProfileContext->m_EmptyProfile;

        g_ProfileContext->m_BeginTime = dmTime::GetMonotonicTime();

        Profile* profile = g_ProfileContext->m_ActiveProfile;
        ResetProfile(g_ProfileContext, profile);

        return profile;
    }

    static void ResetProperties(Profile* profile)
    {
        uint32_t n = profile->m_PropertyData.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            PropertyData* data = &profile->m_PropertyData[i];
            Property* prop = &g_Properties[i];
            if ((prop->m_Flags & PROFILE_PROPERTY_FRAME_RESET) == PROFILE_PROPERTY_FRAME_RESET)
            {
                data->m_Value = prop->m_DefaultValue;
            }
        }
    }

    void EndFrame(HProfile _profile)
    {
        CHECK_INITIALIZED();
        DM_SPINLOCK_SCOPED_LOCK(g_ProfileContext->m_Lock);

        Profile* profile = (Profile*)_profile;

        // For each top property
        if (g_PropertyTreeCallback)
            g_PropertyTreeCallback(profile, &profile->m_PropertyData[0]);

        ResetProperties(profile);

        g_ProfileContext->m_ActiveProfile = 0;
    }

    uint64_t GetTicksPerSecond()
    {
        return 1000000;
    }

    void LogText(const char* format, ...)
    {
        CHECK_INITIALIZED();

        // va_list lst;
        // va_start(lst, format);
        // LogInternal(LOG_SEVERITY_INFO, "PROFILE", format, lst);
        // va_end(lst);

        char buffer[DM_PROFILE_TEXT_LENGTH];

        va_list lst;
        va_start(lst, format);
        int n = vsnprintf(buffer, DM_PROFILE_TEXT_LENGTH, format, lst);
        if (n < 0)
        {
            buffer[DM_PROFILE_TEXT_LENGTH-1] = '\0';
        }
        va_end(lst);

        // Simply outputting via the logger
        dmLogInfo("%s", buffer);
    }

    void SetSampleTreeCallback(void* ctx, FSampleTreeCallback callback)
    {
        g_SampleTreeCallback = callback;
    }

    void SetPropertyTreeCallback(void* ctx, FPropertyTreeCallback callback)
    {
        g_PropertyTreeCallback = callback;
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

    #define CHECK_HPROPERTY(HPROP) \
        PropertyData* data = (PropertyData*)(HPROP); \
        Property* prop = GetPropertyFromIdx(data->m_Index);

    PropertyIterator* PropertyIterateChildren(HProperty hproperty, PropertyIterator* iter)
    {
        CHECK_HPROPERTY(hproperty);
        (void)prop;
        iter->m_Property = 0;
        iter->m_IteratorImpl = (void*)(uintptr_t)prop->m_FirstChild;
        return iter;
    }

    bool PropertyIterateNext(PropertyIterator* iter)
    {
        dmProfilePropertyIdx idx = (dmProfilePropertyIdx)(uintptr_t)iter->m_IteratorImpl;
        if (!IsValidIndex(idx))
            return false;
        Property* prop = GetPropertyFromIdx(idx);
        PropertyData* data = GetFramePropertyFromIdx(idx);
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

static PropertyData* GetFramePropertyFromIdx(dmProfilePropertyIdx idx)
{
    if (!IsValidIndex(idx))
        return 0;
    dmProfile::Profile* profile = dmProfile::g_ProfileContext->m_ActiveProfile;
    if (!profile)
        return 0;
    if (idx >= profile->m_PropertyData.Size())
        return 0;
    return &profile->m_PropertyData[idx];
}

#define ALLOC_PROP_AND_CHECK() \
    dmProfilePropertyIdx idx; \
    Property* prop = AllocateProperty(&idx); \
    if (!prop) \
        return DM_PROFILE_PROPERTY_INVALID_IDX;

#define GET_PROP_AND_CHECK(IDX) \
    if (!dmProfile::IsInitialized()) \
        return; \
    PropertyData* data = GetFramePropertyFromIdx(IDX); \
    if (!data) \
        return; \
    assert(data->m_Index == (IDX));

static void SetupProperty(Property* prop, dmProfilePropertyIdx idx, const char* name, const char* desc, uint32_t flags, dmProfilePropertyIdx* parentidx)
{
    memset(prop, 0, sizeof(Property));
    //prop->initialised    = RMT_FALSE;
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

dmProfilePropertyIdx dmProfileCreatePropertyGroup(const char* name, const char* desc, dmProfilePropertyIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    PropertyInitialize();
    SetupProperty(prop, idx, name, desc, 0, parentidx);
    prop->m_Type = dmProfile::PROPERTY_TYPE_GROUP;
    //printf("PROPERTY: %s %s - %s  idx: %u  parent: %u  child: %u\n", name, desc, "group", idx, prop->m_Parent, prop->m_FirstChild);
    return idx;
}

dmProfilePropertyIdx dmProfileCreatePropertyBool(const char* name, const char* desc, int value, uint32_t flags, dmProfilePropertyIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    PropertyInitialize();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_Bool = (bool)value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_BOOL;
    //printf("PROPERTY: %s %s - %s  idx: %u  parent: %u  child: %u\n", name, desc, "bool", idx, prop->m_Parent, prop->m_FirstChild);
    return idx;
}

dmProfilePropertyIdx dmProfileCreatePropertyS32(const char* name, const char* desc, int32_t value, uint32_t flags, dmProfilePropertyIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    PropertyInitialize();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_S32 = value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_S32;
    //printf("PROPERTY: %s %s - %s  idx: %u  parent: %u  child: %u\n", name, desc, "s32", idx, prop->m_Parent, prop->m_FirstChild);
    return idx;
}

dmProfilePropertyIdx dmProfileCreatePropertyU32(const char* name, const char* desc, uint32_t value, uint32_t flags, dmProfilePropertyIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    PropertyInitialize();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_U32 = value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_U32;
    //printf("PROPERTY: %s %s - %s  idx: %u  parent: %u  child: %u\n", name, desc, "u32", idx, prop->m_Parent, prop->m_FirstChild);
    return idx;
}

dmProfilePropertyIdx dmProfileCreatePropertyF32(const char* name, const char* desc, float value, uint32_t flags, dmProfilePropertyIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    PropertyInitialize();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_F32 = value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_F32;
    //printf("PROPERTY: %s %s - %s  idx: %u  parent: %u  child: %u\n", name, desc, "f32", idx, prop->m_Parent, prop->m_FirstChild);
    return idx;
}

dmProfilePropertyIdx dmProfileCreatePropertyS64(const char* name, const char* desc, int64_t value, uint32_t flags, dmProfilePropertyIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    PropertyInitialize();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_S64 = value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_S64;
    //printf("PROPERTY: %s %s - %s  idx: %u  parent: %u  child: %u\n", name, desc, "s64", idx, prop->m_Parent, prop->m_FirstChild);
    return idx;
}

dmProfilePropertyIdx dmProfileCreatePropertyU64(const char* name, const char* desc, uint64_t value, uint32_t flags, dmProfilePropertyIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    PropertyInitialize();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_U64 = value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_U64;
    //printf("PROPERTY: %s %s - %s  idx: %u  parent: %u  child: %u\n", name, desc, "u64", idx, prop->m_Parent, prop->m_FirstChild);
    return idx;
}

dmProfilePropertyIdx dmProfileCreatePropertyF64(const char* name, const char* desc, double value, uint32_t flags, dmProfilePropertyIdx* parentidx)
{
    ALLOC_PROP_AND_CHECK();
    PropertyInitialize();
    SetupProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_F64 = value;
    prop->m_Type = dmProfile::PROPERTY_TYPE_F64;
    //printf("PROPERTY: %s %s - %s  idx: %u  parent: %u  child: %u\n", name, desc, "f64", idx, prop->m_Parent, prop->m_FirstChild);
    return idx;
}

void dmProfilePropertySetBool(dmProfilePropertyIdx idx, int v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_Bool = v;
}

void dmProfilePropertySetS32(dmProfilePropertyIdx idx, int32_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_S32 = v;
}

void dmProfilePropertySetU32(dmProfilePropertyIdx idx, uint32_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_U32 = v;
}

void dmProfilePropertySetF32(dmProfilePropertyIdx idx, float v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_F32 = v;
}

void dmProfilePropertySetS64(dmProfilePropertyIdx idx, int64_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_S64 = v;
}

void dmProfilePropertySetU64(dmProfilePropertyIdx idx, uint64_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_U64 = v;
}

void dmProfilePropertySetF64(dmProfilePropertyIdx idx, double v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_F64 = v;
}

void dmProfilePropertyAddS32(dmProfilePropertyIdx idx, int32_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_S32 += v;
}

void dmProfilePropertyAddU32(dmProfilePropertyIdx idx, uint32_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_U32 += v;
}

void dmProfilePropertyAddF32(dmProfilePropertyIdx idx, float v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_F32 += v;
}

void dmProfilePropertyAddS64(dmProfilePropertyIdx idx, int64_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_S64 += v;
}

void dmProfilePropertyAddU64(dmProfilePropertyIdx idx, uint64_t v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_U64 += v;
}

void dmProfilePropertyAddF64(dmProfilePropertyIdx idx, double v)
{
    GET_PROP_AND_CHECK(idx);
    data->m_Value.m_F64 += v;
}

void dmProfilePropertyReset(dmProfilePropertyIdx idx)
{
    GET_PROP_AND_CHECK(idx);
    Property* prop = GetPropertyFromIdx(data->m_Index);
    data->m_Value = prop->m_DefaultValue;
}
