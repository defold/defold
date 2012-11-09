#ifndef DM_GAMESYS_COMP_TILEGRID_H
#define DM_GAMESYS_COMP_TILEGRID_H

#include <dlib/array.h>
#include <gameobject/gameobject.h>
#include "../resources/res_tilegrid.h"
#include "tile_ddf.h"

namespace dmGameSystem
{
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

        TileGridComponent();

        dmArray<Layer>              m_Layers;
        Vectormath::Aos::Vector3    m_Translation;
        Vectormath::Aos::Quat       m_Rotation;
        dmGameObject::HInstance     m_Instance;
        TileGridResource*           m_TileGridResource;
        uint16_t*                   m_Cells;
        uint16_t                    m_RegionsX;
        uint16_t                    m_RegionsY;
        dmArray<TileGridRegion>     m_Regions;
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

    dmGameObject::CreateResult CompTileGridNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompTileGridDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompTileGridCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompTileGridDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::UpdateResult CompTileGridUpdate(const dmGameObject::ComponentsUpdateParams& params);

    dmGameObject::UpdateResult CompTileGridOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompTileGridOnReload(const dmGameObject::ComponentOnReloadParams& params);
}

#endif
