#ifndef DM_GAMESYS_SPAWN_POINT_H
#define DM_GAMESYS_SPAWN_POINT_H

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompSpawnPointNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompSpawnPointDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompSpawnPointCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompSpawnPointDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::UpdateResult CompSpawnPointPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params);

    dmGameObject::UpdateResult CompSpawnPointOnMessage(const dmGameObject::ComponentOnMessageParams& params);
}

#endif
