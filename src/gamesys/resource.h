#ifndef RESOURCE_H
#define RESOURCE_H

#include <ddf/ddf.h>

/*
 * TODO:
 * Add function to translate from factory to create result
 */

namespace dmResource
{
    /**
     * Factory result
     */
    enum FactoryResult
    {
        FACTORY_RESULT_OK                        = 0,    //!< FACTORY_RESULT_OK
        FACTORY_RESULT_INVALID_DATA              = -1,   //!< FACTORY_RESULT_INVALID_DATA
        FACTORY_RESULT_DDF_ERROR                 = -2,   //!< FACTORY_RESULT_DDF_ERROR
        FACTORY_RESULT_RESOURCE_NOT_FOUND        = -3,   //!< FACTORY_RESULT_RESOURCE_NOT_FOUND
        FACTORY_RESULT_MISSING_FILE_EXTENSION    = -4,   //!< FACTORY_RESULT_MISSING_FILE_EXTENSION
        FACTORY_RESULT_ALREADY_REGISTERED        = -5,   //!< FACTORY_RESULT_ALREADY_REGISTERED
        FACTORY_RESULT_INVAL                     = -6,   //!< FACTORY_RESULT_INVAL
        FACTORY_RESULT_UNKNOWN_RESOURCE_TYPE     = -7,   //!< FACTORY_RESULT_UNKNOWN_RESOURCE_TYPE
        FACTORY_RESULT_OUT_OF_MEMORY             = -8,   //!< FACTORY_RESULT_OUT_OF_MEMORY
        FACTORY_RESULT_IO_ERROR                  = -9,   //!< FACTORY_RESULT_IO_ERROR
        FACTORY_RESULT_NOT_LOADED                = -10,  //!< FACTORY_RESULT_NOT_LOADED
        FACTORY_RESULT_OUT_OF_RESOURCES          = -11,  //!< FACTORY_RESULT_OUT_OF_RESOURCES
        FACTORY_RESULT_UNKNOWN                   = -1000,//!< FACTORY_RESULT_UNKNOWN
    };

    /**
     * Resource kind
     */
    enum Kind
    {
        KIND_DDF_DATA,//!< KIND_DDF_DATA
        KIND_POINTER, //!< KIND_POINTER
    };

    /// Resource descriptor
    struct SResourceDescriptor
    {
        /// Hash of resource name
        uint64_t m_NameHash;

        /// Resource kind
        Kind     m_ResourceKind;

        /// Union of DDF descriptor and resource name
        union
        {
            dmDDF::Descriptor* m_Descriptor;
            const char*        m_ResourceTypeName;
        };

        /// Resource pointer. Must be unique and not NULL.
        void*    m_Resource;

        /// Reference count
        uint32_t m_ReferenceCount;

        /// For internal use only
        void*    m_ResourceType; // For internal use.
    };

    /**
     * Factory handle
     */
    typedef struct SResourceFactory* HFactory;

    /**
     * Create result
     */
    enum CreateResult
    {
        CREATE_RESULT_OK             = 0,    //!< CREATE_RESULT_OK
        CREATE_RESULT_OUT_OF_MEMORY  = -1,   //!< CREATE_RESULT_OUT_OF_MEMORY
        CREATE_RESULT_FORMAT_ERROR   = -2,   //!< CREATE_RESULT_FORMAT_ERROR
        CREATE_RESULT_UNKNOWN        = -1000,//!< CREATE_RESULT_UNKNOWN
    };

    /**
     * Resource create function
     * @param factory Factory handle
     * @param context User context
     * @param buffer Buffer
     * @param buffer_size Buffer size
     * @param resource Resource descriptor
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*FResourceCreate)(HFactory factory,
                                           void* context,
                                           const void* buffer, uint32_t buffer_size,
                                           SResourceDescriptor* resource);

    /**
     * Resource destroy function
     * @param factory Factory handle
     * @param context User context
     * @param resource Resource handle
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*FResourceDestroy)(HFactory factory,
                                            void* context,
                                            SResourceDescriptor* resource);

    /**
     * Create a new factory
     * @param max_resources Max resources created simultaneously
     * @param resource_path Resource root path
     * @return Factory handle
     */
    HFactory NewFactory(uint32_t max_resources, const char* resource_path);

    /**
     * Delete a factory
     * @param factory Factory handle
     */
    void DeleteFactory(HFactory factory);

    /**
     * Register a resource type
     * @param factory Factory handle
     * @param extension File extension for resource
     * @param context User context
     * @param create_function Create function pointer
     * @param destroy_function Destroy function pointer
     * @return FACTORY_RESULT_OK on success
     */
    FactoryResult RegisterType(HFactory factory,
                               const char* extension,
                               void* context,
                               FResourceCreate create_function,
                               FResourceDestroy destroy_function);

    /**
     * Get a resource from factory
     * @param factory Factory handle
     * @param name Resource name
     * @param resource Created resource
     * @return FACTORY_RESULT_OK on success
     */
    FactoryResult Get(HFactory factory, const char* name, void** resource);

    /**
     * Get type for resource
     * @param factory Factory handle
     * @param resource Resource
     * @param type Returned type
     * @return FACTORY_RESULT_OK on success
     */
    FactoryResult GetType(HFactory factory, void* resource, uint32_t* type);

    /**
     * Get type from extension
     * @param factory Factory handle
     * @param extension File extension
     * @param type Returned type
     * @return FACTORY_RESULT_OK on success
     */
    FactoryResult GetTypeFromExtension(HFactory factory, const char* extension, uint32_t* type);

    /**
     * Get resource descriptor from resource (name)
     * @param factory Factory handle
     * @param name Resource name
     * @param descriptor Returned resource descriptor
     * @return FACTORY_RESULT_OK on success
     */
    FactoryResult GetDescriptor(HFactory factory, const char* name, SResourceDescriptor* descriptor);

    /**
     * Release resource
     * @param factory Factory handle
     * @param resource Resource
     */
    void Release(HFactory factory, void* resource);
}

#endif // RESOURCE_H
