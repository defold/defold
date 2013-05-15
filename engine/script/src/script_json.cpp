#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <dlib/json.h>
#include "script.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

#include "script_private.h"

namespace dmScript
{
    #define LIB_NAME "json"

    static int ToLua(lua_State*L, dmJson::Document* doc, const char* json, int index)
    {
        const dmJson::Node& n = doc->m_Nodes[index];
        int l = n.m_End - n.m_Start;
        switch (n.m_Type)
        {
        case dmJson::TYPE_PRIMITIVE:
            if (l == 4 && memcmp(json + n.m_Start, "null", 4) == 0)
            {
                lua_pushnil(L);
            }
            else if (l == 4 && memcmp(json + n.m_Start, "true", 4) == 0)
            {
                lua_pushboolean(L, 1);
            }
            else if (l == 5 && memcmp(json + n.m_Start, "false", 5) == 0)
            {
                lua_pushboolean(L, 0);
            }
            else
            {
                double val = atof(json + n.m_Start);
                lua_pushnumber(L, val);
            }
            return index + 1;

        case dmJson::TYPE_STRING:
            lua_pushlstring(L, json + n.m_Start, l);
            return index + 1;

        case dmJson::TYPE_ARRAY:
            lua_createtable(L, n.m_Size, 0);
            ++index;
            for (int i = 0; i < n.m_Size; ++i)
            {
                index = ToLua(L, doc, json, index);
                lua_rawseti(L, -2, i+1);
            }
            return index;

        case dmJson::TYPE_OBJECT:
            lua_createtable(L, 0, n.m_Size);
            ++index;
            for (int i = 0; i < n.m_Size; i += 2)
            {
                index = ToLua(L, doc, json, index);
                index = ToLua(L, doc, json, index);
                lua_rawset(L, -3);
            }

            return index;
        }

        assert(false && "not reached");
        return index;
    }

    /*# decode json from a string to a lua-table
     * A lua error is raised for syntax errors.
     *
     * @name json.decode
     * @param json json data (string)
     * @return decoded json as a lua-table
     */
    int Json_Decode(lua_State* L)
    {
        int top = lua_gettop(L);
        const char* json = luaL_checkstring(L, 1);
        dmJson::Document doc;
        dmJson::Result r = dmJson::Parse(json, &doc);
        if (r == dmJson::RESULT_OK)
        {
            ToLua(L, &doc, json, 0);
            assert(top + 1== lua_gettop(L));
            return 1;
        }
        else
        {
            return luaL_error(L, "Failed to parse json '%s' (%d).", json, r);
        }
    }

    static const luaL_reg ScriptJson_methods[] =
    {
        {"decode", Json_Decode},
        {0, 0}
    };

    void InitializeJson(lua_State* L)
    {
        int top = lua_gettop(L);

        lua_pushvalue(L, LUA_GLOBALSINDEX);
        luaL_register(L, LIB_NAME, ScriptJson_methods);
        lua_pop(L, 2);

        assert(top == lua_gettop(L));
    }
}
