// Copyright 2020-2024 The Defold Foundation
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

#include "res_tilegrid.h"

#include <dlib/log.h>
#include <dlib/math.h>
#include <dmsdk/dlib/vmath.h>

#include "gamesys.h"
#include <gamesys/gamesys_ddf.h>

#include <dmsdk/gamesys/resources/res_material.h>
#include <dmsdk/gamesys/resources/res_textureset.h>

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmPhysics::HContext2D context, dmResource::HFactory factory, dmGameSystemDDF::TileGrid* tile_grid_ddf,
                          TileGridResource* tile_grid, const char* filename, bool reload)
    {
        if (reload)
        {
            // Explicitly reload tileset (textureset) dependency
            dmResource::Result r = dmResource::ReloadResource(factory, tile_grid_ddf->m_TileSet, 0);
            if (r != dmResource::RESULT_OK)
            {
                return r;
            }
        }

        tile_grid->m_TileGrid = tile_grid_ddf;

        dmResource::Result r = dmResource::Get(factory, tile_grid_ddf->m_TileSet, (void**)&tile_grid->m_TextureSet);
        if (r != dmResource::RESULT_OK)
        {
            return r;
        }
        r = dmResource::Get(factory, tile_grid_ddf->m_Material, (void**)&tile_grid->m_Material);
        if (r != dmResource::RESULT_OK)
        {
            return r;
        }
        if(dmRender::GetMaterialVertexSpace(tile_grid->m_Material->m_Material) != dmRenderDDF::MaterialDesc::VERTEX_SPACE_WORLD)
        {
            dmLogError("Failed to create Tile Grid component. This component only supports materials with the Vertex Space property set to 'vertex-space-world'");
            return dmResource::RESULT_NOT_SUPPORTED;
        }

        // Add-alpha is deprecated because of premultiplied alpha and replaced by Add
        if (tile_grid_ddf->m_BlendMode == dmGameSystemDDF::TileGrid::BLEND_MODE_ADD_ALPHA)
            tile_grid_ddf->m_BlendMode = dmGameSystemDDF::TileGrid::BLEND_MODE_ADD;
        TextureSetResource* texture_set = tile_grid->m_TextureSet;

        // find boundaries
        int32_t min_x = INT32_MAX;
        int32_t min_y = INT32_MAX;
        int32_t max_x = INT32_MIN;
        int32_t max_y = INT32_MIN;
        for (uint32_t i = 0; i < tile_grid_ddf->m_Layers.m_Count; ++i)
        {
            dmGameSystemDDF::TileLayer* layer = &tile_grid_ddf->m_Layers[i];
            // we hash it here in the resource loading in order to make it easier to generate the tilemap
            // files from external tools
            layer->m_IdHash = dmHashString64(layer->m_Id);
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

        dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
        dmPhysics::HHullSet2D hull_set = (dmPhysics::HHullSet2D)texture_set->m_HullSet;
        if (hull_set != 0x0)
        {
            // Calculate AABB for offset
            dmVMath::Point3 offset(0.0f, 0.0f, 0.0f);
            uint32_t layer_count = tile_grid_ddf->m_Layers.m_Count;
            tile_grid->m_GridShapes.SetCapacity(layer_count);
            tile_grid->m_GridShapes.SetSize(layer_count);
            uint32_t cell_width = texture_set_ddf->m_TileWidth;
            uint32_t cell_height = texture_set_ddf->m_TileHeight;
            offset.setX(cell_width * 0.5f * (min_x + max_x));
            offset.setY(cell_height * 0.5f * (min_y + max_y));
            for (uint32_t i = 0; i < layer_count; ++i)
            {
                tile_grid->m_GridShapes[i] = dmPhysics::NewGridShape2D(context, hull_set, offset, cell_width, cell_height, tile_grid->m_RowCount, tile_grid->m_ColumnCount);
            }
        }
        return r;
    }

    void ReleaseResources(dmResource::HFactory factory, TileGridResource* tile_grid)
    {
        if (tile_grid->m_TextureSet)
            dmResource::Release(factory, tile_grid->m_TextureSet);

        if (tile_grid->m_Material)
            dmResource::Release(factory, tile_grid->m_Material);

        if (tile_grid->m_TileGrid)
            dmDDF::FreeMessage(tile_grid->m_TileGrid);

        uint32_t n = tile_grid->m_GridShapes.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (tile_grid->m_GridShapes[i])
                dmPhysics::DeleteCollisionShape2D(tile_grid->m_GridShapes[i]);
        }
    }

    static uint32_t GetResourceSize(TileGridResource* res, uint32_t ddf_size)
    {
        uint32_t size = sizeof(TileGridResource);
        size += ddf_size;
        size += res->m_GridShapes.Capacity() * sizeof(dmPhysics::HCollisionShape2D);    // TODO: Get size of CollisionShape2D
        return size;
    }

    dmResource::Result ResTileGridPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::TileGrid* tile_grid_ddf;
        dmDDF::Result e  = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &tile_grid_ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
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

        dmResource::Result r = AcquireResources(((PhysicsContext*) params.m_Context)->m_Context2D, params.m_Factory, tile_grid_ddf, tile_grid, params.m_Filename, false);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) tile_grid;
            params.m_Resource->m_ResourceSize = GetResourceSize(tile_grid, params.m_BufferSize);
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
        dmGameSystemDDF::TileGrid* tile_grid_ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &tile_grid_ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        TileGridResource* tile_grid = (TileGridResource*) params.m_Resource->m_Resource;
        TileGridResource tmp_tile_grid;

        dmResource::Result r = AcquireResources(((PhysicsContext*) params.m_Context)->m_Context2D, params.m_Factory, tile_grid_ddf, &tmp_tile_grid, params.m_Filename, true);
        if (r == dmResource::RESULT_OK)
        {
            uint32_t layer_count_old = tile_grid->m_GridShapes.Size();
            uint32_t layer_count_new = tmp_tile_grid.m_GridShapes.Size();
            uint32_t layer_count     = layer_count_new;

            ReleaseResources(params.m_Factory, tile_grid);

            tile_grid->m_TileGrid = tmp_tile_grid.m_TileGrid;
            tile_grid->m_Material = tmp_tile_grid.m_Material;
            tile_grid->m_ColumnCount = tmp_tile_grid.m_ColumnCount;
            tile_grid->m_RowCount = tmp_tile_grid.m_RowCount;
            tile_grid->m_MinCellX = tmp_tile_grid.m_MinCellX;
            tile_grid->m_MinCellY = tmp_tile_grid.m_MinCellY;

            if (layer_count_old < layer_count_new)
            {
                uint32_t capacity = tile_grid->m_GridShapes.Capacity();
                tile_grid->m_GridShapes.OffsetCapacity(layer_count_new - capacity);
                tile_grid->m_GridShapes.SetSize(tile_grid_ddf->m_Layers.m_Count);
                for (uint32_t i = capacity; i < layer_count_new; ++i)
                {
                    tile_grid->m_GridShapes[i] = tmp_tile_grid.m_GridShapes[i];
                }
                layer_count = layer_count_old;
            }
            else if (layer_count_old > layer_count_new)
            {
                tile_grid->m_GridShapes.SetSize(layer_count_new);
            }
            else
            {
                // Same num of layers, this is fine.
            }

            for (uint32_t i = 0; i < layer_count; ++i)
            {
                tile_grid->m_GridShapes[i] = tmp_tile_grid.m_GridShapes[i];
            }

            tile_grid->m_Dirty = 1;

            params.m_Resource->m_ResourceSize = GetResourceSize(tile_grid, params.m_BufferSize);
        }
        else
        {
            dmLogWarning("Failed AcquireResources, result: %i", r);
            ReleaseResources(params.m_Factory, &tmp_tile_grid);
        }
        return r;
    }
}
