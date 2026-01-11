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

#ifndef DMSDK_PROFILE_H
#define DMSDK_PROFILE_H

/*# Profiling API documentation
 * Profiling macros
 *
 * @document
 * @name Profile
 * @namespace dmProfile
 * @language C++
 */

#include <stdint.h>

/*# Index type to hold internal references of samplers and properties
 * @typedef
 * @name ProfileIdx
 */
typedef uint64_t ProfileIdx;

/*# Handle to a an active profile frame
 * @typedef
 * @name HProfile
 */
typedef uint32_t HProfile;

/*# Enum to describe type of a property
 * @enum
 * @name ProfilePropertyType
 * @member PROFILE_PROPERTY_TYPE_GROUP
 * @member PROFILE_PROPERTY_TYPE_BOOL
 * @member PROFILE_PROPERTY_TYPE_S32
 * @member PROFILE_PROPERTY_TYPE_U32
 * @member PROFILE_PROPERTY_TYPE_F32
 * @member PROFILE_PROPERTY_TYPE_S64
 * @member PROFILE_PROPERTY_TYPE_U64
 * @member PROFILE_PROPERTY_TYPE_F64
 */
enum ProfilePropertyType
{
    PROFILE_PROPERTY_TYPE_GROUP,
    PROFILE_PROPERTY_TYPE_BOOL,
    PROFILE_PROPERTY_TYPE_S32,
    PROFILE_PROPERTY_TYPE_U32,
    PROFILE_PROPERTY_TYPE_F32,
    PROFILE_PROPERTY_TYPE_S64,
    PROFILE_PROPERTY_TYPE_U64,
    PROFILE_PROPERTY_TYPE_F64,
};

/*# Union to hold a property value
 * @union
 * @name ProfilePropertyValue
 */
union ProfilePropertyValue
{
    bool        m_Bool;
    int32_t     m_S32;
    uint32_t    m_U32;
    float       m_F32;
    int64_t     m_S64;
    uint64_t    m_U64;
    double      m_F64;
};

/*# Set of bit flags to be used when declaring propertis
 * @enum
 * @name ProfilePropertyFlags
 * @member PROFILE_PROPERTY_NONE
 * @member PROFILE_PROPERTY_FRAME_RESET
 */
enum ProfilePropertyFlags
{
    PROFILE_PROPERTY_NONE = 0,
    PROFILE_PROPERTY_FRAME_RESET = 1  // reset the property each frame
};

/*# Index constant to mark a a property as invalid
 * @enum
 * @name PROFILE_PROPERTY_INVALID_IDX
 */
const uint32_t PROFILE_PROPERTY_INVALID_IDX = 0xFFFFFFFF;

#if defined(NDEBUG) || defined(DM_PROFILE_NULL)

    typedef uint64_t ProfileScope;

    #define DM_PROFILE_TEXT_LENGTH 1024
    #define DM_PROFILE_TEXT(format, ...)
    #define DM_PROFILE(name)
    #define DM_PROFILE_DYN(name, name_hash)

    #define DM_PROPERTY_GROUP(name, desc, ...)
    #define DM_PROPERTY_BOOL(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_S32(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_U32(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_F32(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_S64(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_U64(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_F64(name, default_value, flag, desc, ...)
    #define DM_PROPERTY_SET_BOOL(name, set_value)
    #define DM_PROPERTY_SET_S32(name, set_value)
    #define DM_PROPERTY_SET_U32(name, set_value)
    #define DM_PROPERTY_SET_F32(name, set_value)
    #define DM_PROPERTY_SET_S64(name, set_value)
    #define DM_PROPERTY_SET_U64(name, set_value)
    #define DM_PROPERTY_SET_F64(name, set_value)
    #define DM_PROPERTY_ADD_S32(name, add_value)
    #define DM_PROPERTY_ADD_U32(name, add_value)
    #define DM_PROPERTY_ADD_F32(name, add_value)
    #define DM_PROPERTY_ADD_S64(name, add_value)
    #define DM_PROPERTY_ADD_U64(name, add_value)
    #define DM_PROPERTY_ADD_F64(name, add_value)
    #define DM_PROPERTY_RESET(name)

    #define DM_PROPERTY_EXTERN(name)
