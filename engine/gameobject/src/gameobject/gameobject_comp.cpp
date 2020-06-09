// Copyright 2020 The Defold Foundation
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

#include "gameobject.h"
#include "component.h"
#include "comp_anim.h"
#include "comp_script.h"

namespace dmGameObject
{
    Result RegisterComponentTypes(dmResource::HFactory factory, HRegister regist, dmScript::HContext script_context)
    {
        ComponentType script_component;
        dmResource::GetTypeFromExtension(factory, "scriptc", &script_component.m_ResourceType);
        script_component.m_Name = "scriptc";
        script_component.m_Context = script_context;
        script_component.m_NewWorldFunction = &CompScriptNewWorld;
        script_component.m_DeleteWorldFunction = &CompScriptDeleteWorld;
        script_component.m_CreateFunction = &CompScriptCreate;
        script_component.m_DestroyFunction = &CompScriptDestroy;
        script_component.m_InitFunction = &CompScriptInit;
        script_component.m_FinalFunction = &CompScriptFinal;
        script_component.m_AddToUpdateFunction = &CompScriptAddToUpdate;
        script_component.m_UpdateFunction = &CompScriptUpdate;
        script_component.m_OnMessageFunction = &CompScriptOnMessage;
        script_component.m_OnInputFunction = &CompScriptOnInput;
        script_component.m_OnReloadFunction = &CompScriptOnReload;
        script_component.m_SetPropertiesFunction = &CompScriptSetProperties;
        script_component.m_GetPropertyFunction = &CompScriptGetProperty;
        script_component.m_SetPropertyFunction = &CompScriptSetProperty;
        script_component.m_InstanceHasUserData = true;
        script_component.m_UpdateOrderPrio = 200;
        script_component.m_ReadsTransforms = 1;
        Result result = RegisterComponentType(regist, script_component);
        if (result != dmGameObject::RESULT_OK)
            return result;
        ComponentType anim_component;
        dmResource::GetTypeFromExtension(factory, "animc", &anim_component.m_ResourceType);
        anim_component.m_Name = "animc";
        anim_component.m_Context = 0x0;
        anim_component.m_NewWorldFunction = &CompAnimNewWorld;
        anim_component.m_DeleteWorldFunction = &CompAnimDeleteWorld;
        anim_component.m_AddToUpdateFunction = &CompAnimAddToUpdate;
        anim_component.m_ReadsTransforms = 1;
        anim_component.m_UpdateFunction = &CompAnimUpdate;
        anim_component.m_UpdateOrderPrio = 250;
        return RegisterComponentType(regist, anim_component);
    }
}
