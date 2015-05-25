#include "res_sprite.h"

#include <string.h>

#include <dlib/log.h>

#include "sprite_ddf.h"

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory,
        SpriteResource* resource, const char* filename)
    {
        // Add-alpha is deprecated because of premultiplied alpha and replaced by Add
        if (resource->m_DDF->m_BlendMode == dmGameSystemDDF::SpriteDesc::BLEND_MODE_ADD_ALPHA)
            resource->m_DDF->m_BlendMode = dmGameSystemDDF::SpriteDesc::BLEND_MODE_ADD;

        dmResource::Result fr = dmResource::Get(factory, resource->m_DDF->m_TileSet, (void**)&resource->m_TextureSet);
        if (fr != dmResource::RESULT_OK)
        {
            return fr;
        }
        fr = dmResource::Get(factory, resource->m_DDF->m_Material, (void**)&resource->m_Material);
        if (fr != dmResource::RESULT_OK)
        {
            return fr;
        }
        resource->m_DefaultAnimation = dmHashString64(resource->m_DDF->m_DefaultAnimation);
        if (!resource->m_TextureSet->m_AnimationIds.Get(resource->m_DefaultAnimation))
        {
            if (resource->m_DDF->m_DefaultAnimation == 0 || resource->m_DDF->m_DefaultAnimation[0] == '\0')
            {
                dmLogError("No default animation specified");
            }
            else
            {
                dmLogError("Default animation '%s' not found", resource->m_DDF->m_DefaultAnimation);
            }
            return dmResource::RESULT_FORMAT_ERROR;;
        }
        else
        {
            return dmResource::RESULT_OK;
        }
    }

    void ReleaseResources(dmResource::HFactory factory, SpriteResource* resource)
    {
        if (resource->m_DDF != 0x0)
            dmDDF::FreeMessage(resource->m_DDF);
        if (resource->m_TextureSet != 0x0)
            dmResource::Release(factory, resource->m_TextureSet);
        if (resource->m_Material != 0x0)
            dmResource::Release(factory, resource->m_Material);
    }

    dmResource::Result ResSpritePreload(dmResource::HFactory factory, dmResource::HPreloadHintInfo hint_info,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            void** preload_data,
                                            const char* filename)
    {
        dmGameSystemDDF::SpriteDesc *ddf;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::PreloadHint(hint_info, ddf->m_TileSet);
        dmResource::PreloadHint(hint_info, ddf->m_Material);

        *preload_data = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSpriteCreate(dmResource::HFactory factory,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            void* preload_data,
                                            dmResource::SResourceDescriptor* resource,
                                            const char* filename)
    {
        SpriteResource* sprite_resource = new SpriteResource();
        memset(sprite_resource, 0, sizeof(SpriteResource));
        sprite_resource->m_DDF = (dmGameSystemDDF::SpriteDesc*) preload_data;

        dmResource::Result r = AcquireResources(factory, sprite_resource, filename);
        if (r == dmResource::RESULT_OK)
        {
            resource->m_Resource = (void*) sprite_resource;
        }
        else
        {
            ReleaseResources(factory, sprite_resource);
            delete sprite_resource;
        }
        return r;
    }

    dmResource::Result ResSpriteDestroy(dmResource::HFactory factory,
                                             void* context,
                                             dmResource::SResourceDescriptor* resource)
    {
        SpriteResource* sprite_resource = (SpriteResource*) resource->m_Resource;
        ReleaseResources(factory, sprite_resource);
        delete sprite_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSpriteRecreate(dmResource::HFactory factory,
                                              void* context,
                                              const void* buffer, uint32_t buffer_size,
                                              dmResource::SResourceDescriptor* resource,
                                              const char* filename)
    {
        SpriteResource tmp_sprite_resource;
        memset(&tmp_sprite_resource, 0, sizeof(SpriteResource));
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &tmp_sprite_resource.m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::Result r = AcquireResources(factory, &tmp_sprite_resource, filename);
        if (r == dmResource::RESULT_OK)
        {
            SpriteResource* sprite_resource = (SpriteResource*)resource->m_Resource;
            ReleaseResources(factory, sprite_resource);
            *sprite_resource = tmp_sprite_resource;
        }
        else
        {
            ReleaseResources(factory, &tmp_sprite_resource);
        }
        return r;
    }
}
