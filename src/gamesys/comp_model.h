#ifndef DM_GAMESYS_COMP_MODEL_H
#define DM_GAMESYS_COMP_MODEL_H

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    const char* MODEL_EXT = "modelc";

    dmGameObject::CreateResult CompModelNewWorld(void* context, void** world);

    dmGameObject::CreateResult CompModelDeleteWorld(void* context, void* world);

    dmGameObject::CreateResult CompModelCreate(dmGameObject::HCollection collection,
                                            dmGameObject::HInstance instance,
                                            void* resource,
                                            void* world,
                                            void* context,
                                            uintptr_t* user_data);

    dmGameObject::CreateResult CompModelDestroy(dmGameObject::HCollection collection,
                                             dmGameObject::HInstance instance,
                                             void* world,
                                             void* context,
                                             uintptr_t* user_data);

    dmGameObject::UpdateResult CompModelUpdate(dmGameObject::HCollection collection,
                         const dmGameObject::UpdateContext* update_context,
                         void* world,
                         void* context);

    dmGameObject::UpdateResult CompModelOnEvent(dmGameObject::HInstance instance,
            const dmGameObject::ScriptEventData* event_data,
            void* context,
            uintptr_t* user_data);
}

#endif
