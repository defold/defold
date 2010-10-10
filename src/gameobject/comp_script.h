#ifndef DM_GAMEOBJECT_COMP_SCRIPT_H
#define DM_GAMEOBJECT_COMP_SCRIPT_H

#include <stdint.h>

#include "gameobject.h"

namespace dmGameObject
{
    CreateResult CompScriptNewWorld(void* context, void** world);

    CreateResult CompScriptDeleteWorld(void* context, void* world);

    CreateResult CompScriptCreate(HCollection collection,
            HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data);

    CreateResult CompScriptInit(HCollection collection,
            HInstance instance,
            void* context,
            uintptr_t* user_data);

    CreateResult CompScriptDestroy(HCollection collection,
            HInstance instance,
            void* world,
            void* context,
            uintptr_t* user_data);

    UpdateResult CompScriptUpdate(HCollection collection,
            const UpdateContext* update_context,
            void* world,
            void* context);

    UpdateResult CompScriptOnMessage(HInstance instance,
            const InstanceMessageData* instance_message_data,
            void* context,
            uintptr_t* user_data);

    InputResult CompScriptOnInput(HInstance instance,
            const InputAction* input_action,
            void* context,
            uintptr_t* user_data);
}

#endif // DM_GAMEOBJECT_COMP_SCRIPT_H
