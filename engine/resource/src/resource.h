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

#ifndef RESOURCE_H
#define RESOURCE_H

#include <stdint.h>
#include <dmsdk/resource/resource.h>
#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/mutex.h>

namespace dmResourceArchive
{
    typedef struct ArchiveIndexContainer* HArchiveIndexContainer;
}

namespace dmResourceMounts
{
    typedef struct ResourceMountsContext* HContext;
}

namespace dmResourceProvider
{
    typedef struct Archive* HArchive;
}

namespace dmResource
{
    // This is both for the total resource path, ie m_UriParts.X concatenated with relative path
    const uint32_t RESOURCE_PATH_MAX = 1024;

    /**
     * Configuration key used to tweak the max number of resources allowed.
     */
    extern const char* MAX_RESOURCES_KEY;

    extern const char* BUNDLE_INDEX_FILENAME;
    extern const char* BUNDLE_DATA_FILENAME;

    /**
     * Empty flags
     */
    #define RESOURCE_FACTORY_FLAGS_EMPTY            (0)

    /**
     * Enable resource reloading support. Both over files and http.
     */
    #define RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT (1 << 0)

    /**
     * Enable HTTP cache
     */
    #define RESOURCE_FACTORY_FLAGS_HTTP_CACHE     (1 << 2)

    /**
     * Enable Live update
     */
    #define RESOURCE_FACTORY_FLAGS_LIVE_UPDATE    (1 << 3)

    typedef uintptr_t ResourceType;


    Result RegisterTypes(HFactory factory, dmHashTable64<void*>* contexts);
    Result DeregisterTypes(HFactory factory, dmHashTable64<void*>* contexts);

    /**
     * Parameters to PreloaderCompleteCallback.
     */
    struct PreloaderCompleteCallbackParams
    {
        /// Factory handle
        HFactory m_Factory;
        /// User data supplied when the callback was registered
        void* m_UserData;
    };

    /**
     * Function called by UpdatePreloader when preoloading is complete and before postcreate callbacks are processed.
     * @param PreloaderCompleteCallbackParams parameters passed to callback function
     * @return true if succeeded
     * @see UpdatePreloader
     */
    typedef bool (*FPreloaderCompleteCallback)(const PreloaderCompleteCallbackParams* params);

    /**
     * Set default NewFactoryParams params
     * @param params
     */
    void SetDefaultNewFactoryParams(struct NewFactoryParams* params);

    struct EmbeddedResource
    {
        EmbeddedResource() : m_Data(NULL), m_Size(0)
        {

        }

        // Pointer to a resource. Set to NULL for no resource (default value)
        const void* m_Data;

        // Size of resource.
        uint32_t    m_Size;
    };

    /**
     * New factory parmetes
     */
    struct NewFactoryParams
    {
        /// Maximum number of resource in factory. Default is 1024
        uint32_t m_MaxResources;

        /// Factory flags. Default is RESOURCE_FACTORY_FLAGS_EMPTY
        uint32_t m_Flags;

        EmbeddedResource m_ArchiveIndex;
        EmbeddedResource m_ArchiveData;
        EmbeddedResource m_ArchiveManifest;

        uint32_t m_Reserved[5];

        NewFactoryParams()
        {
            SetDefaultNewFactoryParams(this);
        }
    };

    /**
     * Create a new factory
     * @param params New factory parameters
     * @param uri Resource root uri, e.g. http://locahost:5000 or build/content
     * @return Factory handle. NULL if out of memory or invalid uri.
     */
    HFactory NewFactory(NewFactoryParams* params, const char* uri);

    /**
     * Delete a factory
     * @param factory Factory handle
     */
    void DeleteFactory(HFactory factory);

    /**
     * Update resource factory. Required to be called periodically when http server support is enabled.
     * @param factory Factory handle
     */
    void UpdateFactory(HFactory factory);

    /**
     * Find a resource by a canonical path hash.
     * @param factory Factory handle
     * @param path_hash Resource path hash
     * @return SResourceDescriptor* pointer to the resource descriptor
     */
    SResourceDescriptor* FindByHash(HFactory factory, uint64_t canonical_path_hash);

