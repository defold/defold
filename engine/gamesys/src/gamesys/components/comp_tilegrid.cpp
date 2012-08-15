#include "comp_tilegrid.h"
//#include "resources/res_light.h"

#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <graphics/graphics.h>
#include <render/render.h>
#include <gameobject/gameobject.h>
#include <vectormath/cpp/vectormath_aos.h>

#include "../proto/tile_ddf.h"
#include "../proto/physics_ddf.h"
#include "../gamesys_private.h"

extern unsigned char SPRITE_VPC[];
extern uint32_t SPRITE_VPC_SIZE;

extern unsigned char SPRITE_FPC[];
extern uint32_t SPRITE_FPC_SIZE;

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    TileGrid::TileGrid()
    : m_Instance(0)
    , m_TileGridResource(0)
    , m_Cells(0)
    {

    }

    dmGameObject::CreateResult CompTileGridNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        TileGridWorld* world = new TileGridWorld;
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)params.m_Context;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);

        // TODO: Everything below here should be move to the "universe" when available
        // and hence shared among all the worlds
        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 3, dmGraphics::TYPE_FLOAT},
                {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT},
        };
        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(graphics_context, ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));
        world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), 0, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

        world->m_VertexProgram = dmGraphics::NewVertexProgram(dmRender::GetGraphicsContext(render_context), SPRITE_VPC, SPRITE_VPC_SIZE);
        world->m_FragmentProgram = dmGraphics::NewFragmentProgram(dmRender::GetGraphicsContext(render_context), SPRITE_FPC, SPRITE_FPC_SIZE);

        world->m_Material = dmRender::NewMaterial(render_context, world->m_VertexProgram, world->m_FragmentProgram);
        dmRender::SetMaterialProgramConstantType(world->m_Material, dmHashString64("view_proj"), dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
        dmRender::SetMaterialProgramConstantType(world->m_Material, dmHashString64("world"), dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLD);
        dmRender::AddMaterialTag(world->m_Material, dmHashString32("tile"));

        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompTileGridDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)params.m_Context;
        TileGridWorld* world = (TileGridWorld*) params.m_World;
        if (world->m_ClientBuffer != 0x0)
            delete [] (char*)world->m_ClientBuffer;
        dmRender::DeleteMaterial(render_context, world->m_Material);
        dmGraphics::DeleteVertexDeclaration(world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(world->m_VertexBuffer);
        dmGraphics::DeleteVertexProgram(world->m_VertexProgram);
        dmGraphics::DeleteFragmentProgram(world->m_FragmentProgram);
        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    uint32_t CalculateCellIndex(uint32_t layer, int32_t cell_x, int32_t cell_y, uint32_t column_count, uint32_t row_count)
    {
        return layer * row_count * column_count + (cell_x + cell_y * column_count);
    }

    bool CreateTileGrid(TileGrid* tile_grid)
    {
        TileGridResource* resource = tile_grid->m_TileGridResource;
        dmGameSystemDDF::TileGrid* tile_grid_ddf = resource->m_TileGrid;
        uint32_t n_layers = tile_grid_ddf->m_Layers.m_Count;
        dmArray<TileGrid::Layer>& layers = tile_grid->m_Layers;
        if (layers.Capacity() < n_layers)
        {
            layers.SetCapacity(n_layers);
            layers.SetSize(n_layers);
            for (uint32_t i = 0; i < n_layers; ++i)
            {
                TileGrid::Layer& layer = layers[i];
                dmGameSystemDDF::TileLayer* layer_ddf = &tile_grid_ddf->m_Layers[i];
                layer.m_Id = dmHashString64(layer_ddf->m_Id);
                layer.m_Visible = layer_ddf->m_IsVisible;
            }
        }
        uint32_t cell_count = resource->m_ColumnCount * resource->m_RowCount * n_layers;
        if (tile_grid->m_Cells != 0x0)
        {
            delete [] tile_grid->m_Cells;
        }
        tile_grid->m_Cells = new uint16_t[cell_count];
        memset(tile_grid->m_Cells, 0xff, cell_count * sizeof(uint16_t));
        int32_t min_x = resource->m_MinCellX;
        int32_t min_y = resource->m_MinCellY;
        uint32_t column_count = resource->m_ColumnCount;
        uint32_t row_count = resource->m_RowCount;
        for (uint32_t i = 0; i < n_layers; ++i)
        {
            dmGameSystemDDF::TileLayer* layer_ddf = &tile_grid_ddf->m_Layers[i];
            uint32_t n_cells = layer_ddf->m_Cell.m_Count;
            for (uint32_t j = 0; j < n_cells; ++j)
            {
                dmGameSystemDDF::TileCell* cell = &layer_ddf->m_Cell[j];
                uint32_t cell_index = CalculateCellIndex(i, cell->m_X - min_x, cell->m_Y - min_y, column_count, row_count);
                tile_grid->m_Cells[cell_index] = (uint16_t)cell->m_Tile;
            }
        }
        return true;
    }

    dmGameObject::CreateResult CompTileGridCreate(const dmGameObject::ComponentCreateParams& params)
    {
        TileGridResource* resource = (TileGridResource*) params.m_Resource;
        TileGridWorld* world = (TileGridWorld*) params.m_World;
        if (world->m_TileGrids.Full())
        {
            world->m_TileGrids.OffsetCapacity(16);
        }
        TileGrid* tile_grid = new TileGrid();
        tile_grid->m_Instance = params.m_Instance;
        tile_grid->m_TileGridResource = resource;
        if (!CreateTileGrid(tile_grid))
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        dmRender::RenderObject* ro = &tile_grid->m_RenderObject;
        ro->m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
        ro->m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        ro->m_SetBlendFactors = 1;
        ro->m_VertexDeclaration = world->m_VertexDeclaration;
        ro->m_VertexBuffer = world->m_VertexBuffer;
        ro->m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro->m_Material = world->m_Material;
        ro->m_CalculateDepthKey = 1;
        world->m_TileGrids.Push(tile_grid);

        *params.m_UserData = (uintptr_t) tile_grid;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompTileGridDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        TileGrid* tile_grid = (TileGrid*) *params.m_UserData;
        TileGridWorld* world = (TileGridWorld*) params.m_World;
        for (uint32_t i = 0; i < world->m_TileGrids.Size(); ++i)
        {
            if (world->m_TileGrids[i] == tile_grid)
            {
                delete [] tile_grid->m_Cells;
                world->m_TileGrids.EraseSwap(i);
                delete tile_grid;
                return dmGameObject::CREATE_RESULT_OK;
            }
        }
        assert(false);
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    uint32_t CalculateTileCount(uint32_t tile_size, uint32_t image_size, uint32_t tile_margin, uint32_t tile_spacing)
    {
        uint32_t actual_tile_size = (2 * tile_margin + tile_spacing + tile_size);
        if (actual_tile_size > 0) {
            return (image_size + tile_spacing)/actual_tile_size;
        } else {
            return 0;
        }
    }

    void CalculateCellBounds(int32_t cell_x, int32_t cell_y, int32_t cell_width, int32_t cell_height, float out_v[4])
    {
        out_v[0] = cell_x * cell_width;
        out_v[1] = cell_y * cell_height;
        out_v[2] = (cell_x + 1) * cell_width;
        out_v[3] = (cell_y + 1) * cell_height;
    }

    void CalculateTileTexCoords(uint32_t tile_index, dmGameSystemDDF::TileSet* tile_set_ddf, uint32_t tiles_per_row, float recip_tex_width, float recip_tex_height, float out_v[4])
    {
        uint32_t tile_x = tile_index % tiles_per_row;
        uint32_t tile_y = tile_index / tiles_per_row;
        uint32_t u0 = tile_x * (tile_set_ddf->m_TileSpacing + 2 * tile_set_ddf->m_TileMargin + tile_set_ddf->m_TileWidth) + tile_set_ddf->m_TileMargin;
        uint32_t u1 = u0 + tile_set_ddf->m_TileWidth;
        uint32_t v1 = tile_y * (tile_set_ddf->m_TileSpacing + 2 * tile_set_ddf->m_TileMargin + tile_set_ddf->m_TileHeight) + tile_set_ddf->m_TileMargin;
        uint32_t v0 = v1 + tile_set_ddf->m_TileHeight;
        out_v[0] = u0 * recip_tex_width;
        out_v[1] = v0 * recip_tex_height;
        out_v[2] = u1 * recip_tex_width;
        out_v[3] = v1 * recip_tex_height;
    }

    dmGameObject::UpdateResult CompTileGridUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)params.m_Context;
        TileGridWorld* world = (TileGridWorld*) params.m_World;

        dmArray<TileGrid*>& tile_grids = world->m_TileGrids;
        uint32_t n = tile_grids.Size();

        // Count cells
        uint32_t max_cell_count = 0;
        for (uint32_t i = 0; i < n; ++i)
        {
            TileGrid* tile_grid = tile_grids[i];
            TileGridResource* resource = tile_grid->m_TileGridResource;
            max_cell_count += resource->m_ColumnCount * resource->m_RowCount * resource->m_TileGrid->m_Layers.m_Count;
        }
        if (max_cell_count > 0)
        {
            // TODO: Reuse vertices for neighbouring cells?
            uint32_t max_vertex_count = 6 * max_cell_count;
            struct Vertex
            {
                float x;
                float y;
                float z;
                float u;
                float v;
            };
            // Recreate client buffer
            uint32_t buffer_size = sizeof(Vertex) * max_vertex_count;
            if (world->m_ClientBufferSize < buffer_size)
            {
                if (world->m_ClientBuffer != 0x0)
                {
                    delete [] (char*)world->m_ClientBuffer;
                }
                world->m_ClientBuffer = new char[buffer_size];
                world->m_ClientBufferSize = buffer_size;
            }
            // Build vertex data
            uint32_t vertex_index = 0;
            for (uint32_t i = 0; i < n; ++i)
            {
                TileGrid* tile_grid = tile_grids[i];
                TileGridResource* resource = tile_grid->m_TileGridResource;
                dmGameSystemDDF::TileGrid* tile_grid_ddf = resource->m_TileGrid;
                uint32_t vertex_count = 0;
                dmGameSystemDDF::TileSet* tile_set_ddf = resource->m_TileSet->m_TileSet;
                dmGraphics::HTexture texture = resource->m_TileSet->m_Texture;
                uint32_t texture_width = dmGraphics::GetTextureWidth(texture);
                uint32_t texture_height = dmGraphics::GetTextureHeight(texture);
                float recip_tex_width = 1.0f / texture_width;
                float recip_tex_height = 1.0f / texture_height;
                uint32_t tiles_per_row = CalculateTileCount(tile_set_ddf->m_TileWidth, texture_width, tile_set_ddf->m_TileMargin, tile_set_ddf->m_TileSpacing);
                Vertex* v = &((Vertex*)world->m_ClientBuffer)[vertex_index];
                float p[4];
                float t[4];

                // TODO Cull against screen
                uint32_t column_count = resource->m_ColumnCount;
                uint32_t row_count = resource->m_RowCount;
                int32_t min_x = resource->m_MinCellX;
                int32_t min_y = resource->m_MinCellY;
                int32_t max_x = min_x + column_count;
                int32_t max_y = min_y + resource->m_RowCount;

                dmArray<TileGrid::Layer>& layers = tile_grid->m_Layers;
                uint32_t layer_count = layers.Size();
                for (uint32_t j = 0; j < layer_count; ++j)
                {
                    TileGrid::Layer* layer = &layers[j];
                    if (layer->m_Visible)
                    {
                        float z = tile_grid_ddf->m_Layers[j].m_Z;
                        for (int32_t y = min_y; y < max_y; ++y)
                        {
                            for (int32_t x = min_x; x < max_x; ++x)
                            {
                                uint32_t cell = CalculateCellIndex(j, x - min_x, y - min_y, column_count, row_count);
                                uint16_t tile = tile_grid->m_Cells[cell];
                                if (tile != 0xffff)
                                {
                                    CalculateCellBounds(x, y, tile_set_ddf->m_TileWidth, tile_set_ddf->m_TileHeight, p);
                                    CalculateTileTexCoords(tile, tile_set_ddf, tiles_per_row, recip_tex_width, recip_tex_height, t);
                                    v->x = p[0]; v->y = p[1]; v->z = z; v->u = t[0]; v->v = t[1]; ++v;
                                    v->x = p[0]; v->y = p[3]; v->z = z; v->u = t[0]; v->v = t[3]; ++v;
                                    v->x = p[2]; v->y = p[3]; v->z = z; v->u = t[2]; v->v = t[3]; ++v;
                                    v->x = p[0]; v->y = p[1]; v->z = z; v->u = t[0]; v->v = t[1]; ++v;
                                    v->x = p[2]; v->y = p[3]; v->z = z; v->u = t[2]; v->v = t[3]; ++v;
                                    v->x = p[2]; v->y = p[1]; v->z = z; v->u = t[2]; v->v = t[1]; ++v;
                                    vertex_count += 6;
                                }
                            }
                        }
                    }
                }
                if (vertex_count > 0)
                {
                    dmRender::RenderObject* ro = &tile_grid->m_RenderObject;
                    ro->m_VertexStart = vertex_index;
                    ro->m_VertexCount = vertex_count;
                    Matrix4 world = Matrix4::rotation(dmGameObject::GetWorldRotation(tile_grid->m_Instance));
                    world.setCol3(Vector4(dmGameObject::GetWorldPosition(tile_grid->m_Instance)));
                    ro->m_WorldTransform = world;
                    ro->m_Textures[0] = texture;
                    vertex_index += vertex_count;
                    dmRender::AddToRender(render_context, ro);
                }
            }
            // Clear the data to avoid locks (according to internet rumors)
            dmGraphics::SetVertexBufferData(world->m_VertexBuffer, 0, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
            dmGraphics::SetVertexBufferData(world->m_VertexBuffer, vertex_index * sizeof(Vertex), world->m_ClientBuffer, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompTileGridOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        TileGrid* tile_grid = (TileGrid*) *params.m_UserData;
        if (params.m_Message->m_Id == dmHashString64(dmGameSystemDDF::SetTile::m_DDFDescriptor->m_Name))
        {
            dmGameSystemDDF::SetTile* st = (dmGameSystemDDF::SetTile*) params.m_Message->m_Data;
            uint32_t layer_count = tile_grid->m_Layers.Size();
            uint32_t layer_index = ~0u;
            for (uint32_t i = 0; i < layer_count; ++i)
            {
                if (st->m_LayerId == tile_grid->m_Layers[i].m_Id)
                {
                    layer_index = i;
                    break;
                }
            }
            if (layer_index == ~0u)
            {
                dmLogError("Could not find layer %s when handling message %s.", (char*)dmHashReverse64(st->m_LayerId, 0x0), dmGameSystemDDF::SetTile::m_DDFDescriptor->m_Name);
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
            dmGameObject::HInstance instance = tile_grid->m_Instance;
            Point3 pos = dmGameObject::GetWorldPosition(instance);
            Quat rot_conj = conj(dmGameObject::GetWorldRotation(instance));
            Vector3 cell = rotate(rot_conj, st->m_Position - pos);
            TileGridResource* resource = tile_grid->m_TileGridResource;
            dmGameSystemDDF::TileSet* tile_set = resource->m_TileSet->m_TileSet;
            cell = mulPerElem(cell, Vector3(1.0f / tile_set->m_TileWidth, 1.0f / tile_set->m_TileHeight, 0.0f));
            int32_t cell_x = (int32_t)floor(cell.getX()) + st->m_Dx - resource->m_MinCellX;
            int32_t cell_y = (int32_t)floor(cell.getY()) + st->m_Dy - resource->m_MinCellY;
            if (cell_x < 0 || cell_x >= (int32_t)resource->m_ColumnCount || cell_y < 0 || cell_y >= (int32_t)resource->m_RowCount)
            {
                dmLogError("Could not set the tile since the supplied tile was out of range.");
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
            uint32_t cell_index = CalculateCellIndex(layer_index, cell_x, cell_y, resource->m_ColumnCount, resource->m_RowCount);
            /*
             * NOTE AND BEWARE: Empty tile is encoded as 0xffffffff
             * That's why tile-index is subtracted by 1
             * See B2GRIDSHAPE_EMPTY_CELL in b2GridShape.h
             */
            uint32_t tile = st->m_Tile - 1;
            tile_grid->m_Cells[cell_index] = (uint16_t)tile;
            // Broadcast to any collision object components
            // TODO Filter broadcast to only collision objects
            dmPhysicsDDF::SetGridShapeHull set_hull_ddf;
            set_hull_ddf.m_Shape = layer_index;
            set_hull_ddf.m_Column = cell_x;
            set_hull_ddf.m_Row = cell_y;
            set_hull_ddf.m_Hull = tile;
            dmhash_t message_id = dmHashString64(dmPhysicsDDF::SetGridShapeHull::m_DDFDescriptor->m_Name);
            uintptr_t descriptor = (uintptr_t)dmPhysicsDDF::SetGridShapeHull::m_DDFDescriptor;
            uint32_t data_size = sizeof(dmPhysicsDDF::SetGridShapeHull);
            dmMessage::URL receiver = params.m_Message->m_Receiver;
            receiver.m_Fragment = 0;
            dmMessage::Result result = dmMessage::Post(&params.m_Message->m_Receiver, &receiver, message_id, 0, descriptor, &set_hull_ddf, data_size);
            if (result != dmMessage::RESULT_OK)
            {
                LogMessageError(params.m_Message, "Could not send %s to components, result: %d.", dmPhysicsDDF::SetGridShapeHull::m_DDFDescriptor->m_Name, result);
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompTileGridOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        TileGrid* tile_grid = (TileGrid*)*params.m_UserData;
        tile_grid->m_TileGridResource = (TileGridResource*)params.m_Resource;
        if (!CreateTileGrid(tile_grid))
        {
            dmLogError("%s", "Could not recreate tile grid component, not reloaded.");
        }
    }
}
