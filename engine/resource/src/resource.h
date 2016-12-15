#ifndef RESOURCE_H
#define RESOURCE_H

#include <ddf/ddf.h>

namespace dmBuffer
{
    typedef struct Buffer* HBuffer;
}

namespace dmResource
{
    /**
     * Configuration key used to tweak the max number of resources allowed.
     */
    extern const char* MAX_RESOURCES_KEY;

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
     * Result
     */
    enum Result
    {
        RESULT_OK                        = 0,    //!< RESULT_OK
        RESULT_INVALID_DATA              = -1,   //!< RESULT_INVALID_DATA
        RESULT_DDF_ERROR                 = -2,   //!< RESULT_DDF_ERROR
        RESULT_RESOURCE_NOT_FOUND        = -3,   //!< RESULT_RESOURCE_NOT_FOUND
        RESULT_MISSING_FILE_EXTENSION    = -4,   //!< RESULT_MISSING_FILE_EXTENSION
        RESULT_ALREADY_REGISTERED        = -5,   //!< RESULT_ALREADY_REGISTERED
        RESULT_INVAL                     = -6,   //!< RESULT_INVAL
        RESULT_UNKNOWN_RESOURCE_TYPE     = -7,   //!< RESULT_UNKNOWN_RESOURCE_TYPE
        RESULT_OUT_OF_MEMORY             = -8,   //!< RESULT_OUT_OF_MEMORY
        RESULT_IO_ERROR                  = -9,   //!< RESULT_IO_ERROR
        RESULT_NOT_LOADED                = -10,  //!< RESULT_NOT_LOADED
        RESULT_OUT_OF_RESOURCES          = -11,  //!< RESULT_OUT_OF_RESOURCES
        RESULT_STREAMBUFFER_TOO_SMALL    = -12,  //!< RESULT_STREAMBUFFER_TOO_SMALL
        RESULT_FORMAT_ERROR              = -13,  //!< RESULT_FORMAT_ERROR
        RESULT_CONSTANT_ERROR            = -14,  //!< RESULT_CONSTANT_ERROR
        RESULT_NOT_SUPPORTED             = -15,  //!< RESULT_NOT_SUPPORTED
        RESULT_RESOURCE_LOOP_ERROR       = -16,  //!< RESULT_RESOURCE_LOOP_ERROR
        RESULT_PENDING                   = -17,  //!< RESULT_PENDING
    };

    /**
     * Resource kind
     */
    enum Kind
    {
        KIND_DDF_DATA,//!< KIND_DDF_DATA
        KIND_POINTER, //!< KIND_POINTER
    };

    /** Data share state
    * Describes the ownage of the resource/data
    * NONE -> The descriptor owns all data -> delete/update all data
    * SHALLOW -> The descriptor owns the "topmost" data, i.e. what's been previously duplicated -> delete the data that was instanced in the duplicate function
    */
    enum DataShareState
    {
        DATA_SHARE_STATE_NONE = 0,
        DATA_SHARE_STATE_SHALLOW = 1,
    };

    /// Resource descriptor
    struct SResourceDescriptor
    {
        /// Hash of resource name
        uint64_t m_NameHash;

        /// Name of original resource
        uint64_t m_OriginalNameHash;

        /// Union of DDF descriptor and resource name
        union
        {
            dmDDF::Descriptor* m_Descriptor;
            const char*        m_ResourceTypeName;
        };

        /// Resource pointer. Must be unique and not NULL.
        void*    m_Resource;

        /// For internal use only
        void*    m_ResourceType;        // For internal use.

        /// Reference count
        uint32_t m_ReferenceCount;

        /// Resource kind
        Kind     m_ResourceKind;

        /// The shared state tells who owns what data
        uint8_t  m_SharedState:1;        // 0 == shared, 1 == "instanced" (not full copy), 2 == New copy of original resource . For internal use.
    };

    /**
     * Factory handle
     */
    typedef struct SResourceFactory* HFactory;

    /**
     * Preloader handle
     */
    typedef struct ResourcePreloader* HPreloader;
    typedef struct PreloadHintInfo* HPreloadHintInfo;

