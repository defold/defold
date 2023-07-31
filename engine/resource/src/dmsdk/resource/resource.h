// Copyright 2020-2023 The Defold Foundation
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


#include <stdint.h>
#include <dmsdk/dlib/align.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/hashtable.h>

namespace dmResource
{
    /**
     * Factory handle
     */
    typedef struct SResourceFactory* HFactory;

    typedef struct ResourcePreloader* HPreloader;
    typedef struct PreloadHintInfo* HPreloadHintInfo;

    typedef struct SResourceDescriptor* HResourceDescriptor;

    /** result
     * Result
     * @enum
     * @name Result
     * @member dmResource::RESULT_OK
     * @member dmResource::RESULT_INVALID_DATA
     * @member dmResource::RESULT_DDF_ERROR
     * @member dmResource::RESULT_RESOURCE_NOT_FOUND
     * @member dmResource::RESULT_MISSING_FILE_EXTENSION
     * @member dmResource::RESULT_ALREADY_REGISTERED
     * @member dmResource::RESULT_INVAL
     * @member dmResource::RESULT_UNKNOWN_RESOURCE_TYPE
     * @member dmResource::RESULT_OUT_OF_MEMORY
     * @member dmResource::RESULT_IO_ERROR
     * @member dmResource::RESULT_NOT_LOADED
     * @member dmResource::RESULT_OUT_OF_RESOURCES
     * @member dmResource::RESULT_STREAMBUFFER_TOO_SMALL
     * @member dmResource::RESULT_FORMAT_ERROR
     * @member dmResource::RESULT_CONSTANT_ERROR
     * @member dmResource::RESULT_NOT_SUPPORTED
     * @member dmResource::RESULT_RESOURCE_LOOP_ERROR
     * @member dmResource::RESULT_PENDING
     * @member dmResource::RESULT_INVALID_FILE_EXTENSION
     * @member dmResource::RESULT_VERSION_MISMATCH
     * @member dmResource::RESULT_SIGNATURE_MISMATCH
     * @member dmResource::RESULT_UNKNOWN_ERROR
     */
    enum Result
    {
        RESULT_OK                        = 0,
        RESULT_INVALID_DATA              = -1,
        RESULT_DDF_ERROR                 = -2,
        RESULT_RESOURCE_NOT_FOUND        = -3,
        RESULT_MISSING_FILE_EXTENSION    = -4,
        RESULT_ALREADY_REGISTERED        = -5,
        RESULT_INVAL                     = -6,
        RESULT_UNKNOWN_RESOURCE_TYPE     = -7,
        RESULT_OUT_OF_MEMORY             = -8,
        RESULT_IO_ERROR                  = -9,
        RESULT_NOT_LOADED                = -10,
        RESULT_OUT_OF_RESOURCES          = -11,
        RESULT_STREAMBUFFER_TOO_SMALL    = -12,
        RESULT_FORMAT_ERROR              = -13,
        RESULT_CONSTANT_ERROR            = -14,
        RESULT_NOT_SUPPORTED             = -15,
        RESULT_RESOURCE_LOOP_ERROR       = -16,
        RESULT_PENDING                   = -17,
        RESULT_INVALID_FILE_EXTENSION    = -18,
        RESULT_VERSION_MISMATCH          = -19,
        RESULT_SIGNATURE_MISMATCH        = -20,
        RESULT_UNKNOWN_ERROR             = -21,
    };

    /**
     * Parameters to ResourcePreload callback.
     */
    struct ResourcePreloadParams
    {
        /// Factory handle
        HFactory m_Factory;
        /// Resource context
        void* m_Context;
        /// File name of the loaded file
        const char* m_Filename;
        /// Buffer containing the loaded file
        const void* m_Buffer;
        /// Size of data buffer
        uint32_t m_BufferSize;
        /// Hinter info. Use this when calling PreloadHint
        HPreloadHintInfo m_HintInfo;
        /// Writable user data that will be passed on to ResourceCreate function
        void** m_PreloadData;
    };

