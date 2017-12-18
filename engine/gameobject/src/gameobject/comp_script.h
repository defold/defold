#ifndef DM_GAMEOBJECT_COMP_SCRIPT_H
#define DM_GAMEOBJECT_COMP_SCRIPT_H

#include <stdint.h>

#include "gameobject.h"

namespace dmGameObject
{
    CreateResult CompScriptNewWorld(const ComponentNewWorldParams& params);

    CreateResult CompScriptDeleteWorld(const ComponentDeleteWorldParams& params);

    CreateResult CompScriptCreate(const ComponentCreateParams& params);

    CreateResult CompScriptDestroy(const ComponentDestroyParams& params);

    CreateResult CompScriptInit(const ComponentInitParams& params);

    CreateResult CompScriptFinal(const ComponentFinalParams& params);

    CreateResult CompScriptAddToUpdate(const ComponentAddToUpdateParams& params);

    UpdateResult CompScriptUpdate(const ComponentsUpdateParams& params, ComponentsUpdateResult& result);

    UpdateResult CompScriptOnMessage(const ComponentOnMessageParams& params);

    InputResult CompScriptOnInput(const ComponentOnInputParams& params);

    void CompScriptOnReload(const ComponentOnReloadParams& params);

    PropertyResult CompScriptSetProperties(const ComponentSetPropertiesParams& params);

    PropertyResult CompScriptGetProperty(const ComponentGetPropertyParams& params, PropertyDesc& out_value);

    PropertyResult CompScriptSetProperty(const ComponentSetPropertyParams& params);
}

#endif // DM_GAMEOBJECT_COMP_SCRIPT_H
