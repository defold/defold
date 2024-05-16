/*# Resource
 *
 * Functions for managing resource types.
 *
 * @document
 * @name Resource
 * @path engine/resource/src/dmsdk/resource/resource.h
 */

/////////////////////////////////////////////////////////////
// Resource type life registering and callbacks

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


/*#
 * Resource type setup function.
 * @note The type is already cerate, and name and name hash properties are valid to get using the RsourceTypeGetName()/RsourceTypeGetNameHash() functions
 * @typedef
 * @name FResourceTypeRegister
 * @params ctx [type: HResourceTypeContext] Context for resource types
 * @params type [type: HResourceType] The registered resource type.
 * @return RESOURCE_RESULT_OK on success
 */

/*#
 * Resource type destroy function. Generally used to destroy the registered resource type context.
 * @typedef
 * @name FResourceTypeDeregister
 * @params ctx [type: HResourceTypeContext] Context for resource types
 * @params type [type: HResourceType] The registered resource type.
 * @return RESOURCE_RESULT_OK on success
 */

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

/*#
 * Resource create function
 * @typedef
 * @name FResourceCreate
 * @param param [type: const ResourceCreateParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */

/*#
 * Resource postcreate function
 * @note returning RESOURCE_CREATE_RESULT_PENDING will result in a repeated callback the following update.
 * @typedef
 * @name FResourcePostCreate
 * @param param [type: const ResourcePostCreateParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_CREATE_RESULT_OK on success or RESOURCE_CREATE_RESULT_PENDING when pending
 */

/*#
 * Resource destroy function
 * @typedef
 * @name FResourceDestroy
 * @param param [type: const ResourceDestroyParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */

/*#
 * Resource recreate function. Recreate resource in-place.
 * @note Beware that any "in flight" resource pointers to the actual resource must remain valid after this call.
 * @typedef
 * @name FResourceRecreate
 * @param param [type: const ResourceRecreateParams*] Resource parameters
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */

/////////////////////////////////////////////////////////////
// Resource type

/** (Internal for now)
 * Register a resource type
 * @name ResourceRegisterType
 * @param factory [type: HResourceFactory] Factory handle
 * @param extension [type: const char*] File extension for resource (e.g. "collectionc")
 * @param context [type: void*] User context for this file type
 * @param type [type: HResourceType*] (out) resource type. Valid if function returns RESOURCE_RESULT_OK
 * @return result [type: ResourceResult] RESOURCE_RESULT_OK on success
 */

/*# get context from type
 * @name ResourceTypeGetContext
 * @param type [type: HResourceType] The type
 * @return context [type: void*] 0 if no context was registered
 */

/*# set context from type
 * @name ResourceTypeSetContext
 * @param type [type: HResourceType] The type
 * @param context [type: void*] The context to associate with the type
 */

/*# get registered extension name of the type
 * @name ResourceTypeGetName
 * @param type [type: HResourceType] The type
 * @return name [type: const char*] The name of the type (e.g. "collectionc")
 */

/*# get registered extension name hash of the type
 * @name ResourceTypeGetNameHash
 * @param type [type: HResourceType] The type
 * @return hash [type: dmhash_t] The name hash
 */

/*# set preload function for type
 * @name ResourceTypeSetPreloadFn
 * @param type [type: HResourceType] The type
 * @param fn [type: FResourcePreload] Function to be called when loading of the resource starts
 */

/*# set create function for type
 * @name ResourceTypeSetCreateFn
 * @param type [type: HResourceType] The type
 * @param fn [type: FResourceCreate] Function to be called to creating the resource
 */

/*# set post create function for type
 * @name ResourceTypeSetPostCreateFn
 * @param type [type: HResourceType] The type
 * @param fn [type: FResourcePostCreate] Function to be called after creating the resource
 */

/*# set destroy function for type
 * @name ResourceTypeSetDestroyFn
 * @param type [type: HResourceType] The type
 * @param fn [type: FResourceDestroy] Function to be called to destroy the resource
 */

/*# set recreate function for type
 * @name ResourceTypeSetRecreateFn
 * @param type [type: HResourceType] The type
 * @param fn [type: FResourceRecreate] Function to be called when recreating the resource
 */

/////////////////////////////////////////////////////////////
// Resource descriptors

/*# get path hash of resource
 * @name ResourceDescriptorGetNameHash
 * @param rd [type: HResourceDescriptor] The resource
 * @return hash [type: dmhash_t] The path hash
 */

/*# set the resource data
 * @name ResourceDescriptorSetResource
 * @param rd [type: HResourceDescriptor] The resource handle
 * @param resource [type: void*] The resource data
 */

/*# get the resource data
 * @name ResourceDescriptorGetResource
 * @param rd [type: HResourceDescriptor] The resource handle
 * @return resource [type: void*] The resource data
 */

/*# set the resource data size
 * @name ResourceDescriptorSetResourceSize
 * @param rd [type: HResourceDescriptor] The resource handle
 * @param size [type: uint32_t] The resource data size (in bytes)
 */

/*# get the resource data size
 * @name ResourceDescriptorGetResourceSize
 * @param rd [type: HResourceDescriptor] The resource handle
 * @return size [type: uint32_t] The resource data size (in bytes)
 */

/*# set the previous resource data
 * @note only used when recreating a resource
 * @name ResourceDescriptorSetPrevResource
 * @param rd [type: HResourceDescriptor] The resource handle
 * @param resource [type: void*] The resource data
 */

/*# get the previous resource data
 * @note only used when recreating a resource
 * @name ResourceDescriptorGetPrevResource
 * @param rd [type: HResourceDescriptor] The resource handle
 * @return resource [type: void*] The resource data
 */

/*# get the resource type
 * @name ResourceDescriptorGetType
 * @param rd [type: HResourceDescriptor] The resource handle
 * @return resource [type: HResourceType] The resource type
 */


/////////////////////////////////////////////////////////////
