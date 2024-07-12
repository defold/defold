// Generated, do not edit!
// Generated with cwd=/Users/mathiaswesterdahl/work/defold/engine/resource and cmd=/Users/mathiaswesterdahl/work/defold/scripts/dmsdk/gen_sdk.py -i /Users/mathiaswesterdahl/work/defold/engine/resource/sdk_gen.json

// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_RESOURCE_GEN_HPP
#define DMSDK_RESOURCE_GEN_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif


/*# Resource
 *
 * Functions for managing resource types.
 *
 * @document
 * @language C++
 * @name Resource
 * @namespace dmResource
 * @path engine/resource/src/dmsdk/resource/resource_gen.hpp
 */

#include <stdint.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/align.h>

#include <dmsdk/resource/resource.h>

namespace dmResource
{
    /*# 
     * ResourceResult
     * @enum
     * @name Result
     * @member  RESOURCE_RESULT_OK
     * @member  RESOURCE_RESULT_INVALID_DATA
     * @member  RESOURCE_RESULT_DDF_ERROR
     * @member  RESOURCE_RESULT_RESOURCE_NOT_FOUND
     * @member  RESOURCE_RESULT_MISSING_FILE_EXTENSION
     * @member  RESOURCE_RESULT_ALREADY_REGISTERED
     * @member  RESOURCE_RESULT_INVAL
     * @member  RESOURCE_RESULT_UNKNOWN_RESOURCE_TYPE
     * @member  RESOURCE_RESULT_OUT_OF_MEMORY
     * @member  RESOURCE_RESULT_IO_ERROR
     * @member  RESOURCE_RESULT_NOT_LOADED
     * @member  RESOURCE_RESULT_OUT_OF_RESOURCES
     * @member  RESOURCE_RESULT_STREAMBUFFER_TOO_SMALL
     * @member  RESOURCE_RESULT_FORMAT_ERROR
     * @member  RESOURCE_RESULT_CONSTANT_ERROR
     * @member  RESOURCE_RESULT_NOT_SUPPORTED
     * @member  RESOURCE_RESULT_RESOURCE_LOOP_ERROR
     * @member  RESOURCE_RESULT_PENDING
     * @member  RESOURCE_RESULT_INVALID_FILE_EXTENSION
     * @member  RESOURCE_RESULT_VERSION_MISMATCH
     * @member  RESOURCE_RESULT_SIGNATURE_MISMATCH
     * @member  RESOURCE_RESULT_UNKNOWN_ERROR
     */
    enum Result {
        RESULT_OK = 0,
        RESULT_INVALID_DATA = -1,
        RESULT_DDF_ERROR = -2,
        RESULT_RESOURCE_NOT_FOUND = -3,
        RESULT_MISSING_FILE_EXTENSION = -4,
        RESULT_ALREADY_REGISTERED = -5,
        RESULT_INVAL = -6,
        RESULT_UNKNOWN_RESOURCE_TYPE = -7,
        RESULT_OUT_OF_MEMORY = -8,
        RESULT_IO_ERROR = -9,
        RESULT_NOT_LOADED = -10,
        RESULT_OUT_OF_RESOURCES = -11,
        RESULT_STREAMBUFFER_TOO_SMALL = -12,
        RESULT_FORMAT_ERROR = -13,
        RESULT_CONSTANT_ERROR = -14,
        RESULT_NOT_SUPPORTED = -15,
        RESULT_RESOURCE_LOOP_ERROR = -16,
        RESULT_PENDING = -17,
        RESULT_INVALID_FILE_EXTENSION = -18,
        RESULT_VERSION_MISMATCH = -19,
        RESULT_SIGNATURE_MISMATCH = -20,
        RESULT_UNKNOWN_ERROR = -21,
    };

    /*# 
     * Resource factory handle. Holds references to all currently loaded resources.
     * @typedef
     * @name HFactory
     */
    typedef HResourceFactory HFactory;

    /*# 
     * Holds information about preloading resources
     * @typedef
     * @name HPreloadHintInfo
     */
    typedef HResourcePreloadHintInfo HPreloadHintInfo;

    /*# 
     * Holds information about a currently loaded resource.
     * @typedef
     * @name HDescriptor
     */
    typedef HResourceDescriptor HDescriptor;

