#include "script_hash.h"

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
    int Script_Hash(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* str = luaL_checkstring(L, 1);
        PushHash(L, str);

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    static const luaL_reg ScriptHash_methods[] =
    {
        {"hash",                Script_Hash},
        {0, 0}
    };

    void RegisterHashLib(lua_State* L)
    {
        int top = lua_gettop(L);

        lua_pushvalue(L, LUA_GLOBALSINDEX);
        luaL_register(L, 0x0, ScriptHash_methods);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

    void PushHash(lua_State* L, const char* s)
    {
        char buf[9];
        DM_SNPRINTF(buf, sizeof(buf), "%X", dmHashString32(s));
        lua_pushstring(L, buf);
    }

    Hash CheckHash(lua_State* L, int index)
    {
        const char* str = luaL_checkstring(L, index);
        Hash hash;
        int items = sscanf(str, "%X", &hash);
        if (items != 1)
            luaL_error(L, "Hash has the wrong format: '%s'.", str);
        return hash;
    }
}
