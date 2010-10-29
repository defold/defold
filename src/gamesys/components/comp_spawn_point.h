#ifndef DM_GAMESYS_SPAWN_POINT_H
#define DM_GAMESYS_SPAWN_POINT_H

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompSpawnPointNewWorld(void* context, void** world);

    dmGameObject::CreateResult CompSpawnPointDeleteWorld(void* context, void* world);

    dmGameObject::CreateResult CompSpawnPointCreate(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data);

    dmGameObject::CreateResult CompSpawnPointDestroy(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* world,
                                                void* context,
                                                uintptr_t* user_data);

    dmGameObject::UpdateResult CompSpawnPointUpdate(dmGameObject::HCollection collection,
                                               const dmGameObject::UpdateContext* update_context,
                                               void* world,
                                               void* context);

    dmGameObject::UpdateResult CompSpawnPointOnMessage(dmGameObject::HInstance instance,
                                                const dmGameObject::InstanceMessageData* message_data,
                                                void* context,
                                                uintptr_t* user_data);
}

#endif