#else

    // ****************************************************************************************
    // Private

    struct ProfileScope
    {
        void* m_Internal; // used by each profiler implementation to store scope relevant data
    };

    #define _DM_PROFILE_PASTE(x, y) x ## y
    #define _DM_PROFILE_PASTE2(x, y) _DM_PROFILE_PASTE(x, y)

    #define _DM_PROFILE_SCOPE(name, idx_ptr) \
        ProfileScope _DM_PROFILE_PASTE2(_scope_info, __LINE__) = {0}; \
        ProfileScopeHelper _DM_PROFILE_PASTE2(_scope_defer, __LINE__)(name, idx_ptr, & _DM_PROFILE_PASTE2(_scope_info, __LINE__));

    #define DM_PROPERTY_TYPE(stype, type) \
        ProfileIdx ProfileRegisterProperty##stype(const char* name, const char* desc, type value, uint32_t flags, ProfileIdx* (*parentfn)()); \
        ProfileIdx ProfileCreateProperty##stype(const char* name, const char* desc, type value, uint32_t flags, ProfileIdx parent); \
        void ProfilePropertySet##stype(ProfileIdx idx, type v); \
        void ProfilePropertyAdd##stype(ProfileIdx idx, type v);

    DM_PROPERTY_TYPE(Bool, int);
    DM_PROPERTY_TYPE(S32, int32_t);
    DM_PROPERTY_TYPE(U32, uint32_t);
    DM_PROPERTY_TYPE(F32, float);
    DM_PROPERTY_TYPE(S64, int64_t);
    DM_PROPERTY_TYPE(U64, uint64_t);
    DM_PROPERTY_TYPE(F64, double);
    #undef DM_PROPERTY_TYPE
    // End private
    // ****************************************************************************************


    #define DM_PROFILE(name) \
        static uint64_t _DM_PROFILE_PASTE2(_scope, __LINE__) = 0; \
        _DM_PROFILE_SCOPE(name, & _DM_PROFILE_PASTE2(_scope, __LINE__))

    #define DM_PROFILE_DYN(name, hash_cache) \
        _DM_PROFILE_SCOPE(name, hash_cache)

    #define DM_PROFILE_TEXT_LENGTH 1024
    #define DM_PROFILE_TEXT(format, ...)              ProfileLogText(format, __VA_ARGS__)

    ProfileIdx ProfileRegisterPropertyGroup(const char* name, const char* desc, ProfileIdx* (*parentfn)());
    ProfileIdx ProfileCreatePropertyGroup(const char* name, const char* desc, ProfileIdx parent);

    // The profiler register property api
    #define DM_PROPERTY_GROUP(name, desc, parentfn)                      ProfileIdx* name() { static ProfileIdx n = ProfileRegisterPropertyGroup(#name, desc, parentfn); return &n; } static ProfileIdx *name##_p = name()
    #define DM_PROPERTY_BOOL(name, default_value, flags, desc, parentfn) ProfileIdx* name() { static ProfileIdx n = ProfileRegisterPropertyBool(#name, desc, default_value, flags, parentfn); return &n; } static ProfileIdx *name##_p = name()
    #define DM_PROPERTY_S32(name, default_value, flags, desc, parentfn)  ProfileIdx* name() { static ProfileIdx n = ProfileRegisterPropertyS32(#name, desc, default_value, flags, parentfn); return &n; } static ProfileIdx *name##_p = name()
    #define DM_PROPERTY_U32(name, default_value, flags, desc, parentfn)  ProfileIdx* name() { static ProfileIdx n = ProfileRegisterPropertyU32(#name, desc, default_value, flags, parentfn); return &n; } static ProfileIdx *name##_p = name()
    #define DM_PROPERTY_F32(name, default_value, flags, desc, parentfn)  ProfileIdx* name() { static ProfileIdx n = ProfileRegisterPropertyF32(#name, desc, default_value, flags, parentfn); return &n; } static ProfileIdx *name##_p = name()
    #define DM_PROPERTY_S64(name, default_value, flags, desc, parentfn)  ProfileIdx* name() { static ProfileIdx n = ProfileRegisterPropertyS64(#name, desc, default_value, flags, parentfn); return &n; } static ProfileIdx *name##_p = name()
    #define DM_PROPERTY_U64(name, default_value, flags, desc, parentfn)  ProfileIdx* name() { static ProfileIdx n = ProfileRegisterPropertyU64(#name, desc, default_value, flags, parentfn); return &n; } static ProfileIdx *name##_p = name()
    #define DM_PROPERTY_F64(name, default_value, flags, desc, parentfn)  ProfileIdx* name() { static ProfileIdx n = ProfileRegisterPropertyF64(#name, desc, default_value, flags, parentfn); return &n; } static ProfileIdx *name##_p = name()

    // Set properties to the given value
    #define DM_PROPERTY_SET_BOOL(name, set_value)   ProfilePropertySetBool(*name##_p, set_value)
    #define DM_PROPERTY_SET_S32(name, set_value)    ProfilePropertySetS32(*name##_p, set_value)
    #define DM_PROPERTY_SET_U32(name, set_value)    ProfilePropertySetU32(*name##_p, set_value)
    #define DM_PROPERTY_SET_F32(name, set_value)    ProfilePropertySetF32(*name##_p, set_value)
    #define DM_PROPERTY_SET_S64(name, set_value)    ProfilePropertySetS64(*name##_p, set_value)
    #define DM_PROPERTY_SET_U64(name, set_value)    ProfilePropertySetU64(*name##_p, set_value)
    #define DM_PROPERTY_SET_F64(name, set_value)    ProfilePropertySetF64(*name##_p, set_value)

    // Add the given value to properties
    #define DM_PROPERTY_ADD_S32(name, add_value)    ProfilePropertyAddS32(*name##_p, add_value)
    #define DM_PROPERTY_ADD_U32(name, add_value)    ProfilePropertyAddU32(*name##_p, add_value)
    #define DM_PROPERTY_ADD_F32(name, add_value)    ProfilePropertyAddF32(*name##_p, add_value)
    #define DM_PROPERTY_ADD_S64(name, add_value)    ProfilePropertyAddS64(*name##_p, add_value)
    #define DM_PROPERTY_ADD_U64(name, add_value)    ProfilePropertyAddU64(*name##_p, add_value)
    #define DM_PROPERTY_ADD_F64(name, add_value)    ProfilePropertyAddF64(*name##_p, add_value)

    #define DM_PROPERTY_RESET(name)                 ProfilePropertyReset(*name##_p)

    #define DM_PROPERTY_EXTERN(name) \
            extern ProfileIdx* name(); \
            static ProfileIdx* name##_p = name()

