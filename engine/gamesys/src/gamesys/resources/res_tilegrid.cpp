#include "res_tilegrid.h"

#include <dlib/log.h>
#include <dlib/math.h>
#include <vectormath/ppu/cpp/vec_aos.h>
#include <render/render.h>
#include <graphics/graphics.h>

#include "gamesys.h"
#include "gamesys_ddf.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    dmResource::Result AcquireResources(dmPhysics::HContext2D context, dmResource::HFactory factory, dmGameSystemDDF::TileGrid* tile_grid_ddf,
                          TileGridResource* tile_grid, const char* filename)
    {
        dmResource::Result result;
        size_t texture_count = dmMath::Min(tile_grid_ddf->m_Textures.m_Count, dmRender::RenderObject::MAX_TEXTURE_COUNT);
        for(uint32_t i = 0; i < texture_count; ++i)
        {
            const char* texture = tile_grid_ddf->m_Textures[i];
            if (*texture == 0)
                continue;
            result = dmResource::Get(factory, texture, (void**) &tile_grid->m_Textures[i]);
            if (result != dmResource::RESULT_OK)
                return result;
        }
        result = dmResource::Get(factory, tile_grid_ddf->m_TileSet, (void**)&tile_grid->m_TextureSet);
        if (result != dmResource::RESULT_OK)
            return result;
        result = dmResource::Get(factory, tile_grid_ddf->m_Material, (void**)&tile_grid->m_Material);
        if (result != dmResource::RESULT_OK)
        {
            return result;
        }

        // Add-alpha is deprecated because of premultiplied alpha and replaced by Add
        if (tile_grid_ddf->m_BlendMode == dmGameSystemDDF::TileGrid::BLEND_MODE_ADD_ALPHA)
            tile_grid_ddf->m_BlendMode = dmGameSystemDDF::TileGrid::BLEND_MODE_ADD;
        tile_grid->m_TileGrid = tile_grid_ddf;
        TextureSetResource* texture_set = tile_grid->m_TextureSet;
        dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
        dmPhysics::HHullSet2D hull_set = texture_set->m_HullSet;
        if (hull_set != 0x0)
        {
            // Calculate AABB for offset
            Point3 offset(0.0f, 0.0f, 0.0f);
            int32_t min_x = INT32_MAX;
            int32_t min_y = INT32_MAX;
            int32_t max_x = INT32_MIN;
            int32_t max_y = INT32_MIN;
            uint32_t layer_count = tile_grid_ddf->m_Layers.m_Count;
            tile_grid->m_GridShapes.SetCapacity(layer_count);
            tile_grid->m_GridShapes.SetSize(layer_count);
            // find boundaries
            for (uint32_t i = 0; i < layer_count; ++i)
            {
                dmGameSystemDDF::TileLayer* layer = &tile_grid_ddf->m_Layers[i];
                uint32_t cell_count = layer->m_Cell.m_Count;
                for (uint32_t j = 0; j < cell_count; ++j)
                {
                    dmGameSystemDDF::TileCell* cell = &layer->m_Cell[j];
                    min_x = dmMath::Min(min_x, cell->m_X);
                    min_y = dmMath::Min(min_y, cell->m_Y);
                    max_x = dmMath::Max(max_x, cell->m_X + 1);
                    max_y = dmMath::Max(max_y, cell->m_Y + 1);
                }
            }
            tile_grid->m_ColumnCount = max_x - min_x;
            tile_grid->m_RowCount = max_y - min_y;
            tile_grid->m_MinCellX = min_x;
            tile_grid->m_MinCellY = min_y;
            uint32_t cell_width = texture_set_ddf->m_TileWidth;
            uint32_t cell_height = texture_set_ddf->m_TileHeight;
            offset.setX(cell_width * 0.5f * (min_x + max_x));
            offset.setY(cell_height * 0.5f * (min_y + max_y));
            for (uint32_t i = 0; i < layer_count; ++i)
            {
                tile_grid->m_GridShapes[i] = dmPhysics::NewGridShape2D(context, hull_set, offset, cell_width, cell_height, tile_grid->m_RowCount, tile_grid->m_ColumnCount);
            }
        }
        return result;
    }

    void ReleaseResources(dmResource::HFactory factory, TileGridResource* tile_grid)
    {
        if (tile_grid->m_TextureSet)
            dmResource::Release(factory, tile_grid->m_TextureSet);

        if (tile_grid->m_Material)
            dmResource::Release(factory, tile_grid->m_Material);

        for(uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (tile_grid->m_Textures[i] != 0x0)
                dmResource::Release(factory, tile_grid->m_Textures[i]);
        }

        if (tile_grid->m_TileGrid)
            dmDDF::FreeMessage(tile_grid->m_TileGrid);

        uint32_t n = tile_grid->m_GridShapes.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (tile_grid->m_GridShapes[i])
                dmPhysics::DeleteCollisionShape2D(tile_grid->m_GridShapes[i]);
        }
    }

    dmResource::Result ResTileGridPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::TileGrid* tile_grid_ddf;
        dmDDF::Result e  = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &tile_grid_ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        for(uint32_t i = 0; i < tile_grid_ddf->m_Textures.m_Count; ++i)
        {
            const char* texture = tile_grid_ddf->m_Textures[i];
            if (*texture == 0)
                continue;
            dmResource::PreloadHint(params.m_HintInfo, texture);
        }

        dmResource::PreloadHint(params.m_HintInfo, tile_grid_ddf->m_TileSet);
        dmResource::PreloadHint(params.m_HintInfo, tile_grid_ddf->m_Material);

        *params.m_PreloadData = tile_grid_ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTileGridCreate(const dmResource::ResourceCreateParams& params)
    {
        TileGridResource* tile_grid = new TileGridResource();
        dmGameSystemDDF::TileGrid* tile_grid_ddf = (dmGameSystemDDF::TileGrid*) params.m_PreloadData;

        dmResource::Result r = AcquireResources(((PhysicsContext*) params.m_Context)->m_Context2D, params.m_Factory, tile_grid_ddf, tile_grid, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) tile_grid;
        }
        else
        {
            ReleaseResources(params.m_Factory, tile_grid);
            delete tile_grid;
        }
        return r;
    }

    dmResource::Result ResTileGridDestroy(const dmResource::ResourceDestroyParams& params)
    {
        TileGridResource* tile_grid = (TileGridResource*) params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, tile_grid);
        delete tile_grid;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResTileGridRecreate(const dmResource::ResourceRecreateParams& params)
    {
        // TODO: Reload is temporarily disabled until issue 678 is fixed
//        TileGridResource* tile_grid = (TileGridResource*)params.m_Resource->m_Resource;
//        TileGridResource tmp_tile_grid;
//        dmResource::Result r = AcquireResources(((PhysicsContext*) params.m_Context)->m_Context2D, factory, buffer, buffer_size, &tmp_tile_grid, filename);
//        if (r == dmResource::RESULT_OK)
//        {
//            ReleaseResources(factory, tile_grid);
//            tile_grid->m_TileGrid = tmp_tile_grid.m_TileGrid;
//            tile_grid->m_TileSet = tmp_tile_grid.m_TileSet;
//            tile_grid->m_GridShape = tmp_tile_grid.m_GridShape;
//        }
//        else
//        {
//            ReleaseResources(factory, &tmp_tile_grid);
//        }
//        return r;
        return dmResource::RESULT_OK;
    }
}
