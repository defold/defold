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

#ifndef DMSDK_RESOURCE_H
#define DMSDK_RESOURCE_H

/*# Resource
 *
 * Functions for managing resource types.
 *
 * @document
 * @name Resource
 * @language C++
 */

#include <stdbool.h>
#include <stdint.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/align.h> // DM_ALIGNED

/*#
* Resource factory handle. Holds references to all currently loaded resources.
* @name HResourceFactory
* @typedef
*/
typedef struct ResourceFactory* HResourceFactory;

/*#
* Holds information about preloading resources
* @name HResourcePreloadHintInfo
* @typedef
*/
typedef struct ResourcePreloadHintInfo* HResourcePreloadHintInfo;

/*#
* Holds the resource types, as well as extra in engine contexts that can be shared across type functions.
* @name HResourceTypeContext
* @typedef
*/
typedef struct ResourceTypeContext* HResourceTypeContext;

/*#
* Represents a resource type, with a context and type functions for creation and destroying a resource.
* @name HResourceType
* @typedef
*/
typedef struct ResourceType* HResourceType;

/*#
* Holds information about a currently loaded resource.
* @name HResourceDescriptor
* @typedef
*/
typedef struct ResourceDescriptor* HResourceDescriptor;

// forward declarations
struct ResourceReloadedParams;
struct ResourcePreloadParams;
struct ResourceCreateParams;
struct ResourcePostCreateParams;
struct ResourceDestroyParams;
struct ResourceRecreateParams;

/*#
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


/*#
 * Function called when a resource has been reloaded.
 * @typedef
 * @name FResourceReloadedCallback
 * @param params [type:ResourceReloadedParams*] Parameters
 * @see RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT (internal flag)
 * @see [ref:ResourceRegisterReloadedCallback]
 */
typedef void (*FResourceReloadedCallback)(const struct ResourceReloadedParams* params);

/**
 * Register a callback function that will be called with the specified user data when a resource has been reloaded.
 * The callbacks will not necessarily be called in the order they were registered.
 * This has only effect when reloading is supported.
 * @name ResourceRegisterReloadedCallback
 * @param factory [type:HResourceFactory] Handle of the factory to which the callback will be registered
 * @param callback [type:FResourceReloadedCallback] Callback function to register
 * @param user_data [type:void*] User data that will be supplied to the callback when it is called
 * @see RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT (internal flag)
 */
void ResourceRegisterReloadedCallback(HResourceFactory factory, FResourceReloadedCallback callback, void* user_data);

/**
 * Remove a registered callback function, O(n).
 * @name ResourceUnregisterReloadedCallback
 * @param factory [type:HResourceFactory] Handle of the factory from which the callback will be removed
 * @param callback [type:FResourceReloadedCallback] Callback function to remove
 * @param user_data [type:void*] User data that was supplied when the callback was registered
 * @see RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT (internal flag)
 */
void ResourceUnregisterReloadedCallback(HResourceFactory factory, FResourceReloadedCallback callback, void* user_data);


/*#
 * Encrypts a resource in-place
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
 * Get (load) a resource from factory
 * @note if successful, it increments the ref count by one
 * @name ResourceGet
 * @param factory [type: HResourceFactory] Factory handle
 * @param path [type: const char*] Resource path
 * @param resource [type: void**] Created resource
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
ResourceResult ResourceGet(HResourceFactory factory, const char* path, void** resource);

/*#
 * Get (load) a resource from factory
 * @note if successful, it increments the ref count by one
 * @name ResourceGetWithExt
 * @param factory [type: HResourceFactory] Factory handle
 * @param path [type: const char*] Resource path
 * @param ext [type: const char*] Resource extension (e.g. "texturec", "ttf"). Must match the extension of the path.
 * @param resource [type: void**] Created resource
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success.
 *                                       RESOURCE_RESULT_INVALID_FILE_EXTENSION if the path extension doesn't match the required extension.
 */
ResourceResult ResourceGetWithExt(HResourceFactory factory, const char* path, const char* ext, void** resource);

