#ifndef DM_GAMESYS_COMP_TILEGRID_H
#define DM_GAMESYS_COMP_TILEGRID_H

#include <dlib/array.h>
#include <gameobject/gameobject.h>
#include "../resources/res_tilegrid.h"
#include "tile_ddf.h"

namespace dmGameSystem
{
    // Number of tiles in a region. A region is a subset of a tile-grid.
    // Tile-grids are divied into regions to reduce the vertex buffer creation cost
    const uint32_t TILEGRID_REGION_WIDTH = 32;
    const uint32_t TILEGRID_REGION_HEIGHT = 32;

    struct TileGridRegion
    {
        dmRender::RenderObject    m_RenderObject;
        void*                     m_ClientBuffer;
        uint32_t                  m_ClientBufferSize;
        uint32_t                  m_Dirty : 1;
    };

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

        TileGridComponent();

        dmArray<Layer>              m_Layers;
        Vectormath::Aos::Vector3    m_Translation;
        Vectormath::Aos::Quat       m_Rotation;
        Vectormath::Aos::Matrix4    m_RenderWorldTransform;
        dmGameObject::HInstance     m_Instance;
        TileGridResource*           m_TileGridResource;
        uint16_t*                   m_Cells;
        Flags*                      m_CellFlags;
        uint16_t                    m_RegionsX;
        uint16_t                    m_RegionsY;
        dmArray<TileGridRegion>     m_Regions;
        uint16_t                    m_Enabled : 1;
        uint16_t                    m_AddedToUpdate : 1;
        uint16_t                    m_Padding : 14;
    };

    struct TileGridWorld
    {
        TileGridWorld()
        {
            memset(this, 0, sizeof(TileGridWorld));
        }

        dmArray<TileGridComponent*>     m_TileGrids;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
    };

    uint32_t CalculateCellIndex(uint32_t layer, int32_t cell_x, int32_t cell_y, uint32_t column_count, uint32_t row_count);

    uint32_t GetLayerIndex(const TileGridComponent* component, dmhash_t layer_id);

    dmGameObject::CreateResult CompTileGridNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompTileGridDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompTileGridCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompTileGridDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompTileGridAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompTileGridUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompTileGridRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompTileGridOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompTileGridOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompTileGridGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompTileGridSetProperty(const dmGameObject::ComponentSetPropertyParams& params);
}

#endif