#endif

typedef void* (*ProfileCreateListenerFn)();
typedef void (*ProfileDestroyListenerFn)(void* ctx);

typedef void (*ProfileFrameBeginFn)(void* ctx);
typedef void (*ProfileFrameEndFn)(void* ctx);
typedef void (*ProfileScopeBeginFn)(void* ctx, const char* name, uint64_t name_hash);
typedef void (*ProfileScopeEndFn)(void* ctx, const char* name, uint64_t name_hash);
typedef void (*ProfileSetThreadNameFn)(void* ctx, const char* name);
typedef void (*ProfileLogTextFn)(void* ctx, const char* text);

typedef void (*ProfilePropertyCreateGroupFn)(void* ctx, const char* name, const char* desc, ProfileIdx idx, ProfileIdx parent);
typedef void (*ProfilePropertyCreateBoolFn)(void* ctx, const char* name, const char* desc, int value, uint32_t flags, ProfileIdx idx, ProfileIdx parent);
typedef void (*ProfilePropertyCreateS32Fn)(void* ctx, const char* name, const char* desc, int32_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent);
typedef void (*ProfilePropertyCreateU32Fn)(void* ctx, const char* name, const char* desc, uint32_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent);
typedef void (*ProfilePropertyCreateF32Fn)(void* ctx, const char* name, const char* desc, float value, uint32_t flags, ProfileIdx idx, ProfileIdx parent);
typedef void (*ProfilePropertyCreateS64Fn)(void* ctx, const char* name, const char* desc, int64_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent);
typedef void (*ProfilePropertyCreateU64Fn)(void* ctx, const char* name, const char* desc, uint64_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent);
typedef void (*ProfilePropertyCreateF64Fn)(void* ctx, const char* name, const char* desc, double value, uint32_t flags, ProfileIdx idx, ProfileIdx parent);

