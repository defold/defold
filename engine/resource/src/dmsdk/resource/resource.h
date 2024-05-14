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

#ifndef DMSDK_RESOURCE_H
#define DMSDK_RESOURCE_H

/*# Resource
 *
 * Functions for managing resource types.
 *
 * @document
 * @name Resource
 * @namespace dmResource
 * @path engine/resource/src/dmsdk/resource/resource.h
 */

#include <stdbool.h>
#include <stdint.h>
#include <dmsdk/dlib/hash.h>

#include <dmsdk/resource/resource_desc.h>

typedef struct ResourceFactory* HResourceFactory;
typedef struct ResourcePreloadHintInfo* HResourcePreloadHintInfo;
typedef struct ResourceType* HResourceType;

struct ResourceReloadedParams;

/**
 * ResourceResult
 * @enum
 * @name ResourceResult
 * @member RESOURCE_RESULT_OK
 * @member RESOURCE_RESULT_INVALID_DATA
 * @member RESOURCE_RESULT_DDF_ERROR
 * @member RESOURCE_RESULT_RESOURCE_NOT_FOUND
 * @member RESOURCE_RESULT_MISSING_FILE_EXTENSION
 * @member RESOURCE_RESULT_ALREADY_REGISTERED
 * @member RESOURCE_RESULT_INVAL
 * @member RESOURCE_RESULT_UNKNOWN_RESOURCE_TYPE
 * @member RESOURCE_RESULT_OUT_OF_MEMORY
 * @member RESOURCE_RESULT_IO_ERROR
 * @member RESOURCE_RESULT_NOT_LOADED
 * @member RESOURCE_RESULT_OUT_OF_RESOURCES
 * @member RESOURCE_RESULT_STREAMBUFFER_TOO_SMALL
 * @member RESOURCE_RESULT_FORMAT_ERROR
 * @member RESOURCE_RESULT_CONSTANT_ERROR
 * @member RESOURCE_RESULT_NOT_SUPPORTED
 * @member RESOURCE_RESULT_RESOURCE_LOOP_ERROR
 * @member RESOURCE_RESULT_PENDING
 * @member RESOURCE_RESULT_INVALID_FILE_EXTENSION
 * @member RESOURCE_RESULT_VERSION_MISMATCH
 * @member RESOURCE_RESULT_SIGNATURE_MISMATCH
 * @member RESOURCE_RESULT_UNKNOWN_ERROR
 */
typedef enum ResourceResult
{
    RESOURCE_RESULT_OK                        = 0,
    RESOURCE_RESULT_INVALID_DATA              = -1,
    RESOURCE_RESULT_DDF_ERROR                 = -2,
    RESOURCE_RESULT_RESOURCE_NOT_FOUND        = -3,
    RESOURCE_RESULT_MISSING_FILE_EXTENSION    = -4,
    RESOURCE_RESULT_ALREADY_REGISTERED        = -5,
    RESOURCE_RESULT_INVAL                     = -6,
    RESOURCE_RESULT_UNKNOWN_RESOURCE_TYPE     = -7,
    RESOURCE_RESULT_OUT_OF_MEMORY             = -8,
    RESOURCE_RESULT_IO_ERROR                  = -9,
    RESOURCE_RESULT_NOT_LOADED                = -10,
    RESOURCE_RESULT_OUT_OF_RESOURCES          = -11,
    RESOURCE_RESULT_STREAMBUFFER_TOO_SMALL    = -12,
    RESOURCE_RESULT_FORMAT_ERROR              = -13,
    RESOURCE_RESULT_CONSTANT_ERROR            = -14,
    RESOURCE_RESULT_NOT_SUPPORTED             = -15,
    RESOURCE_RESULT_RESOURCE_LOOP_ERROR       = -16,
    RESOURCE_RESULT_PENDING                   = -17,
    RESOURCE_RESULT_INVALID_FILE_EXTENSION    = -18,
    RESOURCE_RESULT_VERSION_MISMATCH          = -19,
    RESOURCE_RESULT_SIGNATURE_MISMATCH        = -20,
    RESOURCE_RESULT_UNKNOWN_ERROR             = -21,
} ResourceResult;

// Resource type functions


/*#
 * Function called when a resource has been reloaded.
 * @param params Parameters
 * @see RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT
 * @see ResourceRegisterReloadedCallback
 */
typedef void (*FResourceReloadedCallback)(const struct ResourceReloadedParams* params);

