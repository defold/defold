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
     * @language C++
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
     * @language C++
     */
    typedef HResourceFactory HFactory;

    /*# 
     * Holds information about preloading resources
     * @typedef
     * @name HPreloadHintInfo
     * @language C++
     */
    typedef HResourcePreloadHintInfo HPreloadHintInfo;

    /*# 
     * Holds information about a currently loaded resource.
     * @typedef
     * @name HDescriptor
     * @language C++
     */
    typedef HResourceDescriptor HDescriptor;

    /*# 
     * Parameters to ResourceReloaded function of the resource type
     * @name ResourceReloadedParams
     * @language C++
     */
    typedef ResourceReloadedParams ResourceReloadedParams;

    /*# 
     * Function called when a resource has been reloaded.
     * @name FReloadedCallback
     * @language C++
     */
    typedef FResourceReloadedCallback FReloadedCallback;

    /*# 
     * Get a resource from factory
     * @name Get
     * @language C++
     * @param factory [type:HResourceFactory] Factory handle
     * @param name [type:const char*] Resource name
     * @param resource [type:void**] Created resource
     * @return result [type:ResourceResult] RESULT_OK on success
     */
    Result Get(HFactory factory, const char * name, void ** resource);

    /*# 
     * Get a resource from factory
     * @name GetByHash
     * @language C++
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
     * @language C++
     * @param factory [type:HResourceFactory] Factory handle
     * @param name [type:dmhash_t] Resource name
     * @param resource [type:void**] Created resource
     * @param resource_size [type:uint32_t*] Resource size
     * @return result [type:ResourceResult] RESULT_OK on success
     */
    Result GetRaw(HFactory factory, const char * name, void ** resource, uint32_t * resource_size);

    /*#
        * Generated from [ref:ResourceGetDescriptor]
        */
    Result GetDescriptor(HFactory factory, const char * path, HDescriptor * descriptor);

    /*#
        * Generated from [ref:ResourceGetDescriptorByHash]
        */
    Result GetDescriptorByHash(HFactory factory, dmhash_t path_hash, HDescriptor * descriptor);

    /*# 
     * Release resource
     * @name Release
     * @language C++
     * @param factory [type:HResourceFactory] Factory handle
     * @param resource [type:void*] Resource pointer
     * @note Decreases ref count by 1. If it reaches 0, the resource destroy function is called.
     */
    void Release(HFactory factory, void * resource);

    /*# 
     * Hint the preloader what to load before Create is called on the resource.
     * The resources are not guaranteed to be loaded before Create is called.
     * This function can be called from a worker thread.
     * @name PreloadHint
     * @language C++
     * @param preloader [type:dmResource::HResourcePreloadHintInfo] Preloader handle
     * @param path [type:const char*] Resource path
     * @return result [type:bool] if successfully invoking preloader.
     */
    bool PreloadHint(HPreloadHintInfo preloader, const char * path);

    /*# 
     * Returns the canonical path hash of a resource
     * @name GetPath
     * @language C++
     * @param factory [type:HResourceFactory] Factory handle
     * @param resource [type:void*] The resource pointer
     * @param hash [type:dmhash_t*] (out) The path hash of the resource
     * @return result [type:ResourceResult] RESULT_OK on success
     */
    Result GetPath(HFactory factory, const void * resource, dmhash_t * hash);

    /*# 
     * Adds a file to the resource system
     * Any request for this path will go through any existing mounts first.
     * If you wish to provide file overrides, please use the LiveUpdate feature for that.
     * The file isn't persisted between sessions.
     * @name AddFile
     * @language C++
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
     * @language C++
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
/*# 
 * Resource factory handle. Holds references to all currently loaded resources.
 * @typedef
 * @name HResourceFactory
 * @language C
 */

/*# 
 * Holds information about preloading resources
 * @typedef
 * @name HResourcePreloadHintInfo
 * @language C
 */