typedef void (*ProfilePropertySetBoolFn)(void* ctx, ProfileIdx idx, int v);
typedef void (*ProfilePropertySetS32Fn)(void* ctx, ProfileIdx idx, int32_t v);
typedef void (*ProfilePropertySetU32Fn)(void* ctx, ProfileIdx idx, uint32_t v);
typedef void (*ProfilePropertySetF32Fn)(void* ctx, ProfileIdx idx, float v);
typedef void (*ProfilePropertySetS64Fn)(void* ctx, ProfileIdx idx, int64_t v);
typedef void (*ProfilePropertySetU64Fn)(void* ctx, ProfileIdx idx, uint64_t v);
typedef void (*ProfilePropertySetF64Fn)(void* ctx, ProfileIdx idx, double v);
typedef void (*ProfilePropertyAddS32Fn)(void* ctx, ProfileIdx idx, int32_t v);
typedef void (*ProfilePropertyAddU32Fn)(void* ctx, ProfileIdx idx, uint32_t v);
typedef void (*ProfilePropertyAddF32Fn)(void* ctx, ProfileIdx idx, float v);
typedef void (*ProfilePropertyAddS64Fn)(void* ctx, ProfileIdx idx, int64_t v);
typedef void (*ProfilePropertyAddU64Fn)(void* ctx, ProfileIdx idx, uint64_t v);
typedef void (*ProfilePropertyAddF64Fn)(void* ctx, ProfileIdx idx, double v);
typedef void (*ProfilePropertyResetFn)(void* ctx, ProfileIdx idx);

const uint32_t PROFILER_MAX_NUM_PROPERTIES = 256;

/*# Structure for registering a profile listener
 * @struct ProfileListener
 */
struct ProfileListener
{
    // private
    char                m_Name[16];
    ProfileListener*    m_Next;
    void*               m_Ctx;
    bool                m_Disabled; // if it failed to initialize

    // Public API
    ProfileCreateListenerFn         m_Create;
    ProfileDestroyListenerFn        m_Destroy;

    ProfileFrameBeginFn             m_FrameBegin;
    ProfileFrameEndFn               m_FrameEnd;
    ProfileScopeBeginFn             m_ScopeBegin;
    ProfileScopeEndFn               m_ScopeEnd;
    ProfileSetThreadNameFn          m_SetThreadName;
    ProfileLogTextFn                m_LogText;

    ProfilePropertyCreateGroupFn    m_CreatePropertyGroup;
    ProfilePropertyCreateBoolFn     m_CreatePropertyBool;
    ProfilePropertyCreateS32Fn      m_CreatePropertyS32;
    ProfilePropertyCreateU32Fn      m_CreatePropertyU32;
    ProfilePropertyCreateF32Fn      m_CreatePropertyF32;
    ProfilePropertyCreateS64Fn      m_CreatePropertyS64;
    ProfilePropertyCreateU64Fn      m_CreatePropertyU64;
    ProfilePropertyCreateF64Fn      m_CreatePropertyF64;

    ProfilePropertySetBoolFn        m_PropertySetBool;
    ProfilePropertySetS32Fn         m_PropertySetS32;
    ProfilePropertySetU32Fn         m_PropertySetU32;
    ProfilePropertySetF32Fn         m_PropertySetF32;
    ProfilePropertySetS64Fn         m_PropertySetS64;
    ProfilePropertySetU64Fn         m_PropertySetU64;
    ProfilePropertySetF64Fn         m_PropertySetF64;
    ProfilePropertyAddS32Fn         m_PropertyAddS32;
    ProfilePropertyAddU32Fn         m_PropertyAddU32;
    ProfilePropertyAddF32Fn         m_PropertyAddF32;
    ProfilePropertyAddS64Fn         m_PropertyAddS64;
    ProfilePropertyAddU64Fn         m_PropertyAddU64;
    ProfilePropertyAddF64Fn         m_PropertyAddF64;
    ProfilePropertyResetFn          m_PropertyReset;
};