    /*#
     * Resource descriptor
     * @name SResourceDescriptor
     * @member m_NameHash [type: uint64_t] Hash of resource name
     * @member m_Resource [type: void*] Resource pointer. Must be unique and not NULL.
     * @member m_PrevResource [type: void*] Resource pointer. Resource pointer to a previous version of the resource, iff it exists. Only used when recreating resources.
     * @member m_ResourceSize [type: uint32_t] Resource size in memory. I.e. the payload of m_Resource
     */
    struct SResourceDescriptor
    {
        /// Hash of resource name
        uint64_t m_NameHash;
        /// Resource pointer. Must be unique and not NULL.
        void*    m_Resource;
        /// Resource pointer to a previous version of the resource, iff it exists. Only used when recreating resources.
        void*    m_PrevResource;
        /// Resource size in memory. I.e. the payload of m_Resource
        uint32_t m_ResourceSize;

        // private members
        uint32_t m_ResourceSizeOnDisc;
        void*    m_ResourceType;
        uint32_t m_ReferenceCount;
    };


    /**
     * Resource preloading function. This may be called from a separate loading thread
     * but will not keep any mutexes held while executing the call. During this call
     * PreloadHint can be called with the supplied hint_info handle.
     * If RESULT_OK is returned, the resource Create function is guaranteed to be called
     * with the preload_data value supplied.
     * @param param Resource preloading parameters
     * @return RESULT_OK on success
     */
    typedef Result (*FResourcePreload)(const ResourcePreloadParams& params);

    /**
     * Parameters to ResourceCreate callback.
     */
    struct ResourceCreateParams
    {
        /// Factory handle
        HFactory m_Factory;
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
    };

    /**
     * Resource create function
     * @param params Resource creation arguments
     * @return CREATE_RESULT_OK on success
     */
    typedef Result (*FResourceCreate)(const ResourceCreateParams& params);

    /**
     * Parameters to ResourcePostCreate callback.
     */
    struct ResourcePostCreateParams
    {
        /// Factory handle
        HFactory m_Factory;
        /// Resource context
        void* m_Context;
        /// Preloaded data from Preload phase
        void* m_PreloadData;
        /// Resource descriptor passed from create function
        HResourceDescriptor m_Resource;
    };

    /**
     * Resource postcreate function
     * @param params Resource postcreation arguments
     * @return CREATE_RESULT_OK on success or CREATE_RESULT_PENDING when pending
     * @note returning CREATE_RESULT_PENDING will result in a repeated callback the following update.
     */
    typedef Result (*FResourcePostCreate)(const ResourcePostCreateParams& params);

    /**
     * Parameters to ResourceDestroy callback.
     */
    struct ResourceDestroyParams
    {
        /// Factory handle
        HFactory m_Factory;
        /// Resource context
        void* m_Context;
        /// Resource descriptor for resource to destroy
        HResourceDescriptor m_Resource;
    };

    /**
     * Resource destroy function
     * @param params Resource destruction arguments
     * @return CREATE_RESULT_OK on success
     */
    typedef Result (*FResourceDestroy)(const ResourceDestroyParams& params);

    /**
     * Parameters to ResourceRecreate callback.
     */
    struct ResourceRecreateParams
    {
        /// Factory handle
        HFactory m_Factory;
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
    };

    /**
     * Resource recreate function. Recreate resource in-place.
     * @params params Parameters for resource creation
     * @return CREATE_RESULT_OK on success
     */
    typedef Result (*FResourceRecreate)(const ResourceRecreateParams& params);


    /**
     * Register a resource type
     * @param factory Factory handle
     * @param extension File extension for resource
     * @param context User context
     * @param preload_function Preload function. Optional, 0 if no preloading is used
     * @param create_function Create function pointer
     * @param post_create_function Post create function pointer
     * @param destroy_function Destroy function pointer
     * @param recreate_function Recreate function pointer. Optional, 0 if recreate is not supported.
     * @return RESULT_OK on success
     */
    Result RegisterType(HFactory factory,
                               const char* extension,
                               void* context,
                               FResourcePreload preload_function,
                               FResourceCreate create_function,
                               FResourcePostCreate post_create_function,
                               FResourceDestroy destroy_function,
                               FResourceRecreate recreate_function);


    /**
     * Parameters to ResourceReloaded callback.
     */
    struct ResourceReloadedParams
    {
        /// User data supplied when the callback was registered
        void* m_UserData;
        /// Descriptor of the reloaded resource
        HResourceDescriptor m_Resource;
        /// Name of the resource, same as provided to Get() when the resource was obtained
        const char* m_Name;
        /// Hashed name of the resource
        uint64_t m_NameHash;
    };