    /**
     * Creates and inserts a resource into the factory
     * @param factory Factory handle
     * @param name Name of the resource
     * @param data Resource data
     * @param data_size Resource data size
     * @param resource Will contain a pointer to the resource after this function has completed
     * @return RESULT_OK on success
     */
    Result CreateResource(HFactory factory, const char* name, void* data, uint32_t data_size, void** resource);

    /**
     * Get a resource extension from a path, i.e resource.ext will return .ext. Note the included dot in the output.
     * @param path The path to the resource
     * @return Pointer to extension string if success (same as buffer), 0 otherwise
     */
    const char* GetExtFromPath(const char* path);

    /**
     * Get raw resource data. Unregistered resources can be loaded with this function.
     * The returned resource data must be deallocated with free()
     * @param factory Factory handle
     * @param name Resource name
     * @param resource Resource data
     * @param resource_size Resource size
     * @return RESULT_OK on success
     */
    Result GetRaw(HFactory factory, const char* name, void** resource, uint32_t* resource_size);

    /**
     * Updates a preexisting resource with new data
     * @param factory Factory handle
     * @param hashed_name The hashed canonical name (E.g. hash("/my/icon.texturec") or hash("/my/icon.texturec_123"))
     * @param buffer The buffer
     * @return RESULT_OK on success
     */
    Result SetResource(HFactory factory, uint64_t hashed_name, void* buffer, uint32_t size);

    /**
     * Updates a preexisting resource with new data
     * @param factory Factory handle
     * @param hashed_name The hashed canonical name (E.g. hash("/my/icon.texturec") or hash("/my/icon.texturec_123"))
     * @param buffer The buffer
     * @return RESULT_OK on success
     */
    Result SetResource(HFactory factory, uint64_t hashed_name, void* message);


    /**
     * Reload a specific resource
     * @param factory Resource factory
     * @param name Name that identifies the resource, i.e. the same name used in Get
     * @param out_descriptor The resource descriptor as an output argument. It will not be written to if null, otherwise it will always be written to.
     * @see Get
     */
    Result ReloadResource(HFactory factory, const char* name, SResourceDescriptor** out_descriptor);

    /**
     * Get type for resource
     * @param factory Factory handle
     * @param resource Resource
     * @param type Returned type
     * @return RESULT_OK on success
     */
    Result GetType(HFactory factory, void* resource, ResourceType* type);

    /**
     * Get type from extension
     * @param factory Factory handle
     * @param extension File extension
     * @param type Returned type
     * @return RESULT_OK on success
     */
    Result GetTypeFromExtension(HFactory factory, const char* extension, ResourceType* type);

    /**
     * Get extension from type
     * @param factory Factory handle
     * @param type Resource type
     * @param extension Returned extension
     * @return RESULT_OK on success
     */
    Result GetExtensionFromType(HFactory factory, ResourceType type, const char** extension);

    /**
     * Get resource descriptor from resource (name)
     * @param factory Factory handle
     * @param name Resource name
     * @param descriptor Returned resource descriptor
     * @return RESULT_OK on success
     */
    Result GetDescriptor(HFactory factory, const char* name, SResourceDescriptor* descriptor);

    /**
     * Get resource descriptor from resource (hash) with supplied extensions
     * @param factory Factory handle
     * @param hashed_name Resource name hash
     * @param exts Allowed extension hashes
     * @param ext_count Count of exts
     * @param descriptor pointer to write result to in case of RESULT_OK
     * @return RESULT_OK on success
     */
    Result GetDescriptorWithExt(HFactory factory, uint64_t hashed_name, const uint64_t* exts, uint32_t ext_count, SResourceDescriptor* descriptor);

    /**
     * Increase resource reference count
     * @param factory Factory handle
     * @param resource Resource
     */
    void IncRef(HFactory factory, void* resource);

    /**
     * Create a new preloader
     * @param factory Factory handle
     * @param name Resource to load
     * @return CREATE_RESULT_OK on success
     */
    HPreloader NewPreloader(HFactory factory, const char* name);