/*# Register a new profiler.
 * Register a new profiler. Can be done after the profiling has started.
 * @name ProfileRegisterProfiler
 * @param name [type: const char*] Name of the profiler
 @ @param profiler [type: ProfileListener*] Instance of the profiler. Keep alive until calling ProfileUnregisterProfiler()!
 */
void ProfileRegisterProfiler(const char* name, ProfileListener* profiler);

/*# Unregister a profiler
 * @name ProfileUnregisterProfiler
 * @param name [type: const char*] Name of the profiler
 */
void ProfileUnregisterProfiler(const char* name);

/*# Initialize the profiling system
 * @name ProfileInitialize
 */
void ProfileInitialize();

/*# Finalize the profiling system
 * @name ProfileFinalize
 */
void ProfileFinalize();

/*# Finalize the profiling system
 * @name ProfileIsInitialized
 * @return initialized [type: bool] Returns non zero if the profiler is initialized
 */
bool ProfileIsInitialized();

/*#
 * Set the current thread name to each registered profiler
 * @name ProfileSetThreadName
 * @param name [type:const char*] Name of the thread
 */
void ProfileSetThreadName(const char* name);

/*#
 * Begin profiling, eg start of frame
 * @note NULL is returned if profiling is disabled
 * @name ProfileFrameBegin
 * @return context [type: HProfile] The current profiling context. Must be released by #EndFrame
 */
HProfile ProfileFrameBegin();

/*#
 * Release profile returned by #ProfileFrameBegin
 * @name ProfileFrameEnd
 * @param profile [type: HProfile] Profile to release
 */
void ProfileFrameEnd(HProfile profile);

/*#
 * Start a new profile scope
 * @name ProfileScopeBegin
 * @param name [type:const char*] Name of the scope
 * @param name_hash [type:uint64_t] Hashed name of the scope
 */
void ProfileScopeBegin(const char* name, uint64_t* name_hash);

/*#
 * End the last added scope
 * @name ProfileScopeEnd
 * @param name [type:const char*] Name of the scope
 * @param name_hash [type:uint64_t] Hashed name of the scope
 */
void ProfileScopeEnd(const char* name, uint64_t name_hash);

/*#
 * Log text via the registered profilers
 * @name ProfileLogText
 * @param name [type:const char*] Name of the scope
 * @param ... Arguments for internal logging function
 */
#ifdef __GNUC__
    void ProfileLogText(const char* text, ...) __attribute__ ((format (printf, 1, 2)));
#else
    void ProfileLogText(const char* text, ...);
#endif

/// Internal, do not use. Use DM_PROFILE() instead
struct ProfileScopeHelper
{
    ProfileScope*   m_ScopeInfo;
    const char*     m_Name;
    uint64_t*       m_NameHash;

    inline ProfileScopeHelper(const char* name, uint64_t* name_hash, ProfileScope* info)
        : m_ScopeInfo(info), m_Name(name), m_NameHash(name_hash)
    {
        if (m_Name)
        {
            ProfileScopeBegin(m_Name, m_NameHash);
        }
    }

    inline ~ProfileScopeHelper()
    {
        if (m_Name)
        {
            ProfileScopeEnd(m_Name, m_NameHash ? *m_NameHash : 0);
        }
    }
};

#endif

// ************************************************************************************
// Documentation


/*# add profile scope
 *
 * Adds a profiling scope. Excluded by default in release builds.
 *
 * @macro
 * @name DM_PROFILE
 * @param a [type:const char*] A name for the scope
 *
 * @examples
 *
 * Profile a scope
 *
 * ```cpp
 * {
 *     DM_PROFILE("DoWork");
 *     DoWork1();
 *     DoWork2();
 * }
 * ```
 */

