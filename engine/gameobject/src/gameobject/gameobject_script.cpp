// Copyright 2020-2024 The Defold Foundation
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

#include <assert.h>
#include <string.h>
#include <inttypes.h>

#include <ddf/ddf.h>

#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <dlib/profile.h>
#include <dmsdk/dlib/vmath.h>

#include <script/script.h>

#include "gameobject.h"
#include "gameobject_script.h"
#include "gameobject_script_util.h"
#include "gameobject_private.h"
#include "gameobject_props_lua.h"

#include "gameobject/gameobject_ddf.h"

// Not to pretty to include res_lua.h here but lua-modules
// are released at system shutdown and not on a per parent-resource basis
// as all other resource are. Due to the nature of lua-code and
// code in general, side-effects, we can't "shutdown" a module.
#include "res_lua.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameObject
{
    /*# Game object API documentation
     *
     * Functions, core hooks, messages and constants for manipulation of
     * game objects. The "go" namespace is accessible from game object script
     * files.
     *
     * @document
     * @name Game object
     * @namespace go
     */

    /*# [type:vector3] game object position
     *
     * The position of the game object.
     * The type of the property is vector3.
     *
     * @name position
     * @property
     * @examples
     *
     * How to query a game object's position, either as a vector3 or selecting a specific dimension:
     *
     * ```lua
     * function init(self)
     *   -- get position from "player"
     *   local pos = go.get("player", "position")
     *   local posx = go.get("player", "position.x")
     *   -- do something useful
     *   assert(pos.x == posx)
     * end
     * ```
     */

    /*# [type:quaternion] game object rotation
     *
     * The rotation of the game object.
     * The type of the property is quaternion.
     *
     * @name rotation
     * @property
     * @examples
     *
     * How to set a game object's rotation:
     *
     * ```lua
     * function init(self)
     *   -- set "player" rotation to 45 degrees around z.
     *   local rotz = vmath.quat_rotation_z(3.141592 / 4)
     *   go.set("player", "rotation", rotz)
     * end
     * ```
     */

    /*# [type:vector3] game object euler rotation
     *
     * The rotation of the game object expressed in Euler angles.
     * Euler angles are specified in degrees in the interval (-360, 360).
     * The type of the property is vector3.
     *
     * @name euler
     * @property
     * @examples
     *
     * How to set a game object's rotation with euler angles, either as a vector3 or selecting a specific dimension:
     *
     * ```lua
     * function init(self)
     *   -- set "player" euler z rotation component to 45 degrees around z.
     *   local rotz = 45
     *   go.set("player", "euler.z", rotz)
     *   local rot = go.get("player", "euler")
     *   -- do something useful
     *   assert(rot.z == rotz)
     * end
     * ```
     */

    /*# [type:number] game object scale
     *
     * The uniform scale of the game object. The type of the property is number.
     *
     * @name scale
     * @property
     * @examples
     *
     * How to scale a game object:
     *
     * ```lua
     * function init(self)
     *   -- Double the scaling on "player"
     *   local scale = go.get("player", "scale")
     *   go.set("player", "scale", scale * 2)
     * end
     * ```
     */

#define SCRIPTINSTANCE "GOScriptInstance"
#define SCRIPT "GOScript"

    static uint32_t SCRIPT_TYPE_HASH = 0;
    static uint32_t SCRIPTINSTANCE_TYPE_HASH = 0;

    using namespace dmPropertiesDDF;

    const char* SCRIPT_INSTANCE_TYPE_NAME = SCRIPTINSTANCE;

    const char* SCRIPT_FUNCTION_NAMES[MAX_SCRIPT_FUNCTION_COUNT] =
    {
        "init",
        "final",
        "update",
        "fixed_update",
        "on_message",
        "on_input",
        "on_reload"
    };

    static const char* TYPE_NAMES[PROPERTY_TYPE_COUNT] = {
        "number",        // PROPERTY_TYPE_NUMBER
        "hash",          // PROPERTY_TYPE_HASH
        "msg.url",       // PROPERTY_TYPE_URL
        "vmath.vector3", // PROPERTY_TYPE_VECTOR3
        "vmath.vector4", // PROPERTY_TYPE_VECTOR4
        "vmath.quat",    // PROPERTY_TYPE_QUAT
        "boolean",       // PROPERTY_TYPE_BOOLEAN
        "vmath.matrix4", // PROPERTY_TYPE_MATRIX4
    };

    HRegister g_Register = 0;

    CompScriptWorld::CompScriptWorld(uint32_t max_instance_count)
    : m_Instances()
    , m_ScriptWorld(0x0)
    {
        m_Instances.SetCapacity(max_instance_count);
    }

    static Script* GetScript(lua_State *L)
    {
        int top = lua_gettop(L);
        (void)top;
        dmScript::GetInstance(L);
        Script* script = (Script*)dmScript::ToUserType(L, -1, SCRIPT_TYPE_HASH);
        // Clear stack and return
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return script;
    }

    static int ScriptGetURL(lua_State* L)
    {
        dmMessage::URL url;
        dmMessage::ResetURL(&url);
        dmScript::PushURL(L, url);
        return 1;
    }

    static int ScriptResolvePath(lua_State* L)
    {
        const char* path = luaL_checkstring(L, 2);
        dmScript::PushHash(L, dmHashString64(path));
        return 1;
    }

    static int ScriptIsValid(lua_State* L)
    {
        Script* script = (Script*)lua_touserdata(L, 1);
        lua_pushboolean(L, script != 0x0 && script->m_LuaModule != 0x0);
        return 1;
    }

    static const luaL_reg Script_methods[] =
    {
        {0,0}
    };

    static const luaL_reg Script_meta[] =
    {
        {dmScript::META_TABLE_GET_URL,      ScriptGetURL},
        {dmScript::META_TABLE_RESOLVE_PATH, ScriptResolvePath},
        {dmScript::META_TABLE_IS_VALID,     ScriptIsValid},
        {0, 0}
    };

    static ScriptInstance* ScriptInstance_Check(lua_State *L, int index)
    {
        return (ScriptInstance*)dmScript::CheckUserType(L, index, SCRIPTINSTANCE_TYPE_HASH, "You can only access go.* functions and values from a script instance (.script file)");
    }

    static ScriptInstance* ScriptInstance_Check(lua_State *L)
    {
        dmScript::GetInstance(L);
        ScriptInstance* i = ScriptInstance_Check(L, -1);
        lua_pop(L, 1);
        return i;
    }

    static int ScriptInstance_tostring (lua_State *L)
    {
        lua_pushfstring(L, "Script: %p", lua_touserdata(L, 1));
        return 1;
    }

    static int ScriptInstance_index(lua_State *L)
    {
        ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, 1);
        (void) i;
        assert(i);

        // Try to find value in instance data
        lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_ScriptDataReference);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
        return 1;
    }

    static int ScriptInstance_newindex(lua_State *L)
    {
        int top = lua_gettop(L);

        ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, 1);
        (void) i;
        assert(i);

        lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_ScriptDataReference);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_settable(L, -3);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        return 0;
    }

    static void ScriptInstanceGetURLCB(lua_State* L, dmMessage::URL* out_url)
    {
        dmScript::GetInstance(L);
        ScriptInstance* i = ScriptInstance_Check(L);
        lua_pop(L, 1);
        Instance* instance = i->m_Instance;
        out_url->m_Socket = dmGameObject::GetMessageSocket(instance->m_Collection->m_HCollection);
        out_url->m_Path = instance->m_Identifier;
        out_url->m_Fragment = instance->m_Prototype->m_Components[i->m_ComponentIndex].m_Id;
    }

    static dmhash_t ScriptInstanceResolvePathCB(uintptr_t resolve_user_data, const char* path, uint32_t path_size) {
        ScriptInstance* i = (ScriptInstance*)resolve_user_data;
        if (path != 0x0 && *path != 0)
        {
            return GetAbsoluteIdentifier(i->m_Instance, path, strlen(path));
        }
        else
        {
            return i->m_Instance->m_Identifier;
        }
    }

    static int ScriptInstanceGetURL(lua_State* L)
    {
        ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, 1);
        Instance* instance = i->m_Instance;
        dmMessage::URL url;
        url.m_Socket = dmGameObject::GetMessageSocket(instance->m_Collection->m_HCollection);
        url.m_Path = instance->m_Identifier;
        url.m_Fragment = instance->m_Prototype->m_Components[i->m_ComponentIndex].m_Id;
        dmScript::PushURL(L, url);
        return 1;
    }

    static int ScriptInstanceGetUserData(lua_State* L)
    {
        ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, 1);
        lua_pushlightuserdata(L, i->m_Instance);
        return 1;
    }

    static int ScriptInstanceResolvePath(lua_State* L)
    {
        ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, 1);
        const char* path = luaL_checkstring(L, 2);

        if (path != 0x0 && *path != 0)
        {
            dmScript::PushHash(L, GetAbsoluteIdentifier(i->m_Instance, path, strlen(path)));
        }
        else
        {
            dmScript::PushHash(L, i->m_Instance->m_Identifier);
        }
        return 1;
    }

    static int ScriptInstanceIsValid(lua_State* L)
    {
        ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, 1);
        lua_pushboolean(L, i != 0x0 && i->m_Instance != 0x0);
        return 1;
    }

    static int ScriptGetInstanceContextTableRef(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        const int self_index = 1;

        ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, self_index);
        lua_pushnumber(L, i ? i->m_ContextTableReference : LUA_NOREF);

        return 1;
    }

    static int ScriptGetInstanceDataTableRef(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, 1);
        lua_pushnumber(L, i ? i->m_ScriptDataReference : LUA_NOREF);

        return 1;
    }

    static const luaL_reg ScriptInstance_methods[] =
    {
        {0,0}
    };

    static const luaL_reg ScriptInstance_meta[] =
    {
        {"__tostring",                                  ScriptInstance_tostring},
        {"__index",                                     ScriptInstance_index},
        {"__newindex",                                  ScriptInstance_newindex},
        {dmScript::META_TABLE_GET_URL,                  ScriptInstanceGetURL},
        {dmScript::META_TABLE_GET_USER_DATA,            ScriptInstanceGetUserData},
        {dmScript::META_TABLE_RESOLVE_PATH,             ScriptInstanceResolvePath},
        {dmScript::META_TABLE_IS_VALID,                 ScriptInstanceIsValid},
        {dmScript::META_GET_INSTANCE_CONTEXT_TABLE_REF, ScriptGetInstanceContextTableRef},
        {dmScript::META_GET_INSTANCE_DATA_TABLE_REF,    ScriptGetInstanceDataTableRef},
        {0, 0}
    };

    /**
     * Get instance utility function helper.
     * The function will use the default "this" instance by default
     * but if lua_gettop(L) == instance_arg, i.e. an instance reference is specified,
     * the argument instance_arg will be resolved to an instance. The function
     * only accepts instances in "this" collection. Otherwise a lua-error will be raised.
     * @param L lua state
     * @param instance_arg lua-arg
     * @return instance handler
     */
    static Instance* ResolveInstance(lua_State* L, int instance_arg)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        Instance* instance = i->m_Instance;
        if (lua_gettop(L) == instance_arg && !lua_isnil(L, instance_arg)) {
            dmMessage::URL receiver;
            dmScript::ResolveURL(L, instance_arg, &receiver, 0x0);
            if (receiver.m_Socket != dmGameObject::GetMessageSocket(i->m_Instance->m_Collection->m_HCollection))
            {
                luaL_error(L, "function called can only access instances within the same collection.");
            }

            instance = GetInstanceFromIdentifier(instance->m_Collection->m_HCollection, receiver.m_Path);
            if (!instance)
            {
                luaL_error(L, "Instance %s not found", lua_tostring(L, instance_arg));
                return 0; // Actually never reached
            }
        }
        return instance;
    }

    void GetComponentFromLua(lua_State* L, int index, HCollection collection, const char* component_ext, dmGameObject::HComponent* out_component, dmMessage::URL* url, dmGameObject::HComponentWorld* out_world)
    {
        dmMessage::URL sender;
        if (dmScript::GetURL(L, &sender))
        {
            dmMessage::URL receiver;
            dmScript::ResolveURL(L, index, &receiver, &sender);
            if (sender.m_Socket != receiver.m_Socket || sender.m_Socket != dmGameObject::GetMessageSocket(collection))
            {
                luaL_error(L, "function called can only access instances within the same collection.");
                return; // Actually never reached
            }

            HInstance instance = GetInstanceFromIdentifier(collection, receiver.m_Path);
            if (!instance)
            {
                luaL_error(L, "Instance %s not found", lua_tostring(L, index));
                return; // Actually never reached
            }

            dmGameObject::HComponentWorld world;
            uint32_t component_type_index;
            dmGameObject::Result result = dmGameObject::GetComponent(instance, receiver.m_Fragment, &component_type_index, out_component, &world);
            if ((component_ext != 0x0 || *out_component != 0x0) && result != dmGameObject::RESULT_OK)
            {
                char buffer[128];
                luaL_error(L, "The component could not be found: '%s'", dmScript::UrlToString(&receiver, buffer, sizeof(buffer)));
                return; // Actually never reached
            }

            if (out_world != 0)
            {
                *out_world = world;
            }

            if (component_ext != 0x0)
            {
                HResourceType resource_type;
                dmResource::Result resource_res = dmResource::GetTypeFromExtension(dmGameObject::GetFactory(instance->m_Collection->m_HCollection), component_ext, &resource_type);
                if (resource_res != dmResource::RESULT_OK)
                {
                    luaL_error(L, "Component type '%s' not found", component_ext);
                    return; // Actually never reached
                }
                ComponentType* type = &dmGameObject::GetRegister(instance->m_Collection->m_HCollection)->m_ComponentTypes[component_type_index];
                if (type->m_ResourceType != resource_type)
                {
                    luaL_error(L, "Component expected to be of type '%s' but was '%s'", component_ext, type->m_Name);
                    return; // Actually never reached
                }
            }
            if (url)
            {
                *url = receiver;
            }
        }
        else
        {
            luaL_error(L, "function called is not available from this script-type.");
            return; // Actually never reached
        }
    }

    HInstance GetInstanceFromLua(lua_State* L) {
        uintptr_t user_data;
        if (dmScript::GetUserData(L, &user_data, SCRIPTINSTANCE_TYPE_HASH)) {
            return (HInstance)user_data;
        } else {
            return 0;
        }
    }

    Result PostScriptMessage(const dmDDF::Descriptor* payload_descriptor, const uint8_t* payload, uint32_t payload_size, const dmMessage::URL* sender, const dmMessage::URL* receiver, int function_ref, bool unref_function_after_call)
    {
        dmArray<uint8_t> msg_buffer;
        msg_buffer.SetCapacity(sizeof(dmGameObjectDDF::ScriptMessage) + payload_size);
        msg_buffer.SetSize(msg_buffer.Capacity());

        dmGameObjectDDF::ScriptMessage* script_msg = (dmGameObjectDDF::ScriptMessage*)msg_buffer.Begin();
        script_msg->m_PayloadSize = payload_size;
        script_msg->m_DescriptorHash = payload_descriptor->m_NameHash;
        script_msg->m_Function = function_ref;
        script_msg->m_UnrefFunction = unref_function_after_call;

        uint8_t* message_payload = msg_buffer.Begin() + sizeof(dmGameObjectDDF::ScriptMessage);
        memcpy(message_payload, payload, payload_size);

        dmDDF::Descriptor* descriptor = dmGameObjectDDF::ScriptMessage::m_DDFDescriptor;
        dmMessage::Result result = Post(sender, receiver, descriptor->m_NameHash, 0, 0,
                                        (uintptr_t)descriptor, msg_buffer.Begin(), msg_buffer.Size(), 0);

        if (dmMessage::RESULT_OK != result)
        {
            DM_HASH_REVERSE_MEM(hash_ctx, 512);
            dmLogError("Failed to send message %s to %s:%s/%s", dmHashReverseSafe64Alloc(&hash_ctx, descriptor->m_NameHash), dmMessage::GetSocketName(receiver->m_Socket), dmHashReverseSafe64Alloc(&hash_ctx, receiver->m_Path), dmHashReverseSafe64Alloc(&hash_ctx, receiver->m_Fragment));
            return RESULT_UNKNOWN_ERROR;
        }
        return RESULT_OK;
    }

    static int CheckGoGetResult(lua_State* L, dmGameObject::PropertyResult result, const PropertyDesc& property_desc, dmhash_t property_id, dmGameObject::HInstance target_instance, const dmMessage::URL& target, const dmGameObject::PropertyOptions& property_options, bool index_requested)
    {
        DM_HASH_REVERSE_MEM(hash_ctx, 512);
        switch (result)
        {
        case dmGameObject::PROPERTY_RESULT_OK:
            {
                if (index_requested && (property_desc.m_ValueType != dmGameObject::PROP_VALUE_ARRAY))
                {
                    return luaL_error(L, "Options table contains index, but property '%s' is not an array.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                else if (property_options.m_HasKey && (property_desc.m_ValueType != dmGameObject::PROP_VALUE_HASHTABLE))
                {
                    return luaL_error(L, "Options table contains key, but property '%s' is not a hashtable.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }

                dmGameObject::LuaPushVar(L, property_desc.m_Variant);

                return 1;
            }
        case dmGameObject::PROPERTY_RESULT_RESOURCE_NOT_FOUND:
            {
                if (property_options.m_HasKey)
                {
                    return luaL_error(L, "Resource `%s` for property '%s' not found!", dmHashReverseSafe64Alloc(&hash_ctx, property_options.m_Key), dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                else
                {
                    return luaL_error(L, "Property '%s' not found!", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
            }
        case dmGameObject::PROPERTY_RESULT_INVALID_INDEX:
            {
                if (property_options.m_HasKey)
                {
                    return luaL_error(L, "Property '%s' is an array, but in options table specified key instead of index.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                return luaL_error(L, "Invalid index %d for property '%s'", property_options.m_Index+1, dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
        case dmGameObject::PROPERTY_RESULT_INVALID_KEY:
            {
                if (!property_options.m_HasKey)
                {
                    return luaL_error(L, "Property '%s' is a hashtable, but in options table specified index instead of key.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                return luaL_error(L, "Invalid key '%s' for property '%s'", dmHashReverseSafe64Alloc(&hash_ctx, property_options.m_Key), dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
        case dmGameObject::PROPERTY_RESULT_NOT_FOUND:
            {
                const char* path = dmHashReverseSafe64Alloc(&hash_ctx, target.m_Path);
                const char* property = dmHashReverseSafe64Alloc(&hash_ctx, property_id);
                if (target.m_Fragment)
                {
                    return luaL_error(L, "'%s#%s' does not have any property called '%s'", path, dmHashReverseSafe64Alloc(&hash_ctx, target.m_Fragment), property);
                }
                return luaL_error(L, "'%s' does not have any property called '%s'", path, property);
            }
        case dmGameObject::PROPERTY_RESULT_COMP_NOT_FOUND:
            return luaL_error(L, "Could not find component '%s' when resolving '%s'", dmHashReverseSafe64Alloc(&hash_ctx, target.m_Fragment), lua_tostring(L, 1));
        default:
            // Should never happen, programmer error
            return luaL_error(L, "go.get failed with error code %d", result);
        }
        return 0;
    }

    /*# gets a named property of the specified game object or component
     *
     * @name go.get
     * @param url [type:string|hash|url] url of the game object or component having the property
     * @param property [type:string|hash] id of the property to retrieve
     * @param [options] [type:table] optional options table
     * - index [type:integer] index into array property (1 based)
     * - key [type:hash] name of internal property
     * @return value [type:any] the value of the specified property
     *
     * @examples
     * Get a property "speed" from a script "player", the property must be declared in the player-script:
     *
     * ```lua
     * go.property("speed", 50)
     * ```
     *
     * Then in the calling script (assumed to belong to the same game object, but does not have to):
     *
     * ```lua
     * local speed = go.get("#player", "speed")
     * ```
     *
     * @examples
     * Get a value in a material property array
     *
     * ```lua
     * -- get the first vector4 in the array: example[0] (the glsl indices are 0-based)
     * go.get(url, "example", {index=1})
     *
     * -- get the last vector4 in the array: example[15] (the glsl indices are 0-based)
     * go.get(url, "example", {index=16})
     *
     * -- get an element of a vector4 in the array: example[0].x (the glsl indices are 0-based)
     * go.get(url, "example.x", {index=1})
     * ```
     *
     * Getting all values in a material property array as a table
     *
     * ```lua
     * -- get all vector4's in the constant array
     * go.get(url, "example")
     * -- result: { vector4, vector4, ... }
     *
     * -- get all elements of the vector4's from an array
     * go.get(url, "example.x")
     * -- result: { number1, number2, ... }
     * ```
     *
     * @examples
     * Get a named property
     *
     * ```lua
     * function init(self)
     *     -- get the resource of a certain gui font
     *     local font_hash = go.get("#gui", "fonts", {key = "system_font_BIG"})
     * end
     * ```
     */
    int Script_Get(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        Instance* instance = i->m_Instance;
        dmMessage::URL sender;
        dmScript::GetURL(L, &sender);
        dmMessage::URL target;
        dmScript::ResolveURL(L, 1, &target, &sender);
        DM_HASH_REVERSE_MEM(hash_ctx, 256);
        if (target.m_Socket != dmGameObject::GetMessageSocket(i->m_Instance->m_Collection->m_HCollection))
        {
            return luaL_error(L, "go.get can only access instances within the same collection.");
        }
        dmhash_t property_id = 0;
        if (lua_isstring(L, 2))
        {
            property_id = dmHashString64(lua_tostring(L, 2));
        }
        else
        {
            property_id = dmScript::CheckHash(L, 2);
        }
        dmGameObject::HInstance target_instance = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(instance), target.m_Path);
        if (target_instance == 0)
            return luaL_error(L, "Could not find any instance with id '%s'.", dmHashReverseSafe64Alloc(&hash_ctx, target.m_Path));
        dmGameObject::PropertyOptions property_options;
        property_options.m_Index = 0;
        property_options.m_HasKey = 0;
        bool index_requested = false;

        // Options table
        if (lua_gettop(L) > 2)
        {
            luaL_checktype(L, 3, LUA_TTABLE);
            lua_pushvalue(L, 3);

            lua_getfield(L, -1, "key");
            if (!lua_isnil(L, -1))
            {
                property_options.m_Key = dmScript::CheckHashOrString(L, -1);
                property_options.m_HasKey = 1;
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "index");
            if (!lua_isnil(L, -1)) // make it optional
            {
                if (property_options.m_HasKey)
                {
                    return luaL_error(L, "Options table cannot contain both 'key' and 'index'.");
                }
                if (!lua_isnumber(L, -1))
                {
                    return luaL_error(L, "Invalid number passed as index argument in options table.");
                }

                property_options.m_Index = luaL_checkinteger(L, -1) - 1;

                if (property_options.m_Index < 0)
                {
                    return luaL_error(L, "Trying to get property value from '%s' with an index < 0: %d", dmHashReverseSafe64Alloc(&hash_ctx, property_id), property_options.m_Index);
                }

                index_requested = true;
            }
            lua_pop(L, 1);

            lua_pop(L, 1);
        }
        dmGameObject::PropertyDesc property_desc;
        dmGameObject::PropertyResult result = dmGameObject::GetProperty(target_instance, target.m_Fragment, property_id, property_options, property_desc);

        if (result == dmGameObject::PROPERTY_RESULT_OK && !index_requested && property_desc.m_ValueType == dmGameObject::PROP_VALUE_ARRAY && property_desc.m_ArrayLength > 1)
        {
            lua_newtable(L);

            // We already have the first value, so no need to get it again.
            // But we do need to check the result, we could still get errors even if the result is OK
            int handle_go_get_result = CheckGoGetResult(L, result, property_desc, property_id, target_instance, target, property_options, index_requested);
            if (handle_go_get_result != 1)
            {
                return handle_go_get_result;
            }
            lua_rawseti(L, -2, 1);

            // Get the rest of the array elements and check each result individually
            for (int i = 1; i < property_desc.m_ArrayLength; ++i)
            {
                property_options.m_Index = i;
                result                   = dmGameObject::GetProperty(target_instance, target.m_Fragment, property_id, property_options, property_desc);
                handle_go_get_result     = CheckGoGetResult(L, result, property_desc, property_id, target_instance, target, property_options, index_requested);
                if (handle_go_get_result != 1)
                {
                    return handle_go_get_result;
                }
                lua_rawseti(L, -2, i + 1);
            }

            return 1;
        }

        return CheckGoGetResult(L, result, property_desc, property_id, target_instance, target, property_options, index_requested);
    }

    static int HandleGoSetResult(lua_State* L, dmGameObject::PropertyResult result, dmhash_t property_id, dmGameObject::HInstance target_instance, const dmMessage::URL& target, const dmGameObject::PropertyOptions& property_options)
    {
        DM_HASH_REVERSE_MEM(hash_ctx, 512);

        switch (result)
        {
            case dmGameObject::PROPERTY_RESULT_OK:
                return 0;
            case PROPERTY_RESULT_NOT_FOUND:
            {
                // The supplied URL parameter don't need to be a string,
                // we let Lua handle the "conversion" to string using concatenation.
                const char* name = "nil";
                if (!lua_isnil(L, 1))
                {
                    lua_pushliteral(L, "");
                    lua_pushvalue(L, 1);
                    lua_concat(L, 2);
                    name = lua_tostring(L, -1);
                    lua_pop(L, 1);
                }
                return luaL_error(L, "'%s' does not have any property called '%s'", name, dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
            case PROPERTY_RESULT_UNSUPPORTED_TYPE:
            case PROPERTY_RESULT_TYPE_MISMATCH:
            {
                dmGameObject::PropertyDesc property_desc;
                dmGameObject::GetProperty(target_instance, target.m_Fragment, property_id, property_options, property_desc);
                return luaL_error(L, "the property '%s' of '%s' must be a %s", dmHashReverseSafe64Alloc(&hash_ctx, property_id), lua_tostring(L, 1), TYPE_NAMES[property_desc.m_Variant.m_Type]);
            }
            case PROPERTY_RESULT_READ_ONLY:
            {
                return luaL_error(L, "Unable to set the property '%s' since it is read only", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
            case dmGameObject::PROPERTY_RESULT_INVALID_INDEX:
            {
                if (property_options.m_HasKey)
                {
                    return luaL_error(L, "Property '%s' is an array, but in options table specified key instead of index.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                return luaL_error(L, "Invalid index %d for property '%s'", property_options.m_Index+1, dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
            case dmGameObject::PROPERTY_RESULT_INVALID_KEY:
            {
                if (!property_options.m_HasKey)
                {
                    return luaL_error(L, "Property '%s' is a hashtable, but in options table specified index instead of key.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                return luaL_error(L, "Invalid key '%s' for property '%s'", dmHashReverseSafe64Alloc(&hash_ctx, property_options.m_Key), dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
            case dmGameObject::PROPERTY_RESULT_COMP_NOT_FOUND:
                return luaL_error(L, "could not find component '%s' when resolving '%s'", dmHashReverseSafe64Alloc(&hash_ctx, target.m_Fragment), lua_tostring(L, 1));
            case dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE:
                return luaL_error(L, "go.set failed because the value is unsupported");
            case dmGameObject::PROPERTY_RESULT_UNSUPPORTED_OPERATION:
                return luaL_error(L, "could not perform unsupported operation on '%s'", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            default:
                // Should never happen, programmer error
                return luaL_error(L, "go.set failed with error code %d", result);
        }

        return 0;
    }

    /*# sets a named property of the specified game object or component, or a material constant
     *
     * @name go.set
     * @param url [type:string|hash|url] url of the game object or component having the property
     * @param property [type:string|hash] id of the property to set
     * @param value [type:any|table] the value to set
     * @param [options] [type:table] optional options table
     * - index [type:integer] index into array property (1 based)
     * - key [type:hash] name of internal property
     * @examples
     *
     * Set a property "speed" of a script "player", the property must be declared in the player-script:
     *
     * ```lua
     * go.property("speed", 50)
     * ```
     *
     * Then in the calling script (assumed to belong to the same game object, but does not have to):
     *
     * ```lua
     * go.set("#player", "speed", 100)
     * ```
     *
     * @examples
     * Set a vector4 in a material property array
     *
     * ```lua
     * -- set the first vector4 in the array: example[0] = v (the glsl indices are 0-based)
     * go.set(url, "example", vmath.vector4(1,1,1,1), {index=1})
     *
     * -- set the last vector4 in the array: example[15] = v (the glsl indices are 0-based)
     * go.set(url, "example", vmath.vector4(2,2,2,2), {index=16})
     *
     * -- set an element of a vector4 in the array: example[0].x = 7 (the glsl indices are 0-based)
     * go.set(url, "example.x", 7, {index=1})
     * ```
     *
     * Set a material property array by a table of vector4
     *
     * ```lua
     * -- set the first two vector4's in the array
     * -- if the array has more than two elements in the array they will not be set
     * go.set(url, "example", { vmath.vector4(1,1,1,1), vmath.vector4(2,2,2,2) })
     * ```
     *
     * @examples
     * Set a named property
     *
     * ```lua
     * go.property("big_font", resource.font())
     *
     * function init(self)
     *     go.set("#gui", "fonts", self.big_font, {key = "system_font_BIG"})
     * end
     * ```
     */
    int Script_Set(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        DM_HASH_REVERSE_MEM(hash_ctx, 256);
        ScriptInstance* i = ScriptInstance_Check(L);
        Instance* instance = i->m_Instance;
        dmMessage::URL sender;
        dmScript::GetURL(L, &sender);
        dmMessage::URL target;
        dmScript::ResolveURL(L, 1, &target, &sender);
        if (target.m_Socket != dmGameObject::GetMessageSocket(i->m_Instance->m_Collection->m_HCollection))
        {
            luaL_error(L, "go.set can only access instances within the same collection.");
        }
        dmhash_t property_id = 0;

        if (lua_isstring(L, 2))
        {
            property_id = dmHashString64(lua_tostring(L, 2));
        }
        else
        {
            property_id = dmScript::CheckHash(L, 2);
        }

        dmGameObject::HInstance target_instance = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(instance), target.m_Path);
        if (target_instance == 0)
        {
            return luaL_error(L, "could not find any instance with id '%s'.", dmHashReverseSafe64Alloc(&hash_ctx, target.m_Path));
        }

        dmGameObject::PropertyOptions property_options;
        property_options.m_Index  = 0;
        property_options.m_HasKey = 0;

        bool property_val_is_table = lua_istable(L, 3);

        // Options table
        if (lua_gettop(L) > 3)
        {
            luaL_checktype(L, 4, LUA_TTABLE);
            lua_pushvalue(L, 4);

            lua_getfield(L, -1, "key");
            if (!lua_isnil(L, -1))
            {
                property_options.m_Key = dmScript::CheckHashOrString(L, -1);
                property_options.m_HasKey = 1;
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "index");
            if (!lua_isnil(L, -1)) // make it optional
            {
                if (property_options.m_HasKey)
                {
                    return luaL_error(L, "Options table cannot contain both 'key' and 'index'.");
                }
                if (!lua_isnumber(L, -1))
                {
                    return luaL_error(L, "Invalid number passed as index argument in options table.");
                }
                else if (property_val_is_table)
                {
                    dmLogWarning("Options table has an index, but setting a property value with an array will ignore the index.");
                }

                property_options.m_Index = luaL_checkinteger(L, -1) - 1;

                if (property_options.m_Index < 0)
                {
                    return luaL_error(L, "Trying to set property value for '%s' with an index < 0: %d", dmHashReverseSafe64Alloc(&hash_ctx, property_id), property_options.m_Index);
                }
            }
            lua_pop(L, 1);

            lua_pop(L, 1);
        }

        if (property_val_is_table)
        {
            lua_pushvalue(L, 3);
            lua_pushnil(L);
            while (lua_next(L, -2) != 0)
            {
                if (!lua_isnumber(L, -2))
                {
                    return luaL_error(L, "Trying to set property value '%s' as array with a non-integer key.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }

                int32_t table_index_lua = lua_tonumber(L, -2);

                if (table_index_lua < 1)
                {
                    return luaL_error(L, "Trying to set property value '%s' as array with a negative key (%d) is not permitted.", dmHashReverseSafe64Alloc(&hash_ctx, property_id), table_index_lua);
                }

                dmGameObject::PropertyVar property_var;
                dmGameObject::PropertyResult result = dmGameObject::LuaToVar(L, -1, property_var);

                property_options.m_Index = table_index_lua - 1;

                if (result == PROPERTY_RESULT_OK)
                {
                    result = dmGameObject::SetProperty(target_instance, target.m_Fragment, property_id, property_options, property_var);
                    if (result != PROPERTY_RESULT_OK)
                    {
                        return HandleGoSetResult(L, result, property_id, target_instance, target, property_options);
                    }
                }

                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        else
        {
            dmGameObject::PropertyVar property_var;
            dmGameObject::PropertyResult result = dmGameObject::LuaToVar(L, 3, property_var);

            if (result == PROPERTY_RESULT_OK)
            {
                result = dmGameObject::SetProperty(target_instance, target.m_Fragment, property_id, property_options, property_var);
            }

            return HandleGoSetResult(L, result, property_id, target_instance, target, property_options);
        }

        return 0;
    }

    /*# gets the position of a game object instance
     * The position is relative the parent (if any). Use [ref:go.get_world_position] to retrieve the global world position.
     *
     * @name go.get_position
     * @replaces request_transform transform_response
     * @param [id] [type:string|hash|url] optional id of the game object instance to get the position for, by default the instance of the calling script
     * @return position [type:vector3] instance position
     * @examples
     *
     * Get the position of the game object instance the script is attached to:
     *
     * ```lua
     * local p = go.get_position()
     * ```
     *
     * Get the position of another game object instance "my_gameobject":
     *
     * ```lua
     * local pos = go.get_position("my_gameobject")
     * ```
     */
    int Script_GetPosition(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        dmScript::PushVector3(L, dmVMath::Vector3(dmGameObject::GetPosition(instance)));
        return 1;
    }

    /*# gets the rotation of the game object instance
     * The rotation is relative to the parent (if any). Use [ref:go.get_world_rotation] to retrieve the global world rotation.
     *
     * @name go.get_rotation
     * @param [id] [type:string|hash|url] optional id of the game object instance to get the rotation for, by default the instance of the calling script
     * @return rotation [type:quaternion] instance rotation
     * @examples
     *
     * Get the rotation of the game object instance the script is attached to:
     *
     * ```lua
     * local r = go.get_rotation()
     * ```
     *
     * Get the rotation of another game object instance with id "x":
     *
     * ```lua
     * local r = go.get_rotation("x")
     * ```
     */
    int Script_GetRotation(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        dmScript::PushQuat(L, dmGameObject::GetRotation(instance));
        return 1;
    }

     /*# gets the 3D scale factor of the game object instance
     * The scale is relative the parent (if any). Use [ref:go.get_world_scale] to retrieve the global world 3D scale factor.
     *
     * @name go.get_scale
     * @param [id] [type:string|hash|url] optional id of the game object instance to get the scale for, by default the instance of the calling script
     * @return scale [type:vector3] instance scale factor
     * @examples
     *
     * Get the scale of the game object instance the script is attached to:
     *
     * ```lua
     * local s = go.get_scale()
     * ```
     *
     * Get the scale of another game object instance with id "x":
     *
     * ```lua
     * local s = go.get_scale("x")
     * ```
     */
    static int Script_GetScale(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        dmScript::PushVector3(L, dmGameObject::GetScale(instance));
        return 1;
    }

    /* DEPRECATED gets the 3D scale factor of the instance
     * The scale is relative the parent (if any). Use [ref:go.get_world_scale] to retrieve the global world scale factor.
     *
     * @name go.get_scale_vector
     * @param [id] [type:string|hash|url] optional id of the instance to get the scale for, by default the instance of the calling script
     * @return scale [type:vector3] scale factor
     * @examples
     *
     * Get the scale of the instance the script is attached to:
     *
     * ```lua
     * local s = go.get_scale_vector()
     * ```
     *
     * Get the scale of another instance "x":
     *
     * ```lua
     * local s = go.get_scale_vector("x")
     * ```
     */
    static int Script_GetScaleVector(lua_State* L)
    {
        return Script_GetScale(L);
    }

    /*# gets the uniform scale factor of the game object instance
     * The uniform scale is relative the parent (if any). If the underlying scale vector is non-uniform the min element of the vector is returned as the uniform scale factor.
     *
     * @name go.get_scale_uniform
     * @param [id] [type:string|hash|url] optional id of the game object instance to get the uniform scale for, by default the instance of the calling script
     * @return scale [type:number] uniform instance scale factor
     * @examples
     *
     * Get the scale of the game object instance the script is attached to:
     *
     * ```lua
     * local s = go.get_scale_uniform()
     * ```
     *
     * Get the uniform scale of another game object instance with id "x":
     *
     * ```lua
     * local s = go.get_scale_uniform("x")
     * ```
     */
    int Script_GetScaleUniform(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        lua_pushnumber(L, dmGameObject::GetUniformScale(instance));
        return 1;
    }

    /*# sets the position of the game object instance
     * The position is relative to the parent (if any). The global world position cannot be manually set.
     *
     * @name go.set_position
     * @param position [type:vector3] position to set
     * @param [id] [type:string|hash|url] optional id of the game object instance to set the position for, by default the instance of the calling script
     * @examples
     *
     * Set the position of the game object instance the script is attached to:
     *
     * ```lua
     * local p = ...
     * go.set_position(p)
     * ```
     *
     * Set the position of another game object instance with id "x":
     *
     * ```lua
     * local p = ...
     * go.set_position(p, "x")
     * ```
     */
    int Script_SetPosition(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 2);
        dmVMath::Vector3* v = dmScript::CheckVector3(L, 1);
        dmGameObject::SetPosition(instance, dmVMath::Point3(*v));
        return 0;
    }

    /*# sets the rotation of the game object instance
     * The rotation is relative to the parent (if any). The global world rotation cannot be manually set.
     *
     * @name go.set_rotation
     * @param rotation [type:quaternion] rotation to set
     * @param [id] [type:string|hash|url] optional id of the game object instance to get the rotation for, by default the instance of the calling script
     * @examples
     *
     * Set the rotation of the game object instance the script is attached to:
     *
     * ```lua
     * local r = ...
     * go.set_rotation(r)
     * ```
     *
     * Set the rotation of another game object instance with id "x":
     *
     * ```lua
     * local r = ...
     * go.set_rotation(r, "x")
     * ```
     */
    int Script_SetRotation(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 2);
        dmVMath::Quat* q = dmScript::CheckQuat(L, 1);
        dmGameObject::SetRotation(instance, *q);
        return 0;
    }

    /*# sets the scale factor of the game object instance
     * The scale factor is relative to the parent (if any). The global world scale factor cannot be manually set.
     *
     * [icon:attention] Physics are currently not affected when setting scale from this function.
     *
     * @name go.set_scale
     * @param scale [type:number|vector3] vector or uniform scale factor, must be greater than 0
     * @param [id] [type:string|hash|url] optional id of the game object instance to get the scale for, by default the instance of the calling script
     * @examples
     *
     * Set the scale of the game object instance the script is attached to:
     *
     * ```lua
     * local s = vmath.vector3(2.0, 1.0, 1.0)
     * go.set_scale(s)
     * ```
     *
     * Set the scale of another game object instance with id "x":
     *
     * ```lua
     * local s = 1.2
     * go.set_scale(s, "x")
     * ```
     */
    int Script_SetScale(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 2);

        // Supports both vector and number
        Vector3* v = dmScript::ToVector3(L, 1);
        if (v != 0)
        {
            Vector3 scale = *v;
            if (scale.getX() <= 0.0f || scale.getY() <= 0.0f || scale.getZ() <= 0.0f)
            {
                return luaL_error(L, "Vector passed to go.set_scale contains components that are below or equal to zero");
            }
            dmGameObject::SetScale(instance, scale);
            return 0;
        }

        lua_Number n = luaL_checknumber(L, 1);
        if (n <= 0.0)
        {
            return luaL_error(L, "The scale supplied to go.set_scale must be greater than 0.");
        }
        dmGameObject::SetScale(instance, (float)n);
        return 0;
    }

    /*# sets the parent for a specific game object instance
     * Sets the parent for a game object instance. This means that the instance will exist in the geometrical space of its parent,
     * like a basic transformation hierarchy or scene graph. If no parent is specified, the instance will be detached from any parent and exist in world
     * space.
     * This function will generate a `set_parent` message. It is not until the message has been processed that the change actually takes effect. This
     * typically happens later in the same frame or the beginning of the next frame. Refer to the manual to learn how messages are processed by the
     * engine.
     *
     * @name go.set_parent
     * @param [id] [type:string|hash|url] optional id of the game object instance to set parent for, defaults to the instance containing the calling script
     * @param [parent_id] [type:string|hash|url] optional id of the new parent game object, defaults to detaching game object from its parent
     * @param [keep_world_transform] [type:boolean] optional boolean, set to true to maintain the world transform when changing spaces. Defaults to false.
     * @examples
     *
     * Attach myself to another instance "my_parent":
     *
     * ```lua
     * go.set_parent(go.get_id(),go.get_id("my_parent"))
     * ```
     *
     * Attach an instance "my_instance" to another instance "my_parent":
     *
     * ```lua
     * go.set_parent(go.get_id("my_instance"),go.get_id("my_parent"))
     * ```
     *
     * Detach an instance "my_instance" from its parent (if any):
     *
     * ```lua
     * go.set_parent(go.get_id("my_instance"))
     * ```
     */
    int Script_SetParent(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L,0);

        DM_HASH_REVERSE_MEM(hash_ctx, 256);
        ScriptInstance* i  = ScriptInstance_Check(L);
        Instance* instance = i->m_Instance;

        dmMessage::URL sender, target;
        dmScript::GetURL(L, &sender);
        dmScript::ResolveURL(L, 1, &target, &sender);

        if (target.m_Socket != dmGameObject::GetMessageSocket(instance->m_Collection->m_HCollection))
        {
            return DM_LUA_ERROR("go.set_parent can only access instances within the same collection.");
        }

        HCollection collection    = dmGameObject::GetCollection(instance);
        Instance* child_instance  = dmGameObject::GetInstanceFromIdentifier(collection, target.m_Path);
        Instance* parent_instance = 0x0;

        if (!child_instance)
        {
            return DM_LUA_ERROR("Could not find any instance with id '%s'.", dmHashReverseSafe64Alloc(&hash_ctx, target.m_Path));
        }

        if (lua_gettop(L) > 1 && !lua_isnil(L, 2))
        {
            dmScript::ResolveURL(L, 2, &target, &sender);
            parent_instance = dmGameObject::GetInstanceFromIdentifier(collection, target.m_Path);

            if (!parent_instance)
            {
                return DM_LUA_ERROR("Could not find any instance with id '%s'.", dmHashReverseSafe64Alloc(&hash_ctx, target.m_Path));
            }

            if (target.m_Socket != dmGameObject::GetMessageSocket(instance->m_Collection->m_HCollection))
            {
                return DM_LUA_ERROR("go.set_parent can only access instances within the same collection.");
            }
        }

        dmGameObjectDDF::SetParent ddf;
        ddf.m_KeepWorldTransform = lua_toboolean(L, 3);

        if (parent_instance)
        {
            ddf.m_ParentId = dmGameObject::GetIdentifier(parent_instance);
        }
        else
        {
            ddf.m_ParentId = 0;
        }

        dmMessage::URL receiver;
        receiver.m_Socket   = dmGameObject::GetMessageSocket(child_instance->m_Collection->m_HCollection);
        receiver.m_Path     = dmGameObject::GetIdentifier(child_instance);
        receiver.m_Fragment = 0;

        if (dmMessage::RESULT_OK != dmMessage::Post(0x0, &receiver, dmGameObjectDDF::SetParent::m_DDFDescriptor->m_NameHash,
            (uintptr_t) child_instance, (uintptr_t) dmGameObjectDDF::SetParent::m_DDFDescriptor,
            &ddf, sizeof(dmGameObjectDDF::SetParent), 0))
        {
            return DM_LUA_ERROR("Could not send parenting message!");
        }

        return 0;
    }

    /*# get the parent for a specific game object instance
     * Get the parent for a game object instance.
     *
     * @name go.get_parent
     * @param [id] [type:string|hash|url] optional id of the game object instance to get parent for, defaults to the instance containing the calling script
     * @return parent_id [type:hash|nil] parent instance or `nil`
     * @examples
     *
     * Get parent of the instance containing the calling script:
     *
     * ```lua
     * local parent_id = go.get_parent()
     * ```
     *
     * Get parent of the instance with id "x":
     *
     * ```lua
     * local parent_id = go.get_parent("x")
     * ```
     *
     */
    int Script_GetParent(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        Instance* parent = dmGameObject::GetParent(instance);
        if (parent != 0)
        {
            dmScript::PushHash(L, parent->m_Identifier);
        }
        else
        {
            lua_pushnil(L);
        }
        return 1;

    }

    /*# gets the game object instance world position
     * The function will return the world position calculated at the end of the previous frame.
     * Use [ref:go.get_position] to retrieve the position relative to the parent.
     *
     * @name go.get_world_position
     * @param [id] [type:string|hash|url] optional id of the game object instance to get the world position for, by default the instance of the calling script
     * @return position [type:vector3] instance world position
     * @examples
     *
     * Get the world position of the game object instance the script is attached to:
     *
     * ```lua
     * local p = go.get_world_position()
     * ```
     *
     * Get the world position of another game object instance with id "x":
     *
     * ```lua
     * local p = go.get_world_position("x")
     * ```
     */
    int Script_GetWorldPosition(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        dmScript::PushVector3(L, dmVMath::Vector3(dmGameObject::GetWorldPosition(instance)));
        return 1;
    }

    /*# gets the game object instance world rotation
     * The function will return the world rotation calculated at the end of the previous frame.
     * Use [ref:go.get_rotation] to retrieve the rotation relative to the parent.
     *
     * @name go.get_world_rotation
     * @param [id] [type:string|hash|url] optional id of the game object instance to get the world rotation for, by default the instance of the calling script
     * @return rotation [type:quaternion] instance world rotation
     * @examples
     *
     * Get the world rotation of the game object instance the script is attached to:
     *
     * ```lua
     * local r = go.get_world_rotation()
     * ```
     *
     * Get the world rotation of another game object instance with id "x":
     *
     * ```lua
     * local r = go.get_world_rotation("x")
     * ```
     */
    int Script_GetWorldRotation(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        dmScript::PushQuat(L, dmGameObject::GetWorldRotation(instance));
        return 1;
    }

    /*# gets the game object instance world 3D scale factor
     * The function will return the world 3D scale factor calculated at the end of the previous frame.
     * Use [ref:go.get_scale] to retrieve the 3D scale factor relative to the parent.
     * This vector is derived by decomposing the transformation matrix and should be used with care.
     * For most cases it should be fine to use [ref:go.get_world_scale_uniform] instead.
     *
     * @name go.get_world_scale
     * @param [id] [type:string|hash|url] optional id of the game object instance to get the world scale for, by default the instance of the calling script
     * @return scale [type:vector3] instance world 3D scale factor
     * @examples
     *
     * Get the world 3D scale of the game object instance the script is attached to:
     *
     * ```lua
     * local s = go.get_world_scale()
     * ```
     *
     * Get the world scale of another game object instance "x":
     *
     * ```lua
     * local s = go.get_world_scale("x")
     * ```
     */
    int Script_GetWorldScale(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        dmScript::PushVector3(L, dmGameObject::GetWorldScale(instance));
        return 1;
    }

    /*# gets the uniform game object instance world scale factor
     * The function will return the world scale factor calculated at the end of the previous frame.
     * Use [ref:go.get_scale_uniform] to retrieve the scale factor relative to the parent.
     *
     * @name go.get_world_scale_uniform
     * @param [id] [type:string|hash|url] optional id of the game object instance to get the world scale for, by default the instance of the calling script
     * @return scale [type:number] instance world scale factor
     * @examples
     *
     * Get the world scale of the game object instance the script is attached to:
     *
     * ```lua
     * local s = go.get_world_scale_uniform()
     * ```
     *
     * Get the world scale of another game object instance with id "x":
     *
     * ```lua
     * local s = go.get_world_scale_uniform("x")
     * ```
     */
    int Script_GetWorldScaleUniform(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        lua_pushnumber(L, dmGameObject::GetWorldUniformScale(instance));
        return 1;
    }

    /*# gets the game object instance world transform matrix
     * The function will return the world transform matrix calculated at the end of the previous frame.
     *
     * @name go.get_world_transform
     * @param [id] [type:string|hash|url] optional id of the game object instance to get the world transform for, by default the instance of the calling script
     * @return transform [type:matrix4] instance world transform
     * @examples
     *
     * Get the world transform of the game object instance the script is attached to:
     *
     * ```lua
     * local m = go.get_world_transform()
     * ```
     *
     * Get the world transform of another game object instance with id "x":
     *
     * ```lua
     * local m = go.get_world_transform("x")
     * ```
     */
    int Script_GetWorldTransform(lua_State* L)
    {
        Instance* instance = ResolveInstance(L,1);
        dmScript::PushMatrix4(L, dmGameObject::GetWorldMatrix(instance));
        return 1;
    }

    /*# gets the id of an instance
     * Returns or constructs an instance identifier. The instance id is a hash
     * of the absolute path to the instance.
     *
     * - If `path` is specified, it can either be absolute or relative to the instance of the calling script.
     * - If `path` is not specified, the id of the game object instance the script is attached to will be returned.
     *
     * @name go.get_id
     * @param [path] [type:string] path of the instance for which to return the id
     * @return id [type:hash] instance id
     * @examples
     *
     * For the instance with path `/my_sub_collection/my_instance`, the following calls are equivalent:
     *
     * ```lua
     * local id = go.get_id() -- no path, defaults to the instance containing the calling script
     * print(id) --> hash: [/my_sub_collection/my_instance]
     *
     * local id = go.get_id("/my_sub_collection/my_instance") -- absolute path
     * print(id) --> hash: [/my_sub_collection/my_instance]
     *
     * local id = go.get_id("my_instance") -- relative path
     * print(id) --> hash: [/my_sub_collection/my_instance]
     * ```
     */
    int Script_GetId(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        if (lua_gettop(L) > 0)
        {
            const char* ident = luaL_checkstring(L, 1);
            dmScript::PushHash(L, GetAbsoluteIdentifier(i->m_Instance, ident, strlen(ident)));
        }
        else
        {
            dmScript::PushHash(L, i->m_Instance->m_Identifier);
        }
        return 1;
    }

    static lua_State* GetLuaState(ScriptInstance* instance) {
        return instance->m_Script->m_LuaState;
    }

    void LuaCurveRelease(dmEasing::Curve* curve)
    {
        lua_State *L = (lua_State*)curve->userdata1;

        int top = lua_gettop(L);
        (void) top;

        int ref = (int) (((uintptr_t) curve->userdata2) & 0xffffffff);
        dmScript::Unref(L, LUA_REGISTRYINDEX, ref);

        curve->release_callback = 0x0;
        curve->userdata1 = 0x0;
        curve->userdata2 = 0x0;

        assert(top == lua_gettop(L));
    }

    struct LuaAnimationStoppedArgs
    {
        LuaAnimationStoppedArgs(dmMessage::URL url, dmhash_t property_id)
            : m_URL(url), m_PropertyId(property_id)
        {}
        dmMessage::URL m_URL;
        dmhash_t m_PropertyId;
    };

    static void LuaAnimationStoppedCallback(lua_State* L, void* user_args)
    {
        LuaAnimationStoppedArgs* args = (LuaAnimationStoppedArgs*)user_args;
        dmScript::PushURL(L, args->m_URL);
        dmScript::PushHash(L, args->m_PropertyId);
    }

    static void LuaAnimationStopped(dmGameObject::HInstance instance, dmhash_t component_id, dmhash_t property_id,
                                        bool finished, void* userdata1, void* userdata2)
    {
        dmScript::LuaCallbackInfo* cbk = (dmScript::LuaCallbackInfo*)userdata1;
        if (dmScript::IsCallbackValid(cbk) && finished)
        {
            dmMessage::URL url;
            url.m_Socket = dmGameObject::GetMessageSocket(instance->m_Collection->m_HCollection);
            url.m_Path = instance->m_Identifier;
            url.m_Fragment = component_id;

            LuaAnimationStoppedArgs args(url, property_id);
            dmScript::InvokeCallback(cbk, LuaAnimationStoppedCallback, &args);
        }
        dmScript::DestroyCallback(cbk);
    }

    /*# animates a named property of the specified game object or component
     *
     * This is only supported for numerical properties. If the node property is already being
     * animated, that animation will be canceled and replaced by the new one.
     *
     * If a `complete_function` (lua function) is specified, that function will be called when the animation has completed.
     * By starting a new animation in that function, several animations can be sequenced together. See the examples for more information.
     *
     * [icon:attention] If you call `go.animate()` from a game object's `final()` function,
     * any passed `complete_function` will be ignored and never called upon animation completion.
     *
     * See the <a href="/manuals/properties">properties guide</a> for which properties can be animated and the <a href="/manuals/animation">animation guide</a> for how
     them.
     *
     * @name go.animate
     * @param url [type:string|hash|url] url of the game object or component having the property
     * @param property [type:string|hash] id of the property to animate
     * @param playback [type:constant] playback mode of the animation
     *
     * - `go.PLAYBACK_ONCE_FORWARD`
     * - `go.PLAYBACK_ONCE_BACKWARD`
     * - `go.PLAYBACK_ONCE_PINGPONG`
     * - `go.PLAYBACK_LOOP_FORWARD`
     * - `go.PLAYBACK_LOOP_BACKWARD`
     * - `go.PLAYBACK_LOOP_PINGPONG`
     *
     * @param to [type:number|vector3|vector4|quaternion] target property value
     * @param easing [type:constant|vector] easing to use during animation. Either specify a constant, see the <a href="/manuals/animation#_easing">animation guide</a> for a complete list, or a vmath.vector with a curve
     * @param duration [type:number] duration of the animation in seconds
     * @param [delay] [type:number] delay before the animation starts in seconds
     * @param [complete_function] [type:function(self, url, property)] optional function to call when the animation has completed
     *
     * `self`
     * :        [type:object] The current object.
     *
     * `url`
     * :        [type:url] The game object or component instance for which the property is animated.
     *
     * `property`
     * :        [type:hash] The id of the animated property.
     *
     * @examples
     *
     * Animate the position of a game object to x = 10 during 1 second, then y = 20 during 1 second:
     *
     * ```lua
     * local function x_done(self, url, property)
     *     go.animate(go.get_id(), "position.y", go.PLAYBACK_ONCE_FORWARD, 20, go.EASING_LINEAR, 1)
     * end
     *
     * function init(self)
     *     go.animate(go.get_id(), "position.x", go.PLAYBACK_ONCE_FORWARD, 10, go.EASING_LINEAR, 1, 0, x_done)
     * end
     * ```
     *
     * Animate the y position of a game object using a crazy custom easing curve:
     *
     * ```lua
     * local values = { 0, 0, 0, 0, 0, 0, 0, 0,
     *                  1, 1, 1, 1, 1, 1, 1, 1,
     *                  0, 0, 0, 0, 0, 0, 0, 0,
     *                  1, 1, 1, 1, 1, 1, 1, 1,
     *                  0, 0, 0, 0, 0, 0, 0, 0,
     *                  1, 1, 1, 1, 1, 1, 1, 1,
     *                  0, 0, 0, 0, 0, 0, 0, 0,
     *                  1, 1, 1, 1, 1, 1, 1, 1 }
     * local vec = vmath.vector(values)
     * go.animate("go", "position.y", go.PLAYBACK_LOOP_PINGPONG, 100, vec, 2.0)
     * ```
     */
    int Script_Animate(lua_State* L)
    {
        int top = lua_gettop(L);
        (void)top;

        DM_HASH_REVERSE_MEM(hash_ctx, 256);
        ScriptInstance* i = ScriptInstance_Check(L);
        Instance* instance = i->m_Instance;
        dmMessage::URL sender;
        dmScript::GetURL(L, &sender);
        dmMessage::URL target;
        dmScript::ResolveURL(L, 1, &target, &sender);
        HCollection collection = dmGameObject::GetCollection(instance);
        if (target.m_Socket != dmGameObject::GetMessageSocket(collection))
        {
            luaL_error(L, "go.animate can only animate instances within the same collection.");
        }
        dmhash_t property_id = 0;
        if (lua_isstring(L, 2))
        {
            property_id = dmHashString64(lua_tostring(L, 2));
        }
        else
        {
            property_id = dmScript::CheckHash(L, 2);
        }
        dmGameObject::HInstance target_instance = dmGameObject::GetInstanceFromIdentifier(collection, target.m_Path);
        if (target_instance == 0)
            return luaL_error(L, "Could not find any instance with id '%s'.", dmHashReverseSafe64Alloc(&hash_ctx, target.m_Path));
        lua_Integer playback = luaL_checkinteger(L, 3);
        if (playback >= PLAYBACK_COUNT)
            return luaL_error(L, "invalid playback mode when starting an animation");
        dmGameObject::PropertyVar property_var;
        dmGameObject::PropertyResult result = dmGameObject::LuaToVar(L, 4, property_var);
        if (result != PROPERTY_RESULT_OK)
        {
            return luaL_error(L, "only numerical values can be used as target values for animation");
        }

        dmEasing::Curve curve;
        if (lua_isnumber(L, 5))
        {
            curve.type = (dmEasing::Type)luaL_checkinteger(L, 5);
            if (curve.type >= dmEasing::TYPE_COUNT)
                return luaL_error(L, "invalid easing constant");
        }
        else if (dmScript::IsVector(L, 5))
        {
            curve.type = dmEasing::TYPE_FLOAT_VECTOR;
            curve.vector = dmScript::CheckVector(L, 5);

            lua_pushvalue(L, 5);
            curve.release_callback = LuaCurveRelease;
            curve.userdata1 = L;
            curve.userdata2 = (void*)(uintptr_t)dmScript::Ref(L, LUA_REGISTRYINDEX);
        }
        else
        {
            return luaL_error(L, "easing must be either a easing constant or a vmath.vector");
        }

        float duration = (float) luaL_checknumber(L, 6);
        float delay = 0.0f;
        if (top > 6)
            delay = (float) luaL_checknumber(L, 7);
        AnimationStopped stopped = 0x0;
        dmScript::LuaCallbackInfo* cbk = 0x0;
        if (top > 7)
        {
            if (lua_isfunction(L, 8))
            {
                cbk = dmScript::CreateCallback(L, 8);
                stopped = LuaAnimationStopped;
            }
        }

        result = dmGameObject::Animate(collection, target_instance, target.m_Fragment, property_id,
                (Playback)playback, property_var, curve, duration, delay, stopped, cbk, 0x0);
        switch (result)
        {
        case dmGameObject::PROPERTY_RESULT_OK:
            break;
        case PROPERTY_RESULT_NOT_FOUND:
            {
                lua_pushliteral(L, "");
                dmScript::PushURL(L, target);
                lua_concat(L, 2);
                const char* name = lua_tostring(L, -1);
                lua_pop(L, 1);
                return luaL_error(L, "'%s' does not have any property called '%s'", name, dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
        case PROPERTY_RESULT_UNSUPPORTED_TYPE:
        case PROPERTY_RESULT_TYPE_MISMATCH:
            {
                lua_pushliteral(L, "");
                dmScript::PushURL(L, target);
                lua_concat(L, 2);
                const char* name = lua_tostring(L, -1);
                lua_pop(L, 1);
                return luaL_error(L, "The property '%s' of '%s' has incorrect type", dmHashReverseSafe64Alloc(&hash_ctx, property_id), name);
            }
        case dmGameObject::PROPERTY_RESULT_COMP_NOT_FOUND:
            return luaL_error(L, "could not find component '%s' when resolving '%s'", dmHashReverseSafe64Alloc(&hash_ctx, target.m_Fragment), lua_tostring(L, 1));
        case dmGameObject::PROPERTY_RESULT_UNSUPPORTED_OPERATION:
            {
                lua_pushliteral(L, "");
                dmScript::PushURL(L, target);
                lua_concat(L, 2);
                const char* name = lua_tostring(L, -1);
                lua_pop(L, 1);
                return luaL_error(L, "Animation of the property '%s' of '%s' is unsupported", dmHashReverseSafe64Alloc(&hash_ctx, property_id), name);
            }
        default:
            // Should never happen, programmer error
            return luaL_error(L, "go.animate failed with error code %d", result);
        }

        assert(lua_gettop(L) == top);
        return 0;
    }

    /*# cancels all or specified property animations of the game object or component
     *
     * By calling this function, all or specified stored property animations of the game object or component will be canceled.
     *
     * See the <a href="/manuals/properties">properties guide</a> for which properties can be animated and the <a href="/manuals/animation">animation guide</a> for how to animate them.
     *
     * @name go.cancel_animations
     * @param url [type:string|hash|url] url of the game object or component
     * @param [property] [type:string|hash] optional id of the property to cancel
     * @examples
     *
     * Cancel the animation of the position of a game object:
     *
     * ```lua
     * go.cancel_animations(go.get_id(), "position")
     * ```
     *
     * Cancel all property animations of the current game object:
     *
     * ```lua
     * go.cancel_animations(".")
     * ```
     *
     * Cancel all property animations of the sprite component of the current game object:
     *
     * ```lua
     * go.cancel_animations("#sprite")
     * ```
     */
    int Script_CancelAnimations(lua_State* L)
    {
        int top = lua_gettop(L);
        (void)top;

        DM_HASH_REVERSE_MEM(hash_ctx, 256);
        ScriptInstance* i = ScriptInstance_Check(L);
        Instance* instance = i->m_Instance;
        dmMessage::URL sender;
        dmScript::GetURL(L, &sender);
        dmMessage::URL target;
        dmScript::ResolveURL(L, 1, &target, &sender);
        HCollection collection = dmGameObject::GetCollection(instance);
        if (target.m_Socket != dmGameObject::GetMessageSocket(collection))
        {
            luaL_error(L, "go.animate can only animate instances within the same collection.");
        }
        dmhash_t property_id = 0;
        if (top >= 2 && !lua_isnil(L, 2))
        {
            if (lua_isstring(L, 2))
            {
                property_id = dmHashString64(lua_tostring(L, 2));
            }
            else
            {
                property_id = dmScript::CheckHash(L, 2);
            }
        }
        dmGameObject::HInstance target_instance = dmGameObject::GetInstanceFromIdentifier(collection, target.m_Path);
        if (target_instance == 0)
            return luaL_error(L, "Could not find any instance with id '%s'.", dmHashReverseSafe64Alloc(&hash_ctx, target.m_Path));

        dmGameObject::PropertyOptions opt;
        opt.m_Index = 0;

        dmGameObject::PropertyResult res = dmGameObject::CancelAnimations(collection, target_instance, target.m_Fragment, property_id);

        switch (res)
        {
        case dmGameObject::PROPERTY_RESULT_OK:
            break;
        case PROPERTY_RESULT_NOT_FOUND:
            {
                lua_pushliteral(L, "");
                dmScript::PushURL(L, target);
                lua_concat(L, 2);
                const char* name = lua_tostring(L, -1);
                lua_pop(L, 1);
                return luaL_error(L, "'%s' does not have any property called '%s'", name, dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
        case PROPERTY_RESULT_UNSUPPORTED_TYPE:
        case PROPERTY_RESULT_TYPE_MISMATCH:
            {
                dmGameObject::PropertyDesc property_desc;
                dmGameObject::GetProperty(target_instance, target.m_Fragment, property_id, opt, property_desc);
                return luaL_error(L, "The property '%s' must be of a numerical type", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
        case dmGameObject::PROPERTY_RESULT_COMP_NOT_FOUND:
            return luaL_error(L, "could not find component '%s' when resolving '%s'", dmHashReverseSafe64Alloc(&hash_ctx, target.m_Fragment), lua_tostring(L, 1));
        default:
            // Should never happen, programmer error
            return luaL_error(L, "go.cancel_animations failed with error code %d", res);
        }

        assert(lua_gettop(L) == top);
        return 0;
    }


    static int DeleteGOTable(lua_State* L, bool recursive)
    {
        DM_HASH_REVERSE_MEM(hash_ctx, 256);
        ScriptInstance* i = ScriptInstance_Check(L);
        HCollection hcollection = i->m_Instance->m_Collection->m_HCollection;

        // read table
        lua_pushnil(L);
        while (lua_next(L, 1)) {

            // value should be hashes
            dmMessage::URL receiver;
            dmScript::ResolveURL(L, -1, &receiver, 0x0);
            if (receiver.m_Socket != dmGameObject::GetMessageSocket(hcollection))
            {
                luaL_error(L, "Function called can only access instances within the same collection.");
            }

            Instance *todelete = GetInstanceFromIdentifier(hcollection, receiver.m_Path);
            if (todelete)
            {
                if(dmGameObject::IsBone(todelete))
                {
                    return luaL_error(L, "Can not delete subinstances of spine or model components. '%s'", dmHashReverseSafe64Alloc(&hash_ctx, dmGameObject::GetIdentifier(todelete)));
                }
                if (todelete->m_Generated)
                {
                    dmScript::ReleaseHash(L, todelete->m_Identifier);
                }
                dmGameObject::Delete(hcollection, todelete, recursive);
            }
            else
            {
                dmLogWarning("go.delete(): instance could not be resolved");
            }

            lua_pop(L, 1);
        }
        return 0;
    }


    /*# delete one or more game object instances
     * Delete one or more game objects identified by id. Deletion is asynchronous meaning that
     * the game object(s) are scheduled for deletion which will happen at the end of the current
     * frame. Note that game objects scheduled for deletion will be counted against
     * `max_instances` in "game.project" until they are actually removed.
     *
     * [icon:attention] Deleting a game object containing a particle FX component emitting particles will not immediately stop the particle FX from emitting particles. You need to manually stop the particle FX using `particlefx.stop()`.
     * [icon:attention] Deleting a game object containing a sound component that is playing will not immediately stop the sound from playing. You need to manually stop the sound using `sound.stop()`.
     *
     * @name go.delete
     * @param [id] [type:string|hash|url|table] optional id or table of id's of the instance(s) to delete, the instance of the calling script is deleted by default
     * @param [recursive] [type:boolean] optional boolean, set to true to recursively delete child hiearchy in child to parent order
     * @examples
     *
     * This example demonstrates how to delete game objects
     *
     * ```lua
     * -- Delete the script game object
     * go.delete()
     * -- Delete a game object with the id "my_game_object".
     * local id = go.get_id("my_game_object") -- retrieve the id of the game object to be deleted
     * go.delete(id)
     * -- Delete a list of game objects.
     * local ids = { hash("/my_object_1"), hash("/my_object_2"), hash("/my_object_3") }
     * go.delete(ids)
     * ```
     *
     * This example demonstrates how to delete a game objects and their children (child to parent order)
     *
     * ```lua
     * -- Delete the script game object and it's children
     * go.delete(true)
     * -- Delete a game object with the id "my_game_object" and it's children.
     * local id = go.get_id("my_game_object") -- retrieve the id of the game object to be deleted
     * go.delete(id, true)
     * -- Delete a list of game objects and their children.
     * local ids = { hash("/my_object_1"), hash("/my_object_2"), hash("/my_object_3") }
     * go.delete(ids, true)
     * ```
     *
     */
    static int Script_Delete(lua_State* L)
    {
        int args = lua_gettop(L);

        if(args > 2)
        {
            return luaL_error(L, "go.delete invoked with too many argumengs");
        }

        // deduct recursive bool parameter (optional last parameter)
        bool recursive = false;
        if(args != 0)
        {
            if(lua_isboolean(L, 1))
            {
                // if argument #1 is boolean, no more arguments are accepted
                if(args > 1)
                {
                    return luaL_error(L, "go.delete expected one argument when argument #1 is boolean type");
                }
                recursive = lua_toboolean(L, 1);
                lua_pop(L, 1);
                --args;
            }
            else if(args > 1)
            {
                // if argument #1 isn't a boolean, it's resolved later. Argument #2 is required to be a boolean
                if(lua_isboolean(L, 2))
                {
                    recursive = lua_toboolean(L, 2);
                }
                else
                {
                    return luaL_error(L, "go.delete expected boolean as argument #2");
                }
                lua_pop(L, 1);
                --args;
            }
        }

        // handle optional parameter #1 is table or nil
        if(args != 0)
        {
            if(lua_istable(L, 1))
            {
                int result = DeleteGOTable(L, recursive);
                if(result == 0)
                {
                    assert(args == lua_gettop(L));
                }
                return result;
            }
            else if(lua_isnil(L, 1))
            {
                dmLogWarning("go.delete() invoked with nil and self will be deleted");
            }
        }

        // Resolive argument #1 url
        dmGameObject::HInstance instance = ResolveInstance(L, 1);
        if(dmGameObject::IsBone(instance))
        {
            DM_HASH_REVERSE_MEM(hash_ctx, 256);
            return luaL_error(L, "Can not delete subinstances of spine or model components. '%s'", dmHashReverseSafe64Alloc(&hash_ctx, dmGameObject::GetIdentifier(instance)));
        }
        if (instance->m_Generated)
        {
            dmScript::ReleaseHash(L, instance->m_Identifier);
        }
        dmGameObject::HCollection collection = instance->m_Collection->m_HCollection;
        dmGameObject::Delete(collection, instance, recursive);
        return 0;
    }

    /* DEPRECATED deletes a set of game object instance
     * Delete all game objects simultaneously as listed in table.
     * The table values (not keys) should be game object ids (hashes).
     *
     * @name go.delete_all
     * @param [ids] [type:table] table with values of instance ids (hashes) to be deleted
     * @examples
     *
     * An example how to delete game objects listed in a table:
     *
     * ```lua
     * -- List the objects to be deleted
     * local ids = { hash("/my_object_1"), hash("/my_object_2"), hash("/my_object_3") }
     * go.delete_all(ids)
     * ```
     *
     * An example how to delete game objects spawned via a collectionfactory:
     *
     * ```lua
     * -- Spawn a collection of game objects.
     * local ids = collectionfactory.create("#collectionfactory")
     * ...
     * -- Delete all objects listed in the table 'ids'.
     * go.delete_all(ids)
     * ```
     */
    static int Script_DeleteAll(lua_State* L)
    {
        return Script_Delete(L);
    }

    /*# define a property for the script
     * This function defines a property which can then be used in the script through the self-reference.
     * The properties defined this way are automatically exposed in the editor in game objects and collections which use the script.
     * Note that you can only use this function outside any callback-functions like init and update.
     *
     * @name go.property
     * @param name [type:string] the id of the property
     * @param value [type:number|hash|url|vector3|vector4|quaternion|resource|boolean] default value of the property. In the case of a url, only the empty constructor msg.url() is allowed. In the case of a resource one of the resource constructors (eg resource.atlas(), resource.font() etc) is expected.
     * @examples
     *
     * This example demonstrates how to define a property called "health" in a script.
     * The health is decreased whenever someone sends a message called "take_damage" to the script.
     *
     * ```lua
     * go.property("health", 100)
     *
     * function init(self)
     *     -- prints 100 to the output
     *     print(self.health)
     * end
     *
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("take_damage") then
     *         self.health = self.health - message.damage
     *         print("Ouch! My health is now: " .. self.health)
     *     end
     * end
     * ```
     */
    int Script_Property(lua_State* L)
    {
        int top = lua_gettop(L);
        (void)top;

        Script* script = GetScript(L);

        if (script == 0x0)
        {
            return luaL_error(L, "go.property can only be called outside the functions.");
        }

        const char* id = luaL_checkstring(L, 1);
        (void)id;

        bool valid_type = false;
        if (lua_isnumber(L, 2))
        {
            valid_type = true;
        }
        else if (dmScript::IsURL(L, 2))
        {
            valid_type = true;
        }
        else if (dmScript::IsHash(L, 2))
        {
            valid_type = true;
        }
        else if (dmScript::ToVector3(L, 2) != 0)
        {
            valid_type = true;
        }
        else if (dmScript::ToVector4(L, 2) != 0)
        {
            valid_type = true;
        }
        else if (dmScript::ToQuat(L, 2) != 0)
        {
            valid_type = true;
        }
        else if (lua_isboolean(L, 2))
        {
            valid_type = true;
        }
        if (!valid_type)
        {
            return luaL_error(L, "Invalid type (%s) supplied to go.property, must be either a number, boolean, hash, URL, vector3, vector4 or quaternion.", lua_typename(L, lua_type(L, 2)));
        }
        assert(top == lua_gettop(L));
        return 0;
    }


    /*# check if the specified game object exists
     *
     * @name go.exists
     * @param url [type:string|hash|url] url of the game object to check
     * @return exists [type:bool] true if the game object exists
     *
     * @examples
     * Check if game object "my_game_object" exists
     *
     * ```lua
     * go.exists("/my_game_object")
     * ```
     */
    int Script_Exists(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        ScriptInstance* i = ScriptInstance_Check(L);
        Instance* instance = i->m_Instance;
        dmMessage::URL sender;
        dmScript::GetURL(L, &sender);
        dmMessage::URL target;
        dmScript::ResolveURL(L, 1, &target, &sender);

        dmGameObject::HInstance target_instance = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(instance), target.m_Path);
        lua_pushboolean(L, target_instance != 0);
        return 1;
    }


    /*# convert position to game object's coordinate space
    * [icon:attention] The function uses world transformation calculated at the end of previous frame.
    *
    * @name go.world_to_local_position
    * @param position [type:vector3] position which need to be converted
    * @param url [type:string|hash|url] url of the game object which coordinate system convert to
    * @return converted_postion [type:vector3] converted position
    *
    * @examples
    * Convert position of "test" game object into coordinate space of "child" object.
    *
    * ```lua
    *   local test_pos = go.get_world_position("/test")
    *   local child_pos = go.get_world_position("/child")
    *   local new_position = go.world_to_local_position(test_pos, "/child")
    * ```
    */
    int Script_WorldToLocalPosition(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmVMath::Vector3* world_position = dmScript::CheckVector3(L, 1);
        Instance* instance = ResolveInstance(L, 2);
        dmVMath::Matrix4 go_transform = dmGameObject::GetWorldMatrix(instance);
        dmVMath::Matrix4 world_transform = dmVMath::Matrix4::identity();
        world_transform.setTranslation(*world_position);
        dmVMath::Matrix4 result_transfrom = world_transform * go_transform;
        dmScript::PushVector3(L, result_transfrom.getTranslation());
        return 1;
    }


    /*# convert transformation matrix to game object's coordinate space
    * [icon:attention] The function uses world transformation calculated at the end of previous frame.
    *
    * @name go.world_to_local_transform
    * @param transformation [type:matrix4] transformation which need to be converted
    * @param url [type:string|hash|url] url of the game object which coordinate system convert to
    * @return converted_transform [type:matrix4] converted transformation
    *
    * @examples
    * Convert transformation of "test" game object into coordinate space of "child" object.
    *
    * ```lua
    *    local test_transform = go.get_world_transform("/test")
    *    local child_transform = go.get_world_transform("/child")
    *    local result_transform = go.world_to_local_transform(test_transform, "/child")
    * ```
    */
    int Script_WorldToLocalTransfrom(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmVMath::Matrix4* world_transform = dmScript::CheckMatrix4(L, 1);
        Instance* instance = ResolveInstance(L, 2);
        const dmVMath::Matrix4& go_transform = dmGameObject::GetWorldMatrix(instance);

        dmScript::PushMatrix4(L,  *world_transform * go_transform);
        return 1;
    }

    static const luaL_reg GO_methods[] =
    {
        {"get",                     Script_Get},
        {"set",                     Script_Set},
        {"get_position",            Script_GetPosition},
        {"get_rotation",            Script_GetRotation},
        {"get_scale",               Script_GetScale},
        {"get_scale_vector",        Script_GetScaleVector},
        {"get_scale_uniform",       Script_GetScaleUniform},
        {"get_parent",              Script_GetParent},
        {"set_position",            Script_SetPosition},
        {"set_rotation",            Script_SetRotation},
        {"set_scale",               Script_SetScale},
        {"set_parent",              Script_SetParent},
        {"get_world_position",      Script_GetWorldPosition},
        {"get_world_rotation",      Script_GetWorldRotation},
        {"get_world_scale",         Script_GetWorldScale},
        {"get_world_scale_uniform", Script_GetWorldScaleUniform},
        {"get_world_transform",     Script_GetWorldTransform},
        {"get_id",                  Script_GetId},
        {"animate",                 Script_Animate},
        {"cancel_animations",       Script_CancelAnimations},
        {"delete",                  Script_Delete},
        {"delete_all",              Script_DeleteAll},
        {"property",                Script_Property},
        {"exists",                  Script_Exists},
        {"world_to_local_position", Script_WorldToLocalPosition},
        {"world_to_local_transform",Script_WorldToLocalTransfrom},
        {0, 0}
    };

    void InitializeScript(HRegister regist, dmScript::HContext context)
    {
        g_Register = regist;

        lua_State* L = dmScript::GetLuaState(context);

        int top = lua_gettop(L);
        (void)top;

        SCRIPT_TYPE_HASH = dmScript::RegisterUserType(L, SCRIPT, Script_methods, Script_meta);

        SCRIPTINSTANCE_TYPE_HASH = dmScript::RegisterUserType(L, SCRIPTINSTANCE, ScriptInstance_methods, ScriptInstance_meta);

        luaL_register(L, "go", GO_methods);

#define SETPLAYBACK(name) \
        lua_pushnumber(L, (lua_Number) PLAYBACK_##name); \
        lua_setfield(L, -2, "PLAYBACK_"#name);\

        SETPLAYBACK(NONE)
        SETPLAYBACK(ONCE_FORWARD)
        SETPLAYBACK(ONCE_BACKWARD)
        SETPLAYBACK(ONCE_PINGPONG)
        SETPLAYBACK(LOOP_FORWARD)
        SETPLAYBACK(LOOP_BACKWARD)
        SETPLAYBACK(LOOP_PINGPONG)

#undef SETPLAYBACK

#define SETEASING(name) \
        lua_pushnumber(L, (lua_Number) dmEasing::TYPE_##name); \
        lua_setfield(L, -2, "EASING_"#name);\

        SETEASING(LINEAR)
        SETEASING(INQUAD)
        SETEASING(OUTQUAD)
        SETEASING(INOUTQUAD)
        SETEASING(OUTINQUAD)
        SETEASING(INCUBIC)
        SETEASING(OUTCUBIC)
        SETEASING(INOUTCUBIC)
        SETEASING(OUTINCUBIC)
        SETEASING(INQUART)
        SETEASING(OUTQUART)
        SETEASING(INOUTQUART)
        SETEASING(OUTINQUART)
        SETEASING(INQUINT)
        SETEASING(OUTQUINT)
        SETEASING(INOUTQUINT)
        SETEASING(OUTINQUINT)
        SETEASING(INSINE)
        SETEASING(OUTSINE)
        SETEASING(INOUTSINE)
        SETEASING(OUTINSINE)
        SETEASING(INEXPO)
        SETEASING(OUTEXPO)
        SETEASING(INOUTEXPO)
        SETEASING(OUTINEXPO)
        SETEASING(INCIRC)
        SETEASING(OUTCIRC)
        SETEASING(INOUTCIRC)
        SETEASING(OUTINCIRC)
        SETEASING(INELASTIC)
        SETEASING(OUTELASTIC)
        SETEASING(INOUTELASTIC)
        SETEASING(OUTINELASTIC)
        SETEASING(INBACK)
        SETEASING(OUTBACK)
        SETEASING(INOUTBACK)
        SETEASING(OUTINBACK)
        SETEASING(INBOUNCE)
        SETEASING(OUTBOUNCE)
        SETEASING(INOUTBOUNCE)
        SETEASING(OUTINBOUNCE)

#undef SETEASING

        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

    static bool LoadScript(lua_State* L, dmLuaDDF::LuaSource *source, Script* script)
    {
        for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
            script->m_FunctionReferences[i] = LUA_NOREF;

        bool result = false;
        int top = lua_gettop(L);
        (void) top;

        int ret = dmScript::LuaLoad(L, source);
        if (ret == 0)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, script->m_InstanceReference);
            dmScript::SetInstance(L);

            ret = dmScript::PCall(L, 0, 0);
            if (ret == 0)
            {
                for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
                {
                    lua_getglobal(L, SCRIPT_FUNCTION_NAMES[i]);
                    if (lua_isnil(L, -1) == 0)
                    {
                        if (lua_type(L, -1) == LUA_TFUNCTION)
                        {
                            script->m_FunctionReferences[i] = dmScript::Ref(L, LUA_REGISTRYINDEX);
                        }
                        else
                        {
                            dmLogError("The global name '%s' in '%s' must be a function.", SCRIPT_FUNCTION_NAMES[i], source->m_Filename);
                            lua_pop(L, 1);
                            goto bail;
                        }
                    }
                    else
                    {
                        script->m_FunctionReferences[i] = LUA_NOREF;
                        lua_pop(L, 1);
                    }
                }
                result = true;
            }
            lua_pushnil(L);
            dmScript::SetInstance(L);
        } else {
            dmLogError("Error running script: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
        }
bail:
        for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
        {
            lua_pushnil(L);
            lua_setglobal(L, SCRIPT_FUNCTION_NAMES[i]);
        }
        assert(top == lua_gettop(L));
        return result;
    }

    static PropertyResult GetPropertyDefault(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var);

    static void ResetScript(HScript script) {
        memset(script, 0, sizeof(Script));
        for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i) {
            script->m_FunctionReferences[i] = LUA_NOREF;
        }
        script->m_InstanceReference = LUA_NOREF;
    }

    HScript NewScript(lua_State* L, dmLuaDDF::LuaModule* lua_module)
    {
        Script* script = (Script*)lua_newuserdata(L, sizeof(Script));
        ResetScript(script);
        script->m_LuaState = L;

        lua_pushvalue(L, -1);
        script->m_InstanceReference = dmScript::Ref(L, LUA_REGISTRYINDEX);

        script->m_PropertySet.m_UserData = (uintptr_t)script;
        script->m_PropertySet.m_GetPropertyCallback = GetPropertyDefault;
        script->m_LuaModule = lua_module;
        luaL_getmetatable(L, SCRIPT);
        lua_setmetatable(L, -2);

        if (!LoadScript(L, &lua_module->m_Source, script))
        {
            DeleteScript(script);
            return 0;
        }

        lua_pop(L, 1);
        return script;
    }

    bool ReloadScript(HScript script, dmLuaDDF::LuaModule* lua_module)
    {
        script->m_LuaModule = lua_module;
        return LoadScript(script->m_LuaState, &lua_module->m_Source, script);
    }

    void DeleteScript(HScript script)
    {
        lua_State* L = script->m_LuaState;
        for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
        {
            if (script->m_FunctionReferences[i] != LUA_NOREF) {
                dmScript::Unref(L, LUA_REGISTRYINDEX, script->m_FunctionReferences[i]);
            }
        }

        dmScript::Unref(L, LUA_REGISTRYINDEX, script->m_InstanceReference);
        script->~Script();
        ResetScript(script);
    }

    static PropertyResult GetPropertyDefault(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var)
    {
        Script* script = (Script*)user_data;
        const PropertyDeclarations* defs = &script->m_LuaModule->m_Properties;
        uint32_t n = defs->m_NumberEntries.m_Count;
        for (uint32_t i = 0; i < n; ++i)
        {
            const PropertyDeclarationEntry& entry = defs->m_NumberEntries[i];
            if (entry.m_Id == id)
            {
                out_var.m_Type = PROPERTY_TYPE_NUMBER;
                out_var.m_Number = defs->m_FloatValues[entry.m_Index];
                return PROPERTY_RESULT_OK;
            }
        }
        n = defs->m_HashEntries.m_Count;
        for (uint32_t i = 0; i < n; ++i)
        {
            const PropertyDeclarationEntry& entry = defs->m_HashEntries[i];
            if (entry.m_Id == id)
            {
                out_var.m_Type = PROPERTY_TYPE_HASH;
                out_var.m_Hash = defs->m_HashValues[entry.m_Index];
                return PROPERTY_RESULT_OK;
            }
        }
        n = defs->m_UrlEntries.m_Count;
        for (uint32_t i = 0; i < n; ++i)
        {
            const PropertyDeclarationEntry& entry = defs->m_UrlEntries[i];
            if (entry.m_Id == id)
            {
                out_var.m_Type = PROPERTY_TYPE_URL;
                dmMessage::URL default_url;
                lua_State* L = (lua_State*)properties->m_ResolvePathUserData;
                properties->m_GetURLCallback(L, &default_url);
                const char* url_string = defs->m_StringValues[entry.m_Index];
                dmMessage::Result result = dmScript::ResolveURL(L, url_string, (dmMessage::URL*) out_var.m_URL, &default_url);
                if (result != dmMessage::RESULT_OK)
                {
                    return PROPERTY_RESULT_INVALID_FORMAT;
                }
                return PROPERTY_RESULT_OK;
            }
        }
        n = defs->m_Vector3Entries.m_Count;
        for (uint32_t i = 0; i < n; ++i)
        {
            const PropertyDeclarationEntry& entry = defs->m_Vector3Entries[i];
            if (entry.m_Id == id)
            {
                out_var.m_Type = PROPERTY_TYPE_VECTOR3;
                const float* v = &defs->m_FloatValues[entry.m_Index];
                out_var.m_V4[0] = v[0];
                out_var.m_V4[1] = v[1];
                out_var.m_V4[2] = v[2];
                return PROPERTY_RESULT_OK;
            }
        }
        n = defs->m_Vector4Entries.m_Count;
        for (uint32_t i = 0; i < n; ++i)
        {
            const PropertyDeclarationEntry& entry = defs->m_Vector4Entries[i];
            if (entry.m_Id == id)
            {
                out_var.m_Type = PROPERTY_TYPE_VECTOR4;
                const float* v = &defs->m_FloatValues[entry.m_Index];
                out_var.m_V4[0] = v[0];
                out_var.m_V4[1] = v[1];
                out_var.m_V4[2] = v[2];
                out_var.m_V4[3] = v[3];
                return PROPERTY_RESULT_OK;
            }
        }
        n = defs->m_QuatEntries.m_Count;
        for (uint32_t i = 0; i < n; ++i)
        {
            const PropertyDeclarationEntry& entry = defs->m_QuatEntries[i];
            if (entry.m_Id == id)
            {
                out_var.m_Type = PROPERTY_TYPE_QUAT;
                const float* v = &defs->m_FloatValues[entry.m_Index];
                out_var.m_V4[0] = v[0];
                out_var.m_V4[1] = v[1];
                out_var.m_V4[2] = v[2];
                out_var.m_V4[3] = v[3];
                return PROPERTY_RESULT_OK;
            }
        }
        n = defs->m_BoolEntries.m_Count;
        for (uint32_t i = 0; i < n; ++i)
        {
            const PropertyDeclarationEntry& entry = defs->m_BoolEntries[i];
            if (entry.m_Id == id)
            {
                out_var.m_Type = PROPERTY_TYPE_BOOLEAN;
                out_var.m_Bool = defs->m_FloatValues[entry.m_Index] != 0.0f;
                return PROPERTY_RESULT_OK;
            }
        }
        return PROPERTY_RESULT_NOT_FOUND;
    }

    static void ResetScriptInstance(HScriptInstance script_instance) {
        memset(script_instance, 0, sizeof(ScriptInstance));
        script_instance->m_InstanceReference = LUA_NOREF;
        script_instance->m_ScriptDataReference = LUA_NOREF;
        script_instance->m_ContextTableReference = LUA_NOREF;
    }

    HScriptInstance NewScriptInstance(CompScriptWorld* script_world, HScript script, HInstance instance, uint16_t component_index)
    {
        lua_State* L = script->m_LuaState;

        int top = lua_gettop(L);
        (void) top;

        ScriptInstance* i = (ScriptInstance *)lua_newuserdata(L, sizeof(ScriptInstance));
        ResetScriptInstance(i);
        i->m_Script = script;

        lua_pushvalue(L, -1);
        i->m_InstanceReference = dmScript::Ref( L, LUA_REGISTRYINDEX );

        lua_newtable(L);
        i->m_ScriptDataReference = dmScript::Ref( L, LUA_REGISTRYINDEX );

        lua_newtable(L);
        i->m_ContextTableReference = dmScript::Ref( L, LUA_REGISTRYINDEX );

        i->m_Instance = instance;
        i->m_ScriptWorld = script_world->m_ScriptWorld;
        i->m_ComponentIndex = component_index;
        NewPropertiesParams params;
        params.m_ResolvePathCallback = ScriptInstanceResolvePathCB;
        params.m_ResolvePathUserData = (uintptr_t)L;
        params.m_GetURLCallback = ScriptInstanceGetURLCB;
        i->m_Properties = NewProperties(params);
        SetPropertySet(i->m_Properties, PROPERTY_LAYER_DEFAULT, script->m_PropertySet);
        luaL_getmetatable(L, SCRIPTINSTANCE);
        lua_setmetatable(L, -2);

        lua_pop(L, 1);

        lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_InstanceReference);
        dmScript::SetInstance(L);
        dmScript::InitializeInstance(i->m_ScriptWorld);
        lua_pushnil(L);
        dmScript::SetInstance(L);

        assert(top == lua_gettop(L));

        return i;
    }

    void DeleteScriptInstance(HScriptInstance script_instance)
    {
        HCollection collection = script_instance->m_Instance->m_Collection->m_HCollection;
        CancelAnimationCallbacks(collection, script_instance);

        lua_State* L = GetLuaState(script_instance);

        int top = lua_gettop(L);
        (void) top;

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
        dmScript::SetInstance(L);
        dmScript::FinalizeInstance(script_instance->m_ScriptWorld);
        lua_pushnil(L);
        dmScript::SetInstance(L);

        dmScript::Unref(L, LUA_REGISTRYINDEX, script_instance->m_ContextTableReference);
        dmScript::Unref(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
        dmScript::Unref(L, LUA_REGISTRYINDEX, script_instance->m_ScriptDataReference);

        DeleteProperties(script_instance->m_Properties);
        script_instance->~ScriptInstance();
        ResetScriptInstance(script_instance);

        assert(top == lua_gettop(L));
    }

#define CHECK_PROP_RESULT(key, type, expected_type, result)\
    if (result == PROPERTY_RESULT_OK) {\
        if (type != expected_type) {\
            dmLogError("The property '%s' must be of type '%s'.", key, TYPE_NAMES[expected_type]);\
            result = PROPERTY_RESULT_TYPE_MISMATCH;\
        }\
    }\
    if (result != PROPERTY_RESULT_OK) {\
        return result;\
    }

    PropertyResult PropertiesToLuaTable(HInstance instance, HScript script, const HProperties properties, lua_State* L, int index)
    {
        const PropertyDeclarations* declarations = &script->m_LuaModule->m_Properties;
        PropertyVar var;
        uint32_t count = declarations->m_NumberEntries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDeclarationEntry& entry = declarations->m_NumberEntries[i];
            PropertyResult result = GetProperty(properties, entry.m_Id, var);
            CHECK_PROP_RESULT(entry.m_Key, var.m_Type, PROPERTY_TYPE_NUMBER, result)
            lua_pushstring(L, entry.m_Key);
            lua_pushnumber(L, var.m_Number);
            lua_settable(L, index - 2);
        }
        count = declarations->m_HashEntries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDeclarationEntry& entry = declarations->m_HashEntries[i];
            PropertyResult result = GetProperty(properties, entry.m_Id, var);
            CHECK_PROP_RESULT(entry.m_Key, var.m_Type, PROPERTY_TYPE_HASH, result)
            lua_pushstring(L, entry.m_Key);
            dmScript::PushHash(L, var.m_Hash);
            lua_settable(L, index - 2);
        }
        count = declarations->m_UrlEntries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            /*
             * NOTE/TODO: var above is reused and URL::m_FunctionRef must
             * always be zero or a valid lua-reference. By reusing a union-type here, PropertyVar,
             * m_FunctionRef could have an invalid value. We could move PropertyVar var inside every
             * loop but the problem and risk is illustrated here.
             */
            var = PropertyVar();
            const PropertyDeclarationEntry& entry = declarations->m_UrlEntries[i];
            PropertyResult result = GetProperty(properties, entry.m_Id, var);
            CHECK_PROP_RESULT(entry.m_Key, var.m_Type, PROPERTY_TYPE_URL, result)
            lua_pushstring(L, entry.m_Key);
            dmMessage::URL* url = (dmMessage::URL*) var.m_URL;
            dmScript::PushURL(L, *url);
            lua_settable(L, index - 2);
        }
        count = declarations->m_Vector3Entries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDeclarationEntry& entry = declarations->m_Vector3Entries[i];
            PropertyResult result = GetProperty(properties, entry.m_Id, var);
            CHECK_PROP_RESULT(entry.m_Key, var.m_Type, PROPERTY_TYPE_VECTOR3, result)
            lua_pushstring(L, entry.m_Key);
            dmScript::PushVector3(L, Vector3(var.m_V4[0], var.m_V4[1], var.m_V4[2]));
            lua_settable(L, index - 2);
        }
        count = declarations->m_Vector4Entries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDeclarationEntry& entry = declarations->m_Vector4Entries[i];
            PropertyResult result = GetProperty(properties, entry.m_Id, var);
            CHECK_PROP_RESULT(entry.m_Key, var.m_Type, PROPERTY_TYPE_VECTOR4, result)
            lua_pushstring(L, entry.m_Key);
            dmScript::PushVector4(L, Vector4(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]));
            lua_settable(L, index - 2);
        }
        count = declarations->m_QuatEntries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDeclarationEntry& entry = declarations->m_QuatEntries[i];
            PropertyResult result = GetProperty(properties, entry.m_Id, var);
            CHECK_PROP_RESULT(entry.m_Key, var.m_Type, PROPERTY_TYPE_QUAT, result)
            lua_pushstring(L, entry.m_Key);
            dmScript::PushQuat(L, Quat(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]));
            lua_settable(L, index - 2);
        }
        count = declarations->m_BoolEntries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDeclarationEntry& entry = declarations->m_BoolEntries[i];
            PropertyResult result = GetProperty(properties, entry.m_Id, var);
            CHECK_PROP_RESULT(entry.m_Key, var.m_Type, PROPERTY_TYPE_BOOLEAN, result)
            lua_pushstring(L, entry.m_Key);
            lua_pushboolean(L, var.m_Bool);
            lua_settable(L, index - 2);
        }
        return PROPERTY_RESULT_OK;
    }

    // Documentation for the scripts

    /*# called when a script component is initialized
     * This is a callback-function, which is called by the engine when a script component is initialized. It can be used
     * to set the initial state of the script.
     *
     * @name init
     * @param self [type:object] reference to the script state to be used for storing data
     * @examples
     *
     * ```lua
     * function init(self)
     *     -- set up useful data
     *     self.my_value = 1
     * end
     * ```
     */

    /*# called when a script component is finalized
     * This is a callback-function, which is called by the engine when a script component is finalized (destroyed). It can
     * be used to e.g. take some last action, report the finalization to other game object instances, delete spawned objects
     * or release user input focus (see [ref:release_input_focus]).
     *
     * @name final
     * @param self [type:object] reference to the script state to be used for storing data
     * @examples
     *
     * ```lua
     * function final(self)
     *     -- report finalization
     *     msg.post("my_friend_instance", "im_dead", {my_stats = self.some_value})
     * end
     * ```
     */

    /*# called every frame to update the script component
     * This is a callback-function, which is called by the engine every frame to update the state of a script component.
     * It can be used to perform any kind of game related tasks, e.g. moving the game object instance.
     *
     * @name update
     * @param self [type:object] reference to the script state to be used for storing data
     * @param dt [type:number] the time-step of the frame update
     * @examples
     *
     * This example demonstrates how to move a game object instance through the script component:
     *
     * ```lua
     * function init(self)
     *     -- set initial velocity to be 1 along world x-axis
     *     self.my_velocity = vmath.vector3(1, 0, 0)
     * end
     *
     * function update(self, dt)
     *     -- move the game object instance
     *     go.set_position(go.get_position() + dt * self.my_velocity)
     * end
     * ```
     */

    /*# called at fixed intervals to update the script component
     *
     * This is a callback-function, which is called by the engine at fixed intervals to update the state of a script
     * component. The function will be called if 'Fixed Update Frequency' is enabled in the Engine section of game.project.
     * It can for instance be used to update game logic with the physics simulation if using a fixed timestep for the
     * physics (enabled by ticking 'Use Fixed Timestep' in the Physics section of game.project).
     *
     * @name fixed_update
     * @param self [type:object] reference to the script state to be used for storing data
     * @param dt [type:number] the time-step of the frame update
     * @examples
     */

    /*# called when a message has been sent to the script component
     *
     * This is a callback-function, which is called by the engine whenever a message has been sent to the script component.
     * It can be used to take action on the message, e.g. send a response back to the sender of the message.
     *
     * The `message` parameter is a table containing the message data. If the message is sent from the engine, the
     * documentation of the message specifies which data is supplied.
     *
     * @name on_message
     * @param self [type:object] reference to the script state to be used for storing data
     * @param message_id [type:hash] id of the received message
     * @param message [type:table] a table containing the message data
     * @param sender [type:url] address of the sender
     * @examples
     *
     * This example demonstrates how a game object instance, called "a", can communicate with another instance, called "b". It
     * is assumed that both script components of the instances has id "script".
     *
     * Script of instance "a":
     *
     * ```lua
     * function init(self)
     *     -- let b know about some important data
     *     msg.post("b#script", "my_data", {important_value = 1})
     * end
     * ```
     *
     * Script of instance "b":
     *
     * ```lua
     * function init(self)
     *     -- store the url of instance "a" for later use, by specifying nil as socket we
     *     -- automatically use our own socket
     *     self.a_url = msg.url(nil, go.get_id("a"), "script")
     * end
     *
     * function on_message(self, message_id, message, sender)
     *     -- check message and sender
     *     if message_id == hash("my_data") and sender == self.a_url then
     *         -- use the data in some way
     *         self.important_value = message.important_value
     *     end
     * end
     * ```
     */

    /*# called when user input is received
     *
     * This is a callback-function, which is called by the engine when user input is sent to the game object instance of the script.
     * It can be used to take action on the input, e.g. move the instance according to the input.
     *
     * For an instance to obtain user input, it must first acquire input focus
     * through the message `acquire_input_focus`.
     *
     * Any instance that has obtained input will be put on top of an
     * input stack. Input is sent to all listeners on the stack until the
     * end of stack is reached, or a listener returns `true`
     * to signal that it wants input to be consumed.
     *
     * See the documentation of [ref:acquire_input_focus] for more
     * information.
     *
     * The `action` parameter is a table containing data about the input mapped to the
     * `action_id`.
     * For mapped actions it specifies the value of the input and if it was just pressed or released.
     * Actions are mapped to input in an input_binding-file.
     *
     * Mouse movement is specifically handled and uses `nil` as its `action_id`.
     * The `action` only contains positional parameters in this case, such as x and y of the pointer.
     *
     * Here is a brief description of the available table fields:
     *
     * Field       | Description
     * ----------- | ----------------------------------------------------------
     * `value`     | The amount of input given by the user. This is usually 1 for buttons and 0-1 for analogue inputs. This is not present for mouse movement.
     * `pressed`   | If the input was pressed this frame. This is not present for mouse movement.
     * `released`  | If the input was released this frame. This is not present for mouse movement.
     * `repeated`  | If the input was repeated this frame. This is similar to how a key on a keyboard is repeated when you hold it down. This is not present for mouse movement.
     * `x`         | The x value of a pointer device, if present.
     * `y`         | The y value of a pointer device, if present.
     * `screen_x`  | The screen space x value of a pointer device, if present.
     * `screen_y`  | The screen space y value of a pointer device, if present.
     * `dx`        | The change in x value of a pointer device, if present.
     * `dy`        | The change in y value of a pointer device, if present.
     * `screen_dx` | The change in screen space x value of a pointer device, if present.
     * `screen_dy` | The change in screen space y value of a pointer device, if present.
     * `gamepad`   | The index of the gamepad device that provided the input.
     * `touch`     | List of touch input, one element per finger, if present. See table below about touch input
     *
     * Touch input table:
     *
     * Field       | Description
     * ----------- | ----------------------------------------------------------
     * `id`        | A number identifying the touch input during its duration.
     * `pressed`   | True if the finger was pressed this frame.
     * `released`  | True if the finger was released this frame.
     * `tap_count` | Number of taps, one for single, two for double-tap, etc
     * `x`         | The x touch location.
     * `y`         | The y touch location.
     * `dx`        | The change in x value.
     * `dy`        | The change in y value.
     * `acc_x`     | Accelerometer x value (if present).
     * `acc_y`     | Accelerometer y value (if present).
     * `acc_z`     | Accelerometer z value (if present).
     *
     * @name on_input
     * @param self [type:object] reference to the script state to be used for storing data
     * @param action_id [type:hash] id of the received input action, as mapped in the input_binding-file
     * @param action [type:table] a table containing the input data, see above for a description
     * @return consume [type:boolean|nil] optional boolean to signal if the input should be consumed (not passed on to others) or not, default is false
     * @examples
     *
     * This example demonstrates how a game object instance can be moved as a response to user input.
     *
     * ```lua
     * function init(self)
     *     -- acquire input focus
     *     msg.post(".", "acquire_input_focus")
     *     -- maximum speed the instance can be moved
     *     self.max_speed = 2
     *     -- velocity of the instance, initially zero
     *     self.velocity = vmath.vector3()
     * end
     *
     * function update(self, dt)
     *     -- move the instance
     *     go.set_position(go.get_position() + dt * self.velocity)
     * end
     *
     * function on_input(self, action_id, action)
     *     -- check for movement input
     *     if action_id == hash("right") then
     *         if action.released then -- reset velocity if input was released
     *             self.velocity = vmath.vector3()
     *         else -- update velocity
     *             self.velocity = vmath.vector3(action.value * self.max_speed, 0, 0)
     *         end
     *     end
     * end
     * ```
     */

    /*# called when the script component is reloaded
     *
     * This is a callback-function, which is called by the engine when the script component is reloaded, e.g. from the editor.
     * It can be used for live development, e.g. to tweak constants or set up the state properly for the instance.
     *
     * @name on_reload
     * @param self [type:object] reference to the script state to be used for storing data
     * @examples
     *
     * This example demonstrates how to tweak the speed of a game object instance that is moved on user input.
     *
     * ```lua
     * function init(self)
     *     -- acquire input focus
     *     msg.post(".", "acquire_input_focus")
     *     -- maximum speed the instance can be moved, this value is tweaked in the on_reload function below
     *     self.max_speed = 2
     *     -- velocity of the instance, initially zero
     *     self.velocity = vmath.vector3()
     * end
     *
     * function update(self, dt)
     *     -- move the instance
     *     go.set_position(go.get_position() + dt * self.velocity)
     * end
     *
     * function on_input(self, action_id, action)
     *     -- check for movement input
     *     if action_id == hash("right") then
     *         if action.released then -- reset velocity if input was released
     *             self.velocity = vmath.vector3()
     *         else -- update velocity
     *             self.velocity = vmath.vector3(action.value * self.max_speed, 0, 0)
     *         end
     *     end
     * end
     *
     * function on_reload(self)
     *     -- edit this value and reload the script component
     *     self.max_speed = 100
     * end
     * ```
     */

    /*# no playback
     *
     * @name go.PLAYBACK_NONE
     * @variable
     */
    /*# once forward
     *
     * @name go.PLAYBACK_ONCE_FORWARD
     * @variable
     */
    /*# once backward
     *
     * @name go.PLAYBACK_ONCE_BACKWARD
     * @variable
     */
    /*# once ping pong
     *
     * @name go.PLAYBACK_ONCE_PINGPONG
     * @variable
     */
    /*# loop forward
     *
     * @name go.PLAYBACK_LOOP_FORWARD
     * @variable
     */
    /*# loop backward
     *
     * @name go.PLAYBACK_LOOP_BACKWARD
     * @variable
     */
    /*# ping pong loop
     *
     * @name go.PLAYBACK_LOOP_PINGPONG
     * @variable
     */

    /*# linear interpolation
     *
     * @name go.EASING_LINEAR
     * @variable
     */
    /*# in-quadratic
     *
     * @name go.EASING_INQUAD
     * @variable
     */
    /*# out-quadratic
     *
     * @name go.EASING_OUTQUAD
     * @variable
     */
    /*# in-out-quadratic
     *
     * @name go.EASING_INOUTQUAD
     * @variable
     */
    /*# out-in-quadratic
     *
     * @name go.EASING_OUTINQUAD
     * @variable
     */
    /*# in-cubic
     *
     * @name go.EASING_INCUBIC
     * @variable
     */
    /*# out-cubic
     *
     * @name go.EASING_OUTCUBIC
     * @variable
     */
    /*# in-out-cubic
     *
     * @name go.EASING_INOUTCUBIC
     * @variable
     */
    /*# out-in-cubic
     *
     * @name go.EASING_OUTINCUBIC
     * @variable
     */
    /*# in-quartic
     *
     * @name go.EASING_INQUART
     * @variable
     */
    /*# out-quartic
     *
     * @name go.EASING_OUTQUART
     * @variable
     */
    /*# in-out-quartic
     *
     * @name go.EASING_INOUTQUART
     * @variable
     */
    /*# out-in-quartic
     *
     * @name go.EASING_OUTINQUART
     * @variable
     */
    /*# in-quintic
     *
     * @name go.EASING_INQUINT
     * @variable
     */
    /*# out-quintic
     *
     * @name go.EASING_OUTQUINT
     * @variable
     */
    /*# in-out-quintic
     *
     * @name go.EASING_INOUTQUINT
     * @variable
     */
    /*# out-in-quintic
     *
     * @name go.EASING_OUTINQUINT
     * @variable
     */
    /*# in-sine
     *
     * @name go.EASING_INSINE
     * @variable
     */
    /*# out-sine
     *
     * @name go.EASING_OUTSINE
     * @variable
     */
    /*# in-out-sine
     *
     * @name go.EASING_INOUTSINE
     * @variable
     */
    /*# out-in-sine
     *
     * @name go.EASING_OUTINSINE
     * @variable
     */
    /*# in-exponential
     *
     * @name go.EASING_INEXPO
     * @variable
     */
    /*# out-exponential
     *
     * @name go.EASING_OUTEXPO
     * @variable
     */
    /*# in-out-exponential
     *
     * @name go.EASING_INOUTEXPO
     * @variable
     */
    /*# out-in-exponential
     *
     * @name go.EASING_OUTINEXPO
     * @variable
     */
    /*# in-circlic
     *
     * @name go.EASING_INCIRC
     * @variable
     */
    /*# out-circlic
     *
     * @name go.EASING_OUTCIRC
     * @variable
     */
    /*# in-out-circlic
     *
     * @name go.EASING_INOUTCIRC
     * @variable
     */
    /*# out-in-circlic
     *
     * @name go.EASING_OUTINCIRC
     * @variable
     */
    /*# in-elastic
     *
     * @name go.EASING_INELASTIC
     * @variable
     */
    /*# out-elastic
     *
     * @name go.EASING_OUTELASTIC
     * @variable
     */
    /*# in-out-elastic
     *
     * @name go.EASING_INOUTELASTIC
     * @variable
     */
    /*# out-in-elastic
     *
     * @name go.EASING_OUTINELASTIC
     * @variable
     */
    /*# in-back
     *
     * @name go.EASING_INBACK
     * @variable
     */
    /*# out-back
     *
     * @name go.EASING_OUTBACK
     * @variable
     */
    /*# in-out-back
     *
     * @name go.EASING_INOUTBACK
     * @variable
     */
    /*# out-in-back
     *
     * @name go.EASING_OUTINBACK
     * @variable
     */
    /*# in-bounce
     *
     * @name go.EASING_INBOUNCE
     * @variable
     */
    /*# out-bounce
     *
     * @name go.EASING_OUTBOUNCE
     * @variable
     */
    /*# in-out-bounce
     *
     * @name go.EASING_INOUTBOUNCE
     * @variable
     */
    /*# out-in-bounce
     *
     * @name go.EASING_OUTINBOUNCE
     * @variable
     */
}
