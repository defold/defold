
#include "comp_script.h"

#include <dlib/profile.h>

#include <script/script.h>

#include "gameobject_script.h"
#include "gameobject_private.h"
#include "gameobject_props_lua.h"

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
        SetPropertySet(script_instance->m_Properties, PROPERTY_LAYER_PROTOTYPE, params.m_PropertySet);
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

    ScriptResult RunScript(lua_State* L, HScript script, ScriptFunction script_function, HScriptInstance script_instance, const RunScriptParams& params)
    {
        DM_PROFILE(Script, "RunScript");

        ScriptResult result = SCRIPT_RESULT_OK;

        if (script->m_FunctionReferences[script_function] != LUA_NOREF)
        {
            int top = lua_gettop(L);
            (void) top;

            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
            dmScript::SetInstance(L);

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

            int ret = dmScript::PCall(L, arg_count, 0);
            if (ret != 0)
            {
                result = SCRIPT_RESULT_FAILED;
            }

            lua_pushnil(L);
            dmScript::SetInstance(L);

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

    static lua_State* GetLuaState(void* context) {
        return dmScript::GetLuaState((dmScript::HContext)context);
    }

    static lua_State* GetLuaState(HScriptInstance instance) {
        return instance->m_Script->m_LuaState;
    }

    CreateResult CompScriptInit(const ComponentInitParams& params)
    {
        HScriptInstance script_instance = (HScriptInstance)*params.m_UserData;

        RunScriptParams run_params;
        ScriptResult ret = RunScript(GetLuaState(params.m_Context), script_instance->m_Script, SCRIPT_FUNCTION_INIT, script_instance, run_params);
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

        lua_State* L = GetLuaState(params.m_Context);
        int top = lua_gettop(L);
        (void)top;
        ScriptResult ret = RunScript(L, script_instance->m_Script, SCRIPT_FUNCTION_FINAL, script_instance, RunScriptParams());
        assert(top == lua_gettop(L));
        if (ret == SCRIPT_RESULT_FAILED)
        {
            return CREATE_RESULT_UNKNOWN_ERROR;
        }
        else
        {
            return CREATE_RESULT_OK;
        }
    }

    CreateResult CompScriptAddToUpdate(const ComponentAddToUpdateParams& params)
    {
        HScriptInstance script_instance = (HScriptInstance)*params.m_UserData;
        script_instance->m_Update = 1;
        return CREATE_RESULT_OK;
    }

    UpdateResult CompScriptUpdate(const ComponentsUpdateParams& params)
    {
        lua_State* L = GetLuaState(params.m_Context);
        int top = lua_gettop(L);
        (void)top;
        UpdateResult result = UPDATE_RESULT_OK;
        RunScriptParams run_params;
        run_params.m_UpdateContext = params.m_UpdateContext;
        ScriptWorld* script_world = (ScriptWorld*)params.m_World;
        uint32_t size = script_world->m_Instances.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            HScriptInstance script_instance = script_world->m_Instances[i];
            if (script_instance->m_Update) {
                ScriptResult ret = RunScript(L, script_instance->m_Script, SCRIPT_FUNCTION_UPDATE, script_instance, run_params);
                if (ret == SCRIPT_RESULT_FAILED)
                {
                    result = UPDATE_RESULT_UNKNOWN_ERROR;
                }
            }
        }

        assert(top == lua_gettop(L));
        return result;
    }

    UpdateResult CompScriptOnMessage(const ComponentOnMessageParams& params)
    {
        UpdateResult result = UPDATE_RESULT_OK;

        ScriptInstance* script_instance = (ScriptInstance*)*params.m_UserData;

        int function_ref;
        bool is_callback = false;
        if (params.m_Message->m_Receiver.m_Function) {
            // NOTE: By convention m_Function is the ref + 2, see message.h in dlib
            function_ref = params.m_Message->m_Receiver.m_Function - 2;
            is_callback = true;
        } else {
            function_ref = script_instance->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_ONMESSAGE];
        }

        if (function_ref != LUA_NOREF)
        {
            lua_State* L = GetLuaState(params.m_Context);
            int top = lua_gettop(L);
            (void) top;
            int ret;

            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
            dmScript::SetInstance(L);

            lua_rawgeti(L, LUA_REGISTRYINDEX, function_ref);
            if (is_callback) {
                dmScript::Unref(L, LUA_REGISTRYINDEX, function_ref);
            }
            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

            dmScript::PushHash(L, params.m_Message->m_Id);

            if (params.m_Message->m_Descriptor != 0)
            {
                // TODO: setjmp/longjmp here... how to handle?!!! We are not running "from lua" here
                // lua_cpcall?
                dmScript::PushDDF(L, (const dmDDF::Descriptor*)params.m_Message->m_Descriptor, (const char*) params.m_Message->m_Data, true);
            }
            else
            {
                if (params.m_Message->m_DataSize > 0)
                    dmScript::PushTable(L, (const char*)params.m_Message->m_Data, params.m_Message->m_DataSize);
                else
                    lua_newtable(L);
            }

            dmScript::PushURL(L, params.m_Message->m_Sender);

            // An on_message function shouldn't return anything.
            ret = dmScript::PCall(L, 4, 0);
            if (ret != 0)
            {
                result = UPDATE_RESULT_UNKNOWN_ERROR;
            }

            lua_pushnil(L);
            dmScript::SetInstance(L);

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
            lua_State* L = GetLuaState(params.m_Context);
            int top = lua_gettop(L);
            (void)top;

            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
            dmScript::SetInstance(L);

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

            lua_createtable(L, 0, 16);

            int action_table = lua_gettop(L);

            if (params.m_InputAction->m_IsGamepad) {
                lua_pushliteral(L, "gamepad");
                lua_pushnumber(L, params.m_InputAction->m_GamepadIndex);
                lua_settable(L, action_table);
            }

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

                lua_pushliteral(L, "screen_x");
                lua_pushnumber(L, params.m_InputAction->m_ScreenX);
                lua_settable(L, action_table);

                lua_pushliteral(L, "screen_y");
                lua_pushnumber(L, params.m_InputAction->m_ScreenY);
                lua_settable(L, action_table);

                lua_pushliteral(L, "screen_dx");
                lua_pushnumber(L, params.m_InputAction->m_ScreenDX);
                lua_settable(L, action_table);

                lua_pushliteral(L, "screen_dy");
                lua_pushnumber(L, params.m_InputAction->m_ScreenDY);
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

            if (params.m_InputAction->m_TouchCount > 0)
            {
                int tc = params.m_InputAction->m_TouchCount;
                lua_pushliteral(L, "touch");
                lua_createtable(L, tc, 0);
                for (int i = 0; i < tc; ++i)
                {
                    const dmHID::Touch& t = params.m_InputAction->m_Touch[i];

                    lua_pushinteger(L, (lua_Integer) (i+1));
                    lua_createtable(L, 0, 6);

                    lua_pushliteral(L, "id");
                    lua_pushinteger(L, (lua_Integer) t.m_Id);
                    lua_settable(L, -3);

                    lua_pushliteral(L, "tap_count");
                    lua_pushinteger(L, (lua_Integer) t.m_TapCount);
                    lua_settable(L, -3);

                    lua_pushliteral(L, "pressed");
                    lua_pushboolean(L, t.m_Phase == dmHID::PHASE_BEGAN);
                    lua_settable(L, -3);

                    lua_pushliteral(L, "released");
                    lua_pushboolean(L, t.m_Phase == dmHID::PHASE_ENDED || t.m_Phase == dmHID::PHASE_CANCELLED);
                    lua_settable(L, -3);

                    lua_pushliteral(L, "x");
                    lua_pushinteger(L, (lua_Integer) t.m_X);
                    lua_settable(L, -3);

                    lua_pushliteral(L, "y");
                    lua_pushinteger(L, (lua_Integer) t.m_Y);
                    lua_settable(L, -3);

                    lua_pushliteral(L, "screen_x");
                    lua_pushnumber(L, (lua_Integer) t.m_ScreenX);
                    lua_settable(L, -3);

                    lua_pushliteral(L, "screen_y");
                    lua_pushnumber(L, (lua_Integer) t.m_ScreenY);
                    lua_settable(L, -3);

                    lua_pushliteral(L, "dx");
                    lua_pushinteger(L, (lua_Integer) t.m_DX);
                    lua_settable(L, -3);

                    lua_pushliteral(L, "dy");
                    lua_pushinteger(L, (lua_Integer) t.m_DY);
                    lua_settable(L, -3);

                    lua_pushstring(L, "screen_dx");
                    lua_pushnumber(L, (lua_Integer) t.m_ScreenDX);
                    lua_rawset(L, -3);

                    lua_pushstring(L, "screen_dy");
                    lua_pushnumber(L, (lua_Integer) t.m_ScreenDY);
                    lua_rawset(L, -3);

                    lua_settable(L, -3);
                }
                lua_settable(L, -3);
            }

            if (params.m_InputAction->m_TextCount > 0 || params.m_InputAction->m_HasText)
            {
                uint32_t tc = params.m_InputAction->m_TextCount;
                lua_pushliteral(L, "text");
                if (tc == 0) {
                    lua_pushstring(L, "");
                } else {
                    lua_pushlstring(L, params.m_InputAction->m_Text, tc);
                }
                lua_settable(L, -3);
            }

            int arg_count = 3;
            int input_ret = lua_gettop(L) - arg_count;
            int ret = dmScript::PCall(L, arg_count, LUA_MULTRET);
            const char* function_name = SCRIPT_FUNCTION_NAMES[SCRIPT_FUNCTION_ONINPUT];
            if (ret != 0)
            {
                result = INPUT_RESULT_UNKNOWN_ERROR;
            }
            else {
                int nretval = lua_gettop(L) - input_ret + 1;
                if (nretval > 0) {
                    if (nretval == 1 && lua_isboolean(L, -1))
                    {
                        if (lua_toboolean(L, -1))
                            result = INPUT_RESULT_CONSUMED;
                    }
                    else
                    {
                        dmLogError("Script %s must return a boolean value (true/false), or no value at all.", function_name);
                        result = INPUT_RESULT_UNKNOWN_ERROR;
                    }

                    lua_pop(L, nretval);
                }
            }

            lua_pushnil(L);
            dmScript::SetInstance(L);

            assert(top == lua_gettop(L));
        }
        return result;
    }

    void CompScriptOnReload(const ComponentOnReloadParams& params)
    {
        HScriptInstance script_instance = (HScriptInstance)*params.m_UserData;

        lua_State* L = GetLuaState(params.m_Context);

        int top = lua_gettop(L);
        (void)top;

        // Clean stale properties and add new

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
        dmScript::SetInstance(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_ScriptDataReference);
        PropertiesToLuaTable(script_instance->m_Instance, script_instance->m_Script, script_instance->m_Properties, L, -1);
        lua_pop(L, 1);

        lua_pushnil(L);
        dmScript::SetInstance(L);

        RunScriptParams run_params;
        RunScript(L, script_instance->m_Script, SCRIPT_FUNCTION_ONRELOAD, script_instance, run_params);
        assert(top == lua_gettop(L));
    }

    PropertyResult CompScriptSetProperties(const ComponentSetPropertiesParams& params)
    {
        HScriptInstance script_instance = (HScriptInstance)*params.m_UserData;
        SetPropertySet(script_instance->m_Properties, PROPERTY_LAYER_INSTANCE, params.m_PropertySet);

        lua_State* L = GetLuaState(script_instance);

        int top = lua_gettop(L);
        (void)top;

        dmScript::GetInstance(L);
        void* user_data = lua_touserdata(L, -1);
        lua_pop(L, 1);

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
        dmScript::SetInstance(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_ScriptDataReference);
        PropertyResult result = PropertiesToLuaTable(script_instance->m_Instance, script_instance->m_Script, script_instance->m_Properties, L, -1);
        lua_pop(L, 1);

        if (user_data != 0x0) {
            lua_pushlightuserdata(L, user_data);
        } else {
            lua_pushnil(L);
        }
        dmScript::SetInstance(L);

        assert(top == lua_gettop(L));
        return result;
    }

    static bool FindPropertyNameFromEntries(dmPropertiesDDF::PropertyDeclarationEntry* entries, uint32_t entry_count, dmhash_t id, const char** out_key, dmhash_t** out_element_ids)
    {
        for (uint32_t i = 0; i < entry_count; ++i)
        {
            dmPropertiesDDF::PropertyDeclarationEntry& entry = entries[i];
            if (entry.m_Id == id)
            {
                *out_key = entry.m_Key;
                *out_element_ids = entry.m_ElementIds.m_Data;
                return true;
            }
        }
        return false;
    }

    static bool FindPropertyNameFromElements(dmPropertiesDDF::PropertyDeclarationEntry* entries, uint32_t entry_count, dmhash_t id, const char** out_key, uint32_t* out_index)
    {
        for (uint32_t i = 0; i < entry_count; ++i)
        {
            dmPropertiesDDF::PropertyDeclarationEntry& entry = entries[i];
            uint32_t element_count = entry.m_ElementIds.m_Count;
            for (uint32_t i = 0; i < element_count; ++i)
            {
                if (entry.m_ElementIds[i] == id)
                {
                    *out_key = entry.m_Key;
                    *out_index = i;
                    return true;
                }
            }
        }
        return false;
    }

    static bool FindPropertyName(dmPropertiesDDF::PropertyDeclarations* decls, dmhash_t property_id, const char** out_key, PropertyType* out_type, dmhash_t** out_element_ids, bool* out_is_element, uint32_t* out_element_index)
    {
        *out_is_element = false;
        if (FindPropertyNameFromEntries(decls->m_BoolEntries.m_Data, decls->m_BoolEntries.m_Count,
                property_id, out_key, out_element_ids))
        {
            *out_type = PROPERTY_TYPE_BOOLEAN;
            return true;
        }
        if (FindPropertyNameFromEntries(decls->m_NumberEntries.m_Data, decls->m_NumberEntries.m_Count,
                property_id, out_key, out_element_ids))
        {
            *out_type = PROPERTY_TYPE_NUMBER;
            return true;
        }
        if (FindPropertyNameFromEntries(decls->m_HashEntries.m_Data, decls->m_HashEntries.m_Count,
                property_id, out_key, out_element_ids))
        {
            *out_type = PROPERTY_TYPE_HASH;
            return true;
        }
        if (FindPropertyNameFromEntries(decls->m_UrlEntries.m_Data, decls->m_UrlEntries.m_Count,
                property_id, out_key, out_element_ids))
        {
            *out_type = PROPERTY_TYPE_URL;
            return true;
        }
        if (FindPropertyNameFromEntries(decls->m_Vector3Entries.m_Data, decls->m_Vector3Entries.m_Count,
                property_id, out_key, out_element_ids))
        {
            *out_type = PROPERTY_TYPE_VECTOR3;
            return true;
        }
        if (FindPropertyNameFromElements(decls->m_Vector3Entries.m_Data, decls->m_Vector3Entries.m_Count,
                property_id, out_key, out_element_index))
        {
            *out_type = PROPERTY_TYPE_NUMBER;
            *out_is_element = true;
            return true;
        }
        if (FindPropertyNameFromEntries(decls->m_Vector4Entries.m_Data, decls->m_Vector4Entries.m_Count,
                property_id, out_key, out_element_ids))
        {
            *out_type = PROPERTY_TYPE_VECTOR4;
            return true;
        }
        if (FindPropertyNameFromElements(decls->m_Vector4Entries.m_Data, decls->m_Vector4Entries.m_Count,
                property_id, out_key, out_element_index))
        {
            *out_type = PROPERTY_TYPE_NUMBER;
            *out_is_element = true;
            return true;
        }
        if (FindPropertyNameFromEntries(decls->m_QuatEntries.m_Data, decls->m_QuatEntries.m_Count,
                property_id, out_key, out_element_ids))
        {
            *out_type = PROPERTY_TYPE_QUAT;
            return true;
        }
        if (FindPropertyNameFromElements(decls->m_QuatEntries.m_Data, decls->m_QuatEntries.m_Count,
                property_id, out_key, out_element_index))
        {
            *out_type = PROPERTY_TYPE_NUMBER;
            *out_is_element = true;
            return true;
        }
        return false;
    }

    PropertyResult CompScriptGetProperty(const ComponentGetPropertyParams& params, PropertyDesc& out_value)
    {
        HScriptInstance script_instance = (HScriptInstance)*params.m_UserData;

        dmPropertiesDDF::PropertyDeclarations* declarations = &script_instance->m_Script->m_LuaModule->m_Properties;
        PropertyType type = PROPERTY_TYPE_NUMBER;
        dmhash_t* element_ids = 0x0;
        const char* property_name = 0x0;
        bool is_element = false;
        uint32_t element_index = 0;
        if (!FindPropertyName(declarations, params.m_PropertyId, &property_name, &type, &element_ids, &is_element, &element_index))
            return PROPERTY_RESULT_NOT_FOUND;

        if (type == PROPERTY_TYPE_VECTOR3)
        {
            out_value.m_ElementIds[0] = element_ids[0];
            out_value.m_ElementIds[1] = element_ids[1];
            out_value.m_ElementIds[2] = element_ids[2];
        }
        else if (type == PROPERTY_TYPE_VECTOR4 || type == PROPERTY_TYPE_QUAT)
        {
            out_value.m_ElementIds[0] = element_ids[0];
            out_value.m_ElementIds[1] = element_ids[1];
            out_value.m_ElementIds[2] = element_ids[2];
            out_value.m_ElementIds[3] = element_ids[3];
        }

        lua_State* L = GetLuaState(script_instance);

        int top = lua_gettop(L);
        (void)top;

        // Only push the script instance if it's not present already
        dmScript::GetInstance(L);
        bool push_instance = lua_isnil(L, -1);
        lua_pop(L, 1);
        if (push_instance)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
            dmScript::SetInstance(L);
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_ScriptDataReference);

        PropertyResult result = PROPERTY_RESULT_NOT_FOUND;
        lua_pushstring(L, property_name);
        lua_rawget(L, -2);
        if (!lua_isnil(L, -1))
        {
            result = LuaToVar(L, -1, out_value.m_Variant);
            if (result == PROPERTY_RESULT_OK)
            {
                if (is_element)
                {
                    out_value.m_Variant = PropertyVar(out_value.m_Variant.m_V4[element_index]);
                }
            }
        }

        lua_pop(L, 2);

        if (push_instance)
        {
            lua_pushnil(L);
            dmScript::SetInstance(L);
        }

        assert(lua_gettop(L) == top);

        return result;
    }

    PropertyResult CompScriptSetProperty(const ComponentSetPropertyParams& params)
    {
        HScriptInstance script_instance = (HScriptInstance)*params.m_UserData;

        dmPropertiesDDF::PropertyDeclarations* declarations = &script_instance->m_Script->m_LuaModule->m_Properties;
        PropertyType type = PROPERTY_TYPE_NUMBER;
        const char* property_name = 0x0;
        dmhash_t* element_ids = 0x0;
        bool is_element = false;
        uint32_t element_index = 0;
        if (!FindPropertyName(declarations, params.m_PropertyId, &property_name, &type, &element_ids, &is_element, &element_index))
            return PROPERTY_RESULT_NOT_FOUND;

        lua_State* L = GetLuaState(script_instance);

        int top = lua_gettop(L);
        (void)top;

        // Only push the script instance if it's not present already
        bool pushed_instance = false;
        dmScript::GetInstance(L);
        pushed_instance = lua_isnil(L, -1);
        lua_pop(L, 1);
        if (pushed_instance)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
            dmScript::SetInstance(L);
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_ScriptDataReference);

        PropertyVar var = params.m_Value;
        if (is_element)
        {
            PropertyResult result = PROPERTY_RESULT_NOT_FOUND;
            lua_pushstring(L, property_name);
            lua_rawget(L, -2);
            if (!lua_isnil(L, -1))
            {
                result = LuaToVar(L, -1, var);
                if (result == PROPERTY_RESULT_OK)
                {
                    var.m_V4[element_index] = (float) params.m_Value.m_Number;
                }
            }

            lua_pop(L, 1);
        }
        lua_pushstring(L, property_name);
        LuaPushVar(L, var);
        lua_rawset(L, -3);

        lua_pop(L, 1);

        if (pushed_instance)
        {
            lua_pushnil(L);
            dmScript::SetInstance(L);
        }

        assert(lua_gettop(L) == top);

        return PROPERTY_RESULT_OK;
    }

}
