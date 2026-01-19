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

#include "comp_tilegrid.h"
#include "comp_private.h"

#include <new>
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include <dlib/time.h>
#include <graphics/graphics.h>
#include <render/render.h>
#include <gameobject/gameobject.h>
#include <gameobject/gameobject_ddf.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/intersection.h>

#include "../gamesys_private.h"
#include "../gamesys.h"
#include "../resources/res_material.h"
#include "../resources/res_textureset.h"
#include "../resources/res_tilegrid.h"
#include <gamesys/physics_ddf.h>
#include <gamesys/tile_ddf.h>
#include <dmsdk/gamesys/render_constants.h>

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_Tilemap, 0, PROFILE_PROPERTY_FRAME_RESET, "# components", &rmtp_Components);
DM_PROPERTY_U32(rmtp_TilemapTileCount, 0, PROFILE_PROPERTY_FRAME_RESET, "# vertices", &rmtp_Tilemap);
DM_PROPERTY_U32(rmtp_TilemapVertexCount, 0, PROFILE_PROPERTY_FRAME_RESET, "# vertices", &rmtp_Tilemap);
DM_PROPERTY_U32(rmtp_TilemapVertexSize, 0, PROFILE_PROPERTY_FRAME_RESET, "size of vertices in bytes", &rmtp_Tilemap);

namespace dmGameSystem
{
    const uint32_t TILEGRID_REGION_SIZE = 32;

    using namespace dmVMath;

    // A "region" spans all layers (in Z) for a bounding box [(x1,y1), (x2,y2)]
    // where the the box spans TILEGRID_REGION_SIZE tiles in each direction
    struct TileGridRegion
    {
        uint8_t m_Dirty      : 1;
        uint8_t m_Occupied   : 1;
        uint8_t              : 6;
    };

    struct TileGridLayer
    {
        uint8_t m_IsVisible:1;
        uint8_t :7;
    };

    struct TileGridComponent
    {
        struct Flags
        {
            uint8_t    m_TransformMask : 3;
            uint8_t    : 5;
        };

        TileGridComponent()
        : m_Instance(0)
        , m_Cells(0)
        , m_CellFlags(0)
        , m_RenderConstants(0)
        , m_Material(0)
        , m_TextureSet(0)
        , m_Resource(0)
        {
        }

        dmVMath::Vector3            m_Translation;
        dmVMath::Quat               m_Rotation;
        dmVMath::Matrix4            m_World;
        dmGameObject::HInstance     m_Instance;
        uint16_t*                   m_Cells;
        Flags*                      m_CellFlags;
        dmArray<TileGridRegion>     m_Regions;
        dmArray<TileGridLayer>      m_Layers;
        uint32_t                    m_MixedHash;
        HComponentRenderConstants   m_RenderConstants;
        MaterialResource*           m_Material;
        TextureSetResource*         m_TextureSet;
        TileGridResource*           m_Resource;
        uint16_t                    m_RegionsX; // number of regions in the x dimension
        uint16_t                    m_RegionsY; // number of regions in the y dimension
        uint16_t                    m_Occupied; // Number of occupied regions (regions with visible tiles)
        uint8_t                     m_Enabled : 1;
        uint8_t                     m_AddedToUpdate : 1;
        uint8_t                     : 6;
    };

    struct TileGridVertex
    {
        float x, y, z, u, v;
    };

    struct TileGridWorld
    {
        TileGridWorld()
        {
            memset(this, 0, sizeof(TileGridWorld));
        }

        dmRender::HRenderContext        m_RenderContext;
        dmArray<TileGridComponent*>     m_Components;
        dmArray<dmRender::RenderObject> m_RenderObjects;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmRender::HBufferedRenderBuffer m_VertexBuffer;
        TileGridVertex*                 m_VertexBufferData;
        TileGridVertex*                 m_VertexBufferDataEnd;
        TileGridVertex*                 m_VertexBufferWritePtr;

        uint32_t                        m_MaxTilemapCount;
        uint32_t                        m_MaxTileCount;
        uint32_t                        m_DispatchCount;
    };

    static void TileGridWorldAllocate(TileGridWorld* world)
    {
        world->m_RenderObjects.SetCapacity(4);

        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(world->m_RenderContext);
        // TODO: Everything below here should be move to the "universe" when available
        // and hence shared among all the worlds

        dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(graphics_context);
        dmGraphics::AddVertexStream(stream_declaration, "position",  3, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(stream_declaration, "texcoord0", 2, dmGraphics::TYPE_FLOAT, false);
        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(graphics_context, stream_declaration);
        dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);

        world->m_VertexBuffer = dmRender::NewBufferedRenderBuffer(world->m_RenderContext, dmRender::RENDER_BUFFER_TYPE_VERTEX_BUFFER);
        uint32_t vcount = 6 * world->m_MaxTileCount;
        world->m_VertexBufferData = (TileGridVertex*) malloc(sizeof(TileGridVertex) * vcount);
        world->m_VertexBufferDataEnd = world->m_VertexBufferData + vcount;
    }

