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

#include <string.h>
#include <math.h>
#include <dlib/log.h>
#include "res_textureset.h"

#include <dlib/math.h>
#include <render/render_ddf.h>
#include <physics/physics.h>

#include "../gamesys.h"

namespace dmGameSystem
{
    struct VectorAtlasTextures
    {
        dmGraphics::HContext m_Context;
        dmGraphics::HTexture m_Textures[3];
    };

    bool IsVectorTextureSet(const TextureSetResource* resource)
    {
        return resource && resource->m_TextureSet->m_Mode == dmGameSystemDDF::TEXTURE_SET_MODE_VECTOR;
    }

    dmGraphics::HTexture GetVectorAtlasTexture(const TextureSetResource* resource, uint32_t index)
    {
        return resource->m_VectorTextures && index < 3 ? resource->m_VectorTextures->m_Textures[index] : 0;
    }

    static dmResource::Result CreateVectorTextures(TextureSetResource* resource)
    {
        const dmGameSystemDDF::TextureSet*  ddf = resource->m_TextureSet;
        const dmGameSystemDDF::VectorAtlas* data = &ddf->m_Vector;
        if (ddf->m_Texture[0] || ddf->m_TileWidth || ddf->m_TileHeight || ddf->m_UseGeometries ||
            data->m_Images.m_Count != ddf->m_Geometries.m_Count || data->m_Images.m_Count != ddf->m_TileCount ||
            ddf->m_ImageNameHashes.m_Count != ddf->m_FrameIndices.m_Count)
            return dmResource::RESULT_INVALID_DATA;
        uint32_t                        sizes[3] = { data->m_Curves.m_Count, data->m_Bands.m_Count, data->m_Paints.m_Count };
        const void*                     buffers[3] = { data->m_Curves.m_Data, data->m_Bands.m_Data, data->m_Paints.m_Data };
        const uint32_t                  strides[3] = { 8, 4, 16 };
        const dmGraphics::TextureFormat formats[3] = { dmGraphics::TEXTURE_FORMAT_RGBA16F, dmGraphics::TEXTURE_FORMAT_R32UI, dmGraphics::TEXTURE_FORMAT_RGBA32F };
        dmGraphics::HContext            context = dmGraphics::GetInstalledContext();
        for (uint32_t i = 0; i < 3; ++i)
        {
            if (!sizes[i] || sizes[i] % (4096 * strides[i]) || sizes[i] > 4096 * 256 * strides[i])
                return dmResource::RESULT_INVALID_DATA;
            if (!dmGraphics::IsTextureFormatSupported(context, formats[i]))
            {
                dmLogError("SVG atlases require RGBA16F, R32UI and RGBA32F numeric textures");
                return dmResource::RESULT_NOT_SUPPORTED;
            }
        }
        for (uint32_t i = 0; i < data->m_Images.m_Count; ++i)
        {
            const dmGameSystemDDF::VectorImage& image = data->m_Images[i];
            if (!isfinite(image.m_Duration) || image.m_Duration < 0 || !isfinite(image.m_Width) || !isfinite(image.m_Height) || image.m_Width <= 0 || image.m_Height <= 0 ||
                image.m_Paint >= sizes[2] / 16)
                return dmResource::RESULT_INVALID_DATA;
        }
        for (uint32_t i = 0; i < ddf->m_FrameIndices.m_Count; ++i)
            if (ddf->m_FrameIndices[i] >= data->m_Images.m_Count)
                return dmResource::RESULT_INVALID_DATA;
        for (uint32_t i = 0; i < ddf->m_Animations.m_Count; ++i)
        {
            const dmGameSystemDDF::TextureSetAnimation& animation = ddf->m_Animations[i];
            if (animation.m_Start >= animation.m_End || animation.m_End > ddf->m_FrameIndices.m_Count ||
                !isfinite(animation.m_Duration) || animation.m_Duration < 0)
                return dmResource::RESULT_INVALID_DATA;
            if (animation.m_Duration == 0)
                continue;
            if (animation.m_FrameDurations.m_Count != animation.m_End - animation.m_Start)
                return dmResource::RESULT_INVALID_DATA;
            for (uint32_t frame = 0; frame < animation.m_FrameDurations.m_Count; ++frame)
                if (!isfinite(animation.m_FrameDurations[frame]) || animation.m_FrameDurations[frame] <= 0)
                    return dmResource::RESULT_INVALID_DATA;
        }
        resource->m_VectorTextures = new VectorAtlasTextures();
        resource->m_VectorTextures->m_Context = context;
        for (uint32_t i = 0; i < 3; ++i)
        {
            dmGraphics::TextureCreationParams creation;
            creation.m_Width = creation.m_OriginalWidth = 4096;
            creation.m_Height = creation.m_OriginalHeight = sizes[i] / (4096 * strides[i]);
            dmGraphics::HTexture texture = dmGraphics::NewTexture(context, creation);
            resource->m_VectorTextures->m_Textures[i] = texture;
            if (!texture)
                return dmResource::RESULT_OUT_OF_MEMORY;
            dmGraphics::TextureParams params;
            params.m_Width = creation.m_Width;
            params.m_Height = creation.m_Height;
            params.m_Depth = 1;
            params.m_Format = formats[i];
            params.m_MinFilter = params.m_MagFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
            params.m_UWrap = params.m_VWrap = dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE;
            params.m_Data = buffers[i];
            params.m_DataSize = sizes[i];
            dmGraphics::SetTexture(context, texture, params);
        }
        return dmResource::RESULT_OK;
    }

