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


#include "comp_script.h"

#include <dlib/dstrings.h>
#include <dlib/profile.h>

#include <script/script.h>

#include "gameobject_script.h"
#include "gameobject_private.h"
#include "gameobject_props_lua.h"

#include "gameobject/gameobject_ddf.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

DM_PROPERTY_EXTERN(rmtp_GameObject);
DM_PROPERTY_U32(rmtp_ScriptCount, 0, PROFILE_PROPERTY_FRAME_RESET, "# components", &rmtp_GameObject);

namespace dmGameObject
{
    CreateResult CompScriptNewWorld(const ComponentNewWorldParams& params)
    {
        if (params.m_World != 0x0)
        {
            uint32_t component_count = dmMath::Min(params.m_MaxComponentInstances, params.m_MaxInstances);
            CompScriptWorld* w = new CompScriptWorld(component_count);
            w->m_ScriptWorld = dmScript::NewScriptWorld((dmScript::HContext)params.m_Context);
            *params.m_World = w;

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
            CompScriptWorld* w = (CompScriptWorld*)params.m_World;
            dmScript::DeleteScriptWorld(w->m_ScriptWorld);
            delete w;
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
        CompScriptWorld* script_world = (CompScriptWorld*)params.m_World;
        if (script_world->m_Instances.Full())
        {
            dmLogError("Could not create script component, out of resources. Increase the 'collection.max_instances' value in [game.project](defold://open?path=/game.project)");
            return CREATE_RESULT_UNKNOWN_ERROR;
        }

        HScriptInstance script_instance = NewScriptInstance(script_world, script, params.m_Instance, params.m_ComponentIndex);
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
        DM_PROFILE("RunScript");

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
            if (script_function == SCRIPT_FUNCTION_UPDATE || script_function == SCRIPT_FUNCTION_FIXED_UPDATE || script_function == SCRIPT_FUNCTION_LATE_UPDATE)
            {
                lua_pushnumber(L, params.m_UpdateContext->m_DT);
                ++arg_count;
            }

            {
                char buffer[128];
                const char* profiler_string = dmScript::GetProfilerString(L, 0, script->m_LuaModule->m_Source.m_Filename, SCRIPT_FUNCTION_NAMES[script_function], 0, buffer, sizeof(buffer));
                DM_PROFILE_DYN(profiler_string, 0);

                if (dmScript::PCall(L, arg_count, 0) != 0)
                {
                    result = SCRIPT_RESULT_FAILED;
                }
            }

            lua_pushnil(L);
            dmScript::SetInstance(L);

            assert(top == lua_gettop(L));
        }

        return result;
    }

    CreateResult CompScriptDestroy(const ComponentDestroyParams& params)
    {
        CompScriptWorld* script_world = (CompScriptWorld*)params.m_World;
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
        script_instance->m_Initialized = 1;
        return CREATE_RESULT_OK;
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
        if (script_instance->m_Initialized)
        {
            HScript script = script_instance->m_Script;
            script_instance->m_Update = script->m_FunctionReferences[SCRIPT_FUNCTION_UPDATE] != LUA_NOREF 
                || script->m_FunctionReferences[SCRIPT_FUNCTION_FIXED_UPDATE] != LUA_NOREF
                || script->m_FunctionReferences[SCRIPT_FUNCTION_LATE_UPDATE] != LUA_NOREF;
            return CREATE_RESULT_OK;
        }
        return CREATE_RESULT_UNKNOWN_ERROR;
    }


    static UpdateResult CompScriptUpdateInternal(const ComponentsUpdateParams& params, ScriptFunction function, ComponentsUpdateResult& update_result)
    {
        lua_State* L = GetLuaState(params.m_Context);
        int top = lua_gettop(L);
        (void)top;
        UpdateResult result = UPDATE_RESULT_OK;
        RunScriptParams run_params;
        run_params.m_UpdateContext = params.m_UpdateContext;
        CompScriptWorld* script_world = (CompScriptWorld*)params.m_World;
        uint32_t size = script_world->m_Instances.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            HScriptInstance script_instance = script_world->m_Instances[i];
            if (script_instance->m_Update)
            {
                ScriptResult ret = RunScript(L, script_instance->m_Script, function, script_instance, run_params);
                if (ret == SCRIPT_RESULT_FAILED)
                {
                    result = UPDATE_RESULT_UNKNOWN_ERROR;
                }
            }
        }

