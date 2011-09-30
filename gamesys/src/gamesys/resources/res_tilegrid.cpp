#include "res_tilegrid.h"

#include <vectormath/ppu/cpp/vec_aos.h>
#include "gamesys_ddf.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    bool AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size,
                          TileGridResource* tile_grid, const char* filename)
    {
        dmGameSystemDDF::TileGrid* tile_grid_ddf;
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &tile_grid_ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return false;
        }

        if (dmResource::FACTORY_RESULT_OK == dmResource::Get(factory, tile_grid_ddf->m_TileSet, (void**)&tile_grid->m_TileSet))
        {
            tile_grid->m_TileGrid = tile_grid_ddf;
            TileSetResource* tile_set = tile_grid->m_TileSet;
            dmGameSystemDDF::TileSet* tile_set_ddf = tile_set->m_TileSet;
            dmPhysics::HHullSet2D hull_set = tile_set->m_HullSet;
            if (hull_set != 0x0)
            {
                // Calculate AABB for offset
                Point3 offset(0.0f, 0.0f, 0.0f);
                int32_t min_x = INT32_MAX;
                int32_t min_y = INT32_MAX;
                int32_t max_x = INT32_MIN;
                int32_t max_y = INT32_MIN;
                uint32_t layer_count = tile_grid_ddf->m_Layers.m_Count;
                uint32_t total_cell_count = 0;
                for (uint32_t i = 0; i < layer_count; ++i)
                {
                    dmGameSystemDDF::TileLayer* layer = &tile_grid_ddf->m_Layers[i];
                    uint32_t cell_count = layer->m_Cell.m_Count;
                    total_cell_count += cell_count;
                    for (uint32_t j = 0; j < cell_count; ++j)
                    {
                        dmGameSystemDDF::TileCell* cell = &layer->m_Cell[j];
                        if (min_x > cell->m_X)
                            min_x = cell->m_X;
                        if (min_y > cell->m_Y)
                            min_y = cell->m_Y;
                        if (max_x < cell->m_X + 1)
                            max_x = cell->m_X + 1;
                        if (max_y < cell->m_Y + 1)
                            max_y = cell->m_Y + 1;
                    }
                }
                if (total_cell_count > 0)
                {
                    uint32_t cell_width = tile_set_ddf->m_TileWidth;
                    uint32_t cell_height = tile_set_ddf->m_TileHeight;
                    tile_grid->m_ColumnCount = max_x - min_x;
                    tile_grid->m_MinCellX = min_x;
                    tile_grid->m_MinCellY = min_y;
                    offset.setX(cell_width * 0.5f * (min_x + max_x));
                    offset.setY(cell_height * 0.5f * (min_y + max_y));
                    tile_grid->m_GridShape = dmPhysics::NewGridShape2D(hull_set, offset, cell_width, cell_height, max_y - min_y, tile_grid->m_ColumnCount);
                    return true;
                }
            }
        }
        else
        {
            dmDDF::FreeMessage(tile_grid_ddf);
        }
        return false;
    }

    void ReleaseResources(dmResource::HFactory factory, TileGridResource* tile_grid)
    {
        if (tile_grid->m_TileSet)
            dmResource::Release(factory, tile_grid->m_TileSet);

        if (tile_grid->m_TileGrid)
            dmDDF::FreeMessage(tile_grid->m_TileGrid);

        if (tile_grid->m_GridShape)
            dmPhysics::DeleteCollisionShape2D(tile_grid->m_GridShape);
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
            tile_grid->m_GridShape = tmp_tile_grid.m_GridShape;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, &tmp_tile_grid);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }
}
