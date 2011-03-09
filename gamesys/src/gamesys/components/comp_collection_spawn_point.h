#ifndef DM_GAMESYS_COLLECTION_SPAWN_POINT_H
#define DM_GAMESYS_COLLECTION_SPAWN_POINT_H

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompCollectionSpawnPointNewWorld(void* context, void** world);

    dmGameObject::CreateResult CompCollectionSpawnPointDeleteWorld(void* context, void* world);

    dmGameObject::CreateResult CompCollectionSpawnPointCreate(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data);

    dmGameObject::CreateResult CompCollectionSpawnPointDestroy(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* world,
                                                void* context,
                                                uintptr_t* user_data);

    dmGameObject::UpdateResult CompCollectionSpawnPointUpdate(dmGameObject::HCollection collection,
                                               const dmGameObject::UpdateContext* update_context,
                                               void* world,
                                               void* context);

    dmGameObject::UpdateResult CompCollectionSpawnPointOnMessage(dmGameObject::HInstance instance,
                                                const dmGameObject::InstanceMessageData* message_data,
                                                void* context,
                                                uintptr_t* user_data);

    dmGameObject::InputResult CompCollectionSpawnPointOnInput(dmGameObject::HInstance instance,
                                                const dmGameObject::InputAction* input_action,
                                                void* context,
                                                uintptr_t* user_data);
}

#endif // DM_GAMESYS_COLLECTION_SPAWN_POINT_H
