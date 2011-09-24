#include "comp_tilegrid.h"
//#include "resources/res_light.h"

#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <render/render.h>
#include <gameobject/gameobject.h>
#include <vectormath/cpp/vectormath_aos.h>

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    dmGameObject::CreateResult CompTileGridNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        *params.m_World = new TileGridWorld;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompTileGridDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        TileGridWorld* tilegrid_world = (TileGridWorld*) params.m_World;
        delete tilegrid_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompTileGridCreate(const dmGameObject::ComponentCreateParams& params)
    {
        dmGameSystemDDF::TileGrid** tilegrid_resource = (dmGameSystemDDF::TileGrid**) params.m_Resource;
        TileGridWorld* tilegrid_world = (TileGridWorld*) params.m_World;
        if (tilegrid_world->m_TileGrids.Full())
        {
            tilegrid_world->m_TileGrids.OffsetCapacity(16);
        }
        TileGrid* tilegrid = new TileGrid(params.m_Instance, tilegrid_resource);
        tilegrid_world->m_TileGrids.Push(tilegrid);

        *params.m_UserData = (uintptr_t) tilegrid;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompTileGridDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        TileGrid* tilegrid = (TileGrid*) *params.m_UserData;
        TileGridWorld* tilegrid_world = (TileGridWorld*) params.m_World;
        for (uint32_t i = 0; i < tilegrid_world->m_TileGrids.Size(); ++i)
        {
            if (tilegrid_world->m_TileGrids[i] == tilegrid)
            {
                tilegrid_world->m_TileGrids.EraseSwap(i);
                delete tilegrid;
                return dmGameObject::CREATE_RESULT_OK;
            }
        }
        assert(false);
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::UpdateResult CompTileGridUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        TileGridWorld* tilegrid_world = (TileGridWorld*) params.m_World;
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompTileGridOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        return dmGameObject::UPDATE_RESULT_OK;
    }
}
