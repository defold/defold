#ifndef DM_GAMESYS_COMP_TILEGRID_H
#define DM_GAMESYS_COMP_TILEGRID_H

#include <dlib/array.h>
#include <gameobject/gameobject.h>
#include "tile_ddf.h"

namespace dmGameSystem
{
    struct TileGrid
    {
        dmGameObject::HInstance      m_Instance;
        dmGameSystemDDF::TileGrid**  m_TileGridResource;
        TileGrid(dmGameObject::HInstance instance, dmGameSystemDDF::TileGrid** tile_grid_resource)
        {
            m_Instance = instance;
            m_TileGridResource = tile_grid_resource;
        }
    };

    struct TileGridWorld
    {
        dmArray<TileGrid*> m_TileGrids;
    };

    dmGameObject::CreateResult CompTileGridNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompTileGridDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompTileGridCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompTileGridDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::UpdateResult CompTileGridUpdate(const dmGameObject::ComponentsUpdateParams& params);

    dmGameObject::UpdateResult CompTileGridOnMessage(const dmGameObject::ComponentOnMessageParams& params);
}

#endif
