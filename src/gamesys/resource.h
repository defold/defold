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


#if 0
struct SGameObjectInstance
{
    Vector3               m_Position;
    SGameObjectPrototype* m_Prototype;
    PyObject            * m_Self;
};



HFactory ResourceFactoryNew();
void             ResourceFactoryDelete(HFactory resource_factory, );

void             ResourceFactoryRegisterType(HFactory resource_factory, const char* extension, FResourceCreate create_function);

FactoryError    ResourceFactoryCreate(HFactory resource_factory,
                                       const char* name,
                                       const void* data, uint32_t data_size,
                                       SResouce** resource);

//void             ResourceFactoryAdd(HResourceFactory resource_factory, const char* name, SResource* resource);
FactoryError    ResourceFactoryGet(HFactory resource_factory, const char* name, SResouce** resource);
void             ResourceFactoryRelease(HFactory resource_factory, SResourceDescriptor* resource);

ResouceError Texture_ResourceCreate(HFactory resource_factory, const void* data, uint32_t data_size, SResourceDescriptor* resource)
{
    GFX::TextureImage* texture_image;
    DDFError e = DDFLoadMessage(data, data_size, GFX_TextureImage_DESCRIPTOR, (void**) &texture_image);
    if ( e != DDF_ERROR_OK )
    {
        return FACTORY_ERROR_DDF_ERROR;
    }

    GFXTextureFormat format = xyz;
    GFXHTexture texture = GFXCreateTexture(texture_image->m_Width, texture_image->m_Height, format);

    int w = texture_image->m_Width;
    int h = texture_image->m_Height;
    for (int i = 0; i < image->m_MipMapOffset.m_Count; ++i)
    {
        GFXSetTextureData(texture, i, w, h, 0, format, &texture_image->m_Data[image->m_MipMapOffset[i]], texture_image->m_MipMapSize[i]);
        w >>= 1;
        h >>= 1;
    }

    resource->m_ResourceType = RESOURCE_TYPE_HANDLE;
    resource->m_ResourceTypeName = "Texture";
    resource->m_Resource = (void*) texture;

    return FACTORY_ERROR_OK;
}

// GameObjectPrototypeDesc -> GameObjectPrototype
ResouceError GameObjectPrototype_ResourceCreate(HFactory resource_factory, const void* data, uint32_t data_size, SResourceDescriptor* resource)
{
    //assert(resource->m_ResourceType == RESOURCE_TYPE_DDF_DATA);
    //assert(resource->m_Descriptor == &Game_GameObjectPrototypeDesc_DESCRIPTOR);

    GameObjectPrototypeDesc gameobjectproto_desc;
    DDFError e = DDFLoadMessage(data, data_size, Game_GameObjectPrototypeDesc_DESCRIPTOR, (void**) &gameobjectproto_desc);
    if ( e != DDF_ERROR_OK )
    {
        return FACTORY_ERROR_DDF_ERROR;
    }

    SGameObjectPrototype* gameobject_proto = new SGameObjectPrototype();
    gameobject_proto->m_Resources.SetCapacity(desc->m_Resource.m_Count);

    for (int i = 0; i < gameobjectproto_desc->m_Resource.m_Count; ++i)
    {
        const char* resource_name = gameobjectproto_desc->m_Resource[i];
        // Create sub resources.
        SResourceDescriptor* resource;
        FactoryError e = ResourceFactoryGet(resource_factory, resource_name, &resource);
        if (e != FACTORY_ERROR_OK)
        {
            // TODO: Remove created
            return e;
        }
        gameobject_proto->m_Resources.PushBack(resource);
    }

    // resource->m_Name already filled in by factory
    resource->m_ResourceType = RESOURCE_TYPE_POINTER;
    resource->m_ResourceTypeName = "GameObjectPrototype";
    resource->m_Resource = (void*) gameobjectproto_desc;

    return FACTORY_ERROR_OK;
}

struct SGameObjectPrototype // Moby
{
    const char*        m_Name; // eg Crate
    HMesh              m_Mesh;
    HMaterial          m_Material;
    HScript            m_Script;

    TArray<SResourceDescriptor>  m_Resources;
    // or
    TArray<HEffect>    m_Effects;
    TArray<HAnimation> m_Animations;
    // ....
};

#endif

#endif // RESOURCE_H
