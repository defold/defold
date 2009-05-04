#ifndef RESOURCE_H
#define RESOURCE_H

#include <ddf/ddf.h>
#include "gamesys/gameobject_ddf.h"

/*
 * Resource types:
 *  Resource:               Source-type:    Runtime-type:
 *  Animtion                Edge            Handle
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
    enum FactoryError
    {
        FACTORY_ERROR_OK                        = 0,
        FACTORY_ERROR_INVALID_DATA              = 1,
        FACTORY_ERROR_DDF_ERROR                 = 2,
        FACTORY_ERROR_RESOURCE_NOT_FOUND        = 3,
        FACTORY_ERROR_MISSING_FILE_EXTENSION    = 4,
        FACTORY_ERROR_ALREADY_REGISTRED         = 5,
        FACTORY_ERROR_INVAL                     = 6,
        FACTORY_ERROR_UNKNOWN_RESOURCE_TYPE     = 7,
        FACTORY_ERROR_UNKNOWN                   = 8,
        FACTORY_ERROR_OUT_OF_MEMORY             = 9,
        FACTORY_ERROR_IO_ERROR                  = 10,
        FACTORY_ERROR_NOT_LOADED                = 11,
    };

    enum Kind
    {
        KIND_DDF_DATA,
        KIND_POINTER,
    };

    struct SResourceDescriptor
    {
        uint64_t        m_NameHash;
        Kind            m_ResourceKind;

        union
        {
            SDDFDescriptor*  m_Descriptor;
            const char*      m_ResourceTypeName;
        };

        void*           m_Resource;
        uint32_t        m_ReferenceCount;
    };

    typedef struct SResourceFactory* HFactory;

    enum CreateError
    {
        CREATE_ERROR_OK             = 0,
        CREATE_ERROR_OUT_OF_MEMORY  = 1,
        CREATE_ERROR_UNKNOWN        = 1000,
    };

    typedef CreateError (*FResourceCreate)(HFactory factory,
                                           void* context,
                                           const void* buffer, uint32_t buffer_size,
                                           SResourceDescriptor* resource);

    typedef CreateError (*FResourceDestroy)(HFactory factory,
                                            void* context,
                                            SResourceDescriptor* resource);

    HFactory        NewFactory(uint32_t max_resources, const char* resource_path);

    void            DeleteFactory(HFactory factory);

    FactoryError    RegisterType(HFactory factory,
                                 const char* extension,
                                 void* context,
                                 FResourceCreate create_function,
                                 FResourceDestroy destroy_function);

    FactoryError    Get(HFactory factory, const char* name, void** resource);

    FactoryError    GetDescriptor(HFactory factory, const char* name, SResourceDescriptor* descriptor);

    void            Release(HFactory factory, SResourceDescriptor* resource);
}

#endif // RESOURCE_H