    dmGameObject::CreateResult CompTileGridNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        TileGridWorld* world = new TileGridWorld;
        TilemapContext* context = (TilemapContext*)params.m_Context;
        world->m_RenderContext = context->m_RenderContext;
        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, context->m_MaxTilemapCount);
        world->m_MaxTilemapCount = comp_count;
        world->m_MaxTileCount = context->m_MaxTileCount;

        world->m_Components.SetCapacity(comp_count);

        world->m_VertexDeclaration = 0;

        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompTileGridDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        TileGridWorld* world = (TileGridWorld*) params.m_World;
        if (world->m_VertexDeclaration)
        {
            dmGraphics::DeleteVertexDeclaration(world->m_VertexDeclaration);
            dmRender::DeleteBufferedRenderBuffer(world->m_RenderContext, world->m_VertexBuffer);
            free(world->m_VertexBufferData);
        }
        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static inline MaterialResource* GetMaterialResource(const TileGridComponent* component) {
        return component->m_Material ? component->m_Material : component->m_Resource->m_Material;
    }

    static inline dmRender::HMaterial GetMaterial(const TileGridComponent* component) {
        return GetMaterialResource(component)->m_Material;
    }

    static inline TextureSetResource* GetTextureSet(const TileGridComponent* component) {
        return component->m_TextureSet ? component->m_TextureSet : component->m_Resource->m_TextureSet;
    }

    uint32_t CalculateCellIndex(uint32_t layer, int32_t cell_x, int32_t cell_y, uint32_t column_count, uint32_t row_count)
    {
        return layer * row_count * column_count + (cell_x + cell_y * column_count);
    }

    void GetTileGridBounds(const TileGridComponent* component, int32_t* x, int32_t* y, int32_t* w, int32_t* h)
    {
        TileGridResource* resource = component->m_Resource;
        *x = resource->m_MinCellX;
        *y = resource->m_MinCellY;
        *w = resource->m_ColumnCount;
        *h = resource->m_RowCount;
    }

    void GetTileGridCellCoord(const TileGridComponent* component, int32_t x, int32_t y, int32_t& cell_x, int32_t& cell_y)
    {
        cell_x = x - component->m_Resource->m_MinCellX;
        cell_y = y - component->m_Resource->m_MinCellY;
    }

    uint16_t GetTileGridTile(const TileGridComponent* component, uint32_t layer, int32_t cell_x, int32_t cell_y)
    {
        TileGridResource* resource = component->m_Resource;
        uint32_t cell_index = CalculateCellIndex(layer, cell_x, cell_y, resource->m_ColumnCount, resource->m_RowCount);
        uint16_t cell = (component->m_Cells[cell_index] + 1);
        return cell;
    }

    uint8_t GetTileTransformMask(const TileGridComponent* component, uint32_t layer, int32_t cell_x, int32_t cell_y)
    {
        TileGridResource* resource = component->m_Resource;
        uint32_t cell_index = CalculateCellIndex(layer, cell_x, cell_y, resource->m_ColumnCount, resource->m_RowCount);
        TileGridComponent::Flags* flags = &component->m_CellFlags[cell_index];
        return flags->m_TransformMask;
    }

    void SetLayerVisible(TileGridComponent* component, uint32_t layer_index, bool visible)
    {
        TileGridLayer* layer = &component->m_Layers[layer_index];
        layer->m_IsVisible = visible;
    }

    static void SetRegionDirty(TileGridComponent* component, int32_t cell_x, int32_t cell_y)
    {
        uint32_t region_x = cell_x / TILEGRID_REGION_SIZE;
        uint32_t region_y = cell_y / TILEGRID_REGION_SIZE;
        uint32_t region_index = region_y * component->m_RegionsX + region_x;
        TileGridRegion* region = &component->m_Regions[region_index];
        region->m_Dirty = 1;
    }

    void SetTileGridTile(TileGridComponent* component, uint32_t layer, int32_t cell_x, int32_t cell_y, uint32_t tile, uint8_t transform_mask)
    {
        TileGridResource* resource = component->m_Resource;
        uint32_t cell_index = CalculateCellIndex(layer, cell_x, cell_y, resource->m_ColumnCount, resource->m_RowCount);
        component->m_Cells[cell_index] = tile;

        TileGridComponent::Flags* flags = &component->m_CellFlags[cell_index];
        flags->m_TransformMask = transform_mask;

        SetRegionDirty(component, cell_x, cell_y);
    }

    uint16_t GetTileCount(const TileGridComponent* component) {
        return GetTextureSet(component)->m_TextureSet->m_TileCount;
    }

    static void ReHash(TileGridComponent* component)
    {
        HashState32 state;
        TileGridResource* resource = component->m_Resource;
        dmRender::HMaterial material = GetMaterial(component);

        // NOTE: Use the same order as comp_sprite, since they both use the "tile" tag by default
        dmHashInit32(&state, false);
        dmHashUpdateBuffer32(&state, &material, sizeof(material));
        dmHashUpdateBuffer32(&state, GetTextureSet(component), sizeof(TextureSetResource));
        dmHashUpdateBuffer32(&state, &resource->m_TileGrid->m_BlendMode, sizeof(resource->m_TileGrid->m_BlendMode));
        if (component->m_RenderConstants) {
            dmGameSystem::HashRenderConstants(component->m_RenderConstants, &state);
        }

        component->m_MixedHash = dmHashFinal32(&state);
    }

    static void CreateRegions(TileGridComponent* component, TileGridResource* resource)
    {
        // Round up to closest multiple
        component->m_RegionsX = ((resource->m_ColumnCount + TILEGRID_REGION_SIZE - 1) / TILEGRID_REGION_SIZE);
        component->m_RegionsY = ((resource->m_RowCount + TILEGRID_REGION_SIZE - 1) / TILEGRID_REGION_SIZE);
        uint32_t region_count = component->m_RegionsX * component->m_RegionsY;

        component->m_Regions.SetCapacity(region_count);
        component->m_Regions.SetSize(region_count);
        memset(&component->m_Regions[0], 0xFF, region_count * sizeof(TileGridRegion)); // mark them all dirty
    }

    static uint32_t UpdateRegion(TileGridComponent* component, uint32_t region_x, uint32_t region_y)
    {
        uint32_t index = region_y * component->m_RegionsX + region_x;
        TileGridRegion* region = &component->m_Regions[index];
        if (!region->m_Dirty) {
            return region->m_Occupied;
        }
        region->m_Dirty = 0;

        TileGridResource* resource = component->m_Resource;
        dmGameSystemDDF::TileGrid* tile_grid_ddf = resource->m_TileGrid;

        uint32_t n_layers = tile_grid_ddf->m_Layers.m_Count;

        uint32_t column_count = resource->m_ColumnCount;
        uint32_t row_count = resource->m_RowCount;
        int32_t min_x = resource->m_MinCellX + region_x * TILEGRID_REGION_SIZE;
        int32_t min_y = resource->m_MinCellY + region_y * TILEGRID_REGION_SIZE;
        int32_t max_x = dmMath::Min(min_x + (int32_t)TILEGRID_REGION_SIZE, resource->m_MinCellX + (int32_t)column_count);
        int32_t max_y = dmMath::Min(min_y + (int32_t)TILEGRID_REGION_SIZE, resource->m_MinCellY + (int32_t)row_count);

        region->m_Occupied = 0;

        for (uint32_t j = 0; j < n_layers; ++j)
        {
            TileGridLayer* layer = &component->m_Layers[j];
            if (!layer->m_IsVisible)
                continue;

            for (int32_t y = min_y; y < max_y; ++y)
            {
                for (int32_t x = min_x; x < max_x; ++x)
                {
                    uint32_t cell = CalculateCellIndex(j, x - resource->m_MinCellX, y - resource->m_MinCellY, column_count, row_count);
                    uint16_t tile = component->m_Cells[cell];
                    if (tile != 0xffff)
                    {
                        region->m_Occupied = 1;
                        return region->m_Occupied;
                    }
                }
            }
        }

        return region->m_Occupied;
    }

    static uint32_t UpdateRegions(TileGridComponent* component)
    {
        uint32_t occupied = 0;
        for (uint32_t region_y = 0; region_y < component->m_RegionsY; ++region_y) {
            for (uint32_t region_x = 0; region_x < component->m_RegionsX; ++region_x) {
                occupied += UpdateRegion(component, region_x, region_y);
            }
        }
        return occupied;
    }

    static uint32_t CreateTileGrid(TileGridComponent* component)
    {
        TileGridResource* resource = component->m_Resource;
        dmGameSystemDDF::TileGrid* tile_grid_ddf = resource->m_TileGrid;
        uint32_t n_layers = tile_grid_ddf->m_Layers.m_Count;
        uint32_t cell_count = resource->m_ColumnCount * resource->m_RowCount * n_layers;
        if (component->m_Cells != 0x0)
        {
            delete [] component->m_Cells;
        }
        component->m_Cells = new uint16_t[cell_count];
        memset(component->m_Cells, 0xff, cell_count * sizeof(uint16_t));
        if (component->m_CellFlags != 0x0)
        {
            delete [] component->m_CellFlags;
        }
        component->m_CellFlags = new TileGridComponent::Flags[cell_count];
        memset(component->m_CellFlags, 0, cell_count * sizeof(TileGridComponent::Flags));
        int32_t min_x = resource->m_MinCellX;
        int32_t min_y = resource->m_MinCellY;
        uint32_t column_count = resource->m_ColumnCount;
        uint32_t row_count = resource->m_RowCount;

        component->m_Layers.SetCapacity(n_layers);
        component->m_Layers.SetSize(n_layers);

        for (uint32_t i = 0; i < n_layers; ++i)
        {
            dmGameSystemDDF::TileLayer* layer_ddf = &tile_grid_ddf->m_Layers[i];

            component->m_Layers[i].m_IsVisible = layer_ddf->m_IsVisible;

            uint32_t n_cells = layer_ddf->m_Cell.m_Count;
            for (uint32_t j = 0; j < n_cells; ++j)
            {
                dmGameSystemDDF::TileCell* cell = &layer_ddf->m_Cell[j];
                uint32_t cell_index = CalculateCellIndex(i, cell->m_X - min_x, cell->m_Y - min_y, column_count, row_count);
                component->m_Cells[cell_index] = (uint16_t)cell->m_Tile;

                TileGridComponent::Flags* flags = &component->m_CellFlags[cell_index];
                flags->m_TransformMask = 0;
                if (cell->m_HFlip)
                {
                    flags->m_TransformMask = FLIP_HORIZONTAL;
                }
                if (cell->m_VFlip)
                {
                    flags->m_TransformMask |= FLIP_VERTICAL;
                }
                if (cell->m_Rotate90)
                {
                    flags->m_TransformMask |= ROTATE_90;
                }
            }
        }

        CreateRegions(component, resource);
        component->m_Occupied = UpdateRegions(component);
        return n_layers;
    }

    dmGameObject::CreateResult CompTileGridCreate(const dmGameObject::ComponentCreateParams& params)
    {
        TileGridWorld* world = (TileGridWorld*) params.m_World;
        if (world->m_Components.Full())
        {
            ShowFullBufferError("Tilemap", "tilemap.max_count", world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        if (world->m_VertexDeclaration == 0) {
            TileGridWorldAllocate(world);
        }

        TileGridResource* resource = (TileGridResource*) params.m_Resource;
        TileGridComponent* component = new TileGridComponent();
        component->m_Instance = params.m_Instance;
        component->m_Resource = resource;
        component->m_Translation = Vector3(params.m_Position);
        component->m_Rotation = params.m_Rotation;
        component->m_Enabled = 1;

        uint32_t layer_count = CreateTileGrid(component);
        if (layer_count == 0)
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        world->m_Components.Push(component);
        *params.m_UserData = (uintptr_t) component;

        ReHash(component);
        return dmGameObject::CREATE_RESULT_OK;
    }

    void* CompTileGridGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        return (void*)params.m_UserData;
    }

    dmGameObject::CreateResult CompTileGridDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        TileGridComponent* tile_grid = (TileGridComponent*) *params.m_UserData;
        TileGridWorld* world = (TileGridWorld*) params.m_World;
        for (uint32_t i = 0; i < world->m_Components.Size(); ++i)
        {
            if (world->m_Components[i] == tile_grid)
            {
                if (tile_grid->m_Material) {
                    dmResource::Release(dmGameObject::GetFactory(params.m_Instance), tile_grid->m_Material);
                }
                if (tile_grid->m_TextureSet) {
                    dmResource::Release(dmGameObject::GetFactory(params.m_Instance), tile_grid->m_TextureSet);
                }

                delete [] tile_grid->m_Cells;
                delete [] tile_grid->m_CellFlags;

                if (tile_grid->m_RenderConstants)
                {
                    dmGameSystem::DestroyRenderConstants(tile_grid->m_RenderConstants);
                }

                world->m_Components.EraseSwap(i);
                delete tile_grid;
                return dmGameObject::CREATE_RESULT_OK;
            }
        }
        assert(false);
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    static inline void CalculateCellBounds(int32_t cell_x, int32_t cell_y, int32_t cell_width, int32_t cell_height, float out_v[4])
    {
        out_v[0] = cell_x * cell_width;
        out_v[1] = cell_y * cell_height;
        out_v[2] = (cell_x + 1) * cell_width;
        out_v[3] = (cell_y + 1) * cell_height;
    }

    dmGameObject::CreateResult CompTileGridAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        TileGridComponent* component = (TileGridComponent*) *params.m_UserData;
        component->m_AddedToUpdate = 1;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompTileGridLateUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        TileGridWorld* world = (TileGridWorld*)params.m_World;
        dmArray<TileGridComponent*>& components = world->m_Components;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            TileGridComponent* component = components[i];
            if (!component->m_Enabled || !component->m_AddedToUpdate)
            {
                continue;
            }

            component->m_Occupied = UpdateRegions(component);
            if (!component->m_Occupied)
            {
                continue;
            }

            Matrix4 local(component->m_Rotation, component->m_Translation);
            const Matrix4& go_world = dmGameObject::GetWorldMatrix(component->m_Instance);
            component->m_World = go_world * local;
        }
        DM_PROPERTY_ADD_U32(rmtp_Tilemap, world->m_Components.Size());

        TilemapContext* context = (TilemapContext*)params.m_Context;
        dmRender::TrimBuffer(context->m_RenderContext, world->m_VertexBuffer);
        dmRender::RewindBuffer(context->m_RenderContext, world->m_VertexBuffer);
        world->m_DispatchCount = 0;

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static inline uint64_t EncodeRegionInfo(uint32_t tile_grid, uint32_t layer, uint32_t region_x, uint32_t region_y)
    {
        return (uint64_t)( (tile_grid & 0xFFFF) | ((layer & 0xFFFF) << 16) | ((uint64_t)region_x << 32) | ((uint64_t)region_y << 48) );
    }

    static inline void DecodeGridAndLayer(uint64_t ptr, uint32_t& tile_grid, uint32_t& layer, uint32_t& region_x, uint32_t& region_y)
    {
        tile_grid = ptr & 0xFFFF;
        layer = (ptr >> 16) & 0xFFFF;
        region_x = (ptr >> 32) & 0xFFFF;
        region_y = (ptr >> 48) & 0xFFFF;
    }

    TileGridVertex* CreateVertexData(TileGridWorld* world, TileGridVertex* where, TextureSetResource* texture_set, dmRender::RenderListEntry* buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("CreateVertexData");
        /*
         *   0----3
         *   | \  |
         *   |  \ |
         *   1____2
        */
        static int tex_coord_order[] = {
            0,1,2,2,3,0,
            3,2,1,1,0,3,    //h
            1,0,3,3,2,1,    //v
            2,3,0,0,1,2,    //hv
            // rotate 90 degrees:
            3,0,1,1,2,3,
            0,3,2,2,1,0,    //h
            2,1,0,0,3,2,    //v
            1,2,3,3,0,1     //hv
        };

        dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
        const float* tex_coords = (const float*) texture_set_ddf->m_TexCoords.m_Data;

        uint32_t tile_width = texture_set_ddf->m_TileWidth;
        uint32_t tile_height = texture_set_ddf->m_TileHeight;

        for (uint32_t* i = begin; i != end; ++i)
        {
            uint32_t index, layer, region_x, region_y;
            DecodeGridAndLayer(buf[*i].m_UserData, index, layer, region_x, region_y);

            const TileGridComponent* component = world->m_Components[index];
            const TileGridResource* resource = component->m_Resource;
            dmGameSystemDDF::TileGrid* tile_grid_ddf = resource->m_TileGrid;
            dmGameSystemDDF::TileLayer* layer_ddf = &tile_grid_ddf->m_Layers[layer];

            const Matrix4& w = component->m_World;
            const float z = layer_ddf->m_Z;

            uint32_t column_count = resource->m_ColumnCount;
            uint32_t row_count = resource->m_RowCount;

            int32_t min_x = resource->m_MinCellX + region_x * TILEGRID_REGION_SIZE;
            int32_t min_y = resource->m_MinCellY + region_y * TILEGRID_REGION_SIZE;
            int32_t max_x = dmMath::Min(min_x + (int32_t)TILEGRID_REGION_SIZE, resource->m_MinCellX + (int32_t)column_count);
            int32_t max_y = dmMath::Min(min_y + (int32_t)TILEGRID_REGION_SIZE, resource->m_MinCellY + (int32_t)row_count);

            for (int32_t y = min_y; y < max_y; ++y)
            {
                for (int32_t x = min_x; x < max_x; ++x)
                {
                    uint32_t cell = CalculateCellIndex(layer, x - resource->m_MinCellX, y - resource->m_MinCellY, column_count, row_count);
                    uint16_t tile = component->m_Cells[cell];
                    if (tile == 0xffff)
                    {
                        continue;
                    }

                    if( where >= world->m_VertexBufferDataEnd )
                    {
                        dmLogError("Out of tiles to render (%zu). You can change this with the game.project setting tilemap.max_tile_count", (size_t)((world->m_VertexBufferDataEnd - world->m_VertexBufferData) / 6));
                        return world->m_VertexBufferDataEnd;
                    }

                    float p[4];
                    CalculateCellBounds(x, y, 1, 1, p);
                    const float* puv = &tex_coords[tile * 8];

                    TileGridComponent::Flags flags = component->m_CellFlags[cell];
                    const int* tex_lookup = &tex_coord_order[flags.m_TransformMask * 6];

                    #define SET_VERTEX(_I, _X, _Y, _Z, _U, _V) \
                        { \
                            const Vector4 v = w * Point3(_X * tile_width, _Y * tile_height, _Z); \
                            where[_I].x = v.getX(); \
                            where[_I].y = v.getY(); \
                            where[_I].z = v.getZ(); \
                            where[_I].u = _U; \
                            where[_I].v = _V; \
                        }

                    SET_VERTEX(0, p[0], p[1], z, puv[tex_lookup[0] * 2], puv[tex_lookup[0] * 2 + 1]);
                    SET_VERTEX(1, p[0], p[3], z, puv[tex_lookup[1] * 2], puv[tex_lookup[1] * 2 + 1]);
                    SET_VERTEX(2, p[2], p[3], z, puv[tex_lookup[2] * 2], puv[tex_lookup[2] * 2 + 1]);
                    SET_VERTEX(3, p[2], p[3], z, puv[tex_lookup[3] * 2], puv[tex_lookup[3] * 2 + 1]);
                    SET_VERTEX(4, p[2], p[1], z, puv[tex_lookup[4] * 2], puv[tex_lookup[4] * 2 + 1]);
                    SET_VERTEX(5, p[0], p[1], z, puv[tex_lookup[5] * 2], puv[tex_lookup[5] * 2 + 1]);

                    where += 6;

                    #undef SET_VERTEX
                }
            }
        }
        return where;
    }

    static void RenderListFrustumCulling(dmRender::RenderListVisibilityParams const &params)
    {
        DM_PROFILE("TileGridFrustrumCulling");
        TileGridWorld* tilegrid_world = (TileGridWorld*)params.m_UserData;
        const dmIntersection::Frustum frustum = *params.m_Frustum;
        uint32_t num_entries = params.m_NumEntries;
        for (uint32_t i = 0; i < num_entries; ++i)
        {
            dmRender::RenderListEntry* entry = &params.m_Entries[i];
            uint32_t index, layer, region_x, region_y;
            //entry->m_UserData - encoded region info
            DecodeGridAndLayer(entry->m_UserData, index, layer, region_x, region_y);
            TileGridComponent* component = tilegrid_world->m_Components[index];

            TileGridResource* resource = component->m_Resource;
            TextureSetResource* texture_set = GetTextureSet(component);
            int32_t tile_width = (int32_t)texture_set->m_TextureSet->m_TileWidth;
            int32_t tile_height = (int32_t)texture_set->m_TextureSet->m_TileHeight;

            int32_t column_count = (int32_t)resource->m_ColumnCount;
            int32_t row_count = (int32_t)resource->m_RowCount;
            int32_t min_x = resource->m_MinCellX + region_x * TILEGRID_REGION_SIZE;
            int32_t min_y = resource->m_MinCellY + region_y * TILEGRID_REGION_SIZE;

            int32_t region_max_x = min_x + TILEGRID_REGION_SIZE;
            int32_t tilemap_max_x = resource->m_MinCellX + column_count;
            int32_t region_max_y = min_y + TILEGRID_REGION_SIZE;
            int32_t tilemap_max_y = resource->m_MinCellY + row_count;
            int32_t max_x = dmMath::Min(region_max_x, tilemap_max_x);
            int32_t max_y = dmMath::Min(region_max_y, tilemap_max_y);

            dmVMath::Vector3 min_corner = dmVMath::Vector3((float)(min_x * tile_width), (float)(min_y * tile_height), 0.f);
            dmVMath::Vector3 max_corner = dmVMath::Vector3((float)(max_x * tile_width), (float)(max_y * tile_height), 0.f);
            bool intersect = dmIntersection::TestFrustumOBB(frustum, component->m_World, min_corner, max_corner);
            entry->m_Visibility = intersect ? dmRender::VISIBILITY_FULL : dmRender::VISIBILITY_NONE;
        }
    }

    static void RenderBatch(TileGridWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("TileGridRenderBatch");

        uint32_t index, layer, region_x, region_y;
        DecodeGridAndLayer(buf[*begin].m_UserData, index, layer, region_x, region_y);
        TileGridComponent* first = world->m_Components[index];
        assert(first->m_Enabled);

        TileGridResource* resource = first->m_Resource;
        TextureSetResource* texture_set = GetTextureSet(first);

        dmRender::RenderObject& ro = *world->m_RenderObjects.End();
        world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);

        // Fill in vertex buffer
        TileGridVertex* vb_begin = world->m_VertexBufferWritePtr;
        world->m_VertexBufferWritePtr = CreateVertexData(world, vb_begin, texture_set, buf, begin, end);

        if (dmRender::GetBufferIndex(render_context, world->m_VertexBuffer) < world->m_DispatchCount)
        {
            dmRender::AddRenderBuffer(render_context, world->m_VertexBuffer);
        }

        ro.Init();
        ro.m_VertexDeclaration = world->m_VertexDeclaration;
        ro.m_VertexBuffer      = (dmGraphics::HVertexBuffer) dmRender::GetBuffer(render_context, world->m_VertexBuffer);
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = vb_begin - world->m_VertexBufferData;
        ro.m_VertexCount = (world->m_VertexBufferWritePtr - vb_begin);
        ro.m_Material = GetMaterial(first);
        ro.m_Textures[0] = texture_set->m_Texture->m_Texture;

        if (first->m_RenderConstants) {
            dmGameSystem::EnableRenderObjectConstants(&ro, first->m_RenderConstants);
        }

        dmGameSystemDDF::TileGrid::BlendMode blend_mode = resource->m_TileGrid->m_BlendMode;
        switch (blend_mode)
        {
            case dmGameSystemDDF::TileGrid::BLEND_MODE_ALPHA:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmGameSystemDDF::TileGrid::BLEND_MODE_ADD:
            case dmGameSystemDDF::TileGrid::BLEND_MODE_ADD_ALPHA:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmGameSystemDDF::TileGrid::BLEND_MODE_MULT:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_DST_COLOR;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmGameSystemDDF::TileGrid::BLEND_MODE_SCREEN:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_COLOR;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            default:
                dmLogError("Unknown blend mode: %d\n", blend_mode);
                assert(0);
            break;
        }

        ro.m_SetBlendFactors = 1;

        dmRender::AddToRender(render_context, &ro);
    }

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        TileGridWorld* world = (TileGridWorld*) params.m_UserData;

        switch (params.m_Operation)
        {
        case dmRender::RENDER_LIST_OPERATION_BEGIN:
            world->m_VertexBufferWritePtr = world->m_VertexBufferData;
            world->m_RenderObjects.SetSize(0);
            break;
        case dmRender::RENDER_LIST_OPERATION_END:
            {
                uint32_t vertex_count = world->m_VertexBufferWritePtr - world->m_VertexBufferData;
                uint32_t vertex_data_size = sizeof(TileGridVertex) * vertex_count;

                if (vertex_data_size > 0)
                {
                    dmRender::SetBufferData(params.m_Context, world->m_VertexBuffer, vertex_data_size, world->m_VertexBufferData, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                    DM_PROPERTY_ADD_U32(rmtp_TilemapTileCount, vertex_count/6);
                    DM_PROPERTY_ADD_U32(rmtp_TilemapVertexCount, vertex_count);
                    DM_PROPERTY_ADD_U32(rmtp_TilemapVertexSize, vertex_count * sizeof(TileGridVertex));
                    world->m_DispatchCount++;
                }
            } break;
        case dmRender::RENDER_LIST_OPERATION_BATCH:
            assert(params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH);
            RenderBatch(world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
        }
    }

    // Estimates the number of render entries needed
    static uint32_t CalcNumVisibleRegions(TileGridComponent** components, uint32_t num_components)
    {
        uint32_t num_render_entries = 0;
        for (uint32_t i = 0; i < num_components; ++i)
        {
            const TileGridComponent* component = components[i];
            if (!component->m_Enabled || !component->m_AddedToUpdate || !component->m_Occupied) {
                continue;
            }
            uint32_t n_layers = component->m_Layers.Size();
            for (uint32_t l = 0; l < n_layers; ++l)
            {
                const TileGridLayer* layer = &component->m_Layers[l];
                if (!layer->m_IsVisible)
                    continue;

                num_render_entries += component->m_RegionsY * component->m_RegionsX;
            }
        }
        return num_render_entries;
    }

    dmGameObject::UpdateResult CompTileGridRender(const dmGameObject::ComponentsRenderParams& params)
    {
        TilemapContext* context = (TilemapContext*)params.m_Context;
        TileGridWorld* world = (TileGridWorld*)params.m_World;

        dmArray<TileGridComponent*>& components = world->m_Components;
        uint32_t n = components.Size();
        if( n == 0 )
        {
            return dmGameObject::UPDATE_RESULT_OK;
        }

        uint32_t num_render_entries = CalcNumVisibleRegions(&components[0], n);

        // We need to calculate this before actually pushing render object references to the renderer
        // This however means we need to make this array potentially oversized, but this allocation should only occur
        // occasionally after a new tilegrid is added for rendering
        if (num_render_entries > world->m_RenderObjects.Capacity()) {
            world->m_RenderObjects.SetCapacity(num_render_entries);
        }

        dmRender::HRenderContext render_context = context->m_RenderContext;
        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, num_render_entries);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, &RenderListFrustumCulling, world);
        dmRender::RenderListEntry* write_ptr = render_list;

        for (uint32_t i = 0; i < n; ++i)
        {
            TileGridComponent* component = components[i];
            if (!component->m_Enabled || !component->m_AddedToUpdate || !component->m_Occupied) {
                continue;
            }

            if (component->m_RenderConstants && dmGameSystem::AreRenderConstantsUpdated(component->m_RenderConstants))
            {
                ReHash(component);
            }

            TileGridResource* resource = component->m_Resource;
            dmGameSystemDDF::TextureSet* texture_set_ddf = GetTextureSet(component)->m_TextureSet;
            dmGameSystemDDF::TileGrid* tile_grid_ddf = resource->m_TileGrid;

            uint32_t tile_width = texture_set_ddf->m_TileWidth;
            uint32_t tile_height = texture_set_ddf->m_TileHeight;

            uint32_t n_layers = tile_grid_ddf->m_Layers.m_Count;
            for (uint32_t l = 0; l < n_layers; ++l)
            {
                TileGridLayer* layer = &component->m_Layers[l];
                if (!layer->m_IsVisible)
                    continue;

                dmGameSystemDDF::TileLayer* layer_ddf = &tile_grid_ddf->m_Layers[l];
                for (uint32_t y = 0, region_index = 0; y < component->m_RegionsY; ++y) {
                    for (uint32_t x = 0; x < component->m_RegionsX; ++x, ++region_index) {

                        TileGridRegion* region = &component->m_Regions[region_index];
                        if (!region->m_Occupied) {
                            continue;
                        }

                        Vector4 trans = component->m_World * Point3(x * tile_width, y * tile_height, layer_ddf->m_Z);

                        write_ptr->m_WorldPosition = Point3(trans.getXYZ());
                        write_ptr->m_UserData = EncodeRegionInfo(i, l, x, y);
                        write_ptr->m_TagListKey = dmRender::GetMaterialTagListKey(GetMaterial(component));
                        write_ptr->m_BatchKey = component->m_MixedHash;
                        write_ptr->m_Dispatch = dispatch;
                        write_ptr->m_MinorOrder = 0;
                        write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
                        ++write_ptr;
                    }
                }
            }
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);

        return dmGameObject::UPDATE_RESULT_OK;
    }

    uint32_t GetLayerIndex(const TileGridComponent* component, dmhash_t layer_id)
    {
        dmGameSystemDDF::TileGrid* tile_grid_ddf = component->m_Resource->m_TileGrid;
        uint32_t layer_count = tile_grid_ddf->m_Layers.m_Count;
        for (uint32_t i = 0; i < layer_count; ++i)
        {
            if (layer_id == tile_grid_ddf->m_Layers[i].m_IdHash)
            {
                return i;
            }
        }
        return ~0u;
    }

    static void CompTileGridSetConstantCallback(void* user_data, dmhash_t name_hash, int32_t value_index, uint32_t* element_index, const dmGameObject::PropertyVar& var);

    dmGameObject::UpdateResult CompTileGridOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        TileGridComponent* component = (TileGridComponent*) *params.m_UserData;

        if (params.m_Message->m_Id == dmGameSystemDDF::SetConstantTileMap::m_DDFDescriptor->m_NameHash)
        {
            dmGameSystemDDF::SetConstantTileMap* ddf = (dmGameSystemDDF::SetConstantTileMap*)params.m_Message->m_Data;
            if (!component->m_RenderConstants)
                component->m_RenderConstants = dmGameSystem::CreateRenderConstants();
            dmGameSystem::SetRenderConstant(component->m_RenderConstants, GetMaterial(component), ddf->m_NameHash, 0, 0, ddf->m_Value);
            ReHash(component);
        }
        else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstantTileMap::m_DDFDescriptor->m_NameHash)
        {
            dmGameSystemDDF::ResetConstantTileMap* ddf = (dmGameSystemDDF::ResetConstantTileMap*)params.m_Message->m_Data;
            if (component->m_RenderConstants) {
                ClearRenderConstant(component->m_RenderConstants, ddf->m_NameHash);
            }
        }
        else if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 1;
        }
        else if (params.m_Message->m_Id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 0;
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompTileGridOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        TileGridComponent* component = (TileGridComponent*)*params.m_UserData;
        if (!CreateTileGrid(component))
        {
            dmLogError("Could not recreate tile grid component, not reloaded.");
        }
    }

    static bool CompTileGridGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        TileGridComponent* component = (TileGridComponent*)user_data;
        if (!component->m_RenderConstants)
            return false;
        return GetRenderConstant(component->m_RenderConstants, name_hash, out_constant);
    }

    static void CompTileGridSetConstantCallback(void* user_data, dmhash_t name_hash, int32_t value_index, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        TileGridComponent* component = (TileGridComponent*)user_data;
        if (!component->m_RenderConstants)
            component->m_RenderConstants = dmGameSystem::CreateRenderConstants();
        dmGameSystem::SetRenderConstant(component->m_RenderConstants, GetMaterial(component), name_hash, value_index, element_index, var);
        ReHash(component);
    }

    dmGameObject::PropertyResult CompTileGridGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        TileGridComponent* component = (TileGridComponent*)*params.m_UserData;
        if (params.m_PropertyId == PROP_MATERIAL)
        {
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetMaterialResource(component), out_value);
        }
        if (params.m_PropertyId == PROP_TILE_SOURCE)
        {
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetTextureSet(component), out_value);
        }
        return GetMaterialConstant(GetMaterial(component), params.m_PropertyId, params.m_Options.m_Index, out_value, true, CompTileGridGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompTileGridSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        TileGridComponent* component = (TileGridComponent*)*params.m_UserData;
        if (params.m_PropertyId == PROP_MATERIAL)
        {
            return SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, MATERIAL_EXT_HASH, (void**)&component->m_Material);
        }
        if (params.m_PropertyId == PROP_TILE_SOURCE)
        {
            dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, TEXTURE_SET_EXT_HASH, (void**)&component->m_TextureSet);
            ReHash(component);
            return res;
        }
        return SetMaterialConstant(GetMaterial(component), params.m_PropertyId, params.m_Value, params.m_Options.m_Index, CompTileGridSetConstantCallback, component);
    }

    static bool CompTileGridIterPropertiesGetNext(dmGameObject::SceneNodePropertyIterator* pit)
    {
        TileGridComponent* component = (TileGridComponent*)pit->m_Node->m_Component;

        uint64_t index = pit->m_Next++;

        uint32_t num_bool_properties = 1;
        if (index < num_bool_properties)
        {
            if (index == 0)
            {
                pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_BOOLEAN;
                pit->m_Property.m_Value.m_Bool = component->m_Enabled;
                pit->m_Property.m_NameHash = dmHashString64("enabled");
            }
            return true;
        }
        index -= num_bool_properties;

        return false;
    }

    void CompTileGridIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node)
    {
        assert(node->m_Type == dmGameObject::SCENE_NODE_TYPE_COMPONENT);
        assert(node->m_ComponentType != 0);
        pit->m_Node = node;
        pit->m_Next = 0;
        pit->m_FnIterateNext = CompTileGridIterPropertiesGetNext;
    }

    // For tests
    void GetTileGridWorldRenderBuffers(void* tilegrid_world, dmRender::HBufferedRenderBuffer* vx_buffer)
    {
        TileGridWorld* world = (TileGridWorld*) tilegrid_world;
        *vx_buffer = world->m_VertexBuffer;
    }
}
