// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "res_sprite.h"

#include <string.h>

#include <dlib/log.h>

#include <gamesys/sprite_ddf.h>

#include <dmsdk/gamesys/resources/res_material.h>
#include <dmsdk/gamesys/resources/res_textureset.h>

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory, SpriteResource* resource, const char* filename)
    {
        // Add-alpha is deprecated because of premultiplied alpha and replaced by Add
        if (resource->m_DDF->m_BlendMode == dmGameSystemDDF::SpriteDesc::BLEND_MODE_ADD_ALPHA)
            resource->m_DDF->m_BlendMode = dmGameSystemDDF::SpriteDesc::BLEND_MODE_ADD;


        dmResource::Result fr = dmResource::RESULT_OK;

        uint32_t num_textures = resource->m_DDF->m_Textures.m_Count;
        if (num_textures)
        {
            resource->m_NumTextures = num_textures;
            resource->m_Textures = (SpriteTexture*)malloc(num_textures * sizeof(SpriteTexture));
            memset(resource->m_Textures, 0, num_textures * sizeof(SpriteTexture));

            dmResource::Result fr = dmResource::RESULT_OK;
            for (uint32_t i = 0; i < num_textures; ++i)
            {
                fr = dmResource::Get(factory, resource->m_DDF->m_Textures[i].m_Texture, (void**)&resource->m_Textures[i].m_TextureSet);
                if (fr != dmResource::RESULT_OK)
                {
                    return fr;
                }
                resource->m_Textures[i].m_SamplerNameHash = dmHashString64(resource->m_DDF->m_Textures[i].m_Sampler);
            }
        }

        fr = dmResource::Get(factory, resource->m_DDF->m_Material, (void**)&resource->m_Material);
        if (fr != dmResource::RESULT_OK)
        {
            return fr;
        }
        if(dmRender::GetMaterialVertexSpace(resource->m_Material->m_Material) != dmRenderDDF::MaterialDesc::VERTEX_SPACE_WORLD)
        {
            dmLogError("Failed to create Sprite component. This component only supports materials with the Vertex Space property set to 'vertex-space-world'");
            return dmResource::RESULT_NOT_SUPPORTED;
        }
        resource->m_DefaultAnimation = dmHashString64(resource->m_DDF->m_DefaultAnimation);
        if (num_textures && !resource->m_Textures[0].m_TextureSet->m_AnimationIds.Get(resource->m_DefaultAnimation))
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

        for (uint32_t i = 0; i < resource->m_NumTextures; ++i)
        {
            if (resource->m_Textures[i].m_TextureSet)
                dmResource::Release(factory, resource->m_Textures[i].m_TextureSet);
        }
        free((void*)resource->m_Textures);
    }

    dmResource::Result ResSpritePreload(const dmResource::ResourcePreloadParams* params)
    {
        dmGameSystemDDF::SpriteDesc *ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        uint32_t num_textures = ddf->m_Textures.m_Count;
        if (num_textures)
        {
            for (uint32_t i = 0; i < num_textures; ++i)
            {
                dmResource::PreloadHint(params->m_HintInfo, ddf->m_Textures[i].m_Texture);
            }
        }

        if (ddf->m_TileSet[0] != '\0')
        {
            dmLogInfo("Using tilesets for sprites is deprecated. '%s' will not be loaded or used.", ddf->m_TileSet);
        }

        dmResource::PreloadHint(params->m_HintInfo, ddf->m_Material);

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSpriteCreate(const dmResource::ResourceCreateParams* params)
    {
        SpriteResource* sprite_resource = new SpriteResource();
        memset(sprite_resource, 0, sizeof(SpriteResource));
        sprite_resource->m_DDF = (dmGameSystemDDF::SpriteDesc*) params->m_PreloadData;

        dmResource::Result r = AcquireResources(params->m_Factory, sprite_resource, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, sprite_resource);
        }
        else
        {
            ReleaseResources(params->m_Factory, sprite_resource);
            delete sprite_resource;
        }
        return r;
    }

    dmResource::Result ResSpriteDestroy(const dmResource::ResourceDestroyParams* params)
    {
        SpriteResource* sprite_resource = (SpriteResource*) dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, sprite_resource);
        delete sprite_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSpriteRecreate(const dmResource::ResourceRecreateParams* params)
    {
        SpriteResource tmp_sprite_resource;
        memset(&tmp_sprite_resource, 0, sizeof(SpriteResource));
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &tmp_sprite_resource.m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::Result r = AcquireResources(params->m_Factory, &tmp_sprite_resource, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            SpriteResource* sprite_resource = (SpriteResource*)dmResource::GetResource(params->m_Resource);
            ReleaseResources(params->m_Factory, sprite_resource);
            *sprite_resource = tmp_sprite_resource;
        }
        else
        {
            ReleaseResources(params->m_Factory, &tmp_sprite_resource);
        }
        return r;
    }
}