/*#
 * Get a loaded resource from factory
 * @note this currently doesn't load a resource
 * @note if successful, it increments the ref count by one
 * @name ResourceGetByHash
 * @param factory [type: HResourceFactory] Factory handle
 * @param path_hash [type: dmhash_t] Resource path hash
 * @param resource [type: void**] Created resource
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
ResourceResult ResourceGetByHash(HResourceFactory factory, dmhash_t path_hash, void** resource);

/*#
 * Get a loaded resource from factory and also verifying that it's the expected file type
 * @name ResourceGetByHashAndExt
 * @param factory [type: HResourceFactory] Factory handle
 * @param path_hash [type: dmhash_t] Resource path hash
 * @param ext_hash [type: dmhash_t] Resource extension hash (e.g. "texturec", "ttf"). Must match the extension of the path.
 * @param resource [type: void**] Created resource
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success.
 *                                       RESOURCE_RESULT_INVALID_FILE_EXTENSION if the path extension doesn't match the required extension.
 */
ResourceResult ResourceGetByHashAndExt(HResourceFactory factory, dmhash_t path_hash, dmhash_t ext_hash, void** resource);

/**
 * Increase resource reference count by 1.
 * @name ResourceIncRef
 * @param factory [type: HResourceFactory] Factory handle
 * @param resource [type: void*] The resource
 */
void ResourceIncRef(HResourceFactory factory, void* resource);

/*#
 * Get raw resource data. Unregistered resources can be loaded with this function.
 * If successful, the returned resource data must be deallocated with free()
 * @name ResourceGetRaw
 * @param factory [type: HResourceFactory] Factory handle
 * @param name [type: dmhash_t] Resource name
 * @param resource [type: void**] Created resource
 * @param resource_size [type: uint32_t*] Resource size
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
ResourceResult ResourceGetRaw(HResourceFactory factory, const char* name, void** resource, uint32_t* resource_size);

/*#
 * Get resource descriptor from resource (name)
 * @name GetDescriptor
 * @param factory [type: HResourceFactory] Factory handle
 * @param path [type: dmhash_t] Resource path
 * @param descriptor [type: HResourceDescriptor*] Returned resource descriptor. Temporary, don't copy.
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
ResourceResult ResourceGetDescriptor(HResourceFactory factory, const char* path, HResourceDescriptor* descriptor);

/*#
 * Get resource descriptor from resource (name)
 * @name GetDescriptorByHash
 * @param factory [type: HResourceFactory] Factory handle
 * @param path_hash [type: dmhash_t] Resource path hash
 * @param descriptor [type: HResourceDescriptor*] Returned resource descriptor. Temporary, don't copy.
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
ResourceResult ResourceGetDescriptorByHash(HResourceFactory factory, dmhash_t path_hash, HResourceDescriptor* descriptor);

/*#
 * Release resource
 * @note Decreases ref count by 1. If it reaches 0, the resource destroy function is called.
 * @name ResourceRelease
 * @param factory [type: HResourceFactory] Factory handle
 * @param resource [type: void*] Resource pointer
 */
void ResourceRelease(HResourceFactory factory, void* resource);

/*#
 * Hint the preloader what to load before Create is called on the resource.
 * The resources are not guaranteed to be loaded before Create is called.
 * This function can be called from a worker thread.
 * @name ResourcePreloadHint
 * @param preloader [type: dmResource::HResourcePreloadHintInfo] Preloader handle
 * @param path [type: const char*] Resource path
 * @return result [type: bool] if successfully invoking preloader.
 */
bool ResourcePreloadHint(HResourcePreloadHintInfo preloader, const char* path);

/*#
 * Returns the canonical path hash of a resource
 * @name ResourceGetPath
 * @param factory [type: HResourceFactory] Factory handle
 * @param resource [type: void*] The resource pointer
 * @param hash [type: dmhash_t*] (out) The path hash of the resource
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
ResourceResult ResourceGetPath(HResourceFactory factory, const void* resource, dmhash_t* hash);

/*#
 * Get a resource extension from a path, i.e "resource.ext" will return "ext".
 * @name ResourceGetExtFromPath
 * @param path [type: const char*] The path to the resource
 * @return extension [type: const char*] Pointer to extension string if an extension was found, 0 otherwise
 */
const char* ResourceGetExtFromPath(const char* path);

