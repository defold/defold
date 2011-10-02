#ifndef DM_GAMESYS_COMP_TILEGRID_H
#define DM_GAMESYS_COMP_TILEGRID_H

#include <dlib/array.h>
#include <gameobject/gameobject.h>
#include "../resources/res_tilegrid.h"
#include "tile_ddf.h"

namespace dmGameSystem
{
    struct TileGrid
    {
        struct Layer
        {
            dmhash_t    m_Id;
            uint32_t    m_Visible : 1;
        };

        TileGrid();

        // TODO: Batch multiple grids into shared ROs
        dmRender::RenderObject      m_RenderObject;
        dmArray<Layer>              m_Layers;
        dmGameObject::HInstance     m_Instance;
        TileGridResource*           m_TileGridResource;
        uint16_t*                   m_Cells;
    };

    struct TileGridWorld
    {
        TileGridWorld()
        {
            memset(this, 0, sizeof(TileGridWorld));
        }

        dmArray<TileGrid*>              m_TileGrids;
        dmGraphics::HVertexBuffer       m_VertexBuffer;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmRender::HMaterial             m_Material;
        dmGraphics::HVertexProgram      m_VertexProgram;
        dmGraphics::HFragmentProgram    m_FragmentProgram;
        void*                           m_ClientBuffer;
        uint32_t                        m_ClientBufferSize;
    };

    dmGameObject::CreateResult CompTileGridNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompTileGridDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompTileGridCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompTileGridDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::UpdateResult CompTileGridUpdate(const dmGameObject::ComponentsUpdateParams& params);

    dmGameObject::UpdateResult CompTileGridOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompTileGridOnReload(const dmGameObject::ComponentOnReloadParams& params);
}

#endif
