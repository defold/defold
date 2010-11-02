#include <stdlib.h>
#include <dlib/dstrings.h>
#include "script.h"

#include <string.h>
extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmScript
{
#define LIB_NAME "pickle"
const uint32_t MAX_PICKLE_BUFFER_SIZE =  64 * 1024;

    int Script_Dumps(lua_State* L)
    {
        char buffer[MAX_PICKLE_BUFFER_SIZE];
        luaL_checktype(L, 1, LUA_TTABLE);
        uint32_t n_used = CheckTable(L, buffer, sizeof(buffer), 1);
        lua_pushlstring(L, buffer, n_used);
        return 1;
    }

    int Script_Loads(lua_State* L)
    {
        const char* buf = luaL_checkstring(L, 1);
        PushTable(L, buf);
        return 1;
    }

    static const luaL_reg ScriptPickle_methods[] =
    {
        {"dumps", Script_Dumps},
        {"loads", Script_Loads},
        {0, 0}
    };

    void InitializePickle(lua_State* L)
    {
        int top = lua_gettop(L);

        lua_pushvalue(L, LUA_GLOBALSINDEX);
        luaL_register(L, LIB_NAME, ScriptPickle_methods);
        lua_pop(L, 2);
        assert(top == lua_gettop(L));
    }
}
