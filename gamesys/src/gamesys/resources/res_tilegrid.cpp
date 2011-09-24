#include "res_tilegrid.h"

#include "gamesys_ddf.h"

namespace dmGameSystem
{
    bool AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size,
                          TileGridResource* tile_grid, const char* filename)
    {
        dmGameSystemDDF::TileGrid* tile_grid_ddf;
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &tile_grid_ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return false;
        }

        bool result = true;
        if (dmResource::FACTORY_RESULT_OK == dmResource::Get(factory, tile_grid_ddf->m_TileSet, (void**)&tile_grid->m_TileSet))
        {
            tile_grid->m_TileGrid = tile_grid_ddf;
        }
        else
        {
            dmDDF::FreeMessage(tile_grid_ddf);
            result = false;
        }
        return result;
    }

    void ReleaseResources(dmResource::HFactory factory, TileGridResource* tile_grid)
    {
        if (tile_grid->m_TileSet)
            dmResource::Release(factory, tile_grid->m_TileSet);

        if (tile_grid->m_TileGrid)
            dmDDF::FreeMessage(tile_grid->m_TileGrid);
    }

    dmResource::CreateResult ResTileGridCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        TileGridResource* tile_grid = new TileGridResource();

        if (AcquireResources(factory, buffer, buffer_size, tile_grid, filename))
        {
            resource->m_Resource = (void*) tile_grid;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, tile_grid);
            delete tile_grid;
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResTileGridDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        TileGridResource* tile_grid = (TileGridResource*) resource->m_Resource;
        ReleaseResources(factory, tile_grid);
        delete tile_grid;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResTileGridRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        TileGridResource* tile_grid = (TileGridResource*)resource->m_Resource;
        TileGridResource tmp_tile_grid;
        if (AcquireResources(factory, buffer, buffer_size, &tmp_tile_grid, filename))
        {
            ReleaseResources(factory, tile_grid);
            tile_grid->m_TileGrid = tmp_tile_grid.m_TileGrid;
            tile_grid->m_TileSet = tmp_tile_grid.m_TileSet;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, &tmp_tile_grid);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }
}
