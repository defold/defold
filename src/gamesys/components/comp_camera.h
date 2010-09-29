#ifndef DM_GAMESYS_COMP_CAMERA_H
#define DM_GAMESYS_COMP_CAMERA_H

#include <stdint.h>
#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompCameraNewWorld(void* context, void** world);

    dmGameObject::CreateResult CompCameraDeleteWorld(void* context, void* world);

    dmGameObject::CreateResult CompCameraCreate(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data);

    dmGameObject::CreateResult CompCameraDestroy(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* world,
            void* context,
            uintptr_t* user_data);

    dmGameObject::UpdateResult CompCameraUpdate(dmGameObject::HCollection collection,
            const dmGameObject::UpdateContext* update_context,
            void* world,
            void* context);

    dmGameObject::UpdateResult CompCameraOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data);
}

#endif // DM_GAMESYS_COMP_CAMERA_H