/*#
 * Gets the normalized resource path: "/my//icon.texturec" -> "/my/icon.texturec". "my/icon.texturec" -> "/my/icon.texturec".
 * @name ResourceGetCanonicalPath
 * @param path [type: const char*] the relative dir of the resource
 * @param buf [type: const char*] the output of the normalization
 * @param buf_len [type: uint32_t] the size of the output buffer
 * @return length [type: uint32_t] the length of the output string
 */
uint32_t ResourceGetCanonicalPath(const char* path, char* buf, uint32_t buf_len);

/*#
* Creates and inserts a resource into the factory
* @note The input data pointer is not stored
* @note The reference count is 1, so make sure it's destruction is handled
* @name ResourceCreateResource
* @param factory [type: HResourceFactory] Factory handle
* @param name [type: const char*] Resource name
* @param data [type: void*] Resource data
* @param data_size [type: uint32_t] Resource data size
* @param resource [type: void**] (out) Stores the created resource
* @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
*/
ResourceResult ResourceCreateResource(HResourceFactory factory, const char* name, void* data, uint32_t data_size, void** resource);

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
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
ResourceResult ResourceAddFile(HResourceFactory factory, const char* path, uint32_t size, const void* resource);

/*#
 * Removes a previously registered file from the resource system
 * @name ResourceRemoveFile
 * @param factory [type: HResourceFactory] Factory handle
 * @param path [type: const char*] The path of the resource
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
ResourceResult ResourceRemoveFile(HResourceFactory factory, const char* path);

/*#
* Get type from extension
* @name ResourceGetTypeFromExtension
* @param factory [type: HResourceFactory] Factory handle
* @param extension [type: const char*] File extension, without leading "." character. E.g. "ttf"
* @param type [type: HResourceType*] (out) returned type if successful
* @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
*/
ResourceResult ResourceGetTypeFromExtension(HResourceFactory factory, const char* extension, HResourceType* type);

/*#
* Get type from extension hash
* @name ResourceGetTypeFromExtensionHash
* @param factory [type: HResourceFactory] Factory handle
* @param extension_hash [type: dmhash_t] Hash of file extension, without leading "." character. E.g. hash("ttf")
* @param type [type: HResourceType*] (out) returned type if successful
* @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
*/
ResourceResult ResourceGetTypeFromExtensionHash(HResourceFactory factory, dmhash_t extension_hash, HResourceType* type);


////////////////////////////////////////////////////////////////////////////////////////////////////////
// Descriptor functions

/*# get path hash of resource
 * @name ResourceDescriptorGetNameHash
 * @param rd [type: HResourceDescriptor] The resource
 * @return hash [type: dmhash_t] The path hash
 */
dmhash_t        ResourceDescriptorGetNameHash(HResourceDescriptor rd);

/*# set the resource data
 * @name ResourceDescriptorSetResource
 * @param rd [type: HResourceDescriptor] The resource handle
 * @param resource [type: void*] The resource data
 */
void            ResourceDescriptorSetResource(HResourceDescriptor rd, void* resource);

/*# get the resource data
 * @name ResourceDescriptorGetResource
 * @param rd [type: HResourceDescriptor] The resource handle
 * @return resource [type: void*] The resource data
 */
void*           ResourceDescriptorGetResource(HResourceDescriptor rd);

/*# set the previous resource data
 * @note only used when recreating a resource
 * @name ResourceDescriptorSetPrevResource
 * @param rd [type: HResourceDescriptor] The resource handle
 * @param resource [type: void*] The resource data
 */
void            ResourceDescriptorSetPrevResource(HResourceDescriptor rd, void* resource);

/*# get the previous resource data
 * @note only used when recreating a resource
 * @name ResourceDescriptorGetPrevResource
 * @param rd [type: HResourceDescriptor] The resource handle
 * @return resource [type: void*] The resource data
 */
void*           ResourceDescriptorGetPrevResource(HResourceDescriptor rd);

/*# set the resource data size
 * @name ResourceDescriptorSetResourceSize
 * @param rd [type: HResourceDescriptor] The resource handle
 * @param size [type: uint32_t] The resource data size (in bytes)
 */
