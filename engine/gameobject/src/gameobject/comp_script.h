// Copyright 2020-2025 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_GAMEOBJECT_COMP_SCRIPT_H
#define DM_GAMEOBJECT_COMP_SCRIPT_H

#include "component.h"

namespace dmGameObject
{
    CreateResult CompScriptNewWorld(const ComponentNewWorldParams& params);

    CreateResult CompScriptDeleteWorld(const ComponentDeleteWorldParams& params);

    CreateResult CompScriptCreate(const ComponentCreateParams& params);

    CreateResult CompScriptDestroy(const ComponentDestroyParams& params);

    CreateResult CompScriptInit(const ComponentInitParams& params);

    CreateResult CompScriptFinal(const ComponentFinalParams& params);

    CreateResult CompScriptAddToUpdate(const ComponentAddToUpdateParams& params);

    UpdateResult CompScriptPreUpdate(const ComponentsUpdateParams& params, ComponentsUpdateResult& result);
    UpdateResult CompScriptFixedUpdate(const ComponentsUpdateParams& params, ComponentsUpdateResult& result);
    UpdateResult CompScriptLateUpdate(const ComponentsUpdateParams& params, ComponentsUpdateResult& result);

    UpdateResult CompScriptOnMessage(const ComponentOnMessageParams& params);

    InputResult CompScriptOnInput(const ComponentOnInputParams& params);

    void CompScriptOnReload(const ComponentOnReloadParams& params);

    PropertyResult CompScriptSetProperties(const ComponentSetPropertiesParams& params);

    PropertyResult CompScriptGetProperty(const ComponentGetPropertyParams& params, PropertyDesc& out_value);

    PropertyResult CompScriptSetProperty(const ComponentSetPropertyParams& params);

    void CompScriptIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node);
}

#endif // DM_GAMEOBJECT_COMP_SCRIPT_H
