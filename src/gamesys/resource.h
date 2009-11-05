#ifndef RESOURCE_H
#define RESOURCE_H

#include <ddf/ddf.h>

/*
 * TODO:
 * Add function to translate from factory to create result
 */

/*
 * Resource types:
 *  Resource:               Source-type:    Runtime-type:
 *  Animation               Edge            Handle
 *  Texture                 DDF             Handle
 *  Material                DDF             Pointer
 *  Program                 Native          Handle
 *  Script                  DDF             Handle
 *  SGameObjectPrototype    DDF             Pointer
 *
 *  DDF resource types...? :-)
 */

namespace Resource
{
    enum FactoryResult
    {
        FACTORY_RESULT_OK                        = 0,
        FACTORY_RESULT_INVALID_DATA              = -1,
        FACTORY_RESULT_DDF_ERROR                 = -2,
        FACTORY_RESULT_RESOURCE_NOT_FOUND        = -3,
        FACTORY_RESULT_MISSING_FILE_EXTENSION    = -4,
        FACTORY_RESULT_ALREADY_REGISTERED        = -5,
        FACTORY_RESULT_INVAL                     = -6,
        FACTORY_RESULT_UNKNOWN_RESOURCE_TYPE     = -7,
        FACTORY_RESULT_OUT_OF_MEMORY             = -8,
        FACTORY_RESULT_IO_ERROR                  = -9,
        FACTORY_RESULT_NOT_LOADED                = -10,
        FACTORY_RESULT_OUT_OF_RESOURCES          = -11,
        FACTORY_RESULT_UNKNOWN                   = -1000,
    };

    enum Kind
    {
        KIND_DDF_DATA,
        KIND_POINTER,
    };

    /// Resource descriptor
    struct SResourceDescriptor
    {
        /// Hash of resource name
        uint64_t        m_NameHash;

        /// Resource kind
        Kind            m_ResourceKind;

        /// Union of DDF descriptor and resource name
        union
        {
            dmDDF::Descriptor* m_Descriptor;
            const char*        m_ResourceTypeName;
        };

        /// Resource pointer. Must be unique and not NULL.
        void*           m_Resource;

        /// Reference count
        uint32_t        m_ReferenceCount;

        /// For internal use only
        void*           m_ResourceType; // For internal use.
    };

    typedef struct SResourceFactory* HFactory;

    enum CreateResult
    {
        CREATE_RESULT_OK             = 0,
        CREATE_RESULT_OUT_OF_MEMORY  = -1,
        CREATE_RESULT_FORMAT_ERROR   = -2,
        CREATE_RESULT_UNKNOWN        = -1000,
    };

    typedef CreateResult (*FResourceCreate)(HFactory factory,
                                           void* context,
                                           const void* buffer, uint32_t buffer_size,
                                           SResourceDescriptor* resource);

    typedef CreateResult (*FResourceDestroy)(HFactory factory,
                                            void* context,
                                            SResourceDescriptor* resource);

    HFactory        NewFactory(uint32_t max_resources, const char* resource_path);

    void            DeleteFactory(HFactory factory);

    FactoryResult    RegisterType(HFactory factory,
                                 const char* extension,
                                 void* context,
                                 FResourceCreate create_function,
                                 FResourceDestroy destroy_function);

    FactoryResult    Get(HFactory factory, const char* name, void** resource);

    FactoryResult    GetType(HFactory factory, void* resource, uint32_t* type);

    FactoryResult    GetTypeFromExtension(HFactory factory, const char* extension, uint32_t* type);

    FactoryResult    GetDescriptor(HFactory factory, const char* name, SResourceDescriptor* descriptor);

    void            Release(HFactory factory, void* resource);
}

#endif // RESOURCE_H