void            ResourceDescriptorSetResourceSize(HResourceDescriptor rd, uint32_t size);

/*# get the resource data size
 * @name ResourceDescriptorGetResourceSize
 * @param rd [type: HResourceDescriptor] The resource handle
 * @return size [type: uint32_t] The resource data size (in bytes)
 */
uint32_t        ResourceDescriptorGetResourceSize(HResourceDescriptor rd);

/*# get the resource type
 * @name ResourceDescriptorGetType
 * @param rd [type: HResourceDescriptor] The resource handle
 * @return resource [type: HResourceType] The resource type
 */
HResourceType   ResourceDescriptorGetType(HResourceDescriptor rd);

/**
 * Increase resource reference count by 1.
 * @name ResourceDescriptorIncRef
 * @param factory [type: HResourceFactory] Factory handle
 * @param rd [type: HResourceDescriptor] The resource handle
 */
void            ResourceDescriptorIncRef(HResourceFactory factory, HResourceDescriptor rd);

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Type functions

void*   ResourceTypeContextGetContextByHash(HResourceTypeContext, dmhash_t extension_hash);

/*#
 * Resource type setup function.
 * @note The type is already cerate, and name and name hash properties are valid to get using the RsourceTypeGetName()/RsourceTypeGetNameHash() functions
 * @typedef
 * @name FResourceTypeRegister
 * @params ctx [type: HResourceTypeContext] Context for resource types
 * @params type [type: HResourceType] The registered resource type.
 * @return RESOURCE_RESULT_OK on success
 */
typedef ResourceResult (*FResourceTypeRegister)(HResourceTypeContext ctx, HResourceType type);

/*#
 * Resource type destroy function. Generally used to destroy the registered resource type context.
 * @typedef
 * @name FResourceTypeDeregister
 * @params ctx [type: HResourceTypeContext] Context for resource types
 * @params type [type: HResourceType] The registered resource type.
 * @return RESOURCE_RESULT_OK on success
 */
typedef ResourceResult (*FResourceTypeDeregister)(HResourceTypeContext ctx, HResourceType type);

/*#
 * Resource preloading function. This may be called from a separate loading thread
 * but will not keep any mutexes held while executing the call. During this call
 * PreloadHint can be called with the supplied hint_info handle.
 * If RESULT_OK is returned, the resource Create function is guaranteed to be called
 * with the preload_data value supplied.
 *
 * @typedef
 * @name FResourcePreload
 * @param param [type: const dmResource::ResourcePreloadParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
typedef ResourceResult (*FResourcePreload)(const struct ResourcePreloadParams* params);

/*#
 * Resource create function
 * @typedef
 * @name FResourceCreate
 * @param param [type: const dmResource::ResourceCreateParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
typedef ResourceResult (*FResourceCreate)(const struct ResourceCreateParams* params);

/*#
 * Resource postcreate function
 * @note returning RESOURCE_CREATE_RESULT_PENDING will result in a repeated callback the following update.
 * @typedef
 * @name FResourcePostCreate
 * @param param [type: const dmResource::ResourcePostCreateParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_CREATE_RESULT_OK on success or RESOURCE_CREATE_RESULT_PENDING when pending
 */
typedef ResourceResult (*FResourcePostCreate)(const struct ResourcePostCreateParams* params);

/*#
 * Resource destroy function
 * @typedef
 * @name FResourceDestroy
 * @param param [type: const dmResource::ResourceDestroyParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
typedef ResourceResult (*FResourceDestroy)(const struct ResourceDestroyParams* params);

/*#
 * Resource recreate function. Recreate resource in-place.
 * @note Beware that any "in flight" resource pointers to the actual resource must remain valid after this call.
 * @typedef
 * @name FResourceRecreate
 * @param param [type: const dmResource::ResourceRecreateParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
typedef ResourceResult (*FResourceRecreate)(const struct ResourceRecreateParams* params);

/*# get context from type
 * @name ResourceTypeGetContext
 * @param type [type: HResourceType] The type
 * @return context [type: void*] 0 if no context was registered
 */
void* ResourceTypeGetContext(HResourceType type);

/*# set context from type
 * @name ResourceTypeSetContext
 * @param type [type: HResourceType] The type
 * @param context [type: void*] The context to associate with the type
 */
