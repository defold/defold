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
 * @path engine/resource/src/dmsdk/resource/resource.h
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
 * @param params Parameters
 * @see RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT (internal flag)
 * @see [ref:ResourceRegisterReloadedCallback]
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
 * @see RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT (internal flag)
 */
void ResourceRegisterReloadedCallback(HResourceFactory factory, FResourceReloadedCallback callback, void* user_data);

/**
 * Remove a registered callback function, O(n).
 * @name ResourceUnregisterReloadedCallback
 * @param factory Handle of the factory from which the callback will be removed
 * @param callback Callback function to remove
 * @param user_data User data that was supplied when the callback was registered
 * @see RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT (internal flag)
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
 * @note Decreases ref count by 1. If it reaches 0, the resource destroy function is called.
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
    HResourcePreloadHintInfo m_HintInfo;
    void**                   m_PreloadData;
    HResourceType            m_Type;
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
 * @member m_Resource [type: HResourceDescriptor] The resource descriptor to update.
 * @member m_Type [type: HResourceType] The resource type
 */
struct ResourceCreateParams
{
    HResourceFactory    m_Factory;
    void*               m_Context;
    const char*         m_Filename;
    const void*         m_Buffer;
    uint32_t            m_BufferSize;
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
 * @member m_Resource [type: HResourceDescriptor] The resource descriptor to update.
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
 * @member m_Resource [type: HResourceDescriptor] The resource descriptor to update
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
 * @member m_Resource [type: HResourceDescriptor] The resource descriptor to update
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
#define DM_DECLARE_RESOURCE_TYPE(symbol, suffix, register_fn, deregister_fn) \
    uint8_t DM_ALIGNED(16) DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__)[ResourceTypeCreatorDescBufferSize]; \
    DM_REGISTER_RESOURCE_TYPE(symbol, DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__), sizeof(DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__)), suffix, register_fn, deregister_fn);

#if defined(__cplusplus)
    #include "resource.hpp"
#endif

#endif // DMSDK_RESOURCE_H
