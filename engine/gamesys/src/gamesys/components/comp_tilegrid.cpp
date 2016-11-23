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
#include <vectormath/cpp/vectormath_aos.h>

#include "tile_ddf.h"
#include "../gamesys.h"
#include "../gamesys_private.h"
#include "../proto/tile_ddf.h"
#include "../proto/physics_ddf.h"
#include "../resources/res_tilegrid.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    struct TileGridComponent
    {
        struct Layer
        {
            dmhash_t    m_Id;
            uint32_t    m_Visible : 1;
        };

        struct Flags
        {
            uint16_t    m_FlipHorizontal : 1;
            uint16_t    m_FlipVertical : 1;
            uint16_t    m_Padding : 14;
        };
    
        TileGridComponent()
        : m_Instance(0)
        , m_Resource(0)
        , m_Cells(0)
        , m_CellFlags(0)
        {

        }

        dmArray<Layer>              m_Layers;
        Vectormath::Aos::Vector3    m_Translation;
        Vectormath::Aos::Quat       m_Rotation;
        Vectormath::Aos::Matrix4    m_RenderWorldTransform;
        dmGameObject::HInstance     m_Instance;
        TileGridResource*           m_Resource;
        uint16_t*                   m_Cells;
        Flags*                      m_CellFlags;
        uint32_t                    m_MixedHash;
        CompRenderConstants         m_RenderConstants;
        uint16_t                    m_Enabled : 1;
        uint16_t                    m_AddedToUpdate : 1;
        uint16_t                    m_Padding : 14;
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

        dmArray<TileGridComponent*>     m_Components;
        dmArray<dmRender::RenderObject> m_RenderObjects;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        
        dmGraphics::HVertexBuffer       m_VertexBuffer;
        TileGridVertex*                 m_VertexBufferData;
        TileGridVertex*                 m_VertexBufferDataEnd;
        TileGridVertex*                 m_VertexBufferWritePtr;

        uint32_t                        m_MaxTilemapCount;
        uint32_t                        m_MaxTileCount;
        uint32_t                        m_NumLayers; // total count to allocate for in render stage
    };


    dmGameObject::CreateResult CompTileGridNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        TileGridWorld* world = new TileGridWorld;
        TilemapContext* context = (TilemapContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);

        world->m_MaxTilemapCount = context->m_MaxTilemapCount;
        world->m_MaxTileCount = context->m_MaxTileCount;
        world->m_RenderObjects.SetCapacity(world->m_MaxTilemapCount);
        world->m_Components.SetCapacity(world->m_MaxTilemapCount);

        // TODO: Everything below here should be move to the "universe" when available
        // and hence shared among all the worlds
        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT, false},
        };
        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(graphics_context, ve, sizeof(ve) / sizeof(ve[0]));
        world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), 0, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        world->m_VertexBufferData = (TileGridVertex*) malloc(sizeof(TileGridVertex) * 6 * world->m_MaxTileCount);
        world->m_VertexBufferDataEnd = world->m_VertexBufferData + sizeof(TileGridVertex) * 6 * world->m_MaxTileCount;
        world->m_NumLayers = 0;

        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompTileGridDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        TileGridWorld* world = (TileGridWorld*) params.m_World;
        dmGraphics::DeleteVertexDeclaration(world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(world->m_VertexBuffer);
        free(world->m_VertexBufferData);
        delete world;
        return dmGameObject::CREATE_RESULT_OK;
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

    void SetTileGridTile(const TileGridComponent* component, uint32_t layer, int32_t cell_x, int32_t cell_y, uint32_t tile, bool flip_h, bool flip_v)
    {
        TileGridResource* resource = component->m_Resource;
        uint32_t cell_index = CalculateCellIndex(layer, cell_x, cell_y, resource->m_ColumnCount, resource->m_RowCount);
        component->m_Cells[cell_index] = tile;

        TileGridComponent::Flags* flags = &component->m_CellFlags[cell_index];
        flags->m_FlipHorizontal = flip_h;
        flags->m_FlipVertical = flip_v;
    }

    static void ReHash(TileGridComponent* component)
    {
        HashState32 state;
        TileGridResource* resource = component->m_Resource;

        dmHashInit32(&state, false);
        dmHashUpdateBuffer32(&state, &resource->m_Material, sizeof(resource->m_Material));
        dmHashUpdateBuffer32(&state, &resource->m_TileGrid->m_BlendMode, sizeof(resource->m_TileGrid->m_BlendMode));
        ReHashRenderConstants(&component->m_RenderConstants, &state);

        component->m_MixedHash = dmHashFinal32(&state);
    }

    static uint32_t CreateTileGrid(TileGridComponent* tile_grid)
    {
        TileGridResource* resource = tile_grid->m_Resource;
        dmGameSystemDDF::TileGrid* tile_grid_ddf = resource->m_TileGrid;
        uint32_t n_layers = tile_grid_ddf->m_Layers.m_Count;
        dmArray<TileGridComponent::Layer>& layers = tile_grid->m_Layers;
        uint32_t layer_count = 0;
        if (layers.Capacity() < n_layers)
        {
            layers.SetCapacity(n_layers);
            layers.SetSize(n_layers);
            for (uint32_t i = 0; i < n_layers; ++i)
            {
                TileGridComponent::Layer& layer = layers[i];
                dmGameSystemDDF::TileLayer* layer_ddf = &tile_grid_ddf->m_Layers[i];
                layer.m_Id = dmHashString64(layer_ddf->m_Id);
                layer.m_Visible = layer_ddf->m_IsVisible;
                layer_count += layer.m_Visible ? 1 : 0;
            }
        }
        uint32_t cell_count = resource->m_ColumnCount * resource->m_RowCount * n_layers;
        if (tile_grid->m_Cells != 0x0)
        {
            delete [] tile_grid->m_Cells;
        }
        tile_grid->m_Cells = new uint16_t[cell_count];
        memset(tile_grid->m_Cells, 0xff, cell_count * sizeof(uint16_t));
        if (tile_grid->m_CellFlags != 0x0)
        {
            delete [] tile_grid->m_CellFlags;
        }
        tile_grid->m_CellFlags = new TileGridComponent::Flags[cell_count];
        memset(tile_grid->m_CellFlags, 0, cell_count * sizeof(TileGridComponent::Flags));
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
        return layer_count;
    }

    dmGameObject::CreateResult CompTileGridCreate(const dmGameObject::ComponentCreateParams& params)
    {
        TileGridWorld* world = (TileGridWorld*) params.m_World;
        if (world->m_Components.Full())
        {
            dmLogError("Tilemap could not be created since the tilemap buffer is full (%d). You can change this with the config setting tilemap.max_count", world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
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
        world->m_NumLayers += layer_count;

        world->m_Components.Push(component);
        *params.m_UserData = (uintptr_t) component;

        ReHash(component);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompTileGridDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        TileGridComponent* tile_grid = (TileGridComponent*) *params.m_UserData;
        TileGridWorld* world = (TileGridWorld*) params.m_World;
        for (uint32_t i = 0; i < world->m_Components.Size(); ++i)
        {
            if (world->m_Components[i] == tile_grid)
            {
                uint32_t layer_count = 0;

                dmGameSystemDDF::TileGrid* tile_grid_ddf = tile_grid->m_Resource->m_TileGrid;
                uint32_t n_layers = tile_grid_ddf->m_Layers.m_Count;
                for (uint32_t i = 0; i < n_layers; ++i)
                {
                    dmGameSystemDDF::TileLayer* layer_ddf = &tile_grid_ddf->m_Layers[i];
                    layer_count += layer_ddf->m_IsVisible ? 1 : 0;
                }

                world->m_NumLayers -= layer_count;

                delete [] tile_grid->m_Cells;
                delete [] tile_grid->m_CellFlags;
                world->m_Components.EraseSwap(i);
                delete tile_grid;
                return dmGameObject::CREATE_RESULT_OK;
            }
        }
        assert(false);
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    static void CalculateCellBounds(int32_t cell_x, int32_t cell_y, int32_t cell_width, int32_t cell_height, float out_v[4])
    {
        out_v[0] = cell_x * cell_width;
        out_v[1] = cell_y * cell_height;
        out_v[2] = (cell_x + 1) * cell_width;
        out_v[3] = (cell_y + 1) * cell_height;
    }

    dmGameObject::CreateResult CompTileGridAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        TileGridComponent* component = (TileGridComponent*) *params.m_UserData;
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompTileGridUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static inline uintptr_t EncodeGridAndLayer(uint32_t tile_grid, uint32_t layer)
    {
        return (uintptr_t)(tile_grid | (layer << 16));
    }

    static inline void DecodeGridAndLayer(uintptr_t ptr, uint32_t& tile_grid, uint32_t& layer)
    {
        tile_grid = ptr & 0xFFFF;
        layer = (ptr >> 16) & 0xFFFF;
    }

    TileGridVertex* CreateVertexData(TileGridWorld* world, TileGridVertex* where, TextureSetResource* texture_set, dmRender::RenderListEntry* buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(TileGrid, "CreateVertexData");
        static int tex_coord_order[] = {
            0,1,2,2,3,0,
            3,2,1,1,0,3,    //h
            1,0,3,3,2,1,    //v
            2,3,0,0,1,2     //hv
        };

        dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;

        for (uint32_t* i = begin; i != end; ++i)
        {
            uint32_t index, layer;
            DecodeGridAndLayer(buf[*i].m_UserData, index, layer);

            const TileGridComponent* component = world->m_Components[index];
            const TileGridResource* resource = component->m_Resource;

            const float* tex_coords = (const float*) texture_set_ddf->m_TexCoords.m_Data;
            uint32_t column_count = resource->m_ColumnCount;
            uint32_t row_count = resource->m_RowCount;

            int32_t min_x = resource->m_MinCellX;
            int32_t min_y = resource->m_MinCellY;
            int32_t max_x = min_x + (int32_t)column_count;
            int32_t max_y = min_y + (int32_t)row_count;

            for (int32_t y = min_y; y < max_y; ++y)
            {
                for (int32_t x = min_x; x < max_x; ++x)
                {
                    float p[4];
                    uint32_t cell = CalculateCellIndex(layer, x - resource->m_MinCellX, y - resource->m_MinCellY, column_count, row_count);
                    uint16_t tile = component->m_Cells[cell];
                    if (tile == 0xffff)
                    {
                        continue;
                    }

                    if( where >= world->m_VertexBufferDataEnd )
                    {
                        dmLogError("Out of tiles to render (%u). You can change this with the config setting tilemap.max_tile_count", (world->m_VertexBufferDataEnd - world->m_VertexBufferData) / 6);
                        return world->m_VertexBufferDataEnd;
                    }

                    CalculateCellBounds(x, y, texture_set_ddf->m_TileWidth, texture_set_ddf->m_TileHeight, p);
                    const float* puv = &tex_coords[tile * 8];
                    uint32_t flip_flag = 0;

                    TileGridComponent::Flags flags = component->m_CellFlags[cell];
                    if (flags.m_FlipHorizontal)
                    {
                        flip_flag = 1;
                    }
                    if (flags.m_FlipVertical)
                    {
                        flip_flag |= 2;
                    }
                    const int* tex_lookup = &tex_coord_order[flip_flag * 6];

                    #define SET_VERTEX(_I, _X, _Y, _Z, _U, _V) \
                        where[_I].x = _X; \
                        where[_I].y = _Y; \
                        where[_I].z = _Z; \
                        where[_I].u = _U; \
                        where[_I].v = _V;

                    SET_VERTEX(0, p[0], p[1], 0.0f, puv[tex_lookup[0] * 2], puv[tex_lookup[0] * 2 + 1]);
                    SET_VERTEX(1, p[0], p[3], 0.0f, puv[tex_lookup[1] * 2], puv[tex_lookup[1] * 2 + 1]);
                    SET_VERTEX(2, p[2], p[3], 0.0f, puv[tex_lookup[2] * 2], puv[tex_lookup[2] * 2 + 1]);
                    SET_VERTEX(3, p[2], p[3], 0.0f, puv[tex_lookup[3] * 2], puv[tex_lookup[3] * 2 + 1]);
                    SET_VERTEX(4, p[2], p[1], 0.0f, puv[tex_lookup[4] * 2], puv[tex_lookup[4] * 2 + 1]);
                    SET_VERTEX(5, p[0], p[1], 0.0f, puv[tex_lookup[5] * 2], puv[tex_lookup[5] * 2 + 1]);

                    where += 6;

                    #undef SET_VERTEX
                }
            }
        }
        return where;
    }

    static void RenderBatch(TileGridWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(TileGrid, "RenderBatch");

        uint32_t index, layer;
        DecodeGridAndLayer(buf[*begin].m_UserData, index, layer);
        const TileGridComponent* first = world->m_Components[index];
        assert(first->m_Enabled);

        TileGridResource* resource = first->m_Resource;
        TextureSetResource* texture_set = resource->m_TextureSet;

        dmRender::RenderObject& ro = *world->m_RenderObjects.End();
        world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);

        // Fill in vertex buffer
        TileGridVertex* vb_begin = world->m_VertexBufferWritePtr;
        world->m_VertexBufferWritePtr = CreateVertexData(world, vb_begin, texture_set, buf, begin, end);

        ro.Init();
        ro.m_VertexDeclaration = world->m_VertexDeclaration;
        ro.m_VertexBuffer = world->m_VertexBuffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = vb_begin - world->m_VertexBufferData;
        ro.m_VertexCount = (world->m_VertexBufferWritePtr - vb_begin);
        ro.m_Material = resource->m_Material;
        ro.m_Textures[0] = texture_set->m_Texture;
        
        const dmRender::Constant* constants = first->m_RenderConstants.m_RenderConstants;
        uint32_t size = first->m_RenderConstants.m_ConstantCount;
        for (uint32_t i = 0; i < size; ++i)
        {
            const dmRender::Constant& c = constants[i];
            dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
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
            dmGraphics::SetVertexBufferData(world->m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
            dmGraphics::SetVertexBufferData(world->m_VertexBuffer, sizeof(TileGridVertex) * (world->m_VertexBufferWritePtr - world->m_VertexBufferData),
                                            world->m_VertexBufferData, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
            DM_COUNTER("TileGridVertexBuffer", (world->m_VertexBufferWritePtr - world->m_VertexBufferData) * sizeof(TileGridVertex));
            break;

        case dmRender::RENDER_LIST_OPERATION_BATCH:
            assert(params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH);
            RenderBatch(world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
        }
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

        dmRender::HRenderContext render_context = context->m_RenderContext;
        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, world->m_NumLayers + 10);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, world);
        dmRender::RenderListEntry* write_ptr = render_list;

        for (uint32_t i = 0; i < n; ++i)
        {
            TileGridComponent* component = components[i];
            if (!component->m_Enabled || !component->m_AddedToUpdate) {
                continue;
            }

            uint32_t const_count = component->m_RenderConstants.m_ConstantCount;
            for (uint32_t const_i = 0; const_i < const_count; ++const_i)
            {
                if (lengthSqr(component->m_RenderConstants.m_RenderConstants[const_i].m_Value - component->m_RenderConstants.m_PrevRenderConstants[const_i]) > 0)
                {
                    ReHash(component);
                    break;
                }
            }

            TileGridResource* resource = component->m_Resource;
            dmGameSystemDDF::TileGrid* tile_grid_ddf = resource->m_TileGrid;
            uint32_t n_layers = tile_grid_ddf->m_Layers.m_Count;
            for (uint32_t l = 0; l < n_layers; ++l)
            {
                dmGameSystemDDF::TileLayer* layer_ddf = &tile_grid_ddf->m_Layers[l];
                if (!layer_ddf->m_IsVisible)
                    continue;

                Matrix4 local(component->m_Rotation, component->m_Translation + Vector3(0, 0, layer_ddf->m_Z));
                const Matrix4& go_world = dmGameObject::GetWorldMatrix(component->m_Instance);
                if (dmGameObject::ScaleAlongZ(component->m_Instance))
                {
                    component->m_RenderWorldTransform = go_world * local;
                }
                else
                {
                    component->m_RenderWorldTransform = dmTransform::MulNoScaleZ(go_world, local);
                }

                const Vector4 trans = component->m_RenderWorldTransform.getCol(3);
                write_ptr->m_WorldPosition = Point3(trans.getXYZ());
                write_ptr->m_UserData = EncodeGridAndLayer(i, l);
                write_ptr->m_TagMask = dmRender::GetMaterialTagMask(component->m_Resource->m_Material);
                write_ptr->m_BatchKey = component->m_MixedHash;
                write_ptr->m_Dispatch = dispatch;
                write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
                ++write_ptr;
            }
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    uint32_t GetLayerIndex(const TileGridComponent* component, dmhash_t layer_id)
    {
        uint32_t layer_count = component->m_Layers.Size();
        uint32_t layer_index = ~0u;
        for (uint32_t i = 0; i < layer_count; ++i)
        {
            if (layer_id == component->m_Layers[i].m_Id)
            {
                layer_index = i;
                break;
            }
        }
        return layer_index;
    }

    dmGameObject::UpdateResult CompTileGridOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        TileGridComponent* component = (TileGridComponent*) *params.m_UserData;
        if (params.m_Message->m_Id == dmGameSystemDDF::SetTile::m_DDFDescriptor->m_NameHash)
        {
            dmGameSystemDDF::SetTile* st = (dmGameSystemDDF::SetTile*) params.m_Message->m_Data;
            uint32_t layer_count = component->m_Layers.Size();
            uint32_t layer_index = ~0u;
            for (uint32_t i = 0; i < layer_count; ++i)
            {
                if (st->m_LayerId == component->m_Layers[i].m_Id)
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
            dmGameObject::HInstance instance = component->m_Instance;
            dmTransform::Transform inv_world(dmTransform::Inv(dmGameObject::GetWorldTransform(instance)));
            Point3 cell = st->m_Position;
            if (dmGameObject::ScaleAlongZ(instance))
            {
                cell = dmTransform::Apply(inv_world, cell);
            }
            else
            {
                cell = dmTransform::ApplyNoScaleZ(inv_world, cell);
            }
            TileGridResource* resource = component->m_Resource;
            dmGameSystemDDF::TextureSet* texture_set = resource->m_TextureSet->m_TextureSet;
            cell = mulPerElem(cell, Point3(1.0f / texture_set->m_TileWidth, 1.0f / texture_set->m_TileHeight, 0.0f));
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
            component->m_Cells[cell_index] = (uint16_t)tile;
            // Broadcast to any collision object components
            // TODO Filter broadcast to only collision objects
            dmPhysicsDDF::SetGridShapeHull set_hull_ddf;
            set_hull_ddf.m_Shape = layer_index;
            set_hull_ddf.m_Column = cell_x;
            set_hull_ddf.m_Row = cell_y;
            set_hull_ddf.m_Hull = tile;
            dmhash_t message_id = dmPhysicsDDF::SetGridShapeHull::m_DDFDescriptor->m_NameHash;
            uintptr_t descriptor = (uintptr_t)dmPhysicsDDF::SetGridShapeHull::m_DDFDescriptor;
            uint32_t data_size = sizeof(dmPhysicsDDF::SetGridShapeHull);
            dmMessage::URL receiver = params.m_Message->m_Receiver;
            receiver.m_Fragment = 0;
            dmMessage::Result result = dmMessage::Post(&params.m_Message->m_Receiver, &receiver, message_id, 0, descriptor, &set_hull_ddf, data_size, 0);
            if (result != dmMessage::RESULT_OK)
            {
                LogMessageError(params.m_Message, "Could not send %s to components, result: %d.", dmPhysicsDDF::SetGridShapeHull::m_DDFDescriptor->m_Name, result);
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
        }
        else if (params.m_Message->m_Id == dmGameSystemDDF::SetConstantTileMap::m_DDFDescriptor->m_NameHash)
        {
            dmGameSystemDDF::SetConstantTileMap* ddf = (dmGameSystemDDF::SetConstantTileMap*)params.m_Message->m_Data;
            SetRenderConstant(&component->m_RenderConstants, component->m_Resource->m_Material, ddf->m_NameHash, 0, ddf->m_Value);
            ReHash(component);
        }
        else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstantTileMap::m_DDFDescriptor->m_NameHash)
        {
            dmGameSystemDDF::ResetConstantTileMap* ddf = (dmGameSystemDDF::ResetConstantTileMap*)params.m_Message->m_Data;
            ClearRenderConstant(&component->m_RenderConstants, ddf->m_NameHash);
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
        uint32_t layer_count = CreateTileGrid(component);
        if (layer_count == 0)
        {
            dmLogError("Could not recreate tile grid component, not reloaded.");
        }
    }

    static bool CompTileGridGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        TileGridComponent* component = (TileGridComponent*)user_data;
        return GetRenderConstant(&component->m_RenderConstants, name_hash, out_constant);
    }

    static void CompTileGridSetConstantCallback(void* user_data, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        TileGridComponent* component = (TileGridComponent*)user_data;
        SetRenderConstant(&component->m_RenderConstants, component->m_Resource->m_Material, name_hash, element_index, var);
        ReHash(component);
    }

    dmGameObject::PropertyResult CompTileGridGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        TileGridComponent* component = (TileGridComponent*)*params.m_UserData;
        return GetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, out_value, CompTileGridGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompTileGridSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        TileGridComponent* component = (TileGridComponent*)*params.m_UserData;
        return SetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, params.m_Value, CompTileGridSetConstantCallback, component);
    }
}