void ResourceTypeSetContext(HResourceType type, void* context);

/*# get registered extension name of the type
 * @name ResourceTypeGetName
 * @param type [type: HResourceType] The type
 * @return name [type: const char*] The name of the type (e.g. "collectionc")
 */
const char* ResourceTypeGetName(HResourceType type);

/*# get registered extension name hash of the type
 * @name ResourceTypeGetNameHash
 * @param type [type: HResourceType] The type
 * @return hash [type: dmhash_t] The name hash of the type (e.g. "collectionc")
 */
dmhash_t ResourceTypeGetNameHash(HResourceType type);

/*# set preload function for type
 * @name ResourceTypeSetPreloadFn
 * @param type [type: HResourceType] The type
 * @param fn [type: FResourcePreload] Function to be called when loading of the resource starts
 */
void ResourceTypeSetPreloadFn(HResourceType type, FResourcePreload fn);

/*# set create function for type
 * @name ResourceTypeSetCreateFn
 * @param type [type: HResourceType] The type
 * @param fn [type: FResourceCreate] Function to be called to creating the resource
 */
void ResourceTypeSetCreateFn(HResourceType type, FResourceCreate fn);

/*# set post create function for type
 * @name ResourceTypeSetPostCreateFn
 * @param type [type: HResourceType] The type
 * @param fn [type: FResourcePostCreate] Function to be called after creating the resource
 */
void ResourceTypeSetPostCreateFn(HResourceType type, FResourcePostCreate fn);

/*# set destroy function for type
 * @name ResourceTypeSetDestroyFn
 * @param type [type: HResourceType] The type
 * @param fn [type: FResourceDestroy] Function to be called to destroy the resource
 */
void ResourceTypeSetDestroyFn(HResourceType type, FResourceDestroy fn);

/*# set recreate function for type
 * @name ResourceTypeSetRecreateFn
 * @param type [type: HResourceType] The type
 * @param fn [type: FResourceRecreate] Function to be called when recreating the resource
 */
void ResourceTypeSetRecreateFn(HResourceType type, FResourceRecreate fn);
// Enables streaming for a resource
void        ResourceTypeSetStreaming(HResourceType type, uint32_t preload_size);
bool        ResourceTypeIsStreaming(HResourceType type);
uint32_t    ResourceTypeGetPreloadSize(HResourceType type);

// internal
void ResourceTypeReset(HResourceType type);

/** (Internal for now)
 * Register a resource type
 * @name ResourceRegisterType
 * @param factory [type: HResourceFactory] Factory handle
 * @param extension [type: const char*] File extension for resource (e.g. "collectionc")
 * @param context [type: void*] User context for this file type
 * @param type [type: HResourceType*] (out) resource type. Valid if function returns RESOURCE_RESULT_OK
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
ResourceResult ResourceRegisterType(HResourceFactory factory,
                                   const char* extension,
                                   void* context,
                                   HResourceType* type);
// .end internal


/*#
 * Parameters to ResourcePreload function of the resource type
 * @name ResourcePreloadParams
 * @member m_Factory [type: HResourceFactory]
 * @member m_Context [type: void*] The context registered with the resource type
 * @member m_Filename [type: const char*] Path of the loaded file
 * @member m_Buffer [type: const void*] Buffer containing the loaded file
 * @member m_BufferSize [type: uint32_t] Size of data buffer (in bytes)
 * @member m_HintInfo [type: HResourcePreloadHintInfo] Hinter info. Use this when calling [ref:ResourcePreloadHint]
 * @member m_PreloadData [type: void**] User data that is set during the Preload phase, and passed to the Create function.
 * @member m_Type [type: HResourceType] The resource type
 */
struct ResourcePreloadParams
{
    HResourceFactory         m_Factory;
    void*                    m_Context;
    const char*              m_Filename;
    const void*              m_Buffer;
    uint32_t                 m_BufferSize;
    uint32_t                 m_FileSize:30;
    uint32_t                 m_IsBufferPartial:1;
    uint32_t                 m_IsBufferTransferrable:1; // Can the callback take ownership?
    HResourcePreloadHintInfo m_HintInfo;
    HResourceType            m_Type;
    // Out
    void**                   m_PreloadData;
    bool*                    m_IsBufferOwnershipTransferred; // If set, the resource type has taken ownership of the data
};

