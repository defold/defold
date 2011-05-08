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

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameObject
{
    #define SCRIPTINSTANCE "ScriptInstance"

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

    ScriptWorld::ScriptWorld()
    : m_Instances()
    {
        // TODO: How to configure? It should correspond to collection instance count
        m_Instances.SetCapacity(1024);
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
        lua_pushliteral(L, SCRIPT_INSTANCE_NAME);
        lua_rawget(L, LUA_GLOBALSINDEX);
        ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, -1);
        lua_pop(L, 1);
        if (i == NULL) luaL_error(L, "Lua state did not contain any '%s'.", SCRIPT_INSTANCE_NAME);
        return i;
    }

    /*#
     * Get GameObject position
     * @name go.get_position
     * @return GameObject position (vector3)
     */
    int Script_GetPosition(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        dmScript::PushVector3(L, Vectormath::Aos::Vector3(dmGameObject::GetPosition(i->m_Instance)));
        return 1;
    }

    /*#
     * Get GameObject rotation
     * @name go.get_rotation
     * @return GameObject rotation quaternion
     */
    int Script_GetRotation(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        dmScript::PushQuat(L, dmGameObject::GetRotation(i->m_Instance));
        return 1;
    }

    /*#
     * Set GameObject position
     * @name vmath.set_position
     * @param position position to set (vector3)
     */
    int Script_SetPosition(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        Vectormath::Aos::Vector3* v = dmScript::CheckVector3(L, 1);
        dmGameObject::SetPosition(i->m_Instance, Vectormath::Aos::Point3(*v));
        return 0;
    }

    /*#
     * Set GameObject rotation
     * @name vmath.set_rotation
     * @param quat rotation quaternion to set
     */
    int Script_SetRotation(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        Vectormath::Aos::Quat* q = dmScript::CheckQuat(L, 1);
        dmGameObject::SetRotation(i->m_Instance, *q);
        return 0;
    }

    /*#
     * Get GameObject world position
     * @name go.get_world_position
     * @return GameObject position (vector3)
     */
    int Script_GetWorldPosition(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        dmScript::PushVector3(L, Vectormath::Aos::Vector3(dmGameObject::GetWorldPosition(i->m_Instance)));
        return 1;
    }

    /*#
     * Get GameObject world rotation
     * @name go.get_world_rotation
     * @return GameObject rotation (quaternion)
     */
    int Script_GetWorldRotation(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        dmScript::PushQuat(L, dmGameObject::GetWorldRotation(i->m_Instance));
        return 1;
    }

    /*#
     * Get GameObject id
     * @name go.get_id
     * @return GameObject id (hashed format)
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

    /*#
     * Delete GameObject
     * @name go.delete
     */
    int Script_Delete(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L);
        dmGameObject::Delete(i->m_Instance->m_Collection, i->m_Instance);
        return 0;
    }

    void GetURLCallback(lua_State* L, dmMessage::URL* url)
    {
        ScriptInstance* i = ScriptInstance_Check(L);

        url->m_Socket = i->m_Instance->m_Collection->m_Socket;
        url->m_Path = i->m_Instance->m_Identifier;
        url->m_Fragment = i->m_Instance->m_Prototype->m_Components[i->m_ComponentIndex].m_Id;
    }

    dmhash_t ResolvePathCallback(lua_State* L, const char* path, uint32_t path_size)
    {
        ScriptInstance* i = ScriptInstance_Check(L);

        if (path_size > 0)
        {
            return dmGameObject::GetAbsoluteIdentifier(i->m_Instance, path, path_size);
        }
        else
        {
            return i->m_Instance->m_Identifier;
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
        {"set_position",        Script_SetPosition},
        {"set_rotation",        Script_SetRotation},
        {"get_world_position",  Script_GetWorldPosition},
        {"get_world_rotation",  Script_GetWorldRotation},
        {"get_id",              Script_GetId},
        {"delete",              Script_Delete},
        {0, 0}
    };

    void InitializeScript(dmScript::HContext context)
    {
        lua_State *L = lua_open();
        g_LuaState = L;

        int top = lua_gettop(L);

        luaopen_base(L);
        luaopen_table(L);
        luaopen_string(L);
        luaopen_math(L);
        luaopen_debug(L);

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

    void FinalizeScript()
    {
        if (g_LuaState)
            lua_close(g_LuaState);
        g_LuaState = 0;
    }

    struct LuaData
    {
        const char* m_Buffer;
        uint32_t m_Size;
    };

    const char* ReadScript(lua_State *L, void *data, size_t *size)
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

    HScript NewScript(const void* buffer, uint32_t buffer_size, const char* filename)
    {
        lua_State* L = g_LuaState;

        Script temp_script;
        if (LoadScript(L, buffer, buffer_size, filename, &temp_script))
        {
            HScript script = new Script();
            *script = temp_script;
            return script;
        }
        else
        {
            return 0;
        }
    }

    bool ReloadScript(HScript script, const void* buffer, uint32_t buffer_size, const char* filename)
    {
        return LoadScript(g_LuaState, buffer, buffer_size, filename, script);
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

        assert(top == lua_gettop(L));
    }
}