/**
 * Register a callback function that will be called with the specified user data when a resource has been reloaded.
 * The callbacks will not necessarily be called in the order they were registered.
 * This has only effect when reloading is supported.
 * @name ResourceRegisterReloadedCallback
 * @param factory Handle of the factory to which the callback will be registered
 * @param callback Callback function to register
 * @param user_data User data that will be supplied to the callback when it is called
 * @see RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT
 */
void ResourceRegisterReloadedCallback(HResourceFactory factory, FResourceReloadedCallback callback, void* user_data);

/**
 * Remove a registered callback function, O(n).
 * @name ResourceUnregisterReloadedCallback
 * @param factory Handle of the factory from which the callback will be removed
 * @param callback Callback function to remove
 * @param user_data User data that was supplied when the callback was registered
 * @see RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT
 */
void ResourceUnregisterReloadedCallback(HResourceFactory factory, FResourceReloadedCallback callback, void* user_data);



/*#
 * Returns the canonical path hash of a resource
 * @typedef
 * @name FResourceDecrypt
 * @param buffer [type: void*] The input/output buffer
 * @param buffer_len [type: uint32_t] The size of the buffer (in bytes)
 * @return RESULT_OK on success
 */
typedef ResourceResult (*FResourceDecryption)(void* buffer, uint32_t buffer_len);


/*#
 * Registers a custom resource decryption function
 * @name ResourceRegisterDecryptionFunction
 * @param decrypt_resource [type: dmResource::FDecryptResource] The decryption function
 */
void ResourceRegisterDecryptionFunction(FResourceDecryption decrypt_resource);


/*#
 * Get a resource from factory
 * @name ResourceGet
 * @param factory [type: HResourceFactory] Factory handle
 * @param name [type: const char*] Resource name
 * @param resource [type: void**] Created resource
 * @return result [type: ResourceResult]  RESULT_OK on success
 */
ResourceResult ResourceGet(HResourceFactory factory, const char* name, void** resource);

/*#
 * Get a resource from factory
 * @name Get
 * @param factory [type: HResourceFactory] Factory handle
 * @param name [type: dmhash_t] Resource name
 * @param resource [type: void**] Created resource
 * @return result [type: ResourceResult]  RESULT_OK on success
 */
ResourceResult ResourceGetByHash(HResourceFactory factory, dmhash_t name, void** resource);


/*#
 * Release resource
 * @name Release
 * @param factory [type: HResourceFactory] Factory handle
 * @param resource [type: void*] Resource pointer
 */
void ResourceRelease(HResourceFactory factory, void* resource);

/*#
 * Hint the preloader what to load before Create is called on the resource.
 * The resources are not guaranteed to be loaded before Create is called.
 * This function can be called from a worker thread.
 * @name PreloadHint
 * @param factory [type: dmResource::HResourcePreloadHintInfo] Preloader handle
 * @param path [type: const char*] Resource path
 * @return result [type: bool] if successfully invoking preloader.
 */
bool ResourcePreloadHint(HResourcePreloadHintInfo preloader, const char* path);

/*#
 * Returns the canonical path hash of a resource
 * @param factory [type: HResourceFactory] Factory handle
 * @param resource [type: void*] The resource pointer
 * @param hash [type: dmhash_t] The path hash of the resource
 * @return result [type: ResourceResult] RESULT_OK on success
 */
ResourceResult ResourceGetPath(HResourceFactory factory, const void* resource, dmhash_t* hash);

/*#
 * Adds a file to the resource system
 * Any request for this path will go through any existing mounts first.
 * If you wish to provide file overrides, please use the LiveUpdate feature for that.
 * The file isn't persisted between sessions.
 *
 * @name ResourceAddFile
 * @param factory [type: HResourceFactory] Factory handle
 * @param path [type: const char*] The path of the resource
 * @param size [type: uint32_t] The size of the resource (in bytes)
 * @param resource [type: const void*] The resource payload
 * @return result [type: ResourceResult] RESULT_OK on success
 */
ResourceResult ResourceAddFile(HResourceFactory factory, const char* path, uint32_t size, const void* resource);

/*#
 * Removes a previously registered file from the resource system
 * @name ResourceRemoveFile
 * @param factory [type: HResourceFactory] Factory handle
 * @param path [type: const char*] The path of the resource
 * @return result [type: ResourceResult] RESULT_OK on success
 */
ResourceResult ResourceRemoveFile(HResourceFactory factory, const char* path);

#endif // DMSDK_RESOURCE_H
