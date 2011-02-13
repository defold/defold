#include "res_sprite.h"

#include <string.h>

#include <dlib/log.h>

#include "sprite_ddf.h"

namespace dmGameSystem
{
    bool AcquireResources(dmResource::HFactory factory,
        const void* buffer, uint32_t buffer_size,
        SpriteResource* resource, const char* filename)
    {
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &resource->m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            return false;
        }
        if (resource->m_DDF->m_FrameCount == 0 || resource->m_DDF->m_FramesPerRow == 0)
        {
            dmLogError("%s", "frame_count and frames_per_row must both be over 0");
            return false;
        }
        dmResource::FactoryResult fr = dmResource::Get(factory, resource->m_DDF->m_Texture, (void**)&resource->m_Texture);
        if (fr != dmResource::FACTORY_RESULT_OK)
        {
            return false;
        }
        return true;
    }

    void ReleaseResources(dmResource::HFactory factory, SpriteResource* resource)
    {
        if (resource->m_DDF != 0x0)
            dmDDF::FreeMessage(resource->m_DDF);
        if (resource->m_Texture != 0x0)
            dmResource::Release(factory, resource->m_Texture);
    }

    dmResource::CreateResult ResSpriteCreate(dmResource::HFactory factory,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            dmResource::SResourceDescriptor* resource,
                                            const char* filename)
    {
        SpriteResource* sprite_resource = new SpriteResource();
        memset(sprite_resource, 0, sizeof(SpriteResource));
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

    dmResource::CreateResult ResSpriteDestroy(dmResource::HFactory factory,
                                             void* context,
                                             dmResource::SResourceDescriptor* resource)
    {
        SpriteResource* sprite_resource = (SpriteResource*) resource->m_Resource;
        ReleaseResources(factory, sprite_resource);
        delete sprite_resource;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResSpriteRecreate(dmResource::HFactory factory,
                                              void* context,
                                              const void* buffer, uint32_t buffer_size,
                                              dmResource::SResourceDescriptor* resource,
                                              const char* filename)
    {
        SpriteResource tmp_sprite_resource;
        memset(&tmp_sprite_resource, 0, sizeof(SpriteResource));
        if (AcquireResources(factory, buffer, buffer_size, &tmp_sprite_resource, filename))
        {
            SpriteResource* sprite_resource = (SpriteResource*)resource->m_Resource;
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
