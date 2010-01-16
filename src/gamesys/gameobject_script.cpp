#include <assert.h>
#include <string.h>
#include <dlib/log.h>
#include "gameobject_script.h"
#include "gameobject_common.h"

extern "C"
{
#include "../lua/lua.h"
#include "../lua/lauxlib.h"
#include "../lua/lualib.h"
}

namespace dmGameObject
{
    #define SCRIPTINSTANCE "ScriptInstance"

    lua_State* g_LuaState = 0;

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

        const char* key = luaL_checkstring(L, 2);
        if (strcmp(key, "Position") == 0)
        {
            lua_newtable(L);

            Point3& pos = i->m_Instance->m_Position;

            lua_pushnumber(L, pos.getX());
            lua_rawseti(L, -2, 0);

            lua_pushnumber(L, pos.getY());
            lua_rawseti(L, -2, 1);

            lua_pushnumber(L, pos.getZ());
            lua_rawseti(L, -2, 2);
        }
        else
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_ScriptDataReference);
            lua_pushvalue(L, 2);
            lua_gettable(L, -2);
        }

        return 1;
    }

    static int ScriptInstance_newindex(lua_State *L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        (void) i;
        assert(i);

        const char* key = luaL_checkstring(L, 2);
        if (strcmp(key, "Position") == 0)
        {
            luaL_checktype(L, 3, LUA_TTABLE);
            lua_rawgeti(L, 3, 0);
            lua_rawgeti(L, 3, 1);
            lua_rawgeti(L, 3, 2);

            float x = luaL_checknumber(L, -3);
            float y = luaL_checknumber(L, -2);
            float z = luaL_checknumber(L, -1);

            i->m_Instance->m_Position = Point3(x, y, z);
        }
        else
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_ScriptDataReference);
            lua_pushvalue(L, 2);
            lua_pushvalue(L, 3);
            lua_settable(L, -3);
        }

        return 1;
    }

    static const luaL_reg ScriptInstance_methods[] =
    {
        //{"test", ScriptInstance_Test},
        {0,0}
    };

    static const luaL_reg ScriptInstance_meta[] =
    {
        {"__gc",       ScriptInstance_gc},
        {"__tostring", ScriptInstance_tostring},
        {0, 0}
    };

    void InitializeScript()
    {
        lua_State *L = lua_open();
        g_LuaState = L;

        luaopen_base(L);
        luaopen_table(L);
        luaopen_string(L);
        luaopen_math(L);
        luaopen_debug(L);

        luaL_openlib(L, SCRIPTINSTANCE, ScriptInstance_methods, 0);   // create methods table, add it to the globals
        luaL_newmetatable(L, SCRIPTINSTANCE);                         // create metatable for Image, add it to the Lua registry
        luaL_openlib(L, 0, ScriptInstance_meta, 0);                   // fill metatable

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, ScriptInstance_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, ScriptInstance_newindex);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__metatable");
        lua_pushvalue(L, -3);                       // dup methods table
        lua_rawset(L, -3);                          // hide metatable: metatable.__metatable = methods
        lua_pop(L, 1);                              // drop metatable
    }

    void FinalizeScript()
    {
        lua_close(g_LuaState);
    }

    HScript NewScript(const void* memory)
    {
        lua_State* L = g_LuaState;

        int top = lua_gettop(L);
        (void) top;

        int functions;
        Script* script;

        int ret = luaL_loadstring(L, (const char*) memory);
        if (ret != 0)
        {
            dmLogError("Error running script: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
            goto bail;
        }

        ret = lua_pcall(L, 0, LUA_MULTRET, 0);
        if (ret != 0)
        {
            dmLogError("Error running script: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
            goto bail;
        }

        lua_getglobal(L, "functions");
        if (lua_type(L, -1) != LUA_TTABLE)
        {
            dmLogError("No function table found in script");
            lua_pop(L, -1);
            goto bail;
        }

        functions = luaL_ref( L, LUA_REGISTRYINDEX );
        assert(top == lua_gettop(L));

        script = new Script();
        script->m_FunctionsReference = functions;

        return script;
bail:
    assert(top == lua_gettop(L));
        return 0;
    }

    void DeleteScript(HScript script)
    {
        lua_State* L = g_LuaState;
        lua_pushnil(L);
        lua_rawseti(L, LUA_REGISTRYINDEX, script->m_FunctionsReference);
        delete script;
    }

    HScriptInstance NewScriptInstance(HInstance instance)
    {
        lua_State* L = g_LuaState;

        int top = lua_gettop(L);
        (void) top;

        lua_getglobal(L, "__instances__");

        ScriptInstance* i = (ScriptInstance *)lua_newuserdata(L, sizeof(ScriptInstance));

        lua_pushvalue(L, -1);
        i->m_InstanceReference = luaL_ref( L, LUA_REGISTRYINDEX );

        lua_newtable(L);
        i->m_ScriptDataReference = luaL_ref( L, LUA_REGISTRYINDEX );

        i->m_Instance = instance;
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

        lua_pushnil(L);
        lua_rawseti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

        assert(top == lua_gettop(L));
    }

    bool RunScript(HScript script, const char* function_name, HScriptInstance script_instance)
    {
        lua_State* L = g_LuaState;
        int top = lua_gettop(L);
        (void) top;
        int ret;

        lua_rawgeti(L, LUA_REGISTRYINDEX, script->m_FunctionsReference);

        lua_getfield(L, -1, function_name);
        if (lua_type(L, -1) != LUA_TFUNCTION)
        {
            dmLogError("No '%s' function found", function_name);
            lua_pop(L, 2);
            goto bail;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
        ret = lua_pcall(L, 1, LUA_MULTRET, 0);
        if (ret != 0)
        {
            dmLogError("Error running script: %s", lua_tostring(L,-1));
            lua_pop(L, 2);
            goto bail;
        }

        lua_pop(L, 1);

        assert(top == lua_gettop(L));
        return true;
bail:
        assert(top == lua_gettop(L));
        return false;
    }
}