        // TODO: Find out if the scripts actually sent any transform events
        update_result.m_TransformsUpdated = true;

        assert(top == lua_gettop(L));
        return result;
    }

    UpdateResult CompScriptPreUpdate(const ComponentsUpdateParams& params, ComponentsUpdateResult& update_result)
    {
        CompScriptWorld* script_world = (CompScriptWorld*)params.m_World;
        dmScript::UpdateScriptWorld(script_world->m_ScriptWorld, params.m_UpdateContext->m_DT);
        DM_PROPERTY_ADD_U32(rmtp_ScriptCount, script_world->m_Instances.Size());
        return CompScriptUpdateInternal(params, SCRIPT_FUNCTION_UPDATE, update_result);
    }

    UpdateResult CompScriptPreFixedUpdate(const ComponentsUpdateParams& params, ComponentsUpdateResult& update_result)
    {
        CompScriptWorld* script_world = (CompScriptWorld*)params.m_World;
        dmScript::FixedUpdateScriptWorld(script_world->m_ScriptWorld, params.m_UpdateContext->m_DT);
        return CompScriptUpdateInternal(params, SCRIPT_FUNCTION_FIXED_UPDATE, update_result);
    }

    UpdateResult CompScriptLateUpdate(const ComponentsUpdateParams& params, ComponentsUpdateResult& update_result)
    {
        CompScriptWorld* script_world = (CompScriptWorld*)params.m_World;
        dmScript::FixedUpdateScriptWorld(script_world->m_ScriptWorld, params.m_UpdateContext->m_DT);
        return CompScriptUpdateInternal(params, SCRIPT_FUNCTION_LATE_UPDATE, update_result);
    }

    static UpdateResult HandleUnrefMessage(void* context, ScriptInstance* script_instance, int reference)
    {
        lua_State* L = GetLuaState(context);
        DM_LUA_STACK_CHECK(L, 0);

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
        dmScript::SetInstance(L);

        dmScript::ResolveInInstance(L, reference);
        dmScript::UnrefInInstance(L, reference);

        lua_pop(L, 1);

        lua_pushnil(L);
        dmScript::SetInstance(L);

        return UPDATE_RESULT_OK;
    }

    static UpdateResult HandleMessage(void* context, ScriptInstance* script_instance, dmMessage::Message* message, int function_ref, bool is_callback, bool deref_function_ref)
    {
        UpdateResult result = UPDATE_RESULT_OK;

        lua_State* L = GetLuaState(context);
        int top = lua_gettop(L);
        (void) top;

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
        dmScript::SetInstance(L);

        if (is_callback)
        {
            dmScript::ResolveInInstance(L, function_ref);
            if (!lua_isfunction(L, -1))
            {
                // If the script instance is dead we just ignore the callback
                lua_pop(L, 1);
                lua_pushnil(L);
                dmScript::SetInstance(L);
                dmLogWarning("Failed to call message response callback function, has it been deleted?");
                return result;
            }

            if (deref_function_ref)
            {
                // The reason the caller cannot deref this function, is that they've posted the message onto a queue
                // and don't know when it's going to be actually called
                dmScript::UnrefInInstance(L, function_ref);
            }
        }
        else
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, function_ref);
        }

        assert(lua_isfunction(L, -1));

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

        dmScript::PushHash(L, message->m_Id);

        const char* message_name = 0;
        if (message->m_Descriptor != 0)
        {
            // TODO: setjmp/longjmp here... how to handle?!!! We are not running "from lua" here
            // lua_cpcall?
            message_name = ((const dmDDF::Descriptor*)message->m_Descriptor)->m_Name;
            dmScript::PushDDF(L, (const dmDDF::Descriptor*)message->m_Descriptor, (const char*) message->m_Data, true);
        }
        else
        {
            if (ProfileIsInitialized())
            {
                // Try to find the message name via id and reverse hash
                message_name = (const char*)dmHashReverse64(message->m_Id, 0);
            }
            if (message->m_DataSize > 0)
                dmScript::PushTable(L, (const char*)message->m_Data, message->m_DataSize);
            else
                lua_newtable(L);
        }

        dmScript::PushURL(L, message->m_Sender);

        // An on_message function shouldn't return anything.
        {
            char buffer[128];
            const char* profiler_string = dmScript::GetProfilerString(L, is_callback ? -5 : 0, script_instance->m_Script->m_LuaModule->m_Source.m_Filename, SCRIPT_FUNCTION_NAMES[SCRIPT_FUNCTION_ONMESSAGE], message_name, buffer, sizeof(buffer));
            DM_PROFILE_DYN(profiler_string, 0);

            if (dmScript::PCall(L, 4, 0) != 0)
            {
                result = UPDATE_RESULT_UNKNOWN_ERROR;
            }
        }

        lua_pushnil(L);
        dmScript::SetInstance(L);

        assert(top == lua_gettop(L));
        return result;
    }

    UpdateResult CompScriptOnMessage(const ComponentOnMessageParams& params)
    {
        DM_PROFILE("RunScript");
        UpdateResult result = UPDATE_RESULT_OK;

        ScriptInstance* script_instance = (ScriptInstance*)*params.m_UserData;

        int function_ref;
        bool is_callback = false;
        bool deref_function_ref = true;

        dmMessage::Message* message = 0;
        void*               payload_message = 0;

        if (params.m_Message->m_Descriptor != 0)
        {
            if (params.m_Message->m_Id == dmGameObjectDDF::ScriptMessage::m_DDFDescriptor->m_NameHash)
            {
                dmGameObjectDDF::ScriptMessage* script_message = (dmGameObjectDDF::ScriptMessage*)params.m_Message->m_Data;
                uint32_t payload_message_size = 0;

                const dmDDF::Descriptor* descriptor = dmDDF::GetDescriptorFromHash(script_message->m_DescriptorHash);
                if (!descriptor)
                {
                    dmLogWarning("Failed to get message descriptor for message type %s", dmHashReverseSafe64(script_message->m_DescriptorHash));
                    return UPDATE_RESULT_OK;
                }

                const uint8_t* packed_payload = ((uint8_t*)params.m_Message->m_Data) + sizeof(dmGameObjectDDF::ScriptMessage);

                dmDDF::Result ddf_result = dmDDF::LoadMessage(packed_payload, script_message->m_PayloadSize, descriptor, &payload_message, 0, &payload_message_size);
                if (ddf_result != dmDDF::RESULT_OK)
                {
                    dmLogWarning("Failed to load message for type '%s'", descriptor->m_Name);
                    return UPDATE_RESULT_OK;
                }

                uint32_t new_message_size = sizeof(dmMessage::Message) + payload_message_size;
                message = (dmMessage::Message*)malloc(new_message_size);

                message->m_Sender       = params.m_Message->m_Sender;
                message->m_Receiver     = params.m_Message->m_Receiver;
                message->m_Id           = descriptor->m_NameHash;
                message->m_DataSize     = payload_message_size;
                message->m_Descriptor   = (uintptr_t)descriptor;
                message->m_UserData1    = 0; // should we copy the current m_UserData1?
                message->m_UserData2    = 0; // deprecated (the Lua function reference)
                message->m_Next         = 0;

                memcpy(&message->m_Data[0], payload_message, payload_message_size);

                if (script_message->m_Function)
                {
                    is_callback = true;
                    function_ref = script_message->m_Function + LUA_NOREF;
                    deref_function_ref = script_message->m_UnrefFunction; // Should the function be Unref'ed?
                }
                else
                {
                    is_callback = false;
                    deref_function_ref = false;
                    function_ref = script_instance->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_ONMESSAGE];
                }
            }
            else if (params.m_Message->m_Id == dmGameObjectDDF::ScriptUnrefMessage::m_DDFDescriptor->m_NameHash)
            {
                dmGameObjectDDF::ScriptUnrefMessage* unref_message = (dmGameObjectDDF::ScriptUnrefMessage*) params.m_Message->m_Data;
                return HandleUnrefMessage(params.m_Context, script_instance, unref_message->m_Reference + LUA_NOREF);
            }
        }

        // Is it using the old code path?
        if (!payload_message)
        {
            message = params.m_Message;

            // We added m_UserData2 just to make it easier to specify, and to keep Lua away from the dmMessage::URL struct
            // We should move towards the dmGameObjectDDF::ScriptMessage as it's type safe
            if (params.m_Message->m_UserData2) // Deprecated. Use dmGameObject::PostDDF() instead
            {
                function_ref = params.m_Message->m_UserData2 + LUA_NOREF;
                is_callback = true;
            } else {
                function_ref = script_instance->m_Script->m_FunctionReferences[SCRIPT_FUNCTION_ONMESSAGE];
            }
        }

        if (function_ref != LUA_NOREF)
        {
            result = HandleMessage(params.m_Context, script_instance, message, function_ref, is_callback, deref_function_ref);
        }

        if (payload_message)
        {
            dmDDF::FreeMessage(payload_message);
            free((void*)message);
        }

        return result;
    }

    InputResult CompScriptOnInput(const ComponentOnInputParams& params)
    {
        DM_PROFILE("RunScript");
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

            if (params.m_InputAction->m_IsGamepad)
            {
                lua_pushnumber(L, params.m_InputAction->m_GamepadIndex);
                lua_setfield(L, action_table, "gamepad");

                lua_pushinteger(L, params.m_InputAction->m_UserID);
                lua_setfield(L, action_table, "userid");

                lua_pushboolean(L, params.m_InputAction->m_GamepadUnknown);
                lua_setfield(L, action_table, "gamepad_unknown");
            }

            if (params.m_InputAction->m_GamepadConnected)
            {
                lua_pushlstring(L, params.m_InputAction->m_Text, params.m_InputAction->m_TextCount);
                lua_setfield(L, action_table, "gamepad_name");
            }

            if (params.m_InputAction->m_HasGamepadPacket)
            {
                dmHID::GamepadPacket gamepadPacket = params.m_InputAction->m_GamepadPacket;
                lua_pushliteral(L, "gamepad_axis");
                lua_createtable(L, dmHID::MAX_GAMEPAD_AXIS_COUNT, 0);
                for (int i = 0; i < dmHID::MAX_GAMEPAD_AXIS_COUNT; ++i)
                {
                    lua_pushinteger(L, (lua_Integer) (i+1));
                    lua_pushnumber(L, gamepadPacket.m_Axis[i]);
                    lua_settable(L, -3);
                }
                lua_settable(L, -3);

                lua_pushliteral(L, "gamepad_buttons");
                lua_createtable(L, dmHID::MAX_GAMEPAD_BUTTON_COUNT, 0);
                for (int i = 0; i < dmHID::MAX_GAMEPAD_BUTTON_COUNT; ++i)
                {
                    lua_pushinteger(L, (lua_Integer) (i+1));
                    lua_pushnumber(L, dmHID::GetGamepadButton(&gamepadPacket, i));
                    lua_settable(L, -3);
                }
                lua_settable(L, -3);

                lua_pushliteral(L, "gamepad_hats");
                lua_createtable(L, dmHID::MAX_GAMEPAD_HAT_COUNT, 0);
                for (int i = 0; i < dmHID::MAX_GAMEPAD_HAT_COUNT; ++i)
                {
                    lua_pushinteger(L, (lua_Integer) (i+1));
                    uint8_t hat_value;
                    if (dmHID::GetGamepadHat(&gamepadPacket, i, &hat_value))
                    {
                        lua_pushnumber(L, hat_value);
                    }
                    else
                    {
                        lua_pushnumber(L, 0);
                    }
                    lua_settable(L, -3);
                }
                lua_settable(L, -3);
            }

            if (params.m_InputAction->m_ActionId != 0 && !params.m_InputAction->m_HasText)
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

                    lua_pushliteral(L, "screen_dx");
                    lua_pushnumber(L, (lua_Integer) t.m_ScreenDX);
                    lua_rawset(L, -3);

                    lua_pushliteral(L, "screen_dy");
                    lua_pushnumber(L, (lua_Integer) t.m_ScreenDY);
                    lua_rawset(L, -3);

                    lua_settable(L, -3);
                }
                lua_settable(L, -3);
            }

            if (params.m_InputAction->m_HasText)
            {
                int tc = params.m_InputAction->m_TextCount;
                lua_pushliteral(L, "text");
                if (tc == 0) {
                    lua_pushliteral(L, "");
                } else {
                    lua_pushlstring(L, params.m_InputAction->m_Text, tc);
                }
                lua_settable(L, -3);
            }

            int arg_count = 3;
            int input_ret = lua_gettop(L) - arg_count;
            int ret;
            {
                char buffer[128];
                const char* profiler_string = dmScript::GetProfilerString(L, 0, script_instance->m_Script->m_LuaModule->m_Source.m_Filename, SCRIPT_FUNCTION_NAMES[SCRIPT_FUNCTION_ONINPUT], 0, buffer, sizeof(buffer));
                DM_PROFILE_DYN(profiler_string, 0);

                ret = dmScript::PCall(L, arg_count, LUA_MULTRET);
            }
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

    static PropertyResult RetrieveVarFromScript(HScriptInstance script_instance,
                                        const char* property_name, PropertyType type, dmhash_t* element_ids, bool is_element, uint32_t element_index,
                                        PropertyDesc& out_value)
    {
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

        return RetrieveVarFromScript(script_instance, property_name, type, element_ids, is_element, element_index, out_value);
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

    static bool CompScriptIterPropertiesGetNext(dmGameObject::SceneNodePropertyIterator* pit)
    {
        HScriptInstance script_instance = (HScriptInstance)pit->m_Node->m_Component;
        dmPropertiesDDF::PropertyDeclarations* decls = &script_instance->m_Script->m_LuaModule->m_Properties;

        uint32_t property_type = (uint32_t)((pit->m_Next >> 16) & 0xFFFF);
        uint64_t index = pit->m_Next & 0xFFFF;

        // get next element
        dmPropertiesDDF::PropertyDeclarationEntry* entries = 0;
        uint32_t element_count = 0;
        while (property_type < dmGameObject::PROPERTY_TYPE_COUNT)
        {
            element_count = 0;
            switch(property_type)
            {
            case dmGameObject::PROPERTY_TYPE_NUMBER:    entries = decls->m_NumberEntries.m_Data; element_count = decls->m_NumberEntries.m_Count; break;
            case dmGameObject::PROPERTY_TYPE_HASH:      entries = decls->m_HashEntries.m_Data; element_count = decls->m_HashEntries.m_Count; break;
            case dmGameObject::PROPERTY_TYPE_URL:       entries = decls->m_UrlEntries.m_Data; element_count = decls->m_UrlEntries.m_Count; break;
            case dmGameObject::PROPERTY_TYPE_VECTOR3:   entries = decls->m_Vector3Entries.m_Data; element_count = decls->m_Vector3Entries.m_Count; break;
            case dmGameObject::PROPERTY_TYPE_VECTOR4:   entries = decls->m_Vector4Entries.m_Data; element_count = decls->m_Vector4Entries.m_Count; break;
            case dmGameObject::PROPERTY_TYPE_QUAT:      entries = decls->m_QuatEntries.m_Data; element_count = decls->m_QuatEntries.m_Count; break;
            case dmGameObject::PROPERTY_TYPE_BOOLEAN:   entries = decls->m_BoolEntries.m_Data; element_count = decls->m_BoolEntries.m_Count; break;
            default: break;
            }

            if (index < element_count)
                break;

            // We need to check the next list
            property_type++;
            index = 0;
        }

        if (property_type == dmGameObject::PROPERTY_TYPE_COUNT)
            return false;

        assert(entries != 0);

        dmPropertiesDDF::PropertyDeclarationEntry* entry = &entries[index];

        const char* property_name = entry->m_Key;
        dmhash_t* element_ids = entry->m_ElementIds.m_Data;
        bool is_element = false;
        uint32_t element_index = 0;

        PropertyDesc var_desc;
        PropertyResult result = RetrieveVarFromScript(script_instance, property_name, (PropertyType)property_type, element_ids, is_element, element_index, var_desc);
        if (result != PROPERTY_RESULT_OK)
            return false;

        dmGameObject::PropertyVar& var = var_desc.m_Variant;

        pit->m_Next = (uint64_t)(property_type << 16 | (index+1));

        // Since the data is precompiled, we most likely cannot reverse hash it
        pit->m_Property.m_NameHash = dmHashString64(property_name);

        switch(property_type)
        {
        case dmGameObject::PROPERTY_TYPE_HASH:
            pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_HASH;
            pit->m_Property.m_Value.m_Hash = var.m_Hash;
            break;
        case dmGameObject::PROPERTY_TYPE_NUMBER:
            pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_NUMBER;
            pit->m_Property.m_Value.m_Number = var.m_Number;
            break;
        case dmGameObject::PROPERTY_TYPE_BOOLEAN:
            pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_BOOLEAN;
            pit->m_Property.m_Value.m_Bool = var.m_Bool;
            break;
        case dmGameObject::PROPERTY_TYPE_VECTOR3:
        case dmGameObject::PROPERTY_TYPE_VECTOR4:
        case dmGameObject::PROPERTY_TYPE_QUAT:
            if (property_type == dmGameObject::PROPERTY_TYPE_VECTOR4)
                pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_VECTOR4;
            else if (property_type == dmGameObject::PROPERTY_TYPE_VECTOR3)
                pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_VECTOR3;
            else if (property_type == dmGameObject::PROPERTY_TYPE_QUAT)
                pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_QUAT;
            pit->m_Property.m_Value.m_V4[0] = var.m_V4[0];
            pit->m_Property.m_Value.m_V4[1] = var.m_V4[1];
            pit->m_Property.m_Value.m_V4[2] = var.m_V4[2];
            pit->m_Property.m_Value.m_V4[3] = var.m_V4[3];
            break;
        case dmGameObject::PROPERTY_TYPE_URL:
            pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_URL;
            dmMessage::URL* url = (dmMessage::URL*)&var.m_URL[0];
            dmSnPrintf(pit->m_Property.m_Value.m_URL, sizeof(pit->m_Property.m_Value.m_URL), "%s:%s%s%s",
                            dmHashReverseSafe64(url->m_Socket), dmHashReverseSafe64(url->m_Path), url->m_Fragment?"#":"", url->m_Fragment?dmHashReverseSafe64(url->m_Fragment):"");
            break;
        }

        return true;
    }

    void CompScriptIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node)
    {
        assert(node->m_Type == dmGameObject::SCENE_NODE_TYPE_COMPONENT);
        assert(node->m_ComponentType != 0);
        pit->m_Node = node;
        pit->m_Next = (uint64_t)(dmGameObject::PROPERTY_TYPE_NUMBER << 16 | 0);
        pit->m_FnIterateNext = CompScriptIterPropertiesGetNext;
    }
}
