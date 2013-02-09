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
#include "gameobject_private.h"

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
#define SCRIPTINSTANCE "ScriptInstance"

    using namespace dmPropertiesDDF;

    const char* SCRIPT_FUNCTION_NAMES[MAX_SCRIPT_FUNCTION_COUNT] =
    {
        "init",
        "final",
        "update",
        "on_message",
        "on_input",
        "on_reload"
    };

    lua_State* g_LuaState = 0;
    dmScript::HContext g_ScriptContext = 0;

    ScriptWorld::ScriptWorld()
    : m_Instances()
    {
        // TODO: How to configure? It should correspond to collection instance count
        m_Instances.SetCapacity(1024);
    }

    static Script* GetScript(lua_State *L)
    {
        lua_pushliteral(L, SCRIPT_NAME);
        lua_rawget(L, LUA_GLOBALSINDEX);
        Script* script = (Script*)lua_touserdata(L, -1);
        lua_pop(L, 1);
        return script;
    }

    static ScriptInstance* GetScriptInstance(lua_State *L)
    {
        lua_pushliteral(L, SCRIPT_INSTANCE_NAME);
        lua_rawget(L, LUA_GLOBALSINDEX);
        ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, -1);
        lua_pop(L, 1);
        return i;
    }

    static ScriptInstance* ScriptInstance_Check(lua_State *L, int index)
    {
        ScriptInstance* i;
        luaL_checktype(L, index, LUA_TUSERDATA);
        i = (ScriptInstance*)luaL_checkudata(L, index, SCRIPTINSTANCE);
        if (i == NULL) luaL_typerror(L, index, SCRIPTINSTANCE);
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
        lua_pushfstring(L, "GameObject: %p", lua_touserdata(L, 1));
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

    static const luaL_reg ScriptInstance_methods[] =
    {
        {0,0}
    };

    static const luaL_reg ScriptInstance_meta[] =
    {
        {"__gc",        ScriptInstance_gc},
        {"__tostring",  ScriptInstance_tostring},
        {"__index",     ScriptInstance_index},
        {"__newindex",  ScriptInstance_newindex},
        {0, 0}
    };

    static ScriptInstance* ScriptInstance_Check(lua_State *L)
    {
        ScriptInstance* i = GetScriptInstance(L);
        if (i == NULL) luaL_error(L, "Lua state did not contain any '%s'.", SCRIPT_INSTANCE_NAME);
        return i;
    }

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
            dmMessage::URL sender;
            if (dmScript::GetURL(L, &sender))
            {
                if (sender.m_Socket != dmGameObject::GetMessageSocket(i->m_Instance->m_Collection))
                {
                    luaL_error(L, "function called can only access instances within the same collection.");
                }

                dmMessage::URL receiver;
                dmScript::ResolveURL(L, instance_arg, &receiver, &sender);
                instance = GetInstanceFromIdentifier(instance->m_Collection, receiver.m_Path);
                if (!instance)
                {
                    luaL_error(L, "Instance %s not found", lua_tostring(L, instance_arg));
                    return 0; // Actually never reached
                }
            }
            else
            {
                luaL_error(L, "function called is not available from this script-type.");
                return 0; // Actually never reached
            }
        }
        return instance;
    }

    /*# gets the position of the instance
     * The position is relative the parent (if any). Use <code>go.get_world_position</code> to retrieve the global world position.
     *
     * @name go.get_position
     * @param [id] optional id of the instance to get the position for. Default is the "self" instance from the calling script (hash|string|url)
     * @return instance position (vector3)
     */
    int Script_GetPosition(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        dmScript::PushVector3(L, Vectormath::Aos::Vector3(dmGameObject::GetPosition(instance)));
        return 1;
    }

    /*# gets the rotation of the instance
     * The rotation is relative to the parent (if any). Use <code>go.get_world_rotation</code> to retrieve the global world position.
     *
     * @name go.get_rotation
     * @param [id] optional id of the instance to get the rotation for. Default is the "self" instance from the calling script (hash|string|url)
     * @return instance rotation (quaternion)
     */
    int Script_GetRotation(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        dmScript::PushQuat(L, dmGameObject::GetRotation(instance));
        return 1;
    }

    /*# gets the uniform scale factor of the instance
     * The uniform scale is relative the parent (if any). Use <code>go.get_world_scale</code> to retrieve the global world scale factor.
     *
     * @name go.get_scale
     * @param [id] optional id of the instance to get the scale for. Default is the "self" instance from the calling script (hash|string|url)
     * @return uniform instance scale factor (number)
     */
    int Script_GetScale(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        lua_pushnumber(L, dmGameObject::GetScale(instance));
        return 1;
    }

    /*# sets the position of the instance
     * The position is relative to the parent (if any). The global world position cannot be manually set.
     *
     * @name go.set_position
     * @param position position to set (vector3)
     * @param [id] optional id of the instance to set the position for. Default is the "self" instance from the calling script (hash|string|url)
     */
    int Script_SetPosition(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 2);
        Vectormath::Aos::Vector3* v = dmScript::CheckVector3(L, 1);
        dmGameObject::SetPosition(instance, Vectormath::Aos::Point3(*v));
        return 0;
    }

    /*# sets the rotation of the instance
     * The rotation is relative to the parent (if any). The global world rotation cannot be manually set.
     *
     * @name go.set_rotation
     * @param rotation rotation to set (quaternion)
     * @param [id] optional id of the instance to get the rotation for. Default is the "self" instance from the calling script (hash|string|url)
     */
    int Script_SetRotation(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 2);
        Vectormath::Aos::Quat* q = dmScript::CheckQuat(L, 1);
        dmGameObject::SetRotation(instance, *q);
        return 0;
    }

    /*# sets the uniform scale factor of the instance
     * The scale factor is relative to the parent (if any). The global world scale factor cannot be manually set.
     *
     * NOTE! Physics are currently not affected when setting scale from this function.
     *
     * @name go.set_scale
     * @param scale uniform scale factor, must be greater than 0 (number)
     * @param [id] optional id of the instance to get the scale for. Default is the "self" instance from the calling script (hash|string|url)
     */
    int Script_SetScale(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 2);
        lua_Number v = luaL_checknumber(L, 1);
        if (v <= 0.0)
        {
            return luaL_error(L, "The scale supplied to go.set_scale must be greater than 0.");
        }
        dmGameObject::SetScale(instance, (float)v);
        return 0;
    }

    /*# gets the instance world position
     * Use <code>go.get_position</code> to retrieve the position relative to the parent.
     *
     * @name go.get_world_position
     * @param [id] optional id of the instance to get the world position for. Default is the "self" instance from the calling script (hash|string|url)
     * @return instance world position (vector3)
     */
    int Script_GetWorldPosition(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        dmScript::PushVector3(L, Vectormath::Aos::Vector3(dmGameObject::GetWorldPosition(instance)));
        return 1;
    }

    /*# gets the instance world rotation
     * Use <code>go.get_rotation</code> to retrieve the rotation relative to the parent.
     *
     * @name go.get_world_rotation
     * @param [id] optional id of the instance to get the world rotation for. Default is the "self" instance from the calling script (hash|string|url)
     * @return instance world rotation (quaternion)
     */
    int Script_GetWorldRotation(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        dmScript::PushQuat(L, dmGameObject::GetWorldRotation(instance));
        return 1;
    }

    /*# gets the instance world scale factor
     * Use <code>go.get_scale</code> to retrieve the scale factor relative to the parent.
     *
     * @name go.get_world_scale
     * @param [id] optional id of the instance to get the world scale for. Default is the "self" instance from the calling script (hash|string|url)
     * @return uniform instance world scale factor (number)
     */
    int Script_GetWorldScale(lua_State* L)
    {
        Instance* instance = ResolveInstance(L, 1);
        lua_pushnumber(L, dmGameObject::GetWorldScale(instance));
        return 1;
    }

    /*# gets the id of an instance
     * The instance id is a hash of the absolute path.
     * If <code>path</code> is specified it can either be absolute, or relative to the instance of the calling script.
     * If <code>path</code> is not specified, the id of the instance of the calling script will be returned. See the examples below for more information.
     *
     * @name go.get_id
     * @param [path] path of the instance for which to return the id (string)
     * @return instance id (hash)
     * @examples
     * <p>
     * For the instance with path <code>/my_sub_collection/my_instance</code>, the following calls are equivalent:
     * </p>
     * <pre>
     * local id = go.get_id() -- no path, defaults to the instance of the calling script
     * local id = go.get_id("/my_sub_collection/my_instance") -- absolute path
     * </pre>
     * <p>
     * From a script in another instance in the same collection, i.e. path <code>/my_sub_collection/my_other_instance</code>, the id of the first instance can be retrieved in two ways:
     * </p>
     * <pre>
     * local id = go.get_id("my_instance") -- relative path
     * local id = go.get_id("/my_sub_collection/my_instance") -- absolute path
     * </pre>
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

    /*# deletes a game object instance
     * Use this function to delete a game object identified by its id.
     *
     * NOTE! Don't call this function directly or indirectly from a <a href="#final">final</a> call. This will currently result in undefined behaviour.
     *
     * @name go.delete
     * @param [id] optional id of the instance to delete, the instance of the calling script is deleted by default (hash|string|url)
     * @examples
     * <p>
     * This example demonstrates how to delete a game object with the id "my_game_object".
     * </p>
     * <pre>
     * local id = go.get_id("my_game_object") -- retrieve the id of the game object to be deleted
     * go.detele(id) -- delete the game object
     * </pre>
     */
    int Script_Delete(lua_State* L)
    {
        dmGameObject::HInstance instance = ResolveInstance(L, 1);
        dmGameObject::HCollection collection = instance->m_Collection;
        dmGameObject::Delete(collection, instance);
        return 0;
    }

    /*# constructs a ray in world space from a position in screen space
     *
     * NOTE! Don't use this function, WIP!
     *
     * @name go.screen_ray
     * @param x x-coordinate of the screen space position (number)
     * @param y y-coordinate of the screen space position (number)
     * @return position and direction of the ray in world space (vmath.vector3, vmath.vector3)
     */
    int Script_ScreenRay(lua_State* L)
    {
        lua_Number x = luaL_checknumber(L, 1);
        lua_Number y = luaL_checknumber(L, 2);
        // TODO: This temporarily assumes the worldspace is simply screen space
        // Should be fixed in a more robust way.
        Vector3 p(x, y, 1.0f);
        Vector3 d(0.0f, 0.0f, -1.0f);
        dmScript::PushVector3(L, p);
        dmScript::PushVector3(L, d);
        return 2;
    }

    /*# define a property to be used throughout the script
     * This function defines a property which can then be used in the script through the self-reference.
     * The properties defined this way are automatically exposed in the editor in game objects and collections which use the script.
     * Note that you can only use this function outside any callback-functions like init and update.
     *
     * @name go.property
     * @param name the name of the property (string)
     * @param value default value of the property. In the case of a url, only the empty constructor msg.url() is allowed. (number, hash, url, vector3, vector4, quat)
     * @examples
     * <p>
     * This example demonstrates how to define a property called "health" in a script.
     * The health is decreased whenever someone sends a message called "take_damage" to the script.
     * </p>
     * <pre>
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
     * </pre>
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
            return luaL_error(L, "Invalid type (%s) supplied to go.property, must be either a number, boolean, hash, URL, vector3, vector4 or quat.", lua_typename(L, lua_type(L, 2)));
        }
        assert(top == lua_gettop(L));
        return 0;
    }

    void GetURLCallback(lua_State* L, dmMessage::URL* url)
    {
        lua_pushliteral(L, SCRIPT_INSTANCE_NAME);
        lua_rawget(L, LUA_GLOBALSINDEX);
        ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, -1);
        lua_pop(L, 1);

        if (i != 0)
        {
            url->m_Socket = i->m_Instance->m_Collection->m_ComponentSocket;
            url->m_Path = i->m_Instance->m_Identifier;
            url->m_Fragment = i->m_Instance->m_Prototype->m_Components[i->m_ComponentIndex].m_Id;
        }
        else
        {
            dmMessage::ResetURL(*url);
        }
    }

    dmhash_t ResolvePathCallback(uintptr_t user_data, const char* path, uint32_t path_size)
    {
        lua_State* L = (lua_State*)user_data;
        ScriptInstance* i = GetScriptInstance(L);

        if (i != 0)
        {
            if (path_size > 0)
            {
                return GetAbsoluteIdentifier(i->m_Instance, path, path_size);
            }
            else
            {
                return i->m_Instance->m_Identifier;
            }
        }
        else
        {
            Script* script = GetScript(L);
            if (script == 0x0)
            {
                return luaL_error(L, "No context available in which to resolve the supplied URL.");
            }
            else
            {
                // NOTE No resolve takes place here.
                // If it's called from within a go.property() it doesn't matter, since such URLs are resolved at script instance creation
                return dmHashBuffer64(path, path_size);
            }
        }
    }

    uintptr_t GetUserDataCallback(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        return (uintptr_t)i->m_Instance;
    }

    static const luaL_reg Script_methods[] =
    {
        {"get_position",        Script_GetPosition},
        {"get_rotation",        Script_GetRotation},
        {"get_scale",           Script_GetScale},
        {"set_position",        Script_SetPosition},
        {"set_rotation",        Script_SetRotation},
        {"set_scale",           Script_SetScale},
        {"get_world_position",  Script_GetWorldPosition},
        {"get_world_rotation",  Script_GetWorldRotation},
        {"get_world_scale",     Script_GetWorldScale},
        {"get_id",              Script_GetId},
        {"delete",              Script_Delete},
        {"screen_ray",          Script_ScreenRay},
        {"property",            Script_Property},
        {0, 0}
    };

    void InitializeScript(dmScript::HContext context, dmResource::HFactory factory)
    {
        lua_State *L = lua_open();
        g_LuaState = L;
        g_ScriptContext = context;

        int top = lua_gettop(L);
        luaL_openlibs(L);

        // Pop all stack values generated from luaopen_*
        lua_pop(L, lua_gettop(L));

        luaL_register(L, SCRIPTINSTANCE, ScriptInstance_methods);   // create methods table, add it to the globals
        int methods = lua_gettop(L);
        luaL_newmetatable(L, SCRIPTINSTANCE);                         // create metatable for Image, add it to the Lua registry
        int metatable = lua_gettop(L);
        luaL_register(L, 0, ScriptInstance_meta);                   // fill metatable

        lua_pushliteral(L, "__metatable");
        lua_pushvalue(L, methods);                       // dup methods table
        lua_settable(L, metatable);

        lua_pop(L, 2);

        luaL_register(L, "go", Script_methods);
        lua_pop(L, 1);

        dmScript::ScriptParams params;
        params.m_Context = context;
        params.m_GetURLCallback = GetURLCallback;
        params.m_ResolvePathCallback = ResolvePathCallback;
        params.m_GetUserDataCallback = GetUserDataCallback;
        dmScript::Initialize(L, params);

        assert(top == lua_gettop(L));
    }

    static void FreeModule(void* user_context, void* user_data)
    {
        dmResource::HFactory factory = (dmResource::HFactory) user_context;
        LuaScript* lua_script = (LuaScript*) user_data;
        dmResource::Release(factory, lua_script);
    }

    void FinalizeScript(dmResource::HFactory factory)
    {
        if (g_ScriptContext)
        {
            dmScript::IterateModules(g_ScriptContext, factory, FreeModule);
        }
        if (g_LuaState)
        {
            lua_close(g_LuaState);
        }
        g_LuaState = 0;
        g_ScriptContext = 0;
    }

    struct LuaData
    {
        const char* m_Buffer;
        uint32_t m_Size;
    };

    static const char* ReadScript(lua_State *L, void *data, size_t *size)
    {
        LuaData* lua_data = (LuaData*)data;
        if (lua_data->m_Size == 0)
        {
            return 0x0;
        }
        else
        {
            *size = lua_data->m_Size;
            lua_data->m_Size = 0;
            return lua_data->m_Buffer;
        }
    }

    static bool LoadScript(lua_State* L, const void* buffer, uint32_t buffer_size, const char* filename, Script* script)
    {
        for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
            script->m_FunctionReferences[i] = LUA_NOREF;

        bool result = false;
        int top = lua_gettop(L);
        (void) top;

        LuaData data;
        data.m_Buffer = (const char*)buffer;
        data.m_Size = buffer_size;
        int ret = lua_load(L, &ReadScript, &data, filename);
        if (ret == 0)
        {
            // Script-reference
            lua_pushliteral(L, SCRIPT_NAME);
            lua_pushlightuserdata(L, (void*) script);
            lua_rawset(L, LUA_GLOBALSINDEX);

            ret = lua_pcall(L, 0, LUA_MULTRET, 0);
            if (ret == 0)
            {
                for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
                {
                    lua_getglobal(L, SCRIPT_FUNCTION_NAMES[i]);
                    if (lua_isnil(L, -1) == 0)
                    {
                        if (lua_type(L, -1) == LUA_TFUNCTION)
                        {
                            script->m_FunctionReferences[i] = luaL_ref(L, LUA_REGISTRYINDEX);
                        }
                        else
                        {
                            dmLogError("The global name '%s' in '%s' must be a function.", SCRIPT_FUNCTION_NAMES[i], filename);
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
            else
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
            }
            lua_pushliteral(L, SCRIPT_NAME);
            lua_pushnil(L);
            lua_rawset(L, LUA_GLOBALSINDEX);
        }
        else
        {
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

    HScript NewScript(dmLuaDDF::LuaModule* lua_module, const char* filename)
    {
        lua_State* L = g_LuaState;

        HScript script = new Script();
        script->m_PropertySet.m_UserData = (uintptr_t)script;
        script->m_PropertySet.m_GetPropertyCallback = GetPropertyDefault;
        script->m_LuaModule = lua_module;
        if (!LoadScript(L, (const void*)lua_module->m_Script.m_Data, lua_module->m_Script.m_Count, filename, script))
        {
            delete script;
            return 0;
        }

        return script;
    }

    bool ReloadScript(HScript script, dmLuaDDF::LuaModule* lua_module, const char* filename)
    {
        bool result = true;
        script->m_LuaModule = lua_module;
        if (!LoadScript(g_LuaState, (const void*)lua_module->m_Script.m_Data, lua_module->m_Script.m_Count, filename, script))
            result = false;
        return result;
    }

    void DeleteScript(HScript script)
    {
        lua_State* L = g_LuaState;
        for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
        {
            if (script->m_FunctionReferences[i])
                luaL_unref(L, LUA_REGISTRYINDEX, script->m_FunctionReferences[i]);
        }
        delete script;
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
                properties->m_GetURLCallback((lua_State*)properties->m_ResolvePathUserData, &default_url);
                const char* url_string = defs->m_StringValues[entry.m_Index];
                dmMessage::Result result = dmScript::ResolveURL(properties->m_ResolvePathCallback, properties->m_ResolvePathUserData, url_string, &out_var.m_URL, &default_url);
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

    HScriptInstance NewScriptInstance(HScript script, HInstance instance, uint8_t component_index)
    {
        lua_State* L = g_LuaState;

        int top = lua_gettop(L);
        (void) top;

        lua_getglobal(L, "__instances__");

        ScriptInstance* i = (ScriptInstance *)lua_newuserdata(L, sizeof(ScriptInstance));
        i->m_Script = script;

        lua_pushvalue(L, -1);
        i->m_InstanceReference = luaL_ref( L, LUA_REGISTRYINDEX );

        lua_newtable(L);
        i->m_ScriptDataReference = luaL_ref( L, LUA_REGISTRYINDEX );

        i->m_Instance = instance;
        i->m_ComponentIndex = component_index;
        NewPropertiesParams params;
        params.m_ResolvePathCallback = ResolvePathCallback;
        params.m_ResolvePathUserData = (uintptr_t)L;
        params.m_GetURLCallback = GetURLCallback;
        i->m_Properties = NewProperties(params);
        SetPropertySet(i->m_Properties, PROPERTY_LAYER_DEFAULT, script->m_PropertySet);
        luaL_getmetatable(L, SCRIPTINSTANCE);
        lua_setmetatable(L, -2);

        lua_pop(L, 1);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        return i;
    }

    void DeleteScriptInstance(HScriptInstance script_instance)
    {
        lua_State* L = g_LuaState;

        int top = lua_gettop(L);
        (void) top;

        luaL_unref(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
        luaL_unref(L, LUA_REGISTRYINDEX, script_instance->m_ScriptDataReference);

        DeleteProperties(script_instance->m_Properties);

        assert(top == lua_gettop(L));
    }

    void PropertiesToLuaTable(HInstance instance, HScript script, const HProperties properties, lua_State* L, int index)
    {
        const PropertyDeclarations* declarations = &script->m_LuaModule->m_Properties;
        PropertyVar var;
        uint32_t count = declarations->m_NumberEntries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDeclarationEntry& entry = declarations->m_NumberEntries[i];
            lua_pushstring(L, entry.m_Key);
            bool result = GetProperty(properties, entry.m_Id, var);
            (void)result;
            assert(result);
            assert(var.m_Type == PROPERTY_TYPE_NUMBER);
            lua_pushnumber(L, var.m_Number);
            lua_settable(L, index - 2);
        }
        count = declarations->m_HashEntries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDeclarationEntry& entry = declarations->m_HashEntries[i];
            lua_pushstring(L, entry.m_Key);
            bool result = GetProperty(properties, entry.m_Id, var);
            (void)result;
            assert(result);
            assert(var.m_Type == PROPERTY_TYPE_HASH);
            dmScript::PushHash(L, var.m_Hash);
            lua_settable(L, index - 2);
        }
        count = declarations->m_UrlEntries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDeclarationEntry& entry = declarations->m_UrlEntries[i];
            lua_pushstring(L, entry.m_Key);
            bool result = GetProperty(properties, entry.m_Id, var);
            (void)result;
            assert(result);
            assert(var.m_Type == PROPERTY_TYPE_URL);
            dmScript::PushURL(L, var.m_URL);
            lua_settable(L, index - 2);
        }
        count = declarations->m_Vector3Entries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDeclarationEntry& entry = declarations->m_Vector3Entries[i];
            lua_pushstring(L, entry.m_Key);
            bool result = GetProperty(properties, entry.m_Id, var);
            (void)result;
            assert(result);
            assert(var.m_Type == PROPERTY_TYPE_VECTOR3);
            dmScript::PushVector3(L, Vector3(var.m_V4[0], var.m_V4[1], var.m_V4[2]));
            lua_settable(L, index - 2);
        }
        count = declarations->m_Vector4Entries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDeclarationEntry& entry = declarations->m_Vector4Entries[i];
            lua_pushstring(L, entry.m_Key);
            bool result = GetProperty(properties, entry.m_Id, var);
            (void)result;
            assert(result);
            assert(var.m_Type == PROPERTY_TYPE_VECTOR4);
            dmScript::PushVector4(L, Vector4(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]));
            lua_settable(L, index - 2);
        }
        count = declarations->m_QuatEntries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDeclarationEntry& entry = declarations->m_QuatEntries[i];
            lua_pushstring(L, entry.m_Key);
            bool result = GetProperty(properties, entry.m_Id, var);
            (void)result;
            assert(result);
            assert(var.m_Type == PROPERTY_TYPE_QUAT);
            dmScript::PushQuat(L, Quat(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]));
            lua_settable(L, index - 2);
        }
        count = declarations->m_BoolEntries.m_Count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PropertyDeclarationEntry& entry = declarations->m_BoolEntries[i];
            lua_pushstring(L, entry.m_Key);
            bool result = GetProperty(properties, entry.m_Id, var);
            (void)result;
            assert(result);
            assert(var.m_Type == PROPERTY_TYPE_BOOLEAN);
            lua_pushboolean(L, var.m_Bool);
            lua_settable(L, index - 2);
        }
    }

    // Documentation for the scripts

    /*# called when a script component is initialized
     * This is a callback-function, which is called by the engine when a script component is initialized. It can be used
     * to set the initial state of the script.
     *
     * @name init
     * @param self reference to the script state to be used for storing data (script_ref)
     * @examples
     * <pre>
     * function init(self)
     *     -- set up useful data
     *     self.my_value = 1
     * end
     * </pre>
     */

    /*# called when a script component is finalized
     * This is a callback-function, which is called by the engine when a script component is finalized (destroyed). It can
     * be used to e.g. take some last action, report the finalization to other game object instances
     * or release user input focus (see <code>release_input_focus</code>).
     *
     * NOTE! Don't call <a href="#go.delete">go.delete</a> from this function or in any <a href="#on_message">on_message</a> resulting from a message posted from this function. This will currently result in undefined behaviour.
     *
     * @name final
     * @param self reference to the script state to be used for storing data (script_ref)
     * @examples
     * <pre>
     * function final(self)
     *     -- report finalization
     *     msg.post("my_friend_instance", "im_dead", {my_stats = self.some_value})
     * end
     * </pre>
     */

    /*# called every frame to update the script component
     * This is a callback-function, which is called by the engine every frame to update the state of a script component.
     * It can be used to perform any kind of game related tasks, e.g. moving the game object instance.
     *
     * @name update
     * @param self reference to the script state to be used for storing data (script_ref)
     * @param dt the time-step of the frame update
     * @examples
     * <p>
     * This example demonstrates how to move a game object instance through the script component:
     * </p>
     * <pre>
     * function init(self)
     *     -- set initial velocity to be 1 along world x-axis
     *     self.my_velocity = vmath.vector3(1, 0, 0)
     * end
     *
     * function update(self, dt)
     *     -- move the game object instance
     *     go.set_position(go.get_position() + dt * self.my_velocity)
     * end
     * </pre>
     */

    /*# called when a message has been sent to the script component
     * <p>
     * This is a callback-function, which is called by the engine whenever a message has been sent to the script component.
     * It can be used to take action on the message, e.g. send a response back to the sender of the message.
     * </p>
     * <p>
     * The <code>message</code> parameter is a table containing the message data. If the message is sent from the engine, the
     * documentation of the message specifies which data is supplied.
     * </p>
     *
     * @name on_message
     * @param self reference to the script state to be used for storing data (script_ref)
     * @param message_id id of the received message (hash)
     * @param message a table containing the message data (table)
     * @param sender address of the sender (url)
     * @examples
     * <p>
     * This example demonstrates how a game object instance, called "a", can communicate with another instance, called "b". It
     * is assumed that both script components of the instances has id "script".
     * </p>
     * <p>
     * Script of instance "a":
     * </p>
     * <pre>
     * function init(self)
     *     -- let b know about some important data
     *     msg.post("b#script", "my_data", {important_value = 1})
     * end
     * </pre>
     * <p>
     * Script of intance "b":
     * </p>
     * <pre>
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
     * </pre>
     */

    /*# called when user input is received
     * <p>
     * This is a callback-function, which is called by the engine when user input is sent to the game object instance of the script.
     * It can be used to take action on the input, e.g. move the instance according to the input.
     * </p>
     * <p>
     * For an instance to obtain user input, it must first acquire input focuse through the message <code>acquire_input_focus</code>.
     * See the documentation of that message for more information.
     * </p>
     * <p>
     * The <code>action</code> parameter is a table containing data about the input mapped to the <code>action_id</code>.
     * For mapped actions it specifies the value of the input and if it was just pressed or released.
     * Actions are mapped to input in an input_binding-file.
     * </p>
     * <p>
     * Mouse movement is specifically handled and uses <code>nil</code> as its <code>action_id</code>.
     * The <code>action</code> only contains positional parameters in this case, such as x and y of the pointer.
     * </p>
     * <p>
     * Here is a brief description of the available table fields:
     * </p>
     * <table>
     *   <th>Field</th>
     *   <th>Description</th>
     *   <tr><td><code>value</code></td><td>The amount of input given by the user. This is usually 1 for buttons and 0-1 for analogue inputs. This is not present for mouse movement.</td></tr>
     *   <tr><td><code>pressed</code></td><td>If the input was pressed this frame, 0 for false and 1 for true. This is not present for mouse movement.</td></tr>
     *   <tr><td><code>released</code></td><td>If the input was released this frame, 0 for false and 1 for true. This is not present for mouse movement.</td></tr>
     *   <tr><td><code>repeated</code></td><td>If the input was repeated this frame, 0 for false and 1 for true. This is similar to how a key on a keyboard is repeated when you hold it down. This is not present for mouse movement.</td></tr>
     *   <tr><td><code>x</code></td><td>The x value of a pointer device, if present.</td></tr>
     *   <tr><td><code>y</code></td><td>The y value of a pointer device, if present.</td></tr>
     *   <tr><td><code>dx</code></td><td>The change in x value of a pointer device, if present.</td></tr>
     *   <tr><td><code>dy</code></td><td>The change in y value of a pointer device, if present.</td></tr>
     * </table>
     *
     * @name on_input
     * @param self reference to the script state to be used for storing data (script_ref)
     * @param action_id id of the received input action, as mapped in the input_binding-file (hash)
     * @param action a table containing the input data, see above for a description (table)
     * @return optional boolean to signal if the input should be consumed (not passed on to others) or not, default is false (boolean)
     * @examples
     * <p>
     * This example demonstrates how a game object instance can be moved as a response to user input.
     * </p>
     * <pre>
     * function init(self)
     *     -- acquire input focus
     *     msg.post(nil, "acquire_input_focus")
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
     * </pre>
     */

    /*# called when the script component is reloaded
     * <p>
     * This is a callback-function, which is called by the engine when the script component is reloaded, e.g. from the editor.
     * It can be used for live development, e.g. to tweak constants or set up the state properly for the instance.
     * </p>
     *
     * @name on_reload
     * @param self reference to the script state to be used for storing data (script_ref)
     * @examples
     * <p>
     * This example demonstrates how to tweak the speed of a game object instance that is moved on user input.
     * </p>
     * <pre>
     * function init(self)
     *     -- acquire input focus
     *     msg.post(nil, "acquire_input_focus")
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
     * </pre>
     */
}
