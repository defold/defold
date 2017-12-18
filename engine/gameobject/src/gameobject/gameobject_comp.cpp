#include "gameobject.h"
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
