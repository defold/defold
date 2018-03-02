#include "res_sprite.h"

#include <string.h>

#include <dlib/log.h>

#include "sprite_ddf.h"

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory,
        SpriteResource* resource, const char* filename)
    {
        dmResource::Result result;

        // Add-alpha is deprecated because of premultiplied alpha and replaced by Add
        if (resource->m_DDF->m_BlendMode == dmGameSystemDDF::SpriteDesc::BLEND_MODE_ADD_ALPHA)
            resource->m_DDF->m_BlendMode = dmGameSystemDDF::SpriteDesc::BLEND_MODE_ADD;

        size_t texture_count = dmMath::Min(resource->m_DDF->m_Textures.m_Count, dmRender::RenderObject::MAX_TEXTURE_COUNT);
        for(uint32_t i = 0; i < texture_count; ++i)
        {
            const char* texture = resource->m_DDF->m_Textures[i];
            if (*texture == 0)
                continue;
            result = dmResource::Get(factory, texture, (void**) &resource->m_Textures[i]);
            if (result != dmResource::RESULT_OK)
                return result;
        }
        result = dmResource::Get(factory, resource->m_DDF->m_TileSet, (void**)&resource->m_TextureSet);
        if (result != dmResource::RESULT_OK)
            return result;
        result = dmResource::Get(factory, resource->m_DDF->m_Material, (void**)&resource->m_Material);
        if (result != dmResource::RESULT_OK)
        {
            return result;
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
        if (resource->m_Material != 0x0)
            dmResource::Release(factory, resource->m_Material);
        if (resource->m_TextureSet != 0x0)
            dmResource::Release(factory, resource->m_TextureSet);
        for(uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (resource->m_Textures[i] != 0x0)
                dmResource::Release(factory, resource->m_Textures[i]);
        }
    }

    dmResource::Result ResSpritePreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::SpriteDesc *ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        for(uint32_t i = 0; i < ddf->m_Textures.m_Count; ++i)
        {
            const char* texture = ddf->m_Textures[i];
            if (*texture == 0)
                continue;
            dmResource::PreloadHint(params.m_HintInfo, texture);
        }
        dmResource::PreloadHint(params.m_HintInfo, ddf->m_TileSet);
        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSpriteCreate(const dmResource::ResourceCreateParams& params)
    {
        SpriteResource* sprite_resource = new SpriteResource();
        memset(sprite_resource, 0, sizeof(SpriteResource));
        sprite_resource->m_DDF = (dmGameSystemDDF::SpriteDesc*) params.m_PreloadData;

        dmResource::Result r = AcquireResources(params.m_Factory, sprite_resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) sprite_resource;
            params.m_Resource->m_ResourceSize = sizeof(SpriteResource) + params.m_BufferSize;
        }
        else
        {
            ReleaseResources(params.m_Factory, sprite_resource);
            delete sprite_resource;
        }
        return r;
    }

    dmResource::Result ResSpriteDestroy(const dmResource::ResourceDestroyParams& params)
    {
        SpriteResource* sprite_resource = (SpriteResource*) params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, sprite_resource);
        delete sprite_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSpriteRecreate(const dmResource::ResourceRecreateParams& params)
    {
        SpriteResource tmp_sprite_resource;
        memset(&tmp_sprite_resource, 0, sizeof(SpriteResource));
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &tmp_sprite_resource.m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::Result r = AcquireResources(params.m_Factory, &tmp_sprite_resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            SpriteResource* sprite_resource = (SpriteResource*)params.m_Resource->m_Resource;
            ReleaseResources(params.m_Factory, sprite_resource);
            *sprite_resource = tmp_sprite_resource;
            params.m_Resource->m_ResourceSize = sizeof(SpriteResource) + params.m_BufferSize;
        }
        else
        {
            ReleaseResources(params.m_Factory, &tmp_sprite_resource);
        }
        return r;
    }
}