/*# add dynamic profile scope
 *
 * Adds a profiling scope. Excluded by default in release builds.
 * Accepts a name cache value for performance.
 *
 * @macro
 * @name DM_PROFILE_DYN
 * @param a [type:const char*] The scope name
 * @param a [type:uint64_t*] The scope name hash value pointer. May be 0.
 *
 * @examples
 *
 * Create a dynamic profiling scope
 *
 * ```cpp
 * {
 *     DM_PROFILE_DYN(work->m_Name, &work->m_NameHash);
 *     work->DoWork();
 * }
 * ```
 */

/*# send text to the profiler
 *
 * Send text to the profiler
 *
 * @note The max length of the text is DM_PROFILE_TEXT_LENGTH (1024)
 *
 * @macro
 * @name DM_PROFILE_TEXT
 * @param a [type:const char*] The format string
 * @param a [type:va_list] The variable argument list
 *
 * @examples
 *
 * Send a string to the profiler
 *
 * ```cpp
 * DM_PROFILE_TEXT("Some value: %d", value);
 * ```
 */

/*#
 * Declare an extern property
 *
 * @macro
 * @name DM_PROPERTY_EXTERN
 * @param name [type:symbol] The symbol name
 *
 * @examples
 *
 * Use a property declared elsewhere in the same library
 *
 * ```cpp
 * DM_PROPERTY_EXTERN(rmtp_GameObject);
 * DM_PROPERTY_U32(rmtp_ComponentsAnim, 0, PROFILE_PROPERTY_FRAME_RESET, "#", &rmtp_GameObject);
 * ```
 */

/*#
 * Declare a property group
 *
 * @macro
 * @name DM_PROPERTY_GROUP
 * @param name [type:symbol] The group name
 * @param desc [type:const char*] The description
 * @param parent [type: ProfileIdx*] pointer to parent property
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_GROUP(rmtp_GameObject, "My Group", 0);
 * ```
 */

/*# bool property
 * Declare a property of type `bool`
 *
 * @macro
 * @name DM_PROPERTY_BOOL
 * @param name [type:symbol] The property symbol/name
 * @param default [type:bool] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: ProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_BOOL(rmtp_MyBool, 0, PROFILE_PROPERTY_FRAME_RESET, "true or false", &rmtp_MyGroup);
 * ```
 */

/*# int32_t property
 * Declare a property of type `int32_t`
 *
 * @macro
 * @name DM_PROPERTY_S32
 * @param name [type:symbol] The property symbol/name
 * @param default [type:int32_t] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: ProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_S32(rmtp_MyValue, 0, PROFILE_PROPERTY_FRAME_RESET, "a value", &rmtp_MyGroup);
 * ```
 */

/*# uint32_t property
 * Declare a property of type `uint32_t`
 *
 * @macro
 * @name DM_PROPERTY_U32
 * @param name [type:symbol] The property symbol/name
 * @param default [type:uint32_t] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: ProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_U32(rmtp_MyValue, 0, PROFILE_PROPERTY_FRAME_RESET, "a value", &rmtp_MyGroup);
 * ```
 */

/*# float property
 * Declare a property of type `float`
 *
 * @macro
 * @name DM_PROPERTY_F32
 * @param name [type:symbol] The property symbol/name
 * @param default [type:float] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: ProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_F32(rmtp_MyValue, 0, PROFILE_PROPERTY_FRAME_RESET, "a value", &rmtp_MyGroup);
 * ```
 */

/*# int64_t property
 * Declare a property of type `int64_t`
 *
 * @macro
 * @name DM_PROPERTY_S64
 * @param name [type:symbol] The property symbol/name
 * @param default [type:int64_t] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: ProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_S64(rmtp_MyValue, 0, PROFILE_PROPERTY_FRAME_RESET, "a value", &rmtp_MyGroup);
 * ```
 */

/*# uint64_t property
 * Declare a property of type `uint64_t`
 *
 * @macro
 * @name DM_PROPERTY_U64
 * @param name [type:symbol] The property symbol/name
 * @param default [type:uint64_t] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: ProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_U64(rmtp_MyValue, 0, PROFILE_PROPERTY_FRAME_RESET, "a value", &rmtp_MyGroup);
 * ```
 */

