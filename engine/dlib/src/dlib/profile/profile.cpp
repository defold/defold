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

#include "profile.h"
#include <stdio.h>
#include <stdio.h> // vsnprintf
#include <stdarg.h> // va_start et al

#include <dlib/atomic.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/mutex.h>

struct ProfileProperty
{
    const char*             m_Name;
    const char*             m_Desc;
    ProfilePropertyValue    m_DefaultValue;
    uint64_t                m_NameHash;
    ProfileIdx              m_Idx;
    ProfileIdx              m_ParentIdx;
    ProfileIdx              m_FirstChild;
    ProfileIdx              m_Sibling;
    uint32_t                m_Flags;
    ProfilePropertyType     m_Type;
};

// *************************************************************************************************
// State
static int32_atomic_t   g_ProfileInitialized = 0;
static ProfileListener* g_ProfileListeners = 0;
static dmMutex::HMutex  g_ProfileLock = 0;

// Properties are initialized at global scope, so we put them in a preallocated static array
// to avoid dynamic allocations or other setup issues
static int32_atomic_t   g_PropertyInitialized = 0;
static uint32_t         g_ProfilePropertyCount = 1; // 0 is root!
static ProfileProperty  g_ProfileProperties[PROFILER_MAX_NUM_PROPERTIES];

// *************************************************************************************************
// Helpers

static bool IsProfileInitialized()
{
    return dmAtomicGet32(&g_ProfileInitialized) != 0;
}

static void CreateRegisteredProperties();
static void PropertyInitialize();

// *************************************************************************************************
// C api

// *************************************************************************************************
// Listeners

static void CreateListener(ProfileListener* profiler)
{
    profiler->m_Ctx = 0;
    if (profiler->m_Create)
    {
        profiler->m_Ctx = profiler->m_Create();
    }
    profiler->m_Disabled = profiler->m_Ctx == 0;
}

static void CreateProperty(ProfileListener* profiler, ProfileProperty* prop)
{
    #define CREATE_TYPE(STRTYPE) if (profiler->m_CreateProperty ## STRTYPE ) profiler->m_CreateProperty ## STRTYPE (profiler->m_Ctx, prop->m_Name, prop->m_Desc, prop->m_DefaultValue.m_ ## STRTYPE, prop->m_Flags, prop->m_Idx, prop->m_ParentIdx)

    switch(prop->m_Type)
    {
    case PROFILE_PROPERTY_TYPE_GROUP:   if (profiler->m_CreatePropertyGroup) profiler->m_CreatePropertyGroup(profiler->m_Ctx, prop->m_Name, prop->m_Desc, prop->m_Idx, prop->m_ParentIdx); break;
    case PROFILE_PROPERTY_TYPE_U32:     CREATE_TYPE(U32); break;
    case PROFILE_PROPERTY_TYPE_S32:     CREATE_TYPE(S32); break;
    case PROFILE_PROPERTY_TYPE_F32:     CREATE_TYPE(F32); break;
    case PROFILE_PROPERTY_TYPE_U64:     CREATE_TYPE(U64); break;
    case PROFILE_PROPERTY_TYPE_S64:     CREATE_TYPE(S64); break;
    case PROFILE_PROPERTY_TYPE_F64:     CREATE_TYPE(F64); break;
    case PROFILE_PROPERTY_TYPE_BOOL:    CREATE_TYPE(Bool); break;
    }

    #undef CREATE_TYPE
}

static void CreateListenerProperties(ProfileListener* profiler)
{
    for (uint32_t i = 0; i < g_ProfilePropertyCount; ++i)
    {
        ProfileProperty* prop = &g_ProfileProperties[i];
        CreateProperty(profiler, prop);
    }
}

void ProfileRegisterProfiler(const char* name, ProfileListener* profiler)
{
    {
        // Check for duplicates
        ProfileListener* p = g_ProfileListeners;
        while (p)
        {
            if (strcmp(name, p->m_Name) == 0)
            {
                dmLogError("Profiler '%s' already registered", name);
                return;
            }
            p = p->m_Next;
        }
    }

    DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_ProfileLock);

    dmSnPrintf(profiler->m_Name, sizeof(profiler->m_Name), "%s", name);
    profiler->m_Next = g_ProfileListeners; // it's ok to add it first
    profiler->m_Ctx = 0;
    g_ProfileListeners = profiler;

    if (IsProfileInitialized())
    {
        CreateListener(profiler);
        CreateListenerProperties(profiler);
    }

    dmLogDebug("Registered profiler '%s'", profiler->m_Name);
}

