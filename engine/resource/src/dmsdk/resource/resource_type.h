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

#ifndef DMSDK_RESOURCE_TYPE_H
#define DMSDK_RESOURCE_TYPE_H

/*# Resource type
 *
 * Functions for managing resource types.
 *
 * @document
 * @name ResourceType
 * @path engine/resource/src/dmsdk/resource/resource_type.h
 */

#include <stdint.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/align.h> // for DM_ALIGNED macro
#include <dmsdk/resource/resource.h>
#include <dmsdk/resource/resource_params.h>


typedef struct ResourceTypeContext* HResourceTypeContext;
typedef struct ResourceType*        HResourceType;

void*            ResourceTypeContextGetContextByHash(HResourceTypeContext, dmhash_t);

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


/**
* Resource type creator desc bytesize declaration. Internal
*/
const uint32_t ResourceTypeCreatorDescBufferSize = 128;

/** (internal)
* Resource type creator desc bytesize declaration. Internal
* @name ResourceRegisterTypeCreatorDesc
* @param desc [type: void*] Pointer to allocated area.
*                           Must be valid throughout the application life cycle.
* @param size [type: uint32_t] Size of desc. Must be at least ResourceTypeCreatorDescBufferSize bytes
* @param name [type: const char*] Name of resoruce type (e.g. "collectionc")
* @param register_fn [type: FResourceTypeRegister] Type register function. Called at each reboot
* @param deregister_fn [type: FResourceTypeDeregister] Type deregister function. Called at each reboot
*/
void ResourceRegisterTypeCreatorDesc(void* desc, uint32_t size, const char *name, FResourceTypeRegister register_fn, FResourceTypeDeregister deregister_fn);

// internal
#define DM_RESOURCE_PASTE_SYMREG(x, y) x ## y
// internal
#define DM_RESOURCE_PASTE_SYMREG2(x, y) DM_RESOURCE_PASTE_SYMREG(x, y)

/**
 * Resource type declaration helper. Internal
 */
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
    uint8_t DM_ALIGNED(16) DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__)[dmResource::s_ResourceTypeCreatorDescBufferSize]; \
    DM_REGISTER_RESOURCE_TYPE(symbol, DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__), sizeof(DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__)), suffix, register_fn, deregister_fn);

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Type functions

/*#
 * Resource preloading function. This may be called from a separate loading thread
 * but will not keep any mutexes held while executing the call. During this call
 * PreloadHint can be called with the supplied hint_info handle.
 * If RESULT_OK is returned, the resource Create function is guaranteed to be called
 * with the preload_data value supplied.
 *
 * @typedef
 * @name FResourcePreload
 * @param param [type: const ResourcePreloadParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
typedef ResourceResult (*FResourcePreload)(const struct ResourcePreloadParams* params);

/*#
 * Resource create function
 * @typedef
 * @name FResourceCreate
 * @param param [type: const ResourceCreateParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
typedef ResourceResult (*FResourceCreate)(const struct ResourceCreateParams* params);

/*#
 * Resource postcreate function
 * @note returning RESOURCE_CREATE_RESULT_PENDING will result in a repeated callback the following update.
 * @typedef
 * @name FResourcePostCreate
 * @param param [type: const ResourcePostCreateParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_CREATE_RESULT_OK on success or RESOURCE_CREATE_RESULT_PENDING when pending
 */
typedef ResourceResult (*FResourcePostCreate)(const struct ResourcePostCreateParams* params);

/*#
 * Resource destroy function
 * @typedef
 * @name FResourceDestroy
 * @param param [type: const ResourceDestroyParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
typedef ResourceResult (*FResourceDestroy)(const struct ResourceDestroyParams* params);


/*#
 * Resource recreate function. Recreate resource in-place.
 * @note Beware that any "in flight" resource pointers to the actual resource must remain valid after this call.
 * @typedef
 * @name FResourceRecreate
 * @param param [type: const ResourceRecreateParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */
typedef ResourceResult (*FResourceRecreate)(const struct ResourceRecreateParams* params);


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
 * @return hash [type: dmhash_t] The name hash
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

#endif // DMSDK_RESOURCE_TYPE_H
