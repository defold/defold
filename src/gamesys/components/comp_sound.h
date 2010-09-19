#ifndef DM_GAMESYS_SOUND_H
#define DM_GAMESYS_SOUND_H

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompSoundNewWorld(void* context, void** world);

    dmGameObject::CreateResult CompSoundDeleteWorld(void* context, void* world);

    dmGameObject::CreateResult CompSoundCreate(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data);

    dmGameObject::CreateResult CompSoundDestroy(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* world,
                                                void* context,
                                                uintptr_t* user_data);

    dmGameObject::UpdateResult CompSoundUpdate(dmGameObject::HCollection collection,
                                               const dmGameObject::UpdateContext* update_context,
                                               void* world,
                                               void* context);

    dmGameObject::UpdateResult CompSoundOnEvent(dmGameObject::HInstance instance,
                                                const dmGameObject::ScriptEventData* event_data,
                                                void* context,
                                                uintptr_t* user_data);
}

#endif