void ProfileUnregisterProfiler(const char* name)
{
    ProfileListener* prev = 0;
    for (ProfileListener* p = g_ProfileListeners; p; p = p->m_Next)
    {
        if (strcmp(name, p->m_Name) == 0)
        {
            if (prev)
                prev->m_Next = p->m_Next;
            else
                g_ProfileListeners = p->m_Next; // it was the first one in the list
            return;
        }
        prev = p;
    }
    g_ProfileListeners = 0;
}

#define DO_LOOP_NOARGS(_FUNC)                                           \
    for (ProfileListener* p = g_ProfileListeners; p!=0; p = p->m_Next)  \
    {                                                                   \
        if (p->m_Disabled) continue;                                    \
        if (p->m_ ## _FUNC)                                             \
        {                                                               \
            p->m_ ## _FUNC(p->m_Ctx);                                   \
        }                                                               \
    }

#define DO_LOOP(_FUNC, ...)                                             \
    for (ProfileListener* p = g_ProfileListeners; p!=0; p = p->m_Next)  \
    {                                                                   \
        if (p->m_Disabled) continue;                                    \
        if (p->m_ ## _FUNC)                                             \
        {                                                               \
            p->m_ ## _FUNC(p->m_Ctx, __VA_ARGS__);                      \
        }                                                               \
    }


// *****************************************************************************************
// Initialize / Finalize

bool ProfileIsInitialized()
{
    return IsProfileInitialized();
}

void ProfileInitialize()
{
    assert(!IsProfileInitialized());

    PropertyInitialize();

    for (ProfileListener* p = g_ProfileListeners; p; p = p->m_Next)
    {
        CreateListener(p);
    }

    dmAtomicIncrement32(&g_ProfileInitialized);

    CreateRegisteredProperties();
}

void ProfileFinalize()
{
    dmAtomicDecrement32(&g_ProfileInitialized);

    {
        DM_MUTEX_SCOPED_LOCK(g_ProfileLock);
        DO_LOOP_NOARGS(Destroy);
    }

    dmMutex::Delete(g_ProfileLock);

    g_ProfileLock = dmMutex::New();

    dmAtomicDecrement32(&g_PropertyInitialized);
}

// *****************************************************************************************
// Frames and scopes

HProfile ProfileFrameBegin()
{
    HProfile result = 0;
    for (ProfileListener* p = g_ProfileListeners; p; p = p->m_Next)
    {
        if (p->m_FrameBegin)
        {
            p->m_FrameBegin(p->m_Ctx);
            result = 1;
        }
    }
    return result;
}

void ProfileFrameEnd(HProfile profile)
{
    if (!profile)
        return;

    DO_LOOP_NOARGS(FrameEnd);
}

void ProfileScopeBegin(const char* name, uint64_t* name_hash)
{
    DM_MUTEX_SCOPED_LOCK(g_ProfileLock);

    if (name_hash && !*name_hash)
    {
        *name_hash = dmHashString64(name);
    }

    DO_LOOP(ScopeBegin, name, name_hash ? *name_hash : 0);
}

void ProfileScopeEnd(const char* name, uint64_t name_hash)
{
    DM_MUTEX_SCOPED_LOCK(g_ProfileLock);

    DO_LOOP(ScopeEnd, name, name_hash);
}

void ProfileSetThreadName(const char* name)
{
    DM_MUTEX_SCOPED_LOCK(g_ProfileLock);

    DO_LOOP(SetThreadName, name);
}

void ProfileLogText(const char* format, ...)
{
    DM_MUTEX_SCOPED_LOCK(g_ProfileLock);

    char buffer[DM_PROFILE_TEXT_LENGTH];

    va_list lst;
    va_start(lst, format);
    int n = vsnprintf(buffer, DM_PROFILE_TEXT_LENGTH, format, lst);
    if (n < 0)
    {
        buffer[DM_PROFILE_TEXT_LENGTH-1] = '\0';
    }
    va_end(lst);

    DO_LOOP(LogText, buffer);
}

// *****************************************************************************************
// Properties

static void CreateProperty(ProfileProperty* prop)
{
    for (ProfileListener* p = g_ProfileListeners; p!=0; p = p->m_Next)
    {
        CreateProperty(p, prop);
    }
}

static void CreateRegisteredProperties()
{
    DM_MUTEX_SCOPED_LOCK(g_ProfileLock);

    for (ProfileListener* p = g_ProfileListeners; p!=0; p = p->m_Next)
    {
        CreateListenerProperties(p);
    }
}

// Invoked when first property is initialized
static void PropertyInitialize()
{
    if (dmAtomicIncrement32(&g_PropertyInitialized) != 0)
        return;

    g_ProfileLock = dmMutex::New();

    g_ProfileProperties[0].m_Name = "Root";
    g_ProfileProperties[0].m_NameHash = dmHashString32(g_ProfileProperties[0].m_Name);
    g_ProfileProperties[0].m_Type = PROFILE_PROPERTY_TYPE_GROUP;
    g_ProfileProperties[0].m_ParentIdx = PROFILE_PROPERTY_INVALID_IDX;
    g_ProfilePropertyCount = 1;
}

static inline bool IsValidIndex(ProfileIdx idx)
{
    return idx != PROFILE_PROPERTY_INVALID_IDX;
}

static ProfileProperty* GetPropertyFromIdx(ProfileIdx idx)
{
    if (!IsValidIndex(idx))
        return 0;
    return &g_ProfileProperties[idx];
}

static ProfileProperty* AllocateProperty(ProfileIdx* idx)
{
    DM_MUTEX_SCOPED_LOCK(g_ProfileLock);
    if (g_ProfilePropertyCount >= PROFILER_MAX_NUM_PROPERTIES)
        return 0;
    *idx = g_ProfilePropertyCount++;
    return &g_ProfileProperties[*idx];
}

static void RegisterProperty(ProfileProperty* prop, ProfileIdx idx, const char* name, const char* desc, uint32_t flags, ProfileIdx parentidx)
{
    memset(prop, 0, sizeof(ProfileProperty));
    prop->m_Name        = name;
    prop->m_Desc        = desc;
    prop->m_Flags       = flags;
    prop->m_Idx         = idx; // perhaps a little redundant but nice when debugging the property
    prop->m_ParentIdx   = parentidx;
    prop->m_NameHash    = dmHashString64(name);
    prop->m_FirstChild  = PROFILE_PROPERTY_INVALID_IDX;
    prop->m_Sibling     = PROFILE_PROPERTY_INVALID_IDX;

    // If this fails, then we've messed up the dependencies.
    // We must register the parent first. See ALLOC_PROP_AND_CHECK()
    assert(idx > parentidx);

    ProfileProperty* parent = GetPropertyFromIdx(prop->m_ParentIdx);
    if (parent)
    {
        prop->m_Sibling = parent->m_FirstChild;
        parent->m_FirstChild = idx;
    }
}

// This macro also resolves the parentfn() -> parentidx,
// which allows us to create each parent before its children.
#define ALLOC_PROP_AND_CHECK(_PARENTFN)                     \
    PropertyInitialize();                                   \
    ProfileIdx parentidx = (_PARENTFN) ? *(_PARENTFN)() : 0;\
    ProfileIdx idx; \
    ProfileProperty* prop = AllocateProperty(&idx); \
    if (!prop) \
        return PROFILE_PROPERTY_INVALID_IDX;


ProfileIdx ProfileRegisterPropertyGroup(const char* name, const char* desc, ProfileIdx* (*parentfn)())
{
    ALLOC_PROP_AND_CHECK(parentfn);
    RegisterProperty(prop, idx, name, desc, 0, parentidx);
    prop->m_Type = PROFILE_PROPERTY_TYPE_GROUP;
    if (IsProfileInitialized())
        CreateProperty(prop);
    return idx;
}

ProfileIdx ProfileRegisterPropertyBool(const char* name, const char* desc, int value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    ALLOC_PROP_AND_CHECK(parentfn);
    RegisterProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_Bool = (bool)value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_BOOL;
    if (IsProfileInitialized())
        CreateProperty(prop);
    return idx;
}

ProfileIdx ProfileRegisterPropertyS32(const char* name, const char* desc, int32_t value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    ALLOC_PROP_AND_CHECK(parentfn);
    RegisterProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_S32 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_S32;
    if (IsProfileInitialized())
        CreateProperty(prop);
    return idx;
}

ProfileIdx ProfileRegisterPropertyU32(const char* name, const char* desc, uint32_t value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    ALLOC_PROP_AND_CHECK(parentfn);
    RegisterProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_U32 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_U32;
    if (IsProfileInitialized())
        CreateProperty(prop);
    return idx;
}

ProfileIdx ProfileRegisterPropertyF32(const char* name, const char* desc, float value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    ALLOC_PROP_AND_CHECK(parentfn);
    RegisterProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_F32 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_F32;
    if (IsProfileInitialized())
        CreateProperty(prop);
    return idx;
}

ProfileIdx ProfileRegisterPropertyS64(const char* name, const char* desc, int64_t value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    ALLOC_PROP_AND_CHECK(parentfn);
    RegisterProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_S64 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_S64;
    if (IsProfileInitialized())
        CreateProperty(prop);
    return idx;
}

ProfileIdx ProfileRegisterPropertyU64(const char* name, const char* desc, uint64_t value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    ALLOC_PROP_AND_CHECK(parentfn);
    RegisterProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_U64 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_U64;
    if (IsProfileInitialized())
        CreateProperty(prop);
    return idx;
}

ProfileIdx ProfileRegisterPropertyF64(const char* name, const char* desc, double value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    ALLOC_PROP_AND_CHECK(parentfn);
    RegisterProperty(prop, idx, name, desc, flags, parentidx);
    prop->m_DefaultValue.m_F64 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_F64;
    if (IsProfileInitialized())
        CreateProperty(prop);
    return idx;
}

#define GET_PROP_AND_CHECK(IDX) \
    if (!IsProfileInitialized()) \
        return; \
    DM_MUTEX_SCOPED_LOCK(g_ProfileLock); \
    ProfileProperty* prop = &g_ProfileProperties[(IDX)]; \
    assert(prop->m_Idx == (IDX));

void ProfilePropertySetBool(ProfileIdx idx, int v)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertySetBool, idx, v);
}

