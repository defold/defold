#include <string.h>
#include "res_textureset.h"

#include <render/render_ddf.h>

#include "../gamesys.h"

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmPhysics::HContext2D context, dmResource::HFactory factory,  dmGameSystemDDF::TextureSet* texture_set_ddf,
                                        TextureSetResource* tile_set, const char* filename, bool reload)
    {
        if (reload)
        {
            // Will pick up the actual pointer when running get. This is a poor man's rebuild dependency
            // tracking. The editor could in theory send a reload command for the texture as well, but for
            // now trigger it manually here.
            dmResource::Result r = dmResource::ReloadResource(factory, texture_set_ddf->m_Texture, 0);
            if (r != dmResource::RESULT_OK)
            {
                return r;
            }
        }

        dmResource::Result r = dmResource::Get(factory, texture_set_ddf->m_Texture, (void**)&tile_set->m_Texture);
        if (r == dmResource::RESULT_OK)
        {
            // Get path for texture
            r = dmResource::GetPath(factory, tile_set->m_Texture, &tile_set->m_TexturePath);
            if (r != dmResource::RESULT_OK) {
                return r;
            }

            tile_set->m_TextureSet = texture_set_ddf;
            uint16_t width = dmGraphics::GetOriginalTextureWidth(tile_set->m_Texture);
            uint16_t height = dmGraphics::GetOriginalTextureHeight(tile_set->m_Texture);
            // Check dimensions
            if (width < texture_set_ddf->m_TileWidth || height < texture_set_ddf->m_TileHeight)
            {
                return dmResource::RESULT_INVALID_DATA;
            }
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
            uint32_t n_points = texture_set_ddf->m_ConvexHullPoints.m_Count / 2;
            float recip_tile_width = 1.0f / (texture_set_ddf->m_TileWidth - 1);
            float recip_tile_height = 1.0f / (texture_set_ddf->m_TileHeight - 1);
            float* points = texture_set_ddf->m_ConvexHullPoints.m_Data;
            float* norm_points = new float[n_points * 2];
            for (uint32_t i = 0; i < n_points; ++i)
            {
                norm_points[i*2] = (points[i*2]) * recip_tile_width - 0.5f;
                norm_points[i*2+1] = (points[i*2+1]) * recip_tile_height - 0.5f;
            }
            tile_set->m_HullSet = dmPhysics::NewHullSet2D(context, norm_points, n_points, hull_descs, n_hulls);
            delete [] hull_descs;
            delete [] norm_points;

            uint32_t n_animations = texture_set_ddf->m_Animations.m_Count;
            tile_set->m_AnimationIds.Clear();
            // NOTE: 37 is rather arbitrary but probably quite reasonable for most hash-table sizes
            tile_set->m_AnimationIds.SetCapacity(37, n_animations);
            for (uint32_t i = 0; i < n_animations; ++i)
            {
                dmhash_t h = dmHashString64(texture_set_ddf->m_Animations[i].m_Id);
                tile_set->m_AnimationIds.Put(h, i);
            }
        }
        else
        {
            dmDDF::FreeMessage(texture_set_ddf);
        }
        return r;
    }

    void ReleaseResources(dmResource::HFactory factory, TextureSetResource* tile_set)
    {
        if (tile_set->m_Texture)
            dmResource::Release(factory, tile_set->m_Texture);

        if (tile_set->m_TextureSet)
            dmDDF::FreeMessage(tile_set->m_TextureSet);

        if (tile_set->m_HullSet)
            dmPhysics::DeleteHullSet2D(tile_set->m_HullSet);
    }

    dmResource::Result ResTextureSetPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::TextureSet* texture_set_ddf;
        dmDDF::Result e  = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &texture_set_ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, texture_set_ddf->m_Texture);

        *params.m_PreloadData = texture_set_ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTextureSetCreate(const dmResource::ResourceCreateParams& params)
    {
        TextureSetResource* tile_set = new TextureSetResource();

        dmResource::Result r = AcquireResources(((PhysicsContext*) params.m_Context)->m_Context2D, params.m_Factory, (dmGameSystemDDF::TextureSet*) params.m_PreloadData, tile_set, params.m_Filename, false);
        if (r == dmResource::RESULT_OK)
        {
            tile_set->m_DDFSize = params.m_BufferSize;
            params.m_Resource->m_Resource = (void*) tile_set;
        }
        else
        {
            ReleaseResources(params.m_Factory, tile_set);
            delete tile_set;
        }
        return r;
    }

    dmResource::Result ResTextureSetDestroy(const dmResource::ResourceDestroyParams& params)
    {
        TextureSetResource* tile_set = (TextureSetResource*) params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, tile_set);
        delete tile_set;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTextureSetRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmGameSystemDDF::TextureSet* texture_set_ddf;
        dmDDF::Result e  = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &texture_set_ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        TextureSetResource* tile_set = (TextureSetResource*)params.m_Resource->m_Resource;
        TextureSetResource tmp_tile_set;

        dmResource::Result r = AcquireResources(((PhysicsContext*) params.m_Context)->m_Context2D, params.m_Factory, texture_set_ddf, &tmp_tile_set, params.m_Filename, true);
        if (r == dmResource::RESULT_OK)
        {
            ReleaseResources(params.m_Factory, tile_set);
            tile_set->m_TextureSet = tmp_tile_set.m_TextureSet;
            tile_set->m_Texture = tmp_tile_set.m_Texture;
            tile_set->m_HullCollisionGroups.Swap(tmp_tile_set.m_HullCollisionGroups);
            tile_set->m_HullSet = tmp_tile_set.m_HullSet;
            tile_set->m_AnimationIds.Swap(tmp_tile_set.m_AnimationIds);
            tile_set->m_DDFSize = params.m_BufferSize;
        }
        else
        {
            ReleaseResources(params.m_Factory, &tmp_tile_set);
        }
        return r;
    }

    dmResource::Result ResTextureSetGetInfo(dmResource::ResourceGetInfoParams& params)
    {
        TextureSetResource* res  = (TextureSetResource*) params.m_Resource->m_Resource;
        uint32_t size = sizeof(TextureSetResource);
        size += res->m_DDFSize;
        size += res->m_HullCollisionGroups.Capacity()*sizeof(dmhash_t);
        size += res->m_AnimationIds.Capacity()*(sizeof(dmhash_t)+sizeof(uint32_t));
        params.m_DataSize = size;
        params.m_SubResourceIds->SetCapacity(1);
        params.m_SubResourceIds->Push(res->m_TexturePath);
        // TODO: get size of dmPhysics::HullSet2D
        return dmResource::RESULT_OK;
    }

}
