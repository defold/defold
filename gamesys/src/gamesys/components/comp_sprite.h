#ifndef DM_GAMESYS_COMP_SPRITE_H
#define DM_GAMESYS_COMP_SPRITE_H

#include <stdint.h>
#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompSpriteNewWorld(void* context, void** world);

    dmGameObject::CreateResult CompSpriteDeleteWorld(void* context, void* world);

    dmGameObject::CreateResult CompSpriteCreate(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data);

    dmGameObject::CreateResult CompSpriteDestroy(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* world,
            void* context,
            uintptr_t* user_data);

    dmGameObject::UpdateResult CompSpriteUpdate(dmGameObject::HCollection collection,
            const dmGameObject::UpdateContext* update_context,
            void* world,
            void* context);

    dmGameObject::UpdateResult CompSpriteOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data);

    void CompSpriteOnReload(dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data);
}

#endif // DM_GAMESYS_COMP_SPRITE_H
