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

#include <script/script.h>

#include "gameobject.h"
#include "gameobject_script.h"
#include "gameobject_script_util.h"
#include "gameobject_private.h"
#include "gameobject_props_lua.h"

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

    using namespace dmPropertiesDDF;

    const char* SCRIPT_INSTANCE_TYPE_NAME = SCRIPTINSTANCE;

    const char* SCRIPT_FUNCTION_NAMES[MAX_SCRIPT_FUNCTION_COUNT] =
    {
        "init",
        "final",
        "update",
        "on_message",
        "on_input",
        "on_reload"
    };

    HRegister g_Register = 0;

    ScriptWorld::ScriptWorld()
    : m_Instances()
    {
        // TODO: How to configure? It should correspond to collection instance count
        m_Instances.SetCapacity(1024);
    }

    static Script* GetScript(lua_State *L)
    {
        int top = lua_gettop(L);
        (void)top;
        Script* script = 0x0;
        dmScript::GetInstance(L);
        if (dmScript::IsUserType(L, -1, SCRIPT))
        {
            script = (Script*)lua_touserdata(L, -1);
        }
        // Clear stack and return
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return script;
    }

    static int ScriptGetURL(lua_State* L)
    {
        dmMessage::URL url;
        dmMessage::ResetURL(url);
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
        return (ScriptInstance*)dmScript::CheckUserType(L, index, SCRIPTINSTANCE, "You can only access go.* functions and values from a script instance (.script file)");
    }

    static ScriptInstance* ScriptInstance_Check(lua_State *L)
    {
        dmScript::GetInstance(L);
        ScriptInstance* i = ScriptInstance_Check(L, -1);
        lua_pop(L, 1);
        return i;
    }

    static int ScriptInstance_gc (lua_State *L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        memset(i, 0, sizeof(*i));
        (void) i;
        assert(i);
        return 0;
    }

    static int ScriptInstance_tostring (lua_State *L)
    {
        lua_pushfstring(L, "Script: %p", lua_touserdata(L, 1));
        return 1;
    }

    static int ScriptInstance_index(lua_State *L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
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

        ScriptInstance* i = ScriptInstance_Check(L, 1);
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
        out_url->m_Function = 0;
        out_url->m_Socket = instance->m_Collection->m_ComponentSocket;
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
        url.m_Function = 0;
        url.m_Socket = instance->m_Collection->m_ComponentSocket;
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
        if (i != 0x0 && i->m_ContextTableReference != LUA_NOREF)
        {
            lua_pushnumber(L, i->m_ContextTableReference);
        }
        else
        {
            lua_pushnil(L);
        }

        return 1;
    }

    static const luaL_reg ScriptInstance_methods[] =
    {
        {0,0}
    };

    static const luaL_reg ScriptInstance_meta[] =
    {
        {"__gc",                                        ScriptInstance_gc},
        {"__tostring",                                  ScriptInstance_tostring},
        {"__index",                                     ScriptInstance_index},
        {"__newindex",                                  ScriptInstance_newindex},
        {dmScript::META_TABLE_GET_URL,                  ScriptInstanceGetURL},
        {dmScript::META_TABLE_GET_USER_DATA,            ScriptInstanceGetUserData},
        {dmScript::META_TABLE_RESOLVE_PATH,             ScriptInstanceResolvePath},
        {dmScript::META_TABLE_IS_VALID,                 ScriptInstanceIsValid},
        {dmScript::META_GET_INSTANCE_CONTEXT_TABLE_REF, ScriptGetInstanceContextTableRef},
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
        if (lua_gettop(L) == instance_arg) {
            dmMessage::URL receiver;
            dmScript::ResolveURL(L, instance_arg, &receiver, 0x0);
            if (receiver.m_Socket != dmGameObject::GetMessageSocket(i->m_Instance->m_Collection))
            {
                luaL_error(L, "function called can only access instances within the same collection.");
            }

            instance = GetInstanceFromIdentifier(instance->m_Collection, receiver.m_Path);
            if (!instance)
            {
                luaL_error(L, "Instance %s not found", lua_tostring(L, instance_arg));
                return 0; // Actually never reached
            }
        }
        return instance;
    }

    static Result GetComponentUserData(HInstance instance, dmhash_t component_id, uint32_t* component_type, uintptr_t* user_data)
    {
        // TODO: We should probably not store user-data sparse.
        // A lot of loops just to find user-data such as the code below
        assert(instance != 0x0);
        const dmArray<Prototype::Component>& components = instance->m_Prototype->m_Components;
        uint32_t n = components.Size();
        uint32_t component_instance_data = 0;
        for (uint32_t i = 0; i < n; ++i)
        {
            const Prototype::Component* component = &components[i];
            if (component->m_Id == component_id)
            {
                if (component->m_Type->m_InstanceHasUserData)
                {
                    *user_data = instance->m_ComponentInstanceUserData[component_instance_data];
                }
                else
                {
                    *user_data = 0;
                }
                *component_type = component->m_TypeIndex;
                return RESULT_OK;
            }

            if (component->m_Type->m_InstanceHasUserData)
            {
                component_instance_data++;
            }
        }

        return RESULT_COMPONENT_NOT_FOUND;
    }

    void GetComponentUserDataFromLua(lua_State* L, int index, HCollection collection, const char* component_ext, uintptr_t* user_data, dmMessage::URL* url, void** world)
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

            uint32_t component_type_index;
            dmGameObject::Result result = GetComponentUserData(instance, receiver.m_Fragment, &component_type_index, user_data);
            if ((component_ext != 0x0 || user_data != 0x0) && result != dmGameObject::RESULT_OK)
            {
                luaL_error(L, "The component could not be found");
                return; // Actually never reached
            }

            if (world != 0) {
                *world = GetWorld(instance->m_Collection, component_type_index);
            }

            if (component_ext != 0x0)
            {
                dmResource::ResourceType resource_type;
                dmResource::Result resource_res = dmResource::GetTypeFromExtension(instance->m_Collection->m_Factory, component_ext, &resource_type);
                if (resource_res != dmResource::RESULT_OK)
                {
                    luaL_error(L, "Component type '%s' not found", component_ext);
                    return; // Actually never reached
                }
                ComponentType* type = &instance->m_Collection->m_Register->m_ComponentTypes[component_type_index];
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

    HCollection GetCollectionFromURL(const dmMessage::URL& url)
    {
        HCollection* collection = g_Register->m_SocketToCollection.Get(url.m_Socket);
        return collection ? *collection : 0;
    }

    void* GetComponentFromURL(const dmMessage::URL& url)
    {
        HCollection collection = GetCollectionFromURL(url);
        if (!collection) {
            return 0;
        }

        Instance** instance = collection->m_IDToInstance.Get(url.m_Path);
        if (!instance) {
            return 0;
        }

        uintptr_t user_data;
        uint32_t type_index;
        // For loop over all components in the instance
        dmGameObject::GetComponentUserData(*instance, url.m_Fragment, &type_index, &user_data);

        void* world = collection->m_ComponentWorlds[type_index];

        ComponentType* type = &g_Register->m_ComponentTypes[type_index];
        if (!type->m_GetFunction) {
            return 0;
        }
        ComponentGetParams params = {world, &user_data};
        return type->m_GetFunction(params);
    }

    HInstance GetInstanceFromLua(lua_State* L) {
        uintptr_t user_data;
        if (dmScript::GetUserData(L, &user_data, SCRIPTINSTANCE)) {
            return (HInstance)user_data;
        } else {
            return 0;
        }
    }

    /*# gets a named property of the specified game object or component
     *
     * @name go.get
     * @param url [type:string|hash|url] url of the game object or component having the property
     * @param property [type:string|hash] id of the property to retrieve
     * @return value [type:any] the value of the specified property
     * @examples
     *
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
     */
    int Script_Get(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        Instance* instance = i->m_Instance;
        dmMessage::URL sender;
        dmScript::GetURL(L, &sender);
        dmMessage::URL target;
        dmScript::ResolveURL(L, 1, &target, &sender);
        if (target.m_Socket != dmGameObject::GetMessageSocket(i->m_Instance->m_Collection))
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
            return luaL_error(L, "Could not find any instance with id '%s'.", dmHashReverseSafe64(target.m_Path));
        dmGameObject::PropertyDesc property_desc;
        dmGameObject::PropertyResult result = dmGameObject::GetProperty(target_instance, target.m_Fragment, property_id, property_desc);
        switch (result)
        {
        case dmGameObject::PROPERTY_RESULT_OK:
            dmGameObject::LuaPushVar(L, property_desc.m_Variant);
            return 1;
        case dmGameObject::PROPERTY_RESULT_NOT_FOUND:
            {
                // The supplied URL parameter don't need to be a string,
                // we let Lua handle the "conversion" to string using concatenation.
                lua_pushliteral(L, "");
                lua_pushvalue(L, 1);
                lua_concat(L, 2);
                const char* name = lua_tostring(L, -1);
                lua_pop(L, 1);
                return luaL_error(L, "'%s' does not have any property called '%s'", name, dmHashReverseSafe64(property_id));
            }
        case dmGameObject::PROPERTY_RESULT_COMP_NOT_FOUND:
            return luaL_error(L, "could not find component '%s' when resolving '%s'", dmHashReverseSafe64(target.m_Fragment), lua_tostring(L, 1));
        default:
            // Should never happen, programmer error
            return luaL_error(L, "go.get failed with error code %d", result);
        }
    }

    static const char* GetPropertyTypeName(PropertyType type)
    {
        switch (type)
        {
        case PROPERTY_TYPE_NUMBER:
            return "number";
        case PROPERTY_TYPE_HASH:
            return "hash";
        case PROPERTY_TYPE_URL:
            return "msg.url";
        case PROPERTY_TYPE_VECTOR3:
            return "vmath.vector3";
        case PROPERTY_TYPE_VECTOR4:
            return "vmath.vector4";
        case PROPERTY_TYPE_QUAT:
            return "vmath.quat";
        case PROPERTY_TYPE_BOOLEAN:
            return "boolean";
        default:
            return "unknown";
        }
    }

    /*# sets a named property of the specified game object or component
     *
     * @name go.set
     * @param url [type:string|hash|url] url of the game object or component having the property
     * @param property [type:string|hash] id of the property to set
     * @param value [type:any] the value to set
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
     */
    int Script_Set(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        Instance* instance = i->m_Instance;
        dmMessage::URL sender;
        dmScript::GetURL(L, &sender);
        dmMessage::URL target;
        dmScript::ResolveURL(L, 1, &target, &sender);
        if (target.m_Socket != dmGameObject::GetMessageSocket(i->m_Instance->m_Collection))
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
        dmGameObject::PropertyVar property_var;
        dmGameObject::HInstance target_instance = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(instance), target.m_Path);
        if (target_instance == 0)
            return luaL_error(L, "could not find any instance with id '%s'.", dmHashReverseSafe64(target.m_Path));
        dmGameObject::PropertyResult result = dmGameObject::LuaToVar(L, 3, property_var);
        if (result == PROPERTY_RESULT_OK)
        {
            result = dmGameObject::SetProperty(target_instance, target.m_Fragment, property_id, property_var);
        }
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
                return luaL_error(L, "'%s' does not have any property called '%s'", name, dmHashReverseSafe64(property_id));
            }
        case PROPERTY_RESULT_UNSUPPORTED_TYPE:
        case PROPERTY_RESULT_TYPE_MISMATCH:
            {
                dmGameObject::PropertyDesc property_desc;
                dmGameObject::GetProperty(target_instance, target.m_Fragment, property_id, property_desc);
                return luaL_error(L, "the property '%s' of '%s' must be a %s", dmHashReverseSafe64(property_id), lua_tostring(L, 1), GetPropertyTypeName(property_desc.m_Variant.m_Type));
            }
        case dmGameObject::PROPERTY_RESULT_COMP_NOT_FOUND:
            return luaL_error(L, "could not find component '%s' when resolving '%s'", dmHashReverseSafe64(target.m_Fragment), lua_tostring(L, 1));
        case dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE:
            return luaL_error(L, "go.set failed because the value is unsupported");
        case dmGameObject::PROPERTY_RESULT_UNSUPPORTED_OPERATION:
            return luaL_error(L, "could not perform unsupported operation on '%s'", dmHashReverseSafe64(property_id));
        default:
            // Should never happen, programmer error
            return luaL_error(L, "go.set failed with error code %d", result);
        }
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
        dmScript::PushVector3(L, Vectormath::Aos::Vector3(dmGameObject::GetPosition(instance)));
        return 1;
    }

    /*# gets the rotation of the game object instance
     * The rotation is relative to the parent (if any). Use [ref:go.get_world_rotation] to retrieve the global world position.
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
    int Script_GetScale(lua_State* L)
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
    int Script_GetScaleVector(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        dmScript::PushVector3(L, dmGameObject::GetScale(instance));
        return 1;
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
        Vectormath::Aos::Vector3* v = dmScript::CheckVector3(L, 1);
        dmGameObject::SetPosition(instance, Vectormath::Aos::Point3(*v));
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
        Vectormath::Aos::Quat* q = dmScript::CheckQuat(L, 1);
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
        if (dmScript::IsVector3(L, 1))
        {
            Vector3 scale = *dmScript::CheckVector3(L, 1);
            if (scale.getX() <= 0.0f || scale.getY() <= 0.0f || scale.getZ() <= 0.0f)
            {
                return luaL_error(L, "Vector passed to go.set_scale contains components that are below or equal to zero");
            }
            dmGameObject::SetScale(instance, scale);
            return 0;
        }

        lua_Number v = luaL_checknumber(L, 1);
        if (v <= 0.0)
        {
            return luaL_error(L, "The scale supplied to go.set_scale must be greater than 0.");
        }
        dmGameObject::SetScale(instance, (float)v);
        return 0;
    }

    /*# gets the game object instance world position
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
        dmScript::PushVector3(L, Vectormath::Aos::Vector3(dmGameObject::GetWorldPosition(instance)));
        return 1;
    }

    /*# gets the game object instance world rotation
     * Use <code>go.get_rotation</code> to retrieve the rotation relative to the parent.
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
     * Use <code>go.get_scale</code> to retrieve the 3D scale factor relative to the parent.
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
     * Use <code>go.get_scale_uniform</code> to retrieve the scale factor relative to the parent.
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
        ScriptInstance* script_instance = (ScriptInstance*)curve->userdata1;
        lua_State* L = GetLuaState(script_instance);

        int top = lua_gettop(L);
        (void) top;

        int ref = (int) (((uintptr_t) curve->userdata2) & 0xffffffff);
        dmScript::Unref(L, LUA_REGISTRYINDEX, ref);

        curve->release_callback = 0x0;
        curve->userdata1 = 0x0;
        curve->userdata2 = 0x0;

        assert(top == lua_gettop(L));
    }

    void LuaAnimationStopped(dmGameObject::HInstance instance, dmhash_t component_id, dmhash_t property_id,
                                        bool finished, void* userdata1, void* userdata2)
    {
        dmScript::LuaCallbackInfo* cbk = (dmScript::LuaCallbackInfo*)userdata1;
        if (dmScript::IsValidCallback(cbk))
        {
            lua_State* L = cbk->m_L;
            DM_LUA_STACK_CHECK(L, 0);

            dmMessage::URL url;
            url.m_Socket = instance->m_Collection->m_ComponentSocket;
            url.m_Path = instance->m_Identifier;
            url.m_Fragment = component_id;

            if (finished)
            {
                struct Args
                {
                    Args(dmMessage::URL url, dmhash_t property_id)
                        : m_URL(url), m_PropertyId(property_id)
                    {}
                    dmMessage::URL m_URL;
                    dmhash_t m_PropertyId;

                    static void LuaCallbackCustomArgs(lua_State* L, void* user_args)
                    {
                        Args* args = (Args*)user_args;
                        dmScript::PushURL(L, args->m_URL);
                        dmScript::PushHash(L, args->m_PropertyId);
                    }
                };
                
                Args args(url, property_id);
                dmScript::InvokeCallback(cbk, Args::LuaCallbackCustomArgs, &args);
            }
        }
        dmScript::DeleteCallback(cbk);
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
     * See the <a href="/manuals/properties">properties guide</a> for which properties can be animated and the <a href="/manuals/animation">animation guide</a> for how to animate them.
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
     * @param easing [type:constant|vector] easing to use during animation. Either specify a constant, see the <a href="/manuals/animation">animation guide</a> for a complete list, or a vmath.vector with a curve
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
            return luaL_error(L, "Could not find any instance with id '%s'.", dmHashReverseSafe64(target.m_Path));
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
            curve.userdata1 = i;
            curve.userdata2 = (void*)dmScript::Ref(L, LUA_REGISTRYINDEX);
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
                return luaL_error(L, "'%s' does not have any property called '%s'", name, dmHashReverseSafe64(property_id));
            }
        case PROPERTY_RESULT_UNSUPPORTED_TYPE:
        case PROPERTY_RESULT_TYPE_MISMATCH:
            {
                lua_pushliteral(L, "");
                dmScript::PushURL(L, target);
                lua_concat(L, 2);
                const char* name = lua_tostring(L, -1);
                lua_pop(L, 1);
                return luaL_error(L, "The property '%s' of '%s' has incorrect type", dmHashReverseSafe64(property_id), name);
            }
        case dmGameObject::PROPERTY_RESULT_COMP_NOT_FOUND:
            return luaL_error(L, "could not find component '%s' when resolving '%s'", dmHashReverseSafe64(target.m_Fragment), lua_tostring(L, 1));
        case dmGameObject::PROPERTY_RESULT_UNSUPPORTED_OPERATION:
            {
                lua_pushliteral(L, "");
                dmScript::PushURL(L, target);
                lua_concat(L, 2);
                const char* name = lua_tostring(L, -1);
                lua_pop(L, 1);
                return luaL_error(L, "Animation of the property '%s' of '%s' is unsupported", dmHashReverseSafe64(property_id), name);
            }
        default:
            // Should never happen, programmer error
            return luaL_error(L, "go.animate failed with error code %d", result);
        }

        assert(lua_gettop(L) == top);
        return 0;
    }

    /*# cancels all animations of the named property of the specified game object or component
     *
     * By calling this function, all stored animations of the given property will be canceled.
     *
     * See the <a href="/manuals/properties">properties guide</a> for which properties can be animated and the <a href="/manuals/animation">animation guide</a> for how to animate them.
     *
     * @name go.cancel_animations
     * @param url [type:string|hash|url] url of the game object or component having the property
     * @param property [type:string|hash] ide of the property to animate
     * @examples
     *
     * Cancel the animation of the position of a game object:
     *
     * ```lua
     * go.cancel_animations(go.get_id(), "position")
     * ```
     */
    int Script_CancelAnimations(lua_State* L)
    {
        int top = lua_gettop(L);
        (void)top;

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
            return luaL_error(L, "Could not find any instance with id '%s'.", dmHashReverseSafe64(target.m_Path));
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
                return luaL_error(L, "'%s' does not have any property called '%s'", name, dmHashReverseSafe64(property_id));
            }
        case PROPERTY_RESULT_UNSUPPORTED_TYPE:
        case PROPERTY_RESULT_TYPE_MISMATCH:
            {
                dmGameObject::PropertyDesc property_desc;
                dmGameObject::GetProperty(target_instance, target.m_Fragment, property_id, property_desc);
                return luaL_error(L, "The property '%s' must be of a numerical type", dmHashReverseSafe64(property_id));
            }
        case dmGameObject::PROPERTY_RESULT_COMP_NOT_FOUND:
            return luaL_error(L, "could not find component '%s' when resolving '%s'", dmHashReverseSafe64(target.m_Fragment), lua_tostring(L, 1));
        default:
            // Should never happen, programmer error
            return luaL_error(L, "go.cancel_animations failed with error code %d", res);
        }

        assert(lua_gettop(L) == top);
        return 0;
    }


    static int DeleteGOTable(lua_State* L, bool recursive)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        Instance* instance = i->m_Instance;

        // read table
        lua_pushnil(L);
        while (lua_next(L, 1)) {

            // value should be hashes
            dmMessage::URL receiver;
            dmScript::ResolveURL(L, -1, &receiver, 0x0);
            if (receiver.m_Socket != dmGameObject::GetMessageSocket(i->m_Instance->m_Collection))
            {
                luaL_error(L, "Function called can only access instances within the same collection.");
            }

            Instance *todelete = GetInstanceFromIdentifier(instance->m_Collection, receiver.m_Path);
            if (todelete)
            {
                if(dmGameObject::IsBone(todelete))
                {
                    return luaL_error(L, "Can not delete subinstances of spine or model components. '%s'", dmHashReverseSafe64(dmGameObject::GetIdentifier(todelete)));
                }
                dmGameObject::HCollection collection = todelete->m_Collection;
                dmGameObject::Delete(collection, todelete, recursive);
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
     * Delete one or more game objects identified by id.
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
    int Script_Delete(lua_State* L)
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
            return luaL_error(L, "Can not delete subinstances of spine or model components. '%s'", dmHashReverseSafe64(dmGameObject::GetIdentifier(instance)));
        }
        dmGameObject::HCollection collection = instance->m_Collection;
        dmGameObject::Delete(collection, instance, recursive);
        return 0;
    }

    /* deletes a set of game object instance
     * Delete all game objects simultaneously as listed in table.
     * The table values (not keys) should be game object ids (hashes).
     * Note: Deprecated, use go.delete instead.
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
    int Script_DeleteAll(lua_State* L)
    {
        const int top = lua_gettop(L);
        if (lua_gettop(L) != 1 || !lua_istable(L, 1)) {
            dmLogWarning("go.delete_all() needs a table as its first argument");
            return 0;
        }
        int result = DeleteGOTable(L, false);
        if(result == 0)
        {
            assert(top == lua_gettop(L));
        }
        return result;
    }

    /* OMITTED FROM API DOCS!
     * constructs a ray in world space from a position in screen space
     *
     * [icon:alert] Do not use this function, WIP!
     *
     * @name go.screen_ray
     * @param x [type:number] x-coordinate of the screen space position
     * @param y [type:number] y-coordinate of the screen space position
     * @return position [type:vector3] of the ray in world-space
     * @return direction [type:vector3] of the ray in world space
     */
    int Script_ScreenRay(lua_State* L)
    {
        lua_Number x = luaL_checknumber(L, 1);
        lua_Number y = luaL_checknumber(L, 2);
        // TODO: This temporarily assumes the worldspace is simply screen space
        // Should be fixed in a more robust way.
        Vector3 p((float) x, (float) y, 1.0f);
        Vector3 d(0.0f, 0.0f, -1.0f);
        dmScript::PushVector3(L, p);
        dmScript::PushVector3(L, d);
        return 2;
    }

    /*# define a property for the script
     * This function defines a property which can then be used in the script through the self-reference.
     * The properties defined this way are automatically exposed in the editor in game objects and collections which use the script.
     * Note that you can only use this function outside any callback-functions like init and update.
     *
     * @name go.property
     * @param name [type:string] the id of the property
     * @param value [type:number|hash|url|vector3|vector4|quaternion] default value of the property. In the case of a url, only the empty constructor msg.url() is allowed
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
        else if (dmScript::IsVector3(L, 2))
        {
            valid_type = true;
        }
        else if (dmScript::IsVector4(L, 2))
        {
            valid_type = true;
        }
        else if (dmScript::IsQuat(L, 2))
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

    static const luaL_reg GO_methods[] =
    {
        {"get",                     Script_Get},
        {"set",                     Script_Set},
        {"get_position",            Script_GetPosition},
        {"get_rotation",            Script_GetRotation},
        {"get_scale",               Script_GetScale},
        {"get_scale_vector",        Script_GetScaleVector},
        {"get_scale_uniform",       Script_GetScaleUniform},
        {"set_position",            Script_SetPosition},
        {"set_rotation",            Script_SetRotation},
        {"set_scale",               Script_SetScale},
        {"get_world_position",      Script_GetWorldPosition},
        {"get_world_rotation",      Script_GetWorldRotation},
        {"get_world_scale",         Script_GetWorldScale},
        {"get_world_scale_uniform", Script_GetWorldScaleUniform},
        {"get_id",                  Script_GetId},
        {"animate",                 Script_Animate},
        {"cancel_animations",       Script_CancelAnimations},
        {"delete",                  Script_Delete},
        {"delete_all",              Script_DeleteAll},
        {"screen_ray",              Script_ScreenRay},
        {"property",                Script_Property},
        {0, 0}
    };

    void InitializeScript(HRegister regist, dmScript::HContext context)
    {
        g_Register = regist;

        lua_State* L = dmScript::GetLuaState(context);

        int top = lua_gettop(L);
        (void)top;

        dmScript::RegisterUserType(L, SCRIPT, Script_methods, Script_meta);

        dmScript::RegisterUserType(L, SCRIPTINSTANCE, ScriptInstance_methods, ScriptInstance_meta);

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

    HScriptInstance NewScriptInstance(HScript script, HInstance instance, uint16_t component_index)
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

        assert(top == lua_gettop(L));

        return i;
    }

    void DeleteScriptInstance(HScriptInstance script_instance)
    {
        HCollection collection = script_instance->m_Instance->m_Collection;
        CancelAnimationCallbacks(collection, script_instance);

        lua_State* L = GetLuaState(script_instance);

        int top = lua_gettop(L);
        (void) top;

        dmScript::Unref(L, LUA_REGISTRYINDEX, script_instance->m_ContextTableReference);
        dmScript::Unref(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
        dmScript::Unref(L, LUA_REGISTRYINDEX, script_instance->m_ScriptDataReference);

        DeleteProperties(script_instance->m_Properties);
        script_instance->~ScriptInstance();
        ResetScriptInstance(script_instance);

        assert(top == lua_gettop(L));
    }

const char* TYPE_NAMES[PROPERTY_TYPE_COUNT] = {
        "number", // PROPERTY_TYPE_NUMBER
        "hash", // PROPERTY_TYPE_HASH
        "msg.url", // PROPERTY_TYPE_URL
        "vmath.vector3", // PROPERTY_TYPE_VECTOR3
        "vmath.vector4", // PROPERTY_TYPE_VECTOR4
        "vmath.quat", // PROPERTY_TYPE_QUAT
        "boolean", // PROPERTY_TYPE_BOOLEAN
};

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
             * NOTE/TODO: var above is reused and URL::m_Function must
             * always be zero or a valid lua-reference. By reusing a union-type here, PropertyVar,
             * m_Function could have an invalid value. We could move PropertyVar var inside every
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
     * `pressed`   | If the input was pressed this frame, 0 for false and 1 for true. This is not present for mouse movement.
     * `released`  | If the input was released this frame, 0 for false and 1 for true. This is not present for mouse movement.
     * `repeated`  | If the input was repeated this frame, 0 for false and 1 for true. This is similar to how a key on a keyboard is repeated when you hold it down. This is not present for mouse movement.
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
     * @return [consume] [type:boolean] optional boolean to signal if the input should be consumed (not passed on to others) or not, default is false
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
