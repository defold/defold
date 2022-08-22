// Copyright 2020-2022 The Defold Foundation
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
 */

#include <stdint.h>

#include <dmsdk/external/remotery/Remotery.h> // Private, don't use this api directly!

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
 * @param a [type:dmhash_t*] The scope name hash value pointer. May be 0.
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
 * DM_PROPERTY_U32(rmtp_ComponentsAnim, 0, FrameReset, "#", &rmtp_GameObject);
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
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_GROUP(rmtp_GameObject, "My Group");
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
 * @param flags [type:uint32_t] The flags. Either `NoFlags` or `FrameReset`. `FrameReset` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type:property*] [optional] The parent group
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_BOOL(rmtp_MyBool, 0, FrameReset, "true or false", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_BOOL(name, default_value, flag, desc, ...)
#undef DM_PROPERTY_BOOL

/*# int32_t property
 * Declare a property of type `int32_t`
 *
 * @macro
 * @name DM_PROPERTY_S32
 * @param name [type:symbol] The property symbol/name
 * @param default [type:int32_t] The default value
 * @param flags [type:uint32_t] The flags. Either `NoFlags` or `FrameReset`. `FrameReset` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type:property*] [optional] The parent group
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_S32(rmtp_MyValue, 0, FrameReset, "a value", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_S32(name, default_value, flag, desc, ...)
#undef DM_PROPERTY_S32

/*# uint32_t property
 * Declare a property of type `uint32_t`
 *
 * @macro
 * @name DM_PROPERTY_U32
 * @param name [type:symbol] The property symbol/name
 * @param default [type:uint32_t] The default value
 * @param flags [type:uint32_t] The flags. Either `NoFlags` or `FrameReset`. `FrameReset` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type:property*] [optional] The parent group
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_U32(rmtp_MyValue, 0, FrameReset, "a value", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_U32(name, default_value, flag, desc, ...)
#undef DM_PROPERTY_U32

/*# float property
 * Declare a property of type `float`
 *
 * @macro
 * @name DM_PROPERTY_F32
 * @param name [type:symbol] The property symbol/name
 * @param default [type:float] The default value
 * @param flags [type:uint32_t] The flags. Either `NoFlags` or `FrameReset`. `FrameReset` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type:property*] [optional] The parent group
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_F32(rmtp_MyValue, 0, FrameReset, "a value", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_F32(name, default_value, flag, desc, ...)
#undef DM_PROPERTY_F32

/*# int64_t property
 * Declare a property of type `int64_t`
 *
 * @macro
 * @name DM_PROPERTY_S64
 * @param name [type:symbol] The property symbol/name
 * @param default [type:int64_t] The default value
 * @param flags [type:uint32_t] The flags. Either `NoFlags` or `FrameReset`. `FrameReset` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type:property*] [optional] The parent group
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_S64(rmtp_MyValue, 0, FrameReset, "a value", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_S64(name, default_value, flag, desc, ...)
#undef DM_PROPERTY_S64


/*# uint64_t property
 * Declare a property of type `uint64_t`
 *
 * @macro
 * @name DM_PROPERTY_U64
 * @param name [type:symbol] The property symbol/name
 * @param default [type:uint64_t] The default value
 * @param flags [type:uint32_t] The flags. Either `NoFlags` or `FrameReset`. `FrameReset` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type:property*] [optional] The parent group
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_U64(rmtp_MyValue, 0, FrameReset, "a value", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_U64(name, default_value, flag, desc, ...)
#undef DM_PROPERTY_U64

/*# double property
 * Declare a property of type `double`
 *
 * @macro
 * @name DM_PROPERTY_F64
 * @param name [type:symbol] The property symbol/name
 * @param default [type:double] The default value
 * @param flags [type:uint32_t] The flags. Either `NoFlags` or `FrameReset`. `FrameReset` makes the value reset each frame.
 * @param desc [type:const char*] The description
 * @param group [type:property*] [optional] The parent group
 *
 * @examples
 *
 * ```cpp
 * DM_PROPERTY_F64(rmtp_MyValue, 0, FrameReset, "a value", &rmtp_MyGroup);
 * ```
 */
#define DM_PROPERTY_F64(name, default_value, flag, desc, ...)
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


#if defined(NDEBUG) || defined(DM_PROFILE_NULL)
    #define DM_PROFILE_TEXT_LENGTH 1024
    #define DM_PROFILE_TEXT(format, ...)
    #define DM_PROFILE(name)
    #define DM_PROFILE_DYN(name, name_hash)

    #define DM_PROPERTY_EXTERN(name)                extern rmtProperty name;
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
    #define _DM_PROFILE_PASTE(x, y) x ## y
    #define _DM_PROFILE_PASTE2(x, y) _DM_PROFILE_PASTE(x, y)

    #define _DM_PROFILE_SCOPE(name, name_hash_ptr) \
        dmProfile::ProfileScope _DM_PROFILE_PASTE2(profile_scope, __LINE__)(name, name_hash_ptr);

    #define DM_PROFILE(name) \
        static uint64_t _DM_PROFILE_PASTE2(hash, __LINE__)        = 0; \
        _DM_PROFILE_SCOPE(name, & _DM_PROFILE_PASTE2(hash, __LINE__))

    #define DM_PROFILE_DYN(name, hash_cache) \
        _DM_PROFILE_SCOPE(name, hash_cache)

    #define DM_PROFILE_TEXT_LENGTH 1024
    #define DM_PROFILE_TEXT(format, ...)              dmProfile::LogText(format, __VA_ARGS__)

    // The profiler property api
    #define DM_PROPERTY_EXTERN(name)                                rmt_PropertyExtern(name)
    #define DM_PROPERTY_GROUP(name, desc, ...)                      rmt_PropertyDefine_Group(name, desc, __VA_ARGS__)
    #define DM_PROPERTY_BOOL(name, default_value, flag, desc, ...)  rmt_PropertyDefine_Bool(name, default_value, flag, desc, __VA_ARGS__)
    #define DM_PROPERTY_S32(name, default_value, flag, desc, ...)   rmt_PropertyDefine_S32(name, default_value, flag, desc, __VA_ARGS__)
    #define DM_PROPERTY_U32(name, default_value, flag, desc, ...)   rmt_PropertyDefine_U32(name, default_value, flag, desc, __VA_ARGS__)
    #define DM_PROPERTY_F32(name, default_value, flag, desc, ...)   rmt_PropertyDefine_F32(name, default_value, flag, desc, __VA_ARGS__)
    #define DM_PROPERTY_S64(name, default_value, flag, desc, ...)   rmt_PropertyDefine_S64(name, default_value, flag, desc, __VA_ARGS__)
    #define DM_PROPERTY_U64(name, default_value, flag, desc, ...)   rmt_PropertyDefine_U64(name, default_value, flag, desc, __VA_ARGS__)
    #define DM_PROPERTY_F64(name, default_value, flag, desc, ...)   rmt_PropertyDefine_F64(name, default_value, flag, desc, __VA_ARGS__)

    // Set properties to the given value
    #define DM_PROPERTY_SET_BOOL(name, set_value)   rmt_PropertySet_Bool(name, set_value)
    #define DM_PROPERTY_SET_S32(name, set_value)    rmt_PropertySet_S32(name, set_value)
    #define DM_PROPERTY_SET_U32(name, set_value)    rmt_PropertySet_U32(name, set_value)
    #define DM_PROPERTY_SET_F32(name, set_value)    rmt_PropertySet_F32(name, set_value)
    #define DM_PROPERTY_SET_S64(name, set_value)    rmt_PropertySet_S64(name, set_value)
    #define DM_PROPERTY_SET_U64(name, set_value)    rmt_PropertySet_U64(name, set_value)
    #define DM_PROPERTY_SET_F64(name, set_value)    rmt_PropertySet_F64(name, set_value)

    // Add the given value to properties
    #define DM_PROPERTY_ADD_S32(name, add_value)    rmt_PropertyAdd_S32(name, add_value)
    #define DM_PROPERTY_ADD_U32(name, add_value)    rmt_PropertyAdd_U32(name, add_value)
    #define DM_PROPERTY_ADD_F32(name, add_value)    rmt_PropertyAdd_F32(name, add_value)
    #define DM_PROPERTY_ADD_S64(name, add_value)    rmt_PropertyAdd_S64(name, add_value)
    #define DM_PROPERTY_ADD_U64(name, add_value)    rmt_PropertyAdd_U64(name, add_value)
    #define DM_PROPERTY_ADD_F64(name, add_value)    rmt_PropertyAdd_F64(name, add_value)

    #define DM_PROPERTY_RESET(name)                 rmt_PropertyReset(name)

#endif

namespace dmProfile
{
    /// Internal, do not use.
    struct ProfileScope
    {
        inline ProfileScope(const char* name, uint64_t* name_hash) : valid(0)
        {
            StartScope(name, name_hash);
        }

        inline ~ProfileScope()
        {
            EndScope();
        }

        void StartScope(const char* name, uint64_t* name_hash);
        void EndScope();
        int valid;
    };

    void LogText(const char* text, ...);


} // namespace dmProfile

#endif