    /**
     * Function called when a resource has been reloaded.
     * @param params Parameters
     * @see RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT
     * @see RegisterResourceReloadedCallback
     * @see Get
     */
    typedef void (*ResourceReloadedCallback)(const ResourceReloadedParams& params);

    /**
     * Register a callback function that will be called with the specified user data when a resource has been reloaded.
     * The callbacks will not necessarily be called in the order they were registered.
     * This has only effect when reloading is supported.
     * @param factory Handle of the factory to which the callback will be registered
     * @param callback Callback function to register
     * @param user_data User data that will be supplied to the callback when it is called
     * @see RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT
     */
    void RegisterResourceReloadedCallback(HFactory factory, ResourceReloadedCallback callback, void* user_data);

    /**
     * Remove a registered callback function, O(n).
     * @param factory Handle of the factory from which the callback will be removed
     * @param callback Callback function to remove
     * @param user_data User data that was supplied when the callback was registered
     * @see RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT
     */
    void UnregisterResourceReloadedCallback(HFactory factory, ResourceReloadedCallback callback, void* user_data);

    dmhash_t GetNameHash(HResourceDescriptor desc);
    void*    GetResource(HResourceDescriptor desc);
    void*    GetPreviousResource(HResourceDescriptor desc);
    void     SetResource(HResourceDescriptor desc, void* resource);
    void     SetResourceSize(HResourceDescriptor desc, uint32_t size);



    /**
     * Parameters to ResourceRecreate callback.
     */
    struct ResourceTypeRegisterContext
    {
        HFactory m_Factory;
        const char* m_Name; // the name/suffix of the resource type
        uint64_t m_NameHash;
        dmHashTable64<void*>* m_Contexts;
    };

    /**
     * Resource recreate function. Recreate resource in-place.
     * @params params Parameters for resource creation
     * @return CREATE_RESULT_OK on success
     */
    typedef Result (*FResourceTypeRegister)(ResourceTypeRegisterContext& ctx);


    /**
     * Resource type deregister function.
     * @params ctx Parameters for resource creation
     * @return CREATE_RESULT_OK on success
     */
    typedef Result (*FResourceTypeDeregister)(ResourceTypeRegisterContext& ctx);


    /**
    * Resource type creator desc bytesize declaration. Internal
    */
    const size_t s_ResourceTypeCreatorDescBufferSize = 128;

    struct TypeCreatorDesc;
    void RegisterTypeCreatorDesc(TypeCreatorDesc* desc, uint32_t size, const char *name, FResourceTypeRegister register_fn, FResourceTypeDeregister deregister_fn);

    // internal
    #define DM_RESOURCE_PASTE_SYMREG(x, y) x ## y
    // internal
    #define DM_RESOURCE_PASTE_SYMREG2(x, y) DM_RESOURCE_PASTE_SYMREG(x, y)