/*#
 * Parameters to ResourceCreate function of the resource type
 * @name ResourceCreateParams
 * @member m_Factory [type: HResourceFactory]
 * @member m_Context [type: void*] The context registered with the resource type
 * @member m_Filename [type: const char*] Path of the loaded file
 * @member m_Buffer [type: const void*] Buffer containing the loaded file
 * @member m_BufferSize [type: uint32_t] Size of data buffer (in bytes)
 * @member m_PreloadData [type: void*] Preloaded data from Preload phase.
 * @member m_Resource [type: HResourceDescriptor] The resource descriptor to update. Temporary, don't copy.
 * @member m_Type [type: HResourceType] The resource type
 */
struct ResourceCreateParams
{
    HResourceFactory    m_Factory;
    void*               m_Context;
    const char*         m_Filename;
    const void*         m_Buffer;
    uint32_t            m_BufferSize;
    uint32_t            m_FileSize:31;
    uint32_t            m_IsBufferPartial:1;
    void*               m_PreloadData;
    HResourceDescriptor m_Resource;
    HResourceType       m_Type;
};


/*#
 * Parameters to ResourcePostCreate function of the resource type
 * @name ResourcePostCreateParams
 * @member m_Factory [type: HResourceFactory]
 * @member m_Context [type: void*] The context registered with the resource type
 * @member m_Filename [type: const char*] Path of the loaded file
 * @member m_PreloadData [type: void*] Preloaded data from Preload phase.
 * @member m_Resource [type: HResourceDescriptor] The resource descriptor to update. Temporary, don't copy.
 * @member m_Type [type: HResourceType] The resource type
 */
struct ResourcePostCreateParams
{
    HResourceFactory    m_Factory;
    void*               m_Context;
    const char*         m_Filename;
    void*               m_PreloadData;
    HResourceDescriptor m_Resource;
    HResourceType       m_Type;
};

/*#
 * Parameters to ResourceRecreate function of the resource type
 * @name ResourceRecreateParams
 * @member m_Factory [type: HResourceFactory]
 * @member m_Context [type: void*] The context registered with the resource type
 * @member m_FilenameHash [type: dmhash_t] File name hash of the data
 * @member m_Filename [type: const char*] Path of the loaded file
 * @member m_Buffer [type: const void*] Buffer containing the loaded file
 * @member m_BufferSize [type: uint32_t] Size of data buffer (in bytes)
 * @member m_Message [type: const void*] Pointer holding a precreated message
 * @member m_Resource [type: HResourceDescriptor] The resource descriptor to update. Temporary, don't copy.
 * @member m_Type [type: HResourceType] The resource type
 */
struct ResourceRecreateParams
{
    HResourceFactory    m_Factory;
    void*               m_Context;
    dmhash_t            m_FilenameHash;
    const char*         m_Filename;
    const void*         m_Buffer;
    uint32_t            m_BufferSize;
    uint32_t            m_FileSize:31;
    uint32_t            m_IsBufferPartial:1;
    const void*         m_Message;
    HResourceDescriptor m_Resource;
    HResourceType       m_Type;
};

/*#
 * Parameters to ResourceDestroy function of the resource type
 * @name ResourceDestroyParams
 * @member m_Factory [type: HResourceFactory]
 * @member m_Context [type: void*] The context registered with the resource type
 * @member m_Resource [type: HResourceDescriptor] The resource descriptor to destroy
 * @member m_Type [type: HResourceType] The resource type
 */
struct ResourceDestroyParams
{
    HResourceFactory    m_Factory;
    void*               m_Context;
    HResourceDescriptor m_Resource;
    HResourceType       m_Type;
};


/*#
 * Parameters to ResourceReloaded function of the resource type
 * @name ResourceReloadedParams
 * @member m_UserData [type: void*] User data supplied when the callback was registered
 * @member m_FilenameHash [type: dmhash_t] File name hash of the data
 * @member m_Filename [type: const char*] Path of the resource, same as provided to Get() when the resource was obtained
 * @member m_Resource [type: HResourceDescriptor] The resource descriptor to update. Temporary, don't copy.
 * @member m_Type [type: HResourceType] The resource type
 */