    dmResource::Result AcquireResources(dmPhysics::HContext2D context, dmResource::HFactory factory, dmGameSystemDDF::TextureSet* texture_set_ddf, TextureSetResource* tile_set, const char* filename, bool reload)
    {
        dmResource::Result r = dmResource::RESULT_OK;

        tile_set->m_TextureSet = texture_set_ddf;
        if (IsVectorTextureSet(tile_set))
        {
            r = CreateVectorTextures(tile_set);
            if (r != dmResource::RESULT_OK)
                return r;
        }
        else
        {
            TextureResource* texture_res = 0;

            // Get by hash if texture is a dynamically created resource
            if (texture_set_ddf->m_TextureHash)
            {
                r = dmResource::Get(factory, texture_set_ddf->m_TextureHash, (void**)&texture_res);
            }
            else
            {
                r = dmResource::Get(factory, texture_set_ddf->m_Texture, (void**)&texture_res);
            }

            tile_set->m_Texture = texture_res;

            if (r != dmResource::RESULT_OK)
            {
                return r;
            }

            // Get path for texture
            r = dmResource::GetPath(factory, texture_res, &tile_set->m_TexturePath);
            if (r != dmResource::RESULT_OK)
            {
                return r;
            }

            uint16_t width = texture_res->m_OriginalWidth;
            uint16_t height = texture_res->m_OriginalHeight;

            // Check dimensions
            if (width < texture_set_ddf->m_TileWidth || height < texture_set_ddf->m_TileHeight)
            {
                return dmResource::RESULT_INVALID_DATA;
            }

            // Physics convex hulls
            {
                uint32_t n_hulls = texture_set_ddf->m_ConvexHulls.m_Count;
                tile_set->m_HullCollisionGroups.SetCapacity(n_hulls);
                tile_set->m_HullCollisionGroups.SetSize(n_hulls);
                dmPhysics::HullDesc* hull_descs = new dmPhysics::HullDesc[n_hulls];
                for (uint32_t i = 0; i < n_hulls; ++i)
                {
                    dmGameSystemDDF::ConvexHull* hull_ddf = &texture_set_ddf->m_ConvexHulls[i];
                    tile_set->m_HullCollisionGroups[i] = dmHashString64(hull_ddf->m_CollisionGroup);
                    hull_descs[i].m_Index = (uint16_t)hull_ddf->m_Index;
                    hull_descs[i].m_Count = (uint16_t)hull_ddf->m_Count;
                }
                uint32_t n_points = texture_set_ddf->m_CollisionHullPoints.m_Count / 2;
                float    recip_tile_width = 1.0f / (texture_set_ddf->m_TileWidth - 1);
                float    recip_tile_height = 1.0f / (texture_set_ddf->m_TileHeight - 1);
                float*   points = texture_set_ddf->m_CollisionHullPoints.m_Data;
                float*   norm_points = new float[n_points * 2];
                for (uint32_t i = 0; i < n_points; ++i)
                {
                    norm_points[i * 2] = (points[i * 2]) * recip_tile_width - 0.5f;
                    norm_points[i * 2 + 1] = (points[i * 2 + 1]) * recip_tile_height - 0.5f;
                }
                tile_set->m_HullSet = dmPhysics::NewHullSet2D(context, norm_points, n_points, hull_descs, n_hulls);
                delete[] hull_descs;
                delete[] norm_points;
            }
        }

        uint32_t n_animations = texture_set_ddf->m_Animations.m_Count;
        tile_set->m_AnimationIds.Clear();
        tile_set->m_AnimationIds.SetCapacity(dmMath::Max(1U, (2*n_animations)/3), n_animations);
        for (uint32_t i = 0; i < n_animations; ++i)
        {
            dmhash_t h = dmHashString64(texture_set_ddf->m_Animations[i].m_Id);
            tile_set->m_AnimationIds.Put(h, i);
        }

        // This is a mapping from the single frame image names (animation ids) to the frame number
        const uint32_t* frame_indices = texture_set_ddf->m_FrameIndices.m_Data;
        uint32_t n_image_name_hashes = texture_set_ddf->m_ImageNameHashes.m_Count;
        tile_set->m_FrameIds.SetCapacity(dmMath::Max(1U, (n_image_name_hashes*2)/3), n_image_name_hashes);
        for (uint32_t i = 0; i < n_image_name_hashes; ++i)
        {
            dmhash_t h = texture_set_ddf->m_ImageNameHashes[i];
            uint32_t frame_index = frame_indices[i];
            tile_set->m_FrameIds.Put(h, frame_index);
        }

        return r;
    }