    /**
     * Create a new preloader
     * @param factory Factory handle
     * @param array of names of resources to load
     * @return CREATE_RESULT_OK on success
     */
    HPreloader NewPreloader(HFactory factory, const dmArray<const char*>& names);

    /**
     * Perform one update tick of the preloader, with a soft time limit for
     * how much time to spend.
     * @param preloader Preloader
     * @param complete_callback Preloader complete callback
     * @param complete_callback_params PreloaderCompleteCallbackParams passed to the complete callback
     * @param soft_time_limit Time limit in us
     * @return RESULT_PENDING while still loading, otherwise resource load result.
     */
    Result UpdatePreloader(HPreloader preloader, FPreloaderCompleteCallback complete_callback, PreloaderCompleteCallbackParams* complete_callback_params, uint32_t soft_time_limit);

    /**
     * Destroy the preloader. Note that currently it will spin and block until
     * all loads have completed, if there still are any pending.
     * @param preloader Preloader
     * @return RESULT_PENDING while still loading, otherwise resource load result.
     */
    void DeletePreloader(HPreloader preloader);

    /**
     * Returns the mutex held when loading asynchronous
     * @param factory Factory handle
     * @return Mutex pointer
    */
    dmMutex::HMutex GetLoadMutex(const dmResource::HFactory factory);


    /**
     * @name
     * @param factory [type: dmResource::HFactory] The factory handle
     * @return mounts_ctx [type: dmResourceMounts::HContext] the mounts context
     */
    dmResourceMounts::HContext GetMountsContext(const dmResource::HFactory factory);

    struct SGetDependenciesParams
    {
        dmhash_t m_UrlHash; // The requested url
        bool m_OnlyMissing; // Only report assets that aren't available in the mounts
        bool m_Recursive;   // Traverse down for each resource that has dependencies
    };

    struct SGetDependenciesResult
    {
        dmhash_t m_UrlHash;
        uint8_t* m_HashDigest;
        uint32_t m_HashDigestLength;
        bool     m_Missing;
    };

    /**
     * Returns the dependencies for a resource
     * @name GetDependencies
     * @note Only reports dependencies from mounts that have a .dmanifest available
     * @param factory [type: dmResource::HFactory] Factory handle
     * @param url_hash [type: dmhash_t] url hash
     * @return result [type: dmResource::Result] The mounts context
    */
    typedef void (*FGetDependency)(void* context, const SGetDependenciesResult* result);
    dmResource::Result GetDependencies(const dmResource::HFactory factory, const SGetDependenciesParams* params, FGetDependency callback, void* callback_context);

    /**
     * Returns the path to the public key, or null if it was not found
     **/
    const char* GetPublicKeyPath(HFactory factory);

    /**
     * Returns the base archive mount. It is always of type "archive", or it will return 0.
     **/
    dmResourceProvider::HArchive GetBaseArchive(HFactory factory);

    /**
     * Releases the builtins manifest
     * Use when it's no longer needed, e.g. the user project loaded properly
     */
    void ReleaseBuiltinsArchive(HFactory factory);

    /**
     * Struct returned from the resource iterator api
     */
    struct IteratorResource
    {
        dmhash_t m_Id;          // The name of the resource
        uint32_t m_SizeOnDisc;  // The size on disc (i.e. in the .darc file)
        uint32_t m_Size;        // in memory size, may be 0
        uint32_t m_RefCount;    // The current ref count
    };

    typedef bool (*FResourceIterator)(const IteratorResource& resource, void* user_ctx);

    /**
     * Iterates over all loaded resources, and invokes the callback function with the resource informations
     * @param factory   The resource factory holding all resources
     * @param callback  The callback function which is invoked for each resources.
                        It should return true if the iteration should continue, and false otherwise.
     * @param user_ctx  The user defined context which is passed along with each callback
     */
    void IterateResources(HFactory factory, FResourceIterator callback, void* user_ctx);

    /*#
     */
    const char* ResultToString(Result result);

}

#endif // RESOURCE_H
