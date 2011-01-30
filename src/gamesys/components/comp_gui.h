#ifndef DM_GAMESYS_COMP_GUI_H
#define DM_GAMESYS_COMP_GUI_H

#include <stdint.h>
#include <gameobject/gameobject.h>
#include <render/render.h>

namespace dmGameSystem
{
    extern dmRender::HRenderType g_GuiRenderType;

    dmGameObject::CreateResult CompGuiNewWorld(void* context, void** world);

    dmGameObject::CreateResult CompGuiDeleteWorld(void* context, void* world);

    dmGameObject::CreateResult CompGuiCreate(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data);

    dmGameObject::CreateResult CompGuiInit(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* context,
            uintptr_t* user_data);

    dmGameObject::CreateResult CompGuiDestroy(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* world,
            void* context,
            uintptr_t* user_data);

    dmGameObject::UpdateResult CompGuiUpdate(dmGameObject::HCollection collection,
            const dmGameObject::UpdateContext* update_context,
            void* world,
            void* context);

    dmGameObject::UpdateResult CompGuiOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data);

    dmGameObject::InputResult CompGuiOnInput(dmGameObject::HInstance instance,
            const dmGameObject::InputAction* input_action,
            void* context,
            uintptr_t* user_data);

    void CompGuiOnReload(dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data);
}

#endif // DM_GAMESYS_COMP_GUI_H