/*# 
 * Holds the resource types, as well as extra in engine contexts that can be shared across type functions.
 * @typedef
 * @name HResourceTypeContext
 * @language C
 */

/*# 
 * Represents a resource type, with a context and type functions for creation and destroying a resource.
 * @typedef
 * @name HResourceType
 * @language C
 */

/*# 
 * Holds information about a currently loaded resource.
 * @typedef
 * @name HResourceDescriptor
 * @language C
 */

/*# 
 * ResourceResult
 * @enum
 * @name ResourceResult
 * @language C
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

/*# 
 * Function called when a resource has been reloaded.
 * @name FResourceReloadedCallback
 * @language C
 */

/*# 
 * Encrypts a resource in-place
 * @typedef
 * @name FResourceDecrypt
 * @language C
 * @param buffer [type:void*] The input/output buffer
 * @param buffer_len [type:uint32_t] The size of the buffer (in bytes)
 * @return  RESULT_OK on success
 */

/*# 
 * Registers a custom resource decryption function
 * @name ResourceRegisterDecryptionFunction
 * @language C
 * @param decrypt_resource [type:dmResource::FDecryptResource] The decryption function
 */

/*# 
 * Get a resource from factory
 * @name ResourceGet
 * @language C
 * @param factory [type:HResourceFactory] Factory handle
 * @param name [type:const char*] Resource name
 * @param resource [type:void**] Created resource
 * @return result [type:ResourceResult] RESULT_OK on success
 */

/*# 
 * Get a resource from factory
 * @name ResourceGetByHash
 * @language C
 * @param factory [type:HResourceFactory] Factory handle
 * @param name [type:dmhash_t] Resource name
 * @param resource [type:void**] Created resource
 * @return result [type:ResourceResult] RESULT_OK on success
 */

/*# 
 * Get raw resource data. Unregistered resources can be loaded with this function.
 * If successful, the returned resource data must be deallocated with free()
 * @name ResourceGetRaw
 * @language C
 * @param factory [type:HResourceFactory] Factory handle
 * @param name [type:dmhash_t] Resource name
 * @param resource [type:void**] Created resource
 * @param resource_size [type:uint32_t*] Resource size
 * @return result [type:ResourceResult] RESULT_OK on success
 */

/*# 
 * Get resource descriptor from resource (name)
 * @name GetDescriptor
 * @language C
 * @param factory [type:HResourceFactory] Factory handle
 * @param path [type:dmhash_t] Resource path
 * @param descriptor [type:HResourceDescriptor*] Returned resource descriptor
 * @return result [type:ResourceResult] RESULT_OK on success
 */

/*# 
 * Get resource descriptor from resource (name)
 * @name GetDescriptorByHash
 * @language C
 * @param factory [type:HResourceFactory] Factory handle
 * @param path_hash [type:dmhash_t] Resource path hash
 * @param descriptor [type:HResourceDescriptor*] Returned resource descriptor
 * @return result [type:ResourceResult] RESULT_OK on success
 */

/*# 
 * Release resource
 * @name ResourceRelease
 * @language C
 * @param factory [type:HResourceFactory] Factory handle
 * @param resource [type:void*] Resource pointer
 * @note Decreases ref count by 1. If it reaches 0, the resource destroy function is called.
 */

/*# 
 * Hint the preloader what to load before Create is called on the resource.
 * The resources are not guaranteed to be loaded before Create is called.
 * This function can be called from a worker thread.
 * @name ResourcePreloadHint
 * @language C
 * @param preloader [type:dmResource::HResourcePreloadHintInfo] Preloader handle
 * @param path [type:const char*] Resource path
 * @return result [type:bool] if successfully invoking preloader.
 */

/*# 
 * Returns the canonical path hash of a resource
 * @name ResourceGetPath
 * @language C
 * @param factory [type:HResourceFactory] Factory handle
 * @param resource [type:void*] The resource pointer
 * @param hash [type:dmhash_t*] (out) The path hash of the resource
 * @return result [type:ResourceResult] RESULT_OK on success
 */

