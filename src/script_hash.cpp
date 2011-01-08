#include "script.h"

#include <assert.h>

#include <dlib/dstrings.h>
#include <dlib/hash.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    #define SCRIPT_TYPE_NAME "hash"

    int Script_Hash(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* str = luaL_checkstring(L, 1);
        dmhash_t hash = dmHashString64(str);
        PushHash(L, hash);

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    void PushHash(lua_State* L, dmhash_t hash)
    {
        dmhash_t* lua_hash = (dmhash_t*)lua_newuserdata(L, sizeof(dmhash_t));
        *lua_hash = hash;
        luaL_getmetatable(L, SCRIPT_TYPE_NAME);
        lua_setmetatable(L, -2);
    }

    dmhash_t CheckHash(lua_State* L, int index)
    {
        dmhash_t* lua_hash = 0x0;
        if (lua_isuserdata(L, index))
        {
            lua_hash = (dmhash_t*)lua_touserdata(L, index);
            return *lua_hash;
        }

        return 0;
    }

    int Script_eq(lua_State* L)
    {
        dmhash_t hash1 = CheckHash(L, 1);
        dmhash_t hash2 = CheckHash(L, 2);

        lua_pushboolean(L, hash1 == hash2);

        return 1;
    }

    static int Script_gc (lua_State *L)
    {
        dmhash_t hash = CheckHash(L, 1);
        (void)hash;
        return 0;
    }

    static const luaL_reg ScriptHash_methods[] =
    {
        {SCRIPT_TYPE_NAME, Script_Hash},
        {0, 0}
    };

    void InitializeHash(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_newmetatable(L, SCRIPT_TYPE_NAME);

        luaL_openlib(L, 0x0, ScriptHash_methods, 0);
        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, Script_gc);
        lua_settable(L, -3);

        lua_pushstring(L, "__eq");
        lua_pushcfunction(L, Script_eq);
        lua_settable(L, -3);

        lua_pushcfunction(L, Script_Hash);
        lua_setglobal(L, SCRIPT_TYPE_NAME);

        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

    #undef SCRIPT_TYPE_NAME
}