/*# double property
 * Declare a property of type `double`
 *
 * @macro
 * @name DM_PROPERTY_F64
 * @param name [type:symbol] The property symbol/name
 * @param default [type:double] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: ProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_F64(rmtp_MyValue, 0, PROFILE_PROPERTY_FRAME_RESET, "a value", &rmtp_MyGroup);
 * ```
 */

/*# set bool property
 * Set the value of a bool property
 *
 * @macro
 * @name DM_PROPERTY_SET_BOOL
 * @param name [type:symbol] The property
 * @param value [type:bool] The value
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_SET_BOOL(rmtp_MyBool, false);
 * ```
 */

/*# set int32_t property
 * Set the value of a int32_t property
 *
 * @macro
 * @name DM_PROPERTY_SET_S32
 * @param name [type:symbol] The property
 * @param value [type:int32_t] The value
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_SET_S32(rmtp_MyValue, -1);
 * ```
 */

/*# set uint32_t property
 * Set the value of a uint32_t property
 *
 * @macro
 * @name DM_PROPERTY_SET_U32
 * @param name [type:symbol] The property
 * @param value [type:uint32_t] The value
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_SET_U32(rmtp_MyValue, 1);
 * ```
 */

/*# set float property
 * Set the value of a float property
 *
 * @macro
 * @name DM_PROPERTY_SET_F32
 * @param name [type:symbol] The property
 * @param value [type:float] The value
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_SET_F32(rmtp_MyValue, 1.5);
 * ```
 */

/*# set int64_t property
 * Set the value of a int64_t property
 *
 * @macro
 * @name DM_PROPERTY_SET_S64
 * @param name [type:symbol] The property
 * @param value [type:int64_t] The value
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_SET_S64(rmtp_MyValue, -1);
 * ```
 */

/*# set uint64_t property
 * Set the value of a uint64_t property
 *
 * @macro
 * @name DM_PROPERTY_SET_U64
 * @param name [type:symbol] The property
 * @param value [type:uint64_t] The value
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_SET_U64(rmtp_MyValue, 1);
 * ```
 */

/*# set double property
 * Set the value of a double property
 *
 * @macro
 * @name DM_PROPERTY_SET_F64
 * @param name [type:symbol] The property
 * @param value [type:double] The value
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_SET_F64(rmtp_MyValue, 1.5);
 * ```
 */

/*# add to int32_t property
 * Add a value to int32_t property
 *
 * @macro
 * @name DM_PROPERTY_ADD_S32
 * @param name [type:symbol] The property
 * @param value [type:int32_t] The value
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_ADD_S32(rmtp_MyValue, -1);
 * ```
 */

/*# add to uint32_t property
 * Add a value to uint32_t property
 *
 * @macro
 * @name DM_PROPERTY_ADD_U32
 * @param name [type:symbol] The property
 * @param value [type:uint32_t] The value
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_ADD_U32(rmtp_MyValue, 1);
 * ```
 */

/*# add to float property
 * Add a value to float property
 *
 * @macro
 * @name DM_PROPERTY_ADD_F32
 * @param name [type:symbol] The property
 * @param value [type:float] The value
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_ADD_F32(rmtp_MyValue, 1.5);
 * ```
 */

/*# add to int64_t property
 * Add a value to int64_t property
 *
 * @macro
 * @name DM_PROPERTY_ADD_S64
 * @param name [type:symbol] The property
 * @param value [type:int64_t] The value
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_ADD_S64(rmtp_MyValue, -1);
 * ```
 */

/*# add to uint64_t property
 * Add a value to uint64_t property
 *
 * @macro
 * @name DM_PROPERTY_ADD_U64
 * @param name [type:symbol] The property
 * @param value [type:uint64_t] The value
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_ADD_U64(rmtp_MyValue, 1);
 * ```
 */

/*# add to double property
 * Add a value to double property
 *
 * @macro
 * @name DM_PROPERTY_ADD_F64
 * @param name [type:symbol] The property
 * @param value [type:double] The value
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_ADD_F64(rmtp_MyValue, 1.5);
 * ```
 */

/*# reset property
 * Reset a property to its default value
 *
 * @macro
 * @name DM_PROPERTY_RESET
 * @param name [type:symbol] The property
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_RESET(rmtp_MyValue);
 * ```
 */
