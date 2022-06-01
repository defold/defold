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

#define DM_PROFILE_PASTE(x, y) x ## y
#define DM_PROFILE_PASTE2(x, y) DM_PROFILE_PASTE(x, y)

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

#if defined(NDEBUG) || defined(DM_PROFILE_NULL)
    #define DM_PROFILE_TEXT_LENGTH 1024
    #define DM_PROFILE_TEXT(format, ...)
    #define DM_PROFILE(name)
    #define DM_PROFILE_DYN(name, name_hash)

    #define DM_COUNTER_DYN(counter_index, amount)

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
    #define _DM_PROFILE_SCOPE(name, name_hash_ptr) \
        dmProfile::ProfileScope DM_PROFILE_PASTE2(profile_scope, __LINE__)(name, name_hash_ptr);

    #define DM_PROFILE(name) \
        static uint64_t DM_PROFILE_PASTE2(hash, __LINE__)        = 0; \
        _DM_PROFILE_SCOPE(name, & DM_PROFILE_PASTE2(hash, __LINE__))

    #define DM_PROFILE_DYN(name, hash_cache) \
        _DM_PROFILE_SCOPE(name, hash_cache)

    #define DM_PROFILE_TEXT_LENGTH 1024
    #define DM_PROFILE_TEXT(format, ...)              dmProfile::LogText(format, __VA_ARGS__)

    #define DM_COUNTER_DYN(counter_index, amount)

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
        inline ProfileScope(const char* name, uint64_t* name_hash)
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