    /**
     * Resource type declaration helper. Internal
     */
    #ifdef __GNUC__
        // Workaround for dead-stripping on OSX/iOS. The symbol "name" is explicitly exported. See wscript "exported_symbols"
        // Otherwise it's dead-stripped even though -no_dead_strip_inits_and_terms is passed to the linker
        // The bug only happens when the symbol is in a static library though
        #define DM_REGISTER_RESOURCE_TYPE(symbol, desc, desc_size, suffix, register_fn, deregister_fn) extern "C" void __attribute__((constructor)) symbol () { \
            dmResource::RegisterTypeCreatorDesc((struct dmResource::TypeCreatorDesc*) &desc, desc_size, suffix, register_fn, deregister_fn); \
        }
    #else
        #define DM_REGISTER_RESOURCE_TYPE(symbol, desc, desc_size, suffix, register_fn, deregister_fn) extern "C" void symbol () { \
            dmResource::RegisterTypeCreatorDesc((struct dmResource::TypeCreatorDesc*) &desc, desc_size, suffix, register_fn, deregister_fn); \
            }\
            int symbol ## Wrapper(void) { symbol(); return 0; } \
            __pragma(section(".CRT$XCU",read)) \
            __declspec(allocate(".CRT$XCU")) int (* _Fp ## symbol)(void) = symbol ## Wrapper;
    #endif

    /*# declare a new extension
     *
     * Declare and register new extension to the engine.
     * This macro is used to declare the extension callback functions used by the engine to communicate with the extension.
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
     * static dmResource::Result ResourceTypeScriptCreate(...) {}
     * static dmResource::Result ResourceTypeScriptDestroy(...) {}
     * static dmResource::Result ResourceTypeScriptRecreate(...) {}
     *
     * struct BlobContext
     * {
     *     ...
     * };
     *
     * static dmResource::Result RegisterResourceTypeBlob(ResourceTypeRegisterContext& ctx)
     * {
     *     // The engine.cpp creates the contexts for our built in types.
     *     // Here we register a custom type
     *     BlobContext* context = new BlobContext;
     *     ctx.m_Contexts.Put(ctx.m_NameHash, (void*)context);
     *
     *     return dmResource::RegisterType(ctx.m_Factory,
     *                                        ctx.m_Name,
     *                                        context,
     *                                        0,
     *                                        ResourceTypeScriptCreate,
     *                                        0,
     *                                        ResourceTypeScriptDestroy,
     *                                        ResourceTypeScriptRecreate);
     * }
     *
     * static dmResource::Result DeregisterResourceTypeScript(ResourceTypeRegisterContext& ctx)
     * {
     *     BlobContext** context = (BlobContext**)ctx.m_Contexts.Get(ctx.m_NameHash);
     *     delete *context;
     * }
     *
     *
     * DM_DECLARE_RESOURCE_TYPE(ResourceTypeBlob, "blobc", RegisterResourceTypeBlob, DeregisterResourceTypeScript);
     * ```
     */
    #define DM_DECLARE_RESOURCE_TYPE(symbol, suffix, register_fn, deregister_fn) \
        uint8_t DM_ALIGNED(16) DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__)[dmResource::s_ResourceTypeCreatorDescBufferSize]; \
        DM_REGISTER_RESOURCE_TYPE(symbol, DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__), sizeof(DM_RESOURCE_PASTE_SYMREG2(symbol, __LINE__)), suffix, register_fn, deregister_fn);


    /*#
     * Get a resource from factory
     * @name Get
     * @param factory [type: dmResource::HFactory] Factory handle
     * @param name [type: const char*] Resource name
     * @param resource [type: void**] Created resource
     * @return result [type: dmResource::Result]  RESULT_OK on success
     */
    Result Get(HFactory factory, const char* name, void** resource);

    /*#
     * Get a resource from factory
     * @name Get
     * @param factory [type: dmResource::HFactory] Factory handle
     * @param name [type: dmhash_t] Resource name
     * @param resource [type: void**] Created resource
     * @return result [type: dmResource::Result]  RESULT_OK on success
     */
    Result Get(HFactory factory, dmhash_t name, void** resource);


    /*#
     * Release resource
     * @name Release
     * @param factory [type: dmResource::HFactory] Factory handle
     * @param resource [type: void*] Resource pointer
     */
    void Release(HFactory factory, void* resource);

    /*#
     * Hint the preloader what to load before Create is called on the resource.
     * The resources are not guaranteed to be loaded before Create is called.
     * This function can be called from a worker thread.
     * @name PreloadHint
     * @param factory [type: dmResource::HPreloadHintInfo] Preloader handle
     * @param name [type: const char*] Resource name
     * @return result [type: bool] if successfully invoking preloader.
     */
    bool PreloadHint(HPreloadHintInfo preloader, const char *name);

    /*#
     * Returns the canonical path hash of a resource
     * @param factory Factory handle
     * @param resource Resource
     * @param hash Returned hash
     * @return RESULT_OK on success
    */
    Result GetPath(HFactory factory, const void* resource, uint64_t* hash);

    /*#
     * Returns the canonical path hash of a resource
     * @typedef
     * @name FDecryptResource
     * @param buffer [type: void*] The input/output buffer
     * @param buffer_len [type: uint32_t] The size of the buffer (in bytes)
     * @return RESULT_OK on success
    */
    typedef Result (*FDecryptResource)(void* buffer, uint32_t buffer_len);

    /*#
     * Returns the canonical path hash of a resource
     * @name RegisterResourceDecryptionFunction
     * @param decrypt_resource [type: dmResource::FDecryptResource] The decryption function
    */
    void RegisterResourceDecryptionFunction(FDecryptResource decrypt_resource);
}

#endif // DMSDK_RESOURCE_H
