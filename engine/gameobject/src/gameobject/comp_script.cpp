
#include "comp_script.h"

#include <dlib/profile.h>

#include <script/script.h>

#include "gameobject_script.h"
#include "gameobject_private.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameObject
{
    CreateResult CompScriptNewWorld(const ComponentNewWorldParams& params)
    {
        if (params.m_World != 0x0)
        {
            *params.m_World = new ScriptWorld();
            return CREATE_RESULT_OK;
        }
        else
        {
            return CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    CreateResult CompScriptDeleteWorld(const ComponentDeleteWorldParams& params)
    {
        if (params.m_World != 0x0)
        {
            delete (ScriptWorld*)params.m_World;
            return CREATE_RESULT_OK;
        }
        else
        {
            return CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    CreateResult CompScriptCreate(const ComponentCreateParams& params)
    {
        HScript script = (HScript)params.m_Resource;

        ScriptWorld* script_world = (ScriptWorld*)params.m_World;
        if (script_world->m_Instances.Full())
        {
            dmLogError("Could not create script component, out of resources.");
            return CREATE_RESULT_UNKNOWN_ERROR;
        }

        HScriptInstance script_instance = NewScriptInstance(script, params.m_Instance, params.m_ComponentIndex);
        if (script_instance == 0x0)
        {
            dmLogError("Could not create script component, out of memory.");
            return CREATE_RESULT_UNKNOWN_ERROR;
        }

        script_world->m_Instances.Push(script_instance);
        *params.m_UserData = (uintptr_t)script_instance;
        return CREATE_RESULT_OK;
    }

    struct RunScriptParams
    {
        RunScriptParams()
        {
            memset(this, 0, sizeof(*this));
        }
        const UpdateContext* m_UpdateContext;
    };

    ScriptResult RunScript(HScript script, ScriptFunction script_function, HScriptInstance script_instance, const RunScriptParams& params)
    {
        DM_PROFILE(Script, "RunScript");

        ScriptResult result = SCRIPT_RESULT_OK;

        if (script->m_FunctionReferences[script_function] != LUA_NOREF)
        {
            lua_State* L = g_LuaState;
            int top = lua_gettop(L);
            (void) top;

            lua_pushliteral(L, SCRIPT_INSTANCE_NAME);
            lua_pushlightuserdata(L, (void*) script_instance);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_rawgeti(L, LUA_REGISTRYINDEX, script->m_FunctionReferences[script_function]);
            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

            int arg_count = 1;

            if (script_function == SCRIPT_FUNCTION_INIT)
            {
                // Backwards compatibility
                lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
                ++arg_count;
            }
            if (script_function == SCRIPT_FUNCTION_UPDATE)
            {
                lua_pushnumber(L, params.m_UpdateContext->m_DT);
                ++arg_count;
            }

            int ret = lua_pcall(L, arg_count, LUA_MULTRET, 0);
            if (ret != 0)
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
                result = SCRIPT_RESULT_FAILED;
            }

            lua_pushliteral(L, SCRIPT_INSTANCE_NAME);
            lua_pushnil(L);
            lua_rawset(L, LUA_GLOBALSINDEX);

            assert(top == lua_gettop(L));
        }

        return result;
    }

    CreateResult CompScriptDestroy(const ComponentDestroyParams& params)
    {
        ScriptWorld* script_world = (ScriptWorld*)params.m_World;
        HScriptInstance script_instance = (HScriptInstance)*params.m_UserData;
        for (uint32_t i = 0; i < script_world->m_Instances.Size(); ++i)
        {
            if (script_instance == script_world->m_Instances[i])
            {
                script_world->m_Instances.EraseSwap(i);
                break;
            }
        }
        DeleteScriptInstance(script_instance);
        return CREATE_RESULT_OK;
    }

    CreateResult CompScriptInit(const ComponentInitParams& params)
    {
        HScriptInstance script_instance = (HScriptInstance)*params.m_UserData;

        int top = lua_gettop(g_LuaState);
        (void)top;

        AppendProperties(script_instance->m_Properties, params.m_Properties);
        AppendProperties(script_instance->m_Properties, script_instance->m_Script->m_Properties);

        lua_State* L = g_LuaState;
        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_ScriptDataReference);
        const dmArray<PropertyDef>& property_defs = script_instance->m_Script->m_PropertyDefs;
        PropertiesToLuaTable(script_instance->m_Instance, property_defs, script_instance->m_Properties, L, -1);
        lua_pop(L, 1);

        RunScriptParams run_params;
        ScriptResult ret = RunScript(script_instance->m_Script, SCRIPT_FUNCTION_INIT, script_instance, run_params);
        assert(top == lua_gettop(g_LuaState));
        if (ret == SCRIPT_RESULT_FAILED)
        {
            return CREATE_RESULT_UNKNOWN_ERROR;
        }
        else
        {
            return CREATE_RESULT_OK;
        }
    }

    CreateResult CompScriptFinal(const ComponentFinalParams& params)
    {
        HScriptInstance script_instance = (HScriptInstance)*params.m_UserData;

        int top = lua_gettop(g_LuaState);
        (void)top;
        ScriptResult ret = RunScript(script_instance->m_Script, SCRIPT_FUNCTION_FINAL, script_instance, RunScriptParams());
        assert(top == lua_gettop(g_LuaState));
        if (ret == SCRIPT_RESULT_FAILED)
        {
            return CREATE_RESULT_UNKNOWN_ERROR;
        }
        else
        {
            return CREATE_RESULT_OK;
        }
    }

    UpdateResult CompScriptUpdate(const ComponentsUpdateParams& params)
    {
        int top = lua_gettop(g_LuaState);
        (void)top;
        UpdateResult result = UPDATE_RESULT_OK;
        RunScriptParams run_params;
        run_params.m_UpdateContext = params.m_UpdateContext;
        ScriptWorld* script_world = (ScriptWorld*)params.m_World;
        uint32_t size = script_world->m_Instances.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            HScriptInstance script_instance = script_world->m_Instances[i];
            ScriptResult ret = RunScript(script_instance->m_Script, SCRIPT_FUNCTION_UPDATE, script_instance, run_params);
            if (ret == SCRIPT_RESULT_FAILED)
            {
                result = UPDATE_RESULT_UNKNOWN_ERROR;
            }
        }
        assert(top == lua_gettop(g_LuaState));
        return result;
    }

    UpdateResult CompScriptOnMessage(const ComponentOnMessageParams& params)
    {
        UpdateResult result = UPDATE_RESULT_OK;

        ScriptInstance* script_instance = (ScriptInstance*)*params.m_UserData;

        int function_ref = script_instance->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_ONMESSAGE];
        if (function_ref != LUA_NOREF)
        {
            lua_State* L = g_LuaState;
            int top = lua_gettop(L);
            (void) top;
            int ret;

            lua_pushliteral(L, SCRIPT_INSTANCE_NAME);
            lua_pushlightuserdata(L, (void*) script_instance);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_rawgeti(L, LUA_REGISTRYINDEX, function_ref);
            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

            dmScript::PushHash(L, params.m_Message->m_Id);

            if (params.m_Message->m_Descriptor != 0)
            {
                // adjust char ptrs to global mem space
                char* data = (char*)params.m_Message->m_Data;
                dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)params.m_Message->m_Descriptor;
                for (uint8_t i = 0; i < descriptor->m_FieldCount; ++i)
                {
                    dmDDF::FieldDescriptor* field = &descriptor->m_Fields[i];
                    uint32_t field_type = field->m_Type;
                    if (field_type == dmDDF::TYPE_STRING)
                    {
                        *((uintptr_t*)&data[field->m_Offset]) = (uintptr_t)data + *((uintptr_t*)(data + field->m_Offset));
                    }
                }
                // TODO: setjmp/longjmp here... how to handle?!!! We are not running "from lua" here
                // lua_cpcall?
                dmScript::PushDDF(L, descriptor, (const char*) params.m_Message->m_Data);
            }
            else
            {
                if (params.m_Message->m_DataSize > 0)
                    dmScript::PushTable(L, (const char*)params.m_Message->m_Data);
                else
                    lua_newtable(L);
            }

            dmScript::PushURL(L, params.m_Message->m_Sender);

            ret = lua_pcall(L, 4, LUA_MULTRET, 0);
            if (ret != 0)
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
                result = UPDATE_RESULT_UNKNOWN_ERROR;
            }

            lua_pushliteral(L, SCRIPT_INSTANCE_NAME);
            lua_pushnil(L);
            lua_rawset(L, LUA_GLOBALSINDEX);

            assert(top == lua_gettop(L));
        }
        return result;
    }

    InputResult CompScriptOnInput(const ComponentOnInputParams& params)
    {
        InputResult result = INPUT_RESULT_IGNORED;

        ScriptInstance* script_instance = (ScriptInstance*)*params.m_UserData;

        int function_ref = script_instance->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_ONINPUT];
        if (function_ref != LUA_NOREF)
        {
            lua_State* L = g_LuaState;
            int top = lua_gettop(L);
            (void)top;

            lua_pushliteral(L, SCRIPT_INSTANCE_NAME);
            lua_pushlightuserdata(L, (void*) script_instance);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_rawgeti(L, LUA_REGISTRYINDEX, function_ref);
            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

            // 0 is reserved for pure mouse movement
            if (params.m_InputAction->m_ActionId != 0)
            {
                dmScript::PushHash(L, params.m_InputAction->m_ActionId);
            }
            else
            {
                lua_pushnil(L);
            }

            lua_createtable(L, 0, 8);

            int action_table = lua_gettop(L);

            if (params.m_InputAction->m_ActionId != 0)
            {
                lua_pushliteral(L, "value");
                lua_pushnumber(L, params.m_InputAction->m_Value);
                lua_settable(L, action_table);

                lua_pushliteral(L, "pressed");
                lua_pushboolean(L, params.m_InputAction->m_Pressed);
                lua_settable(L, action_table);

                lua_pushliteral(L, "released");
                lua_pushboolean(L, params.m_InputAction->m_Released);
                lua_settable(L, action_table);

                lua_pushliteral(L, "repeated");
                lua_pushboolean(L, params.m_InputAction->m_Repeated);
                lua_settable(L, action_table);
            }

            if (params.m_InputAction->m_PositionSet)
            {
                lua_pushliteral(L, "x");
                lua_pushnumber(L, params.m_InputAction->m_X);
                lua_settable(L, action_table);

                lua_pushliteral(L, "y");
                lua_pushnumber(L, params.m_InputAction->m_Y);
                lua_settable(L, action_table);

                lua_pushliteral(L, "dx");
                lua_pushnumber(L, params.m_InputAction->m_DX);
                lua_settable(L, action_table);

                lua_pushliteral(L, "dy");
                lua_pushnumber(L, params.m_InputAction->m_DY);
                lua_settable(L, action_table);
            }

            if (params.m_InputAction->m_AccelerationSet)
            {
                lua_pushliteral(L, "acc_x");
                lua_pushnumber(L, params.m_InputAction->m_AccX);
                lua_settable(L, action_table);

                lua_pushliteral(L, "acc_y");
                lua_pushnumber(L, params.m_InputAction->m_AccY);
                lua_settable(L, action_table);

                lua_pushliteral(L, "acc_z");
                lua_pushnumber(L, params.m_InputAction->m_AccZ);
                lua_settable(L, action_table);
            }

            int arg_count = 3;
            int input_ret = lua_gettop(L) - arg_count;
            int ret = lua_pcall(L, arg_count, LUA_MULTRET, 0);
            const char* function_name = SCRIPT_FUNCTION_NAMES[SCRIPT_FUNCTION_ONINPUT];
            if (ret != 0)
            {
                dmLogError("Error running script %s: %s", function_name, lua_tostring(L, lua_gettop(L)));
                lua_pop(L, 1);
                result = INPUT_RESULT_UNKNOWN_ERROR;
            }
            else if (input_ret == lua_gettop(L))
            {
                if (!lua_isboolean(L, -1))
                {
                    dmLogError("Script %s must return a boolean value (true/false), or no value at all.", function_name);
                    result = INPUT_RESULT_UNKNOWN_ERROR;
                }
                else
                {
                    if (lua_toboolean(L, -1))
                        result = INPUT_RESULT_CONSUMED;
                }
                lua_pop(L, 1);
            }

            lua_pushliteral(L, SCRIPT_INSTANCE_NAME);
            lua_pushnil(L);
            lua_rawset(L, LUA_GLOBALSINDEX);

            assert(top == lua_gettop(L));
        }
        return result;
    }

    void CompScriptOnReload(const ComponentOnReloadParams& params)
    {
        HScriptInstance script_instance = (HScriptInstance)*params.m_UserData;

        int top = lua_gettop(g_LuaState);
        (void)top;

        // Clean stale properties and add new
        lua_State* L = g_LuaState;
        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_ScriptDataReference);
        ClearPropertiesFromLuaTable(script_instance->m_Script->m_OldPropertyDefs, script_instance->m_Properties, L, -1);
        PropertiesToLuaTable(script_instance->m_Instance, script_instance->m_Script->m_PropertyDefs, script_instance->m_Properties, L, -1);
        lua_pop(L, 1);

        RunScriptParams run_params;
        RunScript(script_instance->m_Script, SCRIPT_FUNCTION_ONRELOAD, script_instance, run_params);
        assert(top == lua_gettop(g_LuaState));
    }

    void CompScriptSetProperties(const ComponentSetPropertiesParams& params)
    {
        HScriptInstance script_instance = (HScriptInstance)*params.m_UserData;
        SetProperties(script_instance->m_Properties, params.m_PropertyBuffer, params.m_PropertyBufferSize);
    }

    Result RegisterComponentTypes(dmResource::HFactory factory, HRegister regist)
    {
        ComponentType script_component;
        dmResource::GetTypeFromExtension(factory, "scriptc", &script_component.m_ResourceType);
        script_component.m_Name = "scriptc";
        script_component.m_Context = 0x0;
        script_component.m_NewWorldFunction = &CompScriptNewWorld;
        script_component.m_DeleteWorldFunction = &CompScriptDeleteWorld;
        script_component.m_CreateFunction = &CompScriptCreate;
        script_component.m_DestroyFunction = &CompScriptDestroy;
        script_component.m_InitFunction = &CompScriptInit;
        script_component.m_FinalFunction = &CompScriptFinal;
        script_component.m_UpdateFunction = &CompScriptUpdate;
        script_component.m_OnMessageFunction = &CompScriptOnMessage;
        script_component.m_OnInputFunction = &CompScriptOnInput;
        script_component.m_OnReloadFunction = &CompScriptOnReload;
        script_component.m_SetPropertiesFunction = &CompScriptSetProperties;
        script_component.m_InstanceHasUserData = true;
        script_component.m_UpdateOrderPrio = 200;
        return RegisterComponentType(regist, script_component);
    }

}