    void ReleaseResources(dmResource::HFactory factory, TextureSetResource* tile_set)
    {
        if (tile_set->m_VectorTextures)
        {
            for (uint32_t i = 0; i < 3; ++i)
                if (tile_set->m_VectorTextures->m_Textures[i])
                    dmGraphics::DeleteTexture(tile_set->m_VectorTextures->m_Context, tile_set->m_VectorTextures->m_Textures[i]);
            delete tile_set->m_VectorTextures;
            tile_set->m_VectorTextures = 0;
        }
        if (tile_set->m_Texture)
            dmResource::Release(factory, tile_set->m_Texture);

        if (tile_set->m_TextureSet)
            dmDDF::FreeMessage(tile_set->m_TextureSet);

        if (tile_set->m_HullSet)
            dmPhysics::DeleteHullSet2D(tile_set->m_HullSet);
        tile_set->m_TexturesGeneration = 0;
    }

    static uint32_t GetResourceSize(TextureSetResource* res, uint32_t ddf_size)
    {
        uint32_t size = sizeof(TextureSetResource);
        size += ddf_size;
        size += res->m_HullCollisionGroups.Capacity() * sizeof(dmhash_t);
        size += res->m_AnimationIds.Capacity() * (sizeof(dmhash_t) + sizeof(uint32_t));
        size += res->m_FrameIds.Capacity() * (sizeof(dmhash_t) + sizeof(uint32_t));
        return size;
    }

    dmResource::Result ResTextureSetPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmGameSystemDDF::TextureSet* texture_set_ddf;
        dmDDF::Result e  = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &texture_set_ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        if (texture_set_ddf->m_Mode == dmGameSystemDDF::TEXTURE_SET_MODE_RASTER)
            dmResource::PreloadHint(params->m_HintInfo, texture_set_ddf->m_Texture);

        *params->m_PreloadData = texture_set_ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTextureSetCreate(const dmResource::ResourceCreateParams* params)
    {
        TextureSetResource* tile_set = new TextureSetResource();

        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*) params->m_Context;
        dmResource::Result r = AcquireResources(physics_context->m_Context, params->m_Factory, (dmGameSystemDDF::TextureSet*) params->m_PreloadData, tile_set, params->m_Filename, false);
        if (r == dmResource::RESULT_OK)
        {
            if (IsVectorTextureSet(tile_set))
                tile_set->m_TexturePath = dmResource::GetNameHash(params->m_Resource);
            dmResource::SetResource(params->m_Resource, tile_set);
            dmResource::SetResourceSize(params->m_Resource, GetResourceSize(tile_set, params->m_BufferSize));
        }
        else
        {
            ReleaseResources(params->m_Factory, tile_set);
            delete tile_set;
        }
        return r;
    }

    dmResource::Result ResTextureSetDestroy(const dmResource::ResourceDestroyParams* params)
    {
        TextureSetResource* tile_set = (TextureSetResource*) dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, tile_set);
        delete tile_set;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTextureSetRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmGameSystemDDF::TextureSet* texture_set_ddf;
        dmDDF::Result e  = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &texture_set_ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        PhysicsContextBox2D* physics_context = (PhysicsContextBox2D*) params->m_Context;
        TextureSetResource* tile_set = (TextureSetResource*)dmResource::GetResource(params->m_Resource);
        // Dependents must never observe a raster atlas becoming a vector atlas
        // (or the reverse) before their material and component are reloaded.
        if (texture_set_ddf->m_Mode != tile_set->m_TextureSet->m_Mode)
        {
            dmLogError("Atlas mode changed during hot reload. Restart the scene to change between raster and SVG content.");
            dmDDF::FreeMessage(texture_set_ddf);
            return dmResource::RESULT_NOT_SUPPORTED;
        }
        TextureSetResource tmp_tile_set;
        dmResource::Result r = AcquireResources(physics_context->m_Context, params->m_Factory, texture_set_ddf, &tmp_tile_set, params->m_Filename, true);
        if (r == dmResource::RESULT_OK)
        {
            if (IsVectorTextureSet(&tmp_tile_set))
                tmp_tile_set.m_TexturePath = dmResource::GetNameHash(params->m_Resource);
            uint8_t current_generation = tile_set->m_TexturesGeneration;
            ReleaseResources(params->m_Factory, tile_set);

            tile_set->m_TextureSet = tmp_tile_set.m_TextureSet;
            tile_set->m_Texture = tmp_tile_set.m_Texture;
            tile_set->m_TexturePath = tmp_tile_set.m_TexturePath;
            tile_set->m_VectorTextures = tmp_tile_set.m_VectorTextures;
            tile_set->m_HullCollisionGroups.Swap(tmp_tile_set.m_HullCollisionGroups);
            tile_set->m_HullSet = tmp_tile_set.m_HullSet;
            tile_set->m_AnimationIds.Swap(tmp_tile_set.m_AnimationIds);
            tile_set->m_FrameIds.Swap(tmp_tile_set.m_FrameIds);
            dmResource::SetResourceSize(params->m_Resource, GetResourceSize(tile_set, params->m_BufferSize));
            tile_set->m_TexturesGeneration = current_generation + 1;
        }
        else
        {
            ReleaseResources(params->m_Factory, &tmp_tile_set);
        }
        return r;
    }
}