void ProfilePropertySetS32(ProfileIdx idx, int32_t v)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertySetS32, idx, v);
}

void ProfilePropertySetU32(ProfileIdx idx, uint32_t v)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertySetU32, idx, v);
}

void ProfilePropertySetF32(ProfileIdx idx, float v)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertySetF32, idx, v);
}

void ProfilePropertySetS64(ProfileIdx idx, int64_t v)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertySetS64, idx, v);
}

void ProfilePropertySetU64(ProfileIdx idx, uint64_t v)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertySetU64, idx, v);
}

void ProfilePropertySetF64(ProfileIdx idx, double v)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertySetF64, idx, v);
}

void ProfilePropertyAddS32(ProfileIdx idx, int32_t v)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertyAddS32, idx, v);
}

void ProfilePropertyAddU32(ProfileIdx idx, uint32_t v)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertyAddU32, idx, v);
}

void ProfilePropertyAddF32(ProfileIdx idx, float v)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertyAddF32, idx, v);
}

void ProfilePropertyAddS64(ProfileIdx idx, int64_t v)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertyAddS64, idx, v);
}

void ProfilePropertyAddU64(ProfileIdx idx, uint64_t v)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertyAddU64, idx, v);
}

void ProfilePropertyAddF64(ProfileIdx idx, double v)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertyAddF64, idx, v);
}

void ProfilePropertyReset(ProfileIdx idx)
{
    GET_PROP_AND_CHECK(idx);
    DO_LOOP(PropertyReset, idx);
}