typedef struct ResourceReloadedParams
{
    void*               m_UserData;
    const char*         m_Filename;
    uint64_t            m_FilenameHash;
    HResourceDescriptor m_Resource;
    HResourceType       m_Type;
} ResourceReloadedParams;


// TYPE REGISTERING MACROS


/*#
 * Resource type creator desc byte size declaration.
 * The registered description data passeed to ResourceRegisterTypeCreatorDesc must be of at least this size.
 * @variable
 * @name ResourceTypeCreatorDescBufferSize
 */
const uint32_t ResourceTypeCreatorDescBufferSize = 128;

/** Internal
* Resource type creator desc bytesize declaration.
* @name ResourceRegisterTypeCreatorDesc
* @param desc [type: void*] Pointer to allocated area.
*                           Must be valid throughout the application life cycle.
* @param size [type: uint32_t] Size of desc. Must be at least ResourceTypeCreatorDescBufferSize bytes
* @param name [type: const char*] Name of resoruce type (e.g. "collectionc")
* @param register_fn [type: FResourceTypeRegister] Type register function. Called at each reboot
* @param deregister_fn [type: FResourceTypeDeregister] Type deregister function. Called at each reboot
*/
void ResourceRegisterTypeCreatorDesc(void* desc, uint32_t size, const char *name, FResourceTypeRegister register_fn, FResourceTypeDeregister deregister_fn);

// Internal
#define DM_RESOURCE_PASTE_SYMREG(x, y) x ## y
// Internal
#define DM_RESOURCE_PASTE_SYMREG2(x, y) DM_RESOURCE_PASTE_SYMREG(x, y)

// Internal. Resource type declaration helper.
#define DM_REGISTER_RESOURCE_TYPE(symbol, desc, desc_size, suffix, register_fn, deregister_fn) extern "C" void symbol () { \
    ResourceRegisterTypeCreatorDesc((void*) &desc, desc_size, suffix, register_fn, deregister_fn); \
}


/*# declare a new resource type
 *
 * Declare and register new resource type to the engine.
 * This macro is used to declare the resource type callback functions used by the engine to communicate with the extension.
 *
 * @macro
 * @name DM_DECLARE_RESOURCE_TYPE
 * @param symbol [type:symbol] external extension symbol description (no quotes).
 * @param suffix [type:const char*] The file resource suffix, without a ".".
 * @param register_fn [type:function(dmResource::ResourceTypeRegisterContext& ctx)] type register function
 *
 * `ctx`
 * : [type:dmResource::ResourceTypeRegisterContext&] Reference to an `ResourceTypeRegisterContext` structure.
 *
 * @param deregister_fn [type:function(dmResource::ResourceTypeRegisterContext& ctx)] type deregister function. May be null.
 *
 * `ctx`
 * : [type:dmResource::ResourceTypeRegisterContext&] Reference to an `ResourceTypeRegisterContext` structure.
 *
 *
 * @examples
 *
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
 * static ResourceResult RegisterResourceTypeBlob(HResourceTypeContext ctx, HResourceType type)
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
 * static ResourceResult DeregisterResourceTypeBlob(HResourceTypeContext ctx, HResourceType type)
 * {
 *     MyContext* context = (MyContext*)ResourceTypeGetContext(type);
 *     delete context;
 * }
 *
 * DM_DECLARE_RESOURCE_TYPE(ResourceTypeBlob, "blobc", RegisterResourceTypeBlob, DeregisterResourceTypeBlob);
 * ```
 */
#define DM_DECLARE_RESOURCE_TYPE(symbol, suffix, register_fn, deregister_fn) \
    uint8_t DM_ALIGNED(16) DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__)[ResourceTypeCreatorDescBufferSize]; \
    DM_REGISTER_RESOURCE_TYPE(symbol, DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__), sizeof(DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__)), suffix, register_fn, deregister_fn);

#if defined(__cplusplus)
    #include "resource.hpp"
#endif

#endif // DMSDK_RESOURCE_H
