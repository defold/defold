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
    int Script_Hash(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* str = luaL_checkstring(L, 1);
        PushHash(L, dmHashString32(str));

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    void PushHash(lua_State* L, uint32_t hash)
    {
        char buf[9];
        DM_SNPRINTF(buf, sizeof(buf), "%X", hash);
        lua_pushstring(L, buf);
    }

    uint32_t CheckHash(lua_State* L, int index)
    {
        const char* str = luaL_checkstring(L, index);
        uint32_t hash;
        int items = sscanf(str, "%X", &hash);
        if (items != 1)
            return luaL_error(L, "Hash has the wrong format: '%s'.", str);
        return hash;
    }

    static const luaL_reg ScriptHash_methods[] =
    {
        {"hash", Script_Hash},
        {0, 0}
    };

    void InitializeHash(lua_State* L)
    {
        int top = lua_gettop(L);

        lua_pushvalue(L, LUA_GLOBALSINDEX);
        luaL_register(L, 0x0, ScriptHash_methods);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }
}