    /*# 
     * Parameters to ResourceReloaded function of the resource type
     * @name ResourceReloadedParams
     */
    typedef ResourceReloadedParams ResourceReloadedParams;

    /*#
        * Generated from [ref:FResourceReloadedCallback]
        */
    typedef FResourceReloadedCallback FReloadedCallback;

    /*# 
     * Get a resource from factory
     * @name Get
     * @param factory [type:HResourceFactory] Factory handle
     * @param name [type:const char*] Resource name
     * @param resource [type:void**] Created resource
     * @return result [type:ResourceResult] RESULT_OK on success
     */
    Result Get(HFactory factory, const char * name, void ** resource);

    /*# 
     * Get a resource from factory
     * @name GetByHash
     * @param factory [type:HResourceFactory] Factory handle
     * @param name [type:dmhash_t] Resource name
     * @param resource [type:void**] Created resource
     * @return result [type:ResourceResult] RESULT_OK on success
     */
    Result GetByHash(HFactory factory, dmhash_t name, void ** resource);

    /*# 
     * Get raw resource data. Unregistered resources can be loaded with this function.
     * If successful, the returned resource data must be deallocated with free()
     * @name GetRaw
     * @param factory [type:HResourceFactory] Factory handle
     * @param name [type:dmhash_t] Resource name
     * @param resource [type:void**] Created resource
     * @param resource_size [type:uint32_t*] Resource size
     * @return result [type:ResourceResult] RESULT_OK on success
     */
    Result GetRaw(HFactory factory, const char * name, void ** resource, uint32_t * resource_size);

    /*#
        * Generated from [ref:ResourceRelease]
        */
    void Release(HFactory factory, void * resource);

    /*#
        * Generated from [ref:ResourcePreloadHint]
        */
    bool PreloadHint(HPreloadHintInfo preloader, const char * path);

    /*#
        * Generated from [ref:ResourceGetPath]
        */
    Result GetPath(HFactory factory, const void * resource, dmhash_t * hash);

    /*# 
     * Adds a file to the resource system
     * Any request for this path will go through any existing mounts first.
     * If you wish to provide file overrides, please use the LiveUpdate feature for that.
     * The file isn't persisted between sessions.
     * @name AddFile
     * @param factory [type:HResourceFactory] Factory handle
     * @param path [type:const char*] The path of the resource
     * @param size [type:uint32_t] The size of the resource (in bytes)
     * @param resource [type:const void*] The resource payload
     * @return result [type:ResourceResult] RESULT_OK on success
     */
    Result AddFile(HFactory factory, const char * path, uint32_t size, const void * resource);

    /*# 
     * Removes a previously registered file from the resource system
     * @name RemoveFile
     * @param factory [type:HResourceFactory] Factory handle
     * @param path [type:const char*] The path of the resource
     * @return result [type:ResourceResult] RESULT_OK on success
     */
    Result RemoveFile(HFactory factory, const char * path);

    /*#
        * Generated from [ref:ResourceDescriptorGetNameHash]
        */
    dmhash_t GetNameHash(HDescriptor rd);

    /*#
        * Generated from [ref:ResourceDescriptorSetResource]
        */
    void SetResource(HDescriptor rd, void * resource);

    /*#
        * Generated from [ref:ResourceDescriptorGetResource]
        */
    void * GetResource(HDescriptor rd);

    /*#
        * Generated from [ref:ResourceDescriptorSetPrevResource]
        */
    void SetPrevResource(HDescriptor rd, void * resource);

    /*#
        * Generated from [ref:ResourceDescriptorGetPrevResource]
        */
    void * GetPrevResource(HDescriptor rd);

    /*#
        * Generated from [ref:ResourceDescriptorSetResourceSize]
        */
    void SetResourceSize(HDescriptor rd, uint32_t size);

    /*#
        * Generated from [ref:ResourceDescriptorGetResourceSize]
        */
    uint32_t GetResourceSize(HDescriptor rd);

    /*#
        * Generated from [ref:ResourceDescriptorGetType]
        */
    HResourceType GetType(HDescriptor rd);


} // namespace dmResource

#endif // #define DMSDK_RESOURCE_GEN_HPP

