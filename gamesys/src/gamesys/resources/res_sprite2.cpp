#include "res_sprite2.h"

#include <string.h>

#include <dlib/log.h>

#include "sprite2_ddf.h"

namespace dmGameSystem
{
    bool AcquireResources(dmResource::HFactory factory,
        const void* buffer, uint32_t buffer_size,
        Sprite2Resource* resource, const char* filename)
    {
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &resource->m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            return false;
        }
        dmResource::FactoryResult fr = dmResource::Get(factory, resource->m_DDF->m_TileSet, (void**)&resource->m_TileSet);
        if (fr != dmResource::FACTORY_RESULT_OK)
        {
            return false;
        }
        resource->m_DefaultAnimation = dmHashString64(resource->m_DDF->m_DefaultAnimation);
        uint32_t n_animations = resource->m_TileSet->m_AnimationIds.Size();
        bool found = false;
        for (uint32_t i = 0; i < n_animations; ++i)
        {
            if (resource->m_TileSet->m_AnimationIds[i] == resource->m_DefaultAnimation)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            if (resource->m_DDF->m_DefaultAnimation == 0 || resource->m_DDF->m_DefaultAnimation[0] == '\0')
            {
                dmLogError("No default animation specified");
            }
            else
            {
                dmLogError("Default animation '%s' not found", resource->m_DDF->m_DefaultAnimation);
            }
        }

        return found;
    }

    void ReleaseResources(dmResource::HFactory factory, Sprite2Resource* resource)
    {
        if (resource->m_DDF != 0x0)
            dmDDF::FreeMessage(resource->m_DDF);
        if (resource->m_TileSet != 0x0)
            dmResource::Release(factory, resource->m_TileSet);
    }

    dmResource::CreateResult ResSprite2Create(dmResource::HFactory factory,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            dmResource::SResourceDescriptor* resource,
                                            const char* filename)
    {
        Sprite2Resource* sprite_resource = new Sprite2Resource();
        memset(sprite_resource, 0, sizeof(Sprite2Resource));
        if (AcquireResources(factory, buffer, buffer_size, sprite_resource, filename))
        {
            resource->m_Resource = (void*) sprite_resource;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, sprite_resource);
            delete sprite_resource;
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResSprite2Destroy(dmResource::HFactory factory,
                                             void* context,
                                             dmResource::SResourceDescriptor* resource)
    {
        Sprite2Resource* sprite_resource = (Sprite2Resource*) resource->m_Resource;
        ReleaseResources(factory, sprite_resource);
        delete sprite_resource;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResSprite2Recreate(dmResource::HFactory factory,
                                              void* context,
                                              const void* buffer, uint32_t buffer_size,
                                              dmResource::SResourceDescriptor* resource,
                                              const char* filename)
    {
        Sprite2Resource tmp_sprite_resource;
        memset(&tmp_sprite_resource, 0, sizeof(Sprite2Resource));
        if (AcquireResources(factory, buffer, buffer_size, &tmp_sprite_resource, filename))
        {
            Sprite2Resource* sprite_resource = (Sprite2Resource*)resource->m_Resource;
            ReleaseResources(factory, sprite_resource);
            *sprite_resource = tmp_sprite_resource;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, &tmp_sprite_resource);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }
}
