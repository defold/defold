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
        if(dmRender::GetMaterialVertexSpace(resource->m_Material) != dmRenderDDF::MaterialDesc::VERTEX_SPACE_WORLD)
        {
            dmLogError("Failed to create Sprite component. Material vertex space option only supports VERTEX_SPACE_WORLD");
            return dmResource::RESULT_NOT_SUPPORTED;
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

    dmResource::Result ResSpritePreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::SpriteDesc *ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
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
        }
        else
        {
            ReleaseResources(params.m_Factory, &tmp_sprite_resource);
        }
        return r;
    }
}
