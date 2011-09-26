#include "res_tileset.h"

#include <render/render_ddf.h>

#include "../gamesys.h"

namespace dmGameSystem
{
    bool AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size,
                          TileSetResource* tile_set, const char* filename)
    {
        dmGameSystemDDF::TileSet* tile_set_ddf;
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &tile_set_ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return false;
        }

        bool result = true;
        if (dmResource::FACTORY_RESULT_OK == dmResource::Get(factory, tile_set_ddf->m_Image, (void**)&tile_set->m_Texture))
        {
            tile_set->m_TileSet = tile_set_ddf;
            uint32_t n_hulls = tile_set_ddf->m_ConvexHulls.m_Count;
            tile_set->m_ConvexHulls = new TileSetResource::ConvexHull[n_hulls];
            for (uint32_t i = 0; i < n_hulls; ++i)
            {
                TileSetResource::ConvexHull* hull = &tile_set->m_ConvexHulls[i];
                dmGameSystemDDF::ConvexHull* hull_ddf = &tile_set_ddf->m_ConvexHulls[i];
                hull->m_Index = hull_ddf->m_Index;
                hull->m_Count = hull_ddf->m_Count;
                hull->m_CollisionGroupHash = dmHashString64(hull_ddf->m_CollisionGroup);
            }

            uint32_t n_points = tile_set_ddf->m_ConvexHullPoints.m_Count;
            tile_set->m_ConvexHullPoints = new float[n_points];
            memcpy(tile_set->m_ConvexHullPoints, tile_set_ddf->m_ConvexHullPoints.m_Data, sizeof(tile_set->m_ConvexHullPoints[0] * n_points));
        }
        else
        {
            dmDDF::FreeMessage(tile_set_ddf);
            result = false;
        }
        return result;
    }

    void ReleaseResources(dmResource::HFactory factory, TileSetResource* tile_set)
    {
        if (tile_set->m_Texture)
            dmResource::Release(factory, tile_set->m_Texture);

        if (tile_set->m_TileSet)
            dmDDF::FreeMessage(tile_set->m_TileSet);

        if (tile_set->m_ConvexHulls)
            delete[] tile_set->m_ConvexHulls;

        if (tile_set->m_ConvexHullPoints)
            delete[] tile_set->m_ConvexHullPoints;
    }

    dmResource::CreateResult ResTileSetCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        TileSetResource* tile_set = new TileSetResource();

        if (AcquireResources(factory, buffer, buffer_size, tile_set, filename))
        {
            resource->m_Resource = (void*) tile_set;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, tile_set);
            delete tile_set;
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResTileSetDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        TileSetResource* tile_set = (TileSetResource*) resource->m_Resource;
        ReleaseResources(factory, tile_set);
        delete tile_set;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResTileSetRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        TileSetResource* tile_set = (TileSetResource*)resource->m_Resource;
        TileSetResource tmp_tile_set;
        if (AcquireResources(factory, buffer, buffer_size, &tmp_tile_set, filename))
        {
            ReleaseResources(factory, tile_set);
            tile_set->m_TileSet = tmp_tile_set.m_TileSet;
            tile_set->m_Texture = tmp_tile_set.m_Texture;
            tile_set->m_ConvexHulls = tmp_tile_set.m_ConvexHulls;
            tile_set->m_ConvexHullPoints = tmp_tile_set.m_ConvexHullPoints;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, &tmp_tile_set);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }
}
