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
#include <dmsdk/dlib/align.h> // DM_ALIGNED

typedef struct ResourceFactory* HResourceFactory;
typedef struct ResourcePreloadHintInfo* HResourcePreloadHintInfo;
typedef struct ResourceDescriptor* HResourceDescriptor;
typedef struct ResourceTypeContext* HResourceTypeContext;
typedef struct ResourceType* HResourceType;

struct ResourceReloadedParams;
struct ResourcePreloadParams;
struct ResourceCreateParams;
struct ResourcePostCreateParams;
struct ResourceDestroyParams;
struct ResourceRecreateParams;

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


////////////////////////////////////////////////////////////////////////////////////////////////////////
// Descriptor functions

dmhash_t        ResourceDescriptorGetNameHash(HResourceDescriptor rd);
void            ResourceDescriptorSetResource(HResourceDescriptor rd, void* resource);
void*           ResourceDescriptorGetResource(HResourceDescriptor rd);
void            ResourceDescriptorSetPrevResource(HResourceDescriptor rd, void* resource);
void*           ResourceDescriptorGetPrevResource(HResourceDescriptor rd);
void            ResourceDescriptorSetResourceSize(HResourceDescriptor rd, uint32_t size);
uint32_t        ResourceDescriptorGetResourceSize(HResourceDescriptor rd);
HResourceType   ResourceDescriptorGetType(HResourceDescriptor rd);


////////////////////////////////////////////////////////////////////////////////////////////////////////
// Type functions

void*  ResourceTypeContextGetContextByHash(HResourceTypeContext, dmhash_t extension_hash);

typedef ResourceResult (*FResourceTypeRegister)(HResourceTypeContext ctx, HResourceType type);
typedef ResourceResult (*FResourceTypeDeregister)(HResourceTypeContext ctx, HResourceType type);
typedef ResourceResult (*FResourcePreload)(const struct ResourcePreloadParams* params);
typedef ResourceResult (*FResourceCreate)(const struct ResourceCreateParams* params);
typedef ResourceResult (*FResourcePostCreate)(const struct ResourcePostCreateParams* params);
typedef ResourceResult (*FResourceDestroy)(const struct ResourceDestroyParams* params);
typedef ResourceResult (*FResourceRecreate)(const struct ResourceRecreateParams* params);

void* ResourceTypeGetContext(HResourceType type);
void ResourceTypeSetContext(HResourceType type, void* context);
const char* ResourceTypeGetName(HResourceType type);
dmhash_t ResourceTypeGetNameHash(HResourceType type);
void ResourceTypeSetPreloadFn(HResourceType type, FResourcePreload fn);
void ResourceTypeSetCreateFn(HResourceType type, FResourceCreate fn);
void ResourceTypeSetPostCreateFn(HResourceType type, FResourcePostCreate fn);
void ResourceTypeSetDestroyFn(HResourceType type, FResourceDestroy fn);
void ResourceTypeSetRecreateFn(HResourceType type, FResourceRecreate fn);

// internal
ResourceResult ResourceRegisterType(HResourceFactory factory,
                                   const char* extension,
                                   void* context,
                                   HResourceType* type);
// .end internal


/*#
 * Parameters to ResourcePreload callback.
 */
struct ResourcePreloadParams
{
    /// Factory handle
    HResourceFactory m_Factory;
    /// Resource context
    void* m_Context;
    /// File name of the loaded file
    const char* m_Filename;
    /// Buffer containing the loaded file
    const void* m_Buffer;
    /// Size of data buffer
    uint32_t m_BufferSize;
    /// Hinter info. Use this when calling PreloadHint
    HResourcePreloadHintInfo m_HintInfo;
    /// User data from the preload step, that will be passed on to ResourceCreate function
    void** m_PreloadData;
    /// the resource type
    HResourceType m_Type;
};

/*#
 * Parameters to ResourceCreate callback.
 */
struct ResourceCreateParams
{
    /// Factory handle
    HResourceFactory m_Factory;
    /// Resource context
    void* m_Context;
    /// File name of the loaded file
    const char* m_Filename;
    /// Buffer containing the loaded file
    const void* m_Buffer;
    /// Size of the data buffer
    uint32_t m_BufferSize;
    /// Preloaded data from Preload phase
    void* m_PreloadData;
    /// Resource descriptor to fill in
    HResourceDescriptor m_Resource;
    /// the resource type
    HResourceType m_Type;
};


/*#
 * Parameters to ResourcePostCreate callback.
 */
struct ResourcePostCreateParams
{
    /// Factory handle
    HResourceFactory m_Factory;
    /// Resource context
    void* m_Context;
    /// Preloaded data from Preload phase
    void* m_PreloadData;
    /// Resource descriptor passed from create function
    HResourceDescriptor m_Resource;
    /// the resource type
    HResourceType m_Type;
    /// File name of the loaded file
    const char* m_Filename;
};

/*#
 * Parameters to ResourceRecreate callback.
 */
struct ResourceRecreateParams
{
    /// Factory handle
    HResourceFactory m_Factory;
    /// Resource context
    void* m_Context;
    /// File name hash of the data
    uint64_t m_NameHash;
    /// File name of the loaded file
    const char* m_Filename;
    /// Data buffer containing the loaded file
    const void* m_Buffer;
    /// Size of data buffer
    uint32_t m_BufferSize;
    /// Pointer holding a precreated message
    const void* m_Message;
    /// Resource descriptor to write into
    HResourceDescriptor m_Resource;
    /// the resource type
    HResourceType m_Type;
};

/*#
 * Parameters to ResourceDestroy callback.
 */
struct ResourceDestroyParams
{
    /// Factory handle
    HResourceFactory m_Factory;
    /// Resource context
    void* m_Context;
    /// Resource descriptor for resource to destroy
    HResourceDescriptor m_Resource;
    /// the resource type
    HResourceType m_Type;
};

/*#
 * Parameters to ResourceReloaded callback.
 */
typedef struct ResourceReloadedParams
{
    /// User data supplied when the callback was registered
    void* m_UserData;
    /// Descriptor of the reloaded resource
    HResourceDescriptor m_Resource;
    /// Name of the resource, same as provided to Get() when the resource was obtained
    const char* m_Name;
    /// Hashed name of the resource
    uint64_t m_NameHash;
    /// the resource type
    HResourceType m_Type;
} ResourceReloadedParams;


// TYPE REGISTERING MACROS


// Internal. Resource type creator desc bytesize declaration.
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

// public
#define DM_DECLARE_RESOURCE_TYPE(symbol, suffix, register_fn, deregister_fn) \
    uint8_t DM_ALIGNED(16) DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__)[ResourceTypeCreatorDescBufferSize]; \
    DM_REGISTER_RESOURCE_TYPE(symbol, DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__), sizeof(DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__)), suffix, register_fn, deregister_fn);


#endif // DMSDK_RESOURCE_H
