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
    CreateResult CompScriptNewWorld(void* context, void** world)
    {
        if (world != 0x0)
        {
            *world = new ScriptWorld();
            return CREATE_RESULT_OK;
        }
        else
        {
            return CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    CreateResult CompScriptDeleteWorld(void* context, void* world)
    {
        if (world != 0x0)
        {
            delete (ScriptWorld*)world;
            return CREATE_RESULT_OK;
        }
        else
        {
            return CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    CreateResult CompScriptCreate(HCollection collection,
            HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data)
    {
        HScript script = (HScript)resource;

        ScriptWorld* script_world = (ScriptWorld*)world;
        if (script_world->m_Instances.Full())
        {
            dmLogError("Could not create script component, out of resources.");
            return CREATE_RESULT_UNKNOWN_ERROR;
        }

        HScriptInstance script_instance = NewScriptInstance(script, instance);
        if (script_instance == 0x0)
        {
            dmLogError("Could not create script component, out of memory.");
            return CREATE_RESULT_UNKNOWN_ERROR;
        }

        instance->m_ScriptInstancePOOOOP = script_instance;
        script_world->m_Instances.Push(script_instance);
        *user_data = (uintptr_t)script_instance;
        return CREATE_RESULT_OK;
    }

    ScriptResult RunScript(HCollection collection, HScript script, ScriptFunction script_function, HScriptInstance script_instance, const UpdateContext* update_context)
    {
        DM_PROFILE(Script, "RunScript");

        ScriptResult result = SCRIPT_RESULT_OK;

        if (script->m_FunctionReferences[script_function] != LUA_NOREF)
        {
            lua_State* L = g_LuaState;
            int top = lua_gettop(L);
            (void) top;

            lua_pushliteral(L, "__collection__");
            lua_pushlightuserdata(L, (void*) collection);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_pushliteral(L, "__update_context__");
            lua_pushlightuserdata(L, (void*) update_context);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_pushliteral(L, "__instance__");
            lua_pushlightuserdata(L, (void*) script_instance->m_Instance);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_rawgeti(L, LUA_REGISTRYINDEX, script->m_FunctionReferences[script_function]);
            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

            int arg_count = 1;

            if (update_context != 0x0)
            {
                lua_pushnumber(L, update_context->m_DT);
                ++arg_count;
            }

            int ret = lua_pcall(L, arg_count, LUA_MULTRET, 0);
            if (ret != 0)
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
                result = SCRIPT_RESULT_FAILED;
            }

            lua_pushliteral(L, "__collection__");
            lua_pushnil(L);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_pushliteral(L, "__update_context__");
            lua_pushnil(L);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_pushliteral(L, "__instance__");
            lua_pushnil(L);
            lua_rawset(L, LUA_GLOBALSINDEX);

            assert(top == lua_gettop(L));
        }

        return result;
    }

    CreateResult CompScriptInit(HCollection collection,
            HInstance instance,
            void* context,
            uintptr_t* user_data)
    {
        HScriptInstance script_instance = (HScriptInstance)*user_data;

        int top = lua_gettop(g_LuaState);
        (void)top;
        ScriptResult ret = RunScript(collection, script_instance->m_Script, SCRIPT_FUNCTION_INIT, script_instance, 0x0);
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

    CreateResult CompScriptDestroy(HCollection collection,
            HInstance instance,
            void* world,
            void* context,
            uintptr_t* user_data)
    {
        ScriptWorld* script_world = (ScriptWorld*)world;
        HScriptInstance script_instance = (HScriptInstance)*user_data;
        for (uint32_t i = 0; i < script_world->m_Instances.Size(); ++i)
        {
            if (script_instance == script_world->m_Instances[i])
            {
                script_world->m_Instances.EraseSwap(i);
                break;
            }
        }
        instance->m_ScriptInstancePOOOOP = 0x0;
        DeleteScriptInstance(script_instance);
        return CREATE_RESULT_OK;
    }

    UpdateResult CompScriptUpdate(HCollection collection,
            const UpdateContext* update_context,
            void* world,
            void* context)
    {
        int top = lua_gettop(g_LuaState);
        (void)top;
        UpdateResult result = UPDATE_RESULT_OK;
        ScriptWorld* script_world = (ScriptWorld*)world;
        uint32_t size = script_world->m_Instances.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            HScriptInstance script_instance = script_world->m_Instances[i];
            ScriptResult ret = RunScript(collection, script_instance->m_Script, SCRIPT_FUNCTION_UPDATE, script_instance, update_context);
            if (ret == SCRIPT_RESULT_FAILED)
            {
                result = UPDATE_RESULT_UNKNOWN_ERROR;
            }
        }
        assert(top == lua_gettop(g_LuaState));
        return result;
    }

    UpdateResult CompScriptOnMessage(HInstance instance,
            const InstanceMessageData* instance_message_data,
            void* context,
            uintptr_t* user_data)
    {
        UpdateResult result = UPDATE_RESULT_OK;

        ScriptInstance* script_instance = (ScriptInstance*)*user_data;
        assert(instance_message_data->m_Instance);

        int function_ref = script_instance->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_ONMESSAGE];
        if (function_ref != LUA_NOREF)
        {
            lua_State* L = g_LuaState;
            int top = lua_gettop(L);
            (void) top;
            int ret;

            lua_pushliteral(L, "__collection__");
            lua_pushlightuserdata(L, (void*) instance->m_Collection);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_pushliteral(L, "__instance__");
            lua_pushlightuserdata(L, (void*) script_instance->m_Instance);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_rawgeti(L, LUA_REGISTRYINDEX, function_ref);
            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

            dmScript::PushHash(L, instance_message_data->m_MessageId);

            if (instance_message_data->m_DDFDescriptor)
            {
                // adjust char ptrs to global mem space
                char* data = (char*)instance_message_data->m_Buffer;
                for (uint8_t i = 0; i < instance_message_data->m_DDFDescriptor->m_FieldCount; ++i)
                {
                    dmDDF::FieldDescriptor* field = &instance_message_data->m_DDFDescriptor->m_Fields[i];
                    uint32_t field_type = field->m_Type;
                    if (field_type == dmDDF::TYPE_STRING)
                    {
                        *((uintptr_t*)&data[field->m_Offset]) = (uintptr_t)data + *((uintptr_t*)(data + field->m_Offset));
                    }
                }
                // TODO: setjmp/longjmp here... how to handle?!!! We are not running "from lua" here
                // lua_cpcall?
                dmScript::PushDDF(L, instance_message_data->m_DDFDescriptor, (const char*) instance_message_data->m_Buffer);
            }
            else
            {
                if (instance_message_data->m_BufferSize > 0)
                    dmScript::PushTable(L, (const char*)instance_message_data->m_Buffer);
                else
                    lua_newtable(L);
            }

            ret = lua_pcall(L, 3, LUA_MULTRET, 0);
            if (ret != 0)
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
                result = UPDATE_RESULT_UNKNOWN_ERROR;
            }

            lua_pushliteral(L, "__collection__");
            lua_pushnil(L);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_pushliteral(L, "__instance__");
            lua_pushnil(L);
            lua_rawset(L, LUA_GLOBALSINDEX);

            assert(top == lua_gettop(L));
        }
        return result;
    }

    InputResult CompScriptOnInput(HInstance instance,
            const InputAction* input_action,
            void* context,
            uintptr_t* user_data)
    {
        InputResult result = INPUT_RESULT_IGNORED;

        ScriptInstance* script_instance = (ScriptInstance*)*user_data;

        int function_ref = script_instance->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_ONINPUT];
        if (function_ref != LUA_NOREF)
        {
            lua_State* L = g_LuaState;
            int top = lua_gettop(L);
            (void)top;

            lua_pushliteral(L, "__collection__");
            lua_pushlightuserdata(L, (void*) instance->m_Collection);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_pushliteral(L, "__instance__");
            lua_pushlightuserdata(L, (void*) script_instance->m_Instance);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_rawgeti(L, LUA_REGISTRYINDEX, function_ref);
            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

            dmScript::PushHash(L, input_action->m_ActionId);

            lua_createtable(L, 0, 5);

            int action_table = lua_gettop(L);

            lua_pushliteral(L, "value");
            lua_pushnumber(L, input_action->m_Value);
            lua_settable(L, action_table);

            lua_pushliteral(L, "pressed");
            lua_pushboolean(L, input_action->m_Pressed);
            lua_settable(L, action_table);

            lua_pushliteral(L, "released");
            lua_pushboolean(L, input_action->m_Released);
            lua_settable(L, action_table);

            lua_pushliteral(L, "repeated");
            lua_pushboolean(L, input_action->m_Repeated);
            lua_settable(L, action_table);

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

            lua_pushliteral(L, "__collection__");
            lua_pushnil(L);
            lua_rawset(L, LUA_GLOBALSINDEX);

            lua_pushliteral(L, "__instance__");
            lua_pushnil(L);
            lua_rawset(L, LUA_GLOBALSINDEX);

            assert(top == lua_gettop(L));
        }
        return result;
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
        script_component.m_InitFunction = &CompScriptInit;
        script_component.m_DestroyFunction = &CompScriptDestroy;
        script_component.m_UpdateFunction = &CompScriptUpdate;
        script_component.m_OnMessageFunction = &CompScriptOnMessage;
        script_component.m_OnInputFunction = &CompScriptOnInput;
        script_component.m_InstanceHasUserData = true;
        return RegisterComponentType(regist, script_component);
    }

}
