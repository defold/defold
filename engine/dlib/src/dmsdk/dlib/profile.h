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

#ifndef DMSDK_PROFILE_H
#define DMSDK_PROFILE_H

/*# Profiling API documentation
 * Profiling macros
 *
 * @document
 * @name Profile
 * @namespace dmProfile
 * @path engine/dlib/src/dmsdk/dlib/profile.h
 * @language C++
 */

#include <stdint.h>

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
#define DM_PROFILE(name)
#undef DM_PROFILE

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
#define DM_PROFILE_DYN(name, name_hash)
#undef DM_PROFILE_DYN

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
#define DM_PROFILE_TEXT(format, ...)
#undef DM_PROFILE_TEXT

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
#define DM_PROPERTY_EXTERN(name)
#undef DM_PROPERTY_EXTERN

/*#
 * Declare a property group
 *
 * @macro
 * @name DM_PROPERTY_GROUP
 * @param name [type:symbol] The group name
 * @param desc [type:const char*] The description
 * @param parent [type: dmProfileIdx*] pointer to parent property
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_GROUP(rmtp_GameObject, "My Group", 0);
 * ```
 */
#define DM_PROPERTY_GROUP(name, desc)
#undef DM_PROPERTY_GROUP

/*# bool property
 * Declare a property of type `bool`
 *
 * @macro
 * @name DM_PROPERTY_BOOL
 * @param name [type:symbol] The property symbol/name
 * @param default [type:bool] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: dmProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_BOOL(rmtp_MyBool, 0, PROFILE_PROPERTY_FRAME_RESET, "true or false", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_BOOL(name, default_value, flag, desc, parent)
#undef DM_PROPERTY_BOOL

/*# int32_t property
 * Declare a property of type `int32_t`
 *
 * @macro
 * @name DM_PROPERTY_S32
 * @param name [type:symbol] The property symbol/name
 * @param default [type:int32_t] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: dmProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_S32(rmtp_MyValue, 0, PROFILE_PROPERTY_FRAME_RESET, "a value", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_S32(name, default_value, flag, desc, parent)
#undef DM_PROPERTY_S32

/*# uint32_t property
 * Declare a property of type `uint32_t`
 *
 * @macro
 * @name DM_PROPERTY_U32
 * @param name [type:symbol] The property symbol/name
 * @param default [type:uint32_t] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: dmProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_U32(rmtp_MyValue, 0, PROFILE_PROPERTY_FRAME_RESET, "a value", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_U32(name, default_value, flag, desc, parent)
#undef DM_PROPERTY_U32

/*# float property
 * Declare a property of type `float`
 *
 * @macro
 * @name DM_PROPERTY_F32
 * @param name [type:symbol] The property symbol/name
 * @param default [type:float] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: dmProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_F32(rmtp_MyValue, 0, PROFILE_PROPERTY_FRAME_RESET, "a value", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_F32(name, default_value, flag, desc, parent)
#undef DM_PROPERTY_F32

/*# int64_t property
 * Declare a property of type `int64_t`
 *
 * @macro
 * @name DM_PROPERTY_S64
 * @param name [type:symbol] The property symbol/name
 * @param default [type:int64_t] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: dmProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_S64(rmtp_MyValue, 0, PROFILE_PROPERTY_FRAME_RESET, "a value", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_S64(name, default_value, flag, desc, parent)
#undef DM_PROPERTY_S64


/*# uint64_t property
 * Declare a property of type `uint64_t`
 *
 * @macro
 * @name DM_PROPERTY_U64
 * @param name [type:symbol] The property symbol/name
 * @param default [type:uint64_t] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: dmProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_U64(rmtp_MyValue, 0, PROFILE_PROPERTY_FRAME_RESET, "a value", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_U64(name, default_value, flag, desc, parent)
#undef DM_PROPERTY_U64

/*# double property
 * Declare a property of type `double`
 *
 * @macro
 * @name DM_PROPERTY_F64
 * @param name [type:symbol] The property symbol/name
 * @param default [type:double] The default value
 * @param flags [type:uint32_t] The flags. Either `PROFILE_PROPERTY_NONE` or `PROFILE_PROPERTY_FRAME_RESET`. `PROFILE_PROPERTY_FRAME_RESET` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type: dmProfileIdx*] The parent group. May be 0.
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_F64(rmtp_MyValue, 0, PROFILE_PROPERTY_FRAME_RESET, "a value", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_F64(name, default_value, flag, desc, parent)
#undef DM_PROPERTY_F64


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
#define DM_PROPERTY_SET_BOOL(name, value)
#undef DM_PROPERTY_SET_BOOL

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
#define DM_PROPERTY_SET_S32(name, value)
#undef DM_PROPERTY_SET_S32

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
#define DM_PROPERTY_SET_U32(name, value)
#undef DM_PROPERTY_SET_U32

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
#define DM_PROPERTY_SET_F32(name, value)
#undef DM_PROPERTY_SET_F32

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
#define DM_PROPERTY_SET_S64(name, value)
#undef DM_PROPERTY_SET_S64

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
#define DM_PROPERTY_SET_U64(name, value)
#undef DM_PROPERTY_SET_U64

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
#define DM_PROPERTY_SET_F64(name, value)
#undef DM_PROPERTY_SET_F64

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
#define DM_PROPERTY_ADD_S32(name, value)
#undef DM_PROPERTY_ADD_S32

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
#define DM_PROPERTY_ADD_U32(name, value)
#undef DM_PROPERTY_ADD_U32

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
#define DM_PROPERTY_ADD_F32(name, value)
#undef DM_PROPERTY_ADD_F32

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
#define DM_PROPERTY_ADD_S64(name, value)
#undef DM_PROPERTY_ADD_S64

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
#define DM_PROPERTY_ADD_U64(name, value)
#undef DM_PROPERTY_ADD_U64

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
#define DM_PROPERTY_ADD_F64(name, value)
#undef DM_PROPERTY_ADD_F64

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
#define DM_PROPERTY_RESET(name)
#undef DM_PROPERTY_RESET

typedef uint64_t dmProfileIdx;

// Private
#define DM_PROFILE_PROPERTY_INVALID_IDX 0xFFFFFFFF

dmProfileIdx dmProfileCreatePropertyGroup(const char* name, const char* desc, dmProfileIdx* (*parent)());

#define DM_PROPERTY_TYPE(stype, type) \
    dmProfileIdx dmProfileCreateProperty##stype(const char* name, const char* desc, type value, uint32_t flags, dmProfileIdx* (*parent)()); \
    void dmProfilePropertySet##stype(dmProfileIdx idx, type v); \
    static inline void dmProfilePropertySet##stype(dmProfileIdx* (*idx)(), type v) { dmProfilePropertySet##stype(*idx(), v); } \
    void dmProfilePropertyAdd##stype(dmProfileIdx idx, type v); \
    static inline void dmProfilePropertyAdd##stype(dmProfileIdx* (*idx)(), type v) { dmProfilePropertyAdd##stype(*idx(), v); }
DM_PROPERTY_TYPE(Bool, int);
DM_PROPERTY_TYPE(S32, int32_t);
DM_PROPERTY_TYPE(U32, uint32_t);
DM_PROPERTY_TYPE(F32, float);
DM_PROPERTY_TYPE(S64, int64_t);
DM_PROPERTY_TYPE(U64, uint64_t);
DM_PROPERTY_TYPE(F64, double);
#undef DM_PROPERTY_TYPE

void dmProfilePropertyReset(dmProfileIdx idx);

// Public
#define DM_PROPERTY_EXTERN(name) extern dmProfileIdx *name()

enum ProfilePropertyFlags
{
    PROFILE_PROPERTY_NONE = 0,
    PROFILE_PROPERTY_FRAME_RESET = 1  // reset the property each frame
};

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

#else

    struct ProfileScope
    {
        void* m_Internal; // used by each profiler implementation to store scope relevant data
        uint32_t m_Generation;
    };

    #define _DM_PROFILE_PASTE(x, y) x ## y
    #define _DM_PROFILE_PASTE2(x, y) _DM_PROFILE_PASTE(x, y)

    #define _DM_PROFILE_SCOPE(name, idx_ptr) \
        ProfileScope _DM_PROFILE_PASTE2(_scope_info, __LINE__) = {0}; \
        dmProfile::ProfileScopeHelper _DM_PROFILE_PASTE2(_scope_defer, __LINE__)(name, idx_ptr, & _DM_PROFILE_PASTE2(_scope_info, __LINE__));

    #define DM_PROFILE(name) \
        static uint64_t _DM_PROFILE_PASTE2(_scope, __LINE__) = 0; \
        _DM_PROFILE_SCOPE(name, & _DM_PROFILE_PASTE2(_scope, __LINE__))

    #define DM_PROFILE_DYN(name, hash_cache) \
        _DM_PROFILE_SCOPE(name, hash_cache)

    #define DM_PROFILE_TEXT_LENGTH 1024
    #define DM_PROFILE_TEXT(format, ...)              dmProfile::LogText(format, __VA_ARGS__)

    // The profiler property api
    #define DM_PROPERTY_GROUP(name, desc, parent)                      dmProfileIdx *name() { static uint8_t gen = 0; static dmProfileIdx n = DM_PROFILE_PROPERTY_INVALID_IDX; if (gen != dmProfile::g_ProfilerGeneration) { n = dmProfileCreatePropertyGroup(#name, desc, parent); gen = dmProfile::g_ProfilerGeneration; } return &n; } static dmProfileIdx *name##_p = name()
    #define DM_PROPERTY_BOOL(name, default_value, flags, desc, parent) dmProfileIdx *name() { static uint8_t gen = 0; static dmProfileIdx n = DM_PROFILE_PROPERTY_INVALID_IDX; if (gen != dmProfile::g_ProfilerGeneration) { n = dmProfileCreatePropertyBool(#name, desc, default_value, flags, parent); gen = dmProfile::g_ProfilerGeneration; } return &n; } static dmProfileIdx *name##_p = name()
    #define DM_PROPERTY_S32(name, default_value, flags, desc, parent)  dmProfileIdx *name() { static uint8_t gen = 0; static dmProfileIdx n = DM_PROFILE_PROPERTY_INVALID_IDX; if (gen != dmProfile::g_ProfilerGeneration) { n = dmProfileCreatePropertyS32(#name, desc, default_value, flags, parent); gen = dmProfile::g_ProfilerGeneration; } return &n; } static dmProfileIdx *name##_p = name()
    #define DM_PROPERTY_U32(name, default_value, flags, desc, parent)  dmProfileIdx *name() { static uint8_t gen = 0; static dmProfileIdx n = DM_PROFILE_PROPERTY_INVALID_IDX; if (gen != dmProfile::g_ProfilerGeneration) { n = dmProfileCreatePropertyU32(#name, desc, default_value, flags, parent); gen = dmProfile::g_ProfilerGeneration; } return &n; } static dmProfileIdx *name##_p = name()
    #define DM_PROPERTY_F32(name, default_value, flags, desc, parent)  dmProfileIdx *name() { static uint8_t gen = 0; static dmProfileIdx n = DM_PROFILE_PROPERTY_INVALID_IDX; if (gen != dmProfile::g_ProfilerGeneration) { n = dmProfileCreatePropertyF32(#name, desc, default_value, flags, parent); gen = dmProfile::g_ProfilerGeneration; } return &n; } static dmProfileIdx *name##_p = name()
    #define DM_PROPERTY_S64(name, default_value, flags, desc, parent)  dmProfileIdx *name() { static uint8_t gen = 0; static dmProfileIdx n = DM_PROFILE_PROPERTY_INVALID_IDX; if (gen != dmProfile::g_ProfilerGeneration) { n = dmProfileCreatePropertyS64(#name, desc, default_value, flags, parent); gen = dmProfile::g_ProfilerGeneration; } return &n; } static dmProfileIdx *name##_p = name()
    #define DM_PROPERTY_U64(name, default_value, flags, desc, parent)  dmProfileIdx *name() { static uint8_t gen = 0; static dmProfileIdx n = DM_PROFILE_PROPERTY_INVALID_IDX; if (gen != dmProfile::g_ProfilerGeneration) { n = dmProfileCreatePropertyU64(#name, desc, default_value, flags, parent); gen = dmProfile::g_ProfilerGeneration; } return &n; } static dmProfileIdx *name##_p = name()
    #define DM_PROPERTY_F64(name, default_value, flags, desc, parent)  dmProfileIdx *name() { static uint8_t gen = 0; static dmProfileIdx n = DM_PROFILE_PROPERTY_INVALID_IDX; if (gen != dmProfile::g_ProfilerGeneration) { n = dmProfileCreatePropertyF64(#name, desc, default_value, flags, parent); gen = dmProfile::g_ProfilerGeneration; } return &n; } static dmProfileIdx *name##_p = name()

    // Set properties to the given value
    #define DM_PROPERTY_SET_BOOL(name, set_value)   dmProfilePropertySetBool(name, set_value)
    #define DM_PROPERTY_SET_S32(name, set_value)    dmProfilePropertySetS32(name, set_value)
    #define DM_PROPERTY_SET_U32(name, set_value)    dmProfilePropertySetU32(name, set_value)
    #define DM_PROPERTY_SET_F32(name, set_value)    dmProfilePropertySetF32(name, set_value)
    #define DM_PROPERTY_SET_S64(name, set_value)    dmProfilePropertySetS64(name, set_value)
    #define DM_PROPERTY_SET_U64(name, set_value)    dmProfilePropertySetU64(name, set_value)
    #define DM_PROPERTY_SET_F64(name, set_value)    dmProfilePropertySetF64(name, set_value)

    // Add the given value to properties
    #define DM_PROPERTY_ADD_S32(name, add_value)    dmProfilePropertyAddS32(name, add_value)
    #define DM_PROPERTY_ADD_U32(name, add_value)    dmProfilePropertyAddU32(name, add_value)
    #define DM_PROPERTY_ADD_F32(name, add_value)    dmProfilePropertyAddF32(name, add_value)
    #define DM_PROPERTY_ADD_S64(name, add_value)    dmProfilePropertyAddS64(name, add_value)
    #define DM_PROPERTY_ADD_U64(name, add_value)    dmProfilePropertyAddU64(name, add_value)
    #define DM_PROPERTY_ADD_F64(name, add_value)    dmProfilePropertyAddF64(name, add_value)

    #define DM_PROPERTY_RESET(name)                 dmProfilePropertyReset(name)

#endif



namespace dmProfile
{
    extern uint8_t g_ProfilerGeneration;

    /*# Profile snapshot handle
     * @typedef
     * @name HProfile
     */
    typedef void* HProfile;

    /*#
     * Begin profiling, eg start of frame
     * @note NULL is returned if profiling is disabled
     * @name BeginFrame
     * @return context [type:dmProfile::HProfile] The current profiling context. Must be released by #EndFrame
     */
    HProfile BeginFrame();

    /*#
     * Release profile returned by #Begin
     * @name EndFrame
     * @param profile [type:dmProfile::HProfile] Profile to release
     */
    void EndFrame(HProfile profile);

    // /// Internal, do not use.
    struct ProfileScopeHelper
    {
        ProfileScope* m_ScopeInfo;

        inline ProfileScopeHelper(const char* name, uint64_t* name_hash, ProfileScope* info)
            : m_ScopeInfo(info), m_Valid(0)
        {
            if (name)
            {
#if !defined(NDEBUG) && !defined(DM_PROFILE_NULL)
                if (m_ScopeInfo->m_Generation != g_ProfilerGeneration)
                {
                    if (name_hash)
                        *name_hash = 0;
                    m_ScopeInfo->m_Generation = g_ProfilerGeneration;
                }
#endif
                m_Valid = 1;
                StartScope(name, name_hash);
            }
        }

        inline ~ProfileScopeHelper()
        {
            if (m_Valid)
                EndScope();
        }

        void StartScope(const char* name, uint64_t* name_hash);
        void EndScope();
        int m_Valid;
    };

    void LogText(const char* text, ...);


} // namespace dmProfile

#endif
