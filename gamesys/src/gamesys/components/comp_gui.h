#ifndef DM_GAMESYS_COMP_GUI_H
#define DM_GAMESYS_COMP_GUI_H

#include <stdint.h>
#include <gameobject/gameobject.h>
#include <render/render.h>

namespace dmGameSystem
{
    extern dmRender::HRenderType g_GuiRenderType;

    dmGameObject::CreateResult CompGuiNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompGuiDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompGuiCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompGuiDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompGuiInit(const dmGameObject::ComponentInitParams& params);

    dmGameObject::CreateResult CompGuiFinal(const dmGameObject::ComponentFinalParams& params);

    dmGameObject::UpdateResult CompGuiUpdate(const dmGameObject::ComponentsUpdateParams& params);

    dmGameObject::UpdateResult CompGuiOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    dmGameObject::InputResult CompGuiOnInput(const dmGameObject::ComponentOnInputParams& params);

    void CompGuiOnReload(const dmGameObject::ComponentOnReloadParams& params);
}

#endif // DM_GAMESYS_COMP_GUI_H
