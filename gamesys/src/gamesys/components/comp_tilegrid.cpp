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

extern unsigned char SPRITE_VPC[];
extern uint32_t SPRITE_VPC_SIZE;

extern unsigned char SPRITE_FPC[];
extern uint32_t SPRITE_FPC_SIZE;

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

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

    dmGameObject::CreateResult CompTileGridCreate(const dmGameObject::ComponentCreateParams& params)
    {
        TileGridResource* resource = (TileGridResource*) params.m_Resource;
        TileGridWorld* world = (TileGridWorld*) params.m_World;
        if (world->m_TileGrids.Full())
        {
            world->m_TileGrids.OffsetCapacity(16);
        }
        TileGrid* tile_grid = new TileGrid(params.m_Instance, resource);
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

    void CalculateCellBounds(dmGameSystemDDF::TileCell* cell_ddf, dmGameSystemDDF::TileSet* tile_set, float out_v[4])
    {
        out_v[0] = tile_set->m_TileWidth * cell_ddf->m_X;
        out_v[1] = tile_set->m_TileHeight * cell_ddf->m_Y;
        out_v[2] = out_v[0] + tile_set->m_TileWidth;
        out_v[3] = out_v[1] + tile_set->m_TileHeight;
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
        uint32_t total_cell_count = 0;
        for (uint32_t i = 0; i < n; ++i)
        {
            TileGrid* tile_grid = tile_grids[i];
            dmGameSystemDDF::TileGrid* tile_grid_ddf = tile_grid->m_TileGridResource->m_TileGrid;
            uint32_t layer_count = tile_grid_ddf->m_Layers.m_Count;
            for (uint32_t j = 0; j < layer_count; ++j)
            {
                dmGameSystemDDF::TileLayer* layer_ddf = &tile_grid_ddf->m_Layers[j];
                if (layer_ddf->m_IsVisible)
                {
                    total_cell_count += layer_ddf->m_Cell.m_Count;
                }
            }
        }
        if (total_cell_count > 0)
        {
            // TODO: Reuse vertices for neighbouring cells?
            uint32_t vertex_count = 6 * total_cell_count;
            struct Vertex
            {
                float x;
                float y;
                float z;
                float u;
                float v;
            };
            // Recreate client buffer
            uint32_t buffer_size = sizeof(Vertex) * vertex_count;
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
                uint32_t layer_count = tile_grid_ddf->m_Layers.m_Count;
                uint32_t vertex_count = 0;
                dmGameSystemDDF::TileSet* tile_set_ddf = resource->m_TileSet->m_TileSet;
                dmGraphics::HTexture texture = resource->m_TileSet->m_Texture;
                uint32_t texture_width = dmGraphics::GetTextureWidth(texture);
                uint32_t texture_height = dmGraphics::GetTextureHeight(texture);
                float recip_tex_width = 1.0f / texture_width;
                float recip_tex_height = 1.0f / texture_height;
                uint32_t tiles_per_row = CalculateTileCount(tile_set_ddf->m_TileWidth, texture_width, tile_set_ddf->m_TileMargin, tile_set_ddf->m_TileSpacing);
                Vertex* v = &((Vertex*)world->m_ClientBuffer)[vertex_index];
                for (uint32_t j = 0; j < layer_count; ++j)
                {
                    dmGameSystemDDF::TileLayer* layer_ddf = &tile_grid_ddf->m_Layers[j];
                    if (layer_ddf->m_IsVisible)
                    {
                        uint32_t cell_count = layer_ddf->m_Cell.m_Count;
                        float z = layer_ddf->m_Z;
                        float p[4];
                        float t[4];
                        for (uint32_t k = 0; k < cell_count; ++k)
                        {
                            dmGameSystemDDF::TileCell* cell_ddf = &layer_ddf->m_Cell[k];
                            CalculateCellBounds(cell_ddf, tile_set_ddf, p);
                            CalculateTileTexCoords(cell_ddf->m_Tile, tile_set_ddf, tiles_per_row, recip_tex_width, recip_tex_height, t);
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
        return dmGameObject::UPDATE_RESULT_OK;
    }
}
