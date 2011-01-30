#ifndef DM_GAMESYS_COMP_EMITTER_H
#define DM_GAMESYS_COMP_EMITTER_H

#include <dlib/configfile.h>

#include <gameobject/gameobject.h>

#include <render/render.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompEmitterNewWorld(void* context, void** world);

    dmGameObject::CreateResult CompEmitterDeleteWorld(void* context, void* world);

    dmGameObject::CreateResult CompEmitterCreate(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data);

    dmGameObject::CreateResult CompEmitterDestroy(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* world,
            void* context,
            uintptr_t* user_data);

    dmGameObject::UpdateResult CompEmitterUpdate(dmGameObject::HCollection collection,
            const dmGameObject::UpdateContext* update_context,
            void* world,
            void* context);

    dmGameObject::UpdateResult CompEmitterOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data);

    void CompEmitterOnReload(dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data);
}

#endif // DM_GAMESYS_COMP_EMITTER_H