    typedef uintptr_t ResourceType;

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
        SResourceDescriptor* m_Resource;
    };

    /**
     * Resource create function
     * @param params Resource creation arguments
     * @return CREATE_RESULT_OK on success
     */
    typedef Result (*FResourceCreate)(const ResourceCreateParams& params);

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
        SResourceDescriptor* m_Resource;
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
        SResourceDescriptor* m_Resource;
    };

    /**
     * Resource recreate function. Recreate resource in-place.
     * @params params Parameters for resource creation
     * @return CREATE_RESULT_OK on success
     */
    typedef Result (*FResourceRecreate)(const ResourceRecreateParams& params);

    /**
     * Parameters to ResourceDuplicate callback.
     */
    struct ResourceDuplicateParams
    {
        /// Factory handle
        HFactory m_Factory;
        /// Resource context
        void* m_Context;
        /// Resource descriptor to copy from
        SResourceDescriptor* m_OriginalResource;
        /// Resource descriptor to write into
        SResourceDescriptor* m_Resource;
    };

    /**
     * Resource duplicate function. Used to create a new resource, while still using the same payload (if possible)
     * The responsibility of the duplicate function is to create a shallow copy: retaining the bulk payload, and handing out a light instance. E.g. HTexture vs OpenGL texture
     * The resource system keeps track of reference counting.
     * @params params Parameters for resource creation
     * @return CREATE_RESULT_OK on success
     */
    typedef Result (*FResourceDuplicate)(const ResourceDuplicateParams& params);


    /**
     * Parameters to ResourceReloaded callback.
     */
    struct ResourceReloadedParams
    {
        /// User data supplied when the callback was registered
        void* m_UserData;
        /// Descriptor of the reloaded resource
        SResourceDescriptor* m_Resource;
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
     * Set default NewFactoryParams params
     * @param params
     */
    void SetDefaultNewFactoryParams(struct NewFactoryParams* params);

    /**
     * New factory parmetes
     */
    struct NewFactoryParams
    {
        /// Maximum number of resource in factory. Default is 1024
        uint32_t m_MaxResources;

        /// Factory flags. Default is RESOURCE_FACTORY_FLAGS_EMPTY
        uint32_t m_Flags;

        /// Pointer to a resource archive for builtin resources. Set to NULL for no archive (default value)
        const void* m_BuiltinsArchive;

        /// sizeof of m_BuiltinsArchive
        uint32_t    m_BuiltinsArchiveSize;

        uint32_t m_Reserved[5];

        NewFactoryParams()
        {
            SetDefaultNewFactoryParams(this);
        }
    };

    /**
     * Create a new factory
     * @param params New factory parameters
     * @param uri Resource root uri, e.g. http://locahost:5000 or build/default/content
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
     * Register a resource type
     * @param factory Factory handle
     * @param extension File extension for resource
     * @param context User context
     * @param preload_function Preload function. Optional, 0 if no preloading is used
     * @param create_function Create function pointer
     * @param destroy_function Destroy function pointer
     * @param recreate_function Recreate function pointer. Optional, 0 if recreate is not supported.
     * @return RESULT_OK on success
     */
    Result RegisterType(HFactory factory,
                               const char* extension,
                               void* context,
                               FResourcePreload preload_function,
                               FResourceCreate create_function,
                               FResourceDestroy destroy_function,
                               FResourceRecreate recreate_function,
                               FResourceDuplicate duplicate_function);

    /**
     * Get a resource from factory
     * @param factory Factory handle
     * @param name Resource name
     * @param resource Created resource
     * @return RESULT_OK on success
     */
    Result Get(HFactory factory, const char* name, void** resource);

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
    Result SetResource(HFactory factory, uint64_t hashed_name, dmBuffer::HBuffer buffer);

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
     * Increase resource reference count
     * @param factory Factory handle
     * @param resource Resource
     */
    void IncRef(HFactory factory, void* resource);

    /**
     * Release resource
     * @param factory Factory handle
     * @param resource Resource
     */
    void Release(HFactory factory, void* resource);

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

    /**
     * Create a new preloader
     * @param factory Factory handle
     * @param name Resource to load
     * @return CREATE_RESULT_OK on success
     */
    HPreloader NewPreloader(HFactory factory, const char* name);

    /**
     * Perform one update tick of the preloader, with a soft time limit for
     * how much time to spend.
     * @param preloader Preloader
     * @param soft_time_limit Time limit in us
     * @return RESULT_PENDING while still loading, otherwise resource load result.
     */
    Result UpdatePreloader(HPreloader preloader, uint32_t soft_time_limit);

    /**
     * Destroy the preloader. Note that currently it will spin and block until
     * all loads have completed, if there still are any pending.
     * @param preloader Preloader
     * @return RESULT_PENDING while still loading, otherwise resource load result.
     */
    void DeletePreloader(HPreloader preloader);

    /**
     * Hint the preloader what to load before Create is called on the resource.
     * The resources are not guaranteed to be loaded before Create is called.
     * @param name Resource name
     * @return RESULT_PENDING while still loading, otherwise resource load result.
     */
    void PreloadHint(HPreloadHintInfo preloader, const char *name);

    /**
     * Determines if the resource could be unique
     * @param name Resource name
    */
    bool IsPathTagged(const char* name);


    /**
     * Returns the hashed name of a resource
     * @param factory Factory handle
     * @param resource Resource
    */
    Result GetPath(HFactory factory, const void* resource, uint64_t* hash);
}

#endif // RESOURCE_H