/*# 
 * Adds a file to the resource system
 * Any request for this path will go through any existing mounts first.
 * If you wish to provide file overrides, please use the LiveUpdate feature for that.
 * The file isn't persisted between sessions.
 * @name ResourceAddFile
 * @language C
 * @param factory [type:HResourceFactory] Factory handle
 * @param path [type:const char*] The path of the resource
 * @param size [type:uint32_t] The size of the resource (in bytes)
 * @param resource [type:const void*] The resource payload
 * @return result [type:ResourceResult] RESULT_OK on success
 */

/*# 
 * Removes a previously registered file from the resource system
 * @name ResourceRemoveFile
 * @language C
 * @param factory [type:HResourceFactory] Factory handle
 * @param path [type:const char*] The path of the resource
 * @return result [type:ResourceResult] RESULT_OK on success
 */

/*# 
 * Parameters to ResourcePreload function of the resource type
 * @name ResourcePreloadParams
 * @language C
 */

/*# 
 * Parameters to ResourceCreate function of the resource type
 * @name ResourceCreateParams
 * @language C
 */

/*# 
 * Parameters to ResourcePostCreate function of the resource type
 * @name ResourcePostCreateParams
 * @language C
 */

/*# 
 * Parameters to ResourceRecreate function of the resource type
 * @name ResourceRecreateParams
 * @language C
 */

/*# 
 * Parameters to ResourceDestroy function of the resource type
 * @name ResourceDestroyParams
 * @language C
 */

/*# 
 * Parameters to ResourceReloaded function of the resource type
 * @name ResourceReloadedParams
 * @language C
 */

/*# 
 * Resource type creator desc byte size declaration.
 * The registered description data passeed to ResourceRegisterTypeCreatorDesc must be of at least this size.
 * @name ResourceTypeCreatorDescBufferSize
 * @language C
 */

/*# declare a new resource type
 * Declare and register new resource type to the engine.
 * This macro is used to declare the resource type callback functions used by the engine to communicate with the extension.
 * @macro
 * @name DM_DECLARE_RESOURCE_TYPE
 * @language C
 * @examples 
 * Register a new type:
 * 
 * ```cpp
 * #include <dmsdk/resource/resource_params.h>
 * #include <dmsdk/resource/resource_type.h>
 * 
 * static ResourceResult MyResourceTypeScriptCreate(const ResourceCreateParams* params) {}
 * static ResourceResult MyResourceTypeScriptDestroy(const ResourceDestroyParams* params) {}
 * static ResourceResult MyResourceTypeScriptRecreate(const ResourceRereateParams* params) {}
 * 
 * struct MyContext
 * {
 *     // ...
 * };
 * 
 * static ResourceResult RegisterResourceTypeBlob(HResourceTypeRegisterContext ctx, HResourceType type)
 * {
 *     // The engine.cpp creates the contexts for our built in types.
 *     // Here we register a custom type
 *     MyContext* context = new MyContext;
 * 
 *     ResourceTypeSetContext(type, (void*)context);
 *     ResourceTypeSetCreateFn(type, MyResourceTypeScriptCreate);
 *     ResourceTypeSetDestroyFn(type, MyResourceTypeScriptDestroy);
 *     ResourceTypeSetRecreateFn(type, MyResourceTypeScriptRecreate);
 * }
 * 
 * static ResourceResult DeregisterResourceTypeBlob(ResourceTypeRegisterContext& ctx)
 * {
 *     MyContext** context = (MyContext*)ResourceTypeGetContext(type);
 *     delete *context;
 * }
 * 
 * DM_DECLARE_RESOURCE_TYPE(ResourceTypeBlob, "blobc", RegisterResourceTypeBlob, DeregisterResourceTypeBlob);
 * ```
 */


