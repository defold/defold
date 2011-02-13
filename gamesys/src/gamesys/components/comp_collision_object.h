#ifndef DM_GAMESYS_COMP_COLLISION_OBJECT_H
#define DM_GAMESYS_COMP_COLLISION_OBJECT_H

#include <stdint.h>

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompCollisionObjectNewWorld(void* context, void** world);

    dmGameObject::CreateResult CompCollisionObjectDeleteWorld(void* context, void* world);

    dmGameObject::CreateResult CompCollisionObjectCreate(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data);

    dmGameObject::CreateResult CompCollisionObjectInit(dmGameObject::HCollection collection,
                                            dmGameObject::HInstance instance,
                                            void* world,
                                            void* context,
                                            uintptr_t* user_data);

    dmGameObject::CreateResult CompCollisionObjectDestroy(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* world,
                                                void* context,
                                                uintptr_t* user_data);

    dmGameObject::UpdateResult CompCollisionObjectUpdate(dmGameObject::HCollection collection,
                         const dmGameObject::UpdateContext* update_context,
                         void* world,
                         void* context);

    dmGameObject::UpdateResult CompCollisionObjectOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data);

    void CompCollisionObjectOnReload(dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data);
}

#endif // DM_GAMESYS_COMP_COLLISION_OBJECT_H
