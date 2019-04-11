#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <float.h>

#include <dlib/dstrings.h>
#include <dlib/json.h>

#include "script.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

#include "script_json.h"
#include "script_private.h"

namespace dmScript
{
    /*# JSON API documentation
     *
     * Manipulation of JSON data strings.
     *
     * @document
     * @name JSON
     * @namespace json
     */

    #define LIB_NAME "json"

    /** Convert a JSON document to Lua table. (See dmScript::JsonToLua)
     * @note Doesn't free the document upon error
     * @note Doesn't reset the Lua stack upon error
     * @return <0 if it fails. >=0 if it succeeds (index of next JSON node to handle)
     */
    static int JsonToLuaInternal(lua_State* L, dmJson::Document* doc, int index, char* error_str_out, size_t error_str_size)
    {
        // The maximum length of a IEEE 754 double (+ \0)
        const uint32_t buffer_len = 3 + DBL_MANT_DIG - DBL_MIN_EXP + 1;

        if (index >= doc->m_NodeCount)
        {
            DM_SNPRINTF(error_str_out, error_str_size, "Unexpected JSON index, unable to parse content.");
            return -1;
        }

        const dmJson::Node& n = doc->m_Nodes[index];
        const char* json = doc->m_Json;
        uint32_t l = n.m_End - n.m_Start;
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
                char buffer[buffer_len] = { 0 };
                memcpy(buffer, json + n.m_Start, dmMath::Min(buffer_len - 1, l));

                uint32_t bytes_read = 0;
                double value = 0.0f;
                int result = sscanf(buffer, "%lf%n", &value, &bytes_read);
                if (result == 1 && bytes_read == dmMath::Min(buffer_len - 1, l))
                {
                    lua_pushnumber(L, value);
                }
                else
                {
                    DM_SNPRINTF(error_str_out, error_str_size, "Invalid JSON primitive: %s", buffer);
                    return -1;
                }
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
                index = JsonToLuaInternal(L, doc, index, error_str_out, error_str_size);
                if (index < 0)
                    return -1;
                lua_rawseti(L, -2, i+1);
            }
            return index;

        case dmJson::TYPE_OBJECT:
            // {1 2 3} is a valid object according to the jsmn parser, we need
            // to protect against this to avoid reading random memory.
            if ((n.m_Size % 2) == 0)
            {
                lua_createtable(L, 0, n.m_Size);
                ++index;
                for (int i = 0; i < n.m_Size; i += 2)
                {
                    index = JsonToLuaInternal(L, doc, index, error_str_out, error_str_size);
                    if (index < 0)
                        return -1;
                    index = JsonToLuaInternal(L, doc, index, error_str_out, error_str_size);
                    if (index < 0)
                        return -1;
                    lua_rawset(L, -3);
                }

                return index;
            }
            else
            {
                char buffer[buffer_len] = { 0 };
                memcpy(buffer, json + n.m_Start, dmMath::Min(buffer_len - 1, l));
                DM_SNPRINTF(error_str_out, error_str_size, "Incomplete JSON object: %s", buffer);
                return -1;
            }
        }

        DM_SNPRINTF(error_str_out, error_str_size, "Unsupported JSON type (%d), unable to parse content.", n.m_Type);
        return -1;
    }

    int JsonToLua(lua_State* L, dmJson::Document* doc, int index, char* error_str_out, size_t error_str_size)
    {
        int top = lua_gettop(L);
        int result = JsonToLuaInternal(L, doc, index, error_str_out, error_str_size);
        if (result < 0)
        {
            lua_pop(L, lua_gettop(L) - top);
        }
        return result;
    }

    /*# decode JSON from a string to a lua-table
     * Decode a string of JSON data into a Lua table.
     * A Lua error is raised for syntax errors.
     *
     * @name json.decode
     * @param json [type:string] json data
     * @return data [type:table] decoded json
     *
     * @examples
     *
     * Converting a string containing JSON data into a Lua table:
     *
     * ```lua
     * function init(self)
     *     local jsonstring = '{"persons":[{"name":"John Doe"},{"name":"Darth Vader"}]}'
     *     local data = json.decode(jsonstring)
     *     pprint(data)
     * end
     * ```
     *
     * Results in the following printout:
     *
     * ```
     * {
     *   persons = {
     *     1 = {
     *       name = John Doe,
     *     }
     *     2 = {
     *       name = Darth Vader,
     *     }
     *   }
     * }
     * ```
     */
    int Json_Decode(lua_State* L)
    {
        int top = lua_gettop(L);
        size_t stringlength = 0;
        const char* json = luaL_checklstring(L, 1, &stringlength);
        dmJson::Document doc;

        dmJson::Result r = dmJson::Parse(json, (uint32_t)stringlength, &doc);

        if (r == dmJson::RESULT_OK && doc.m_NodeCount > 0)
        {
            char err_str[128];
            if (JsonToLua(L, &doc, 0, err_str, sizeof(err_str)) < 0) {
                dmJson::Free(&doc);
                return luaL_error(L, "%s", err_str);
            }
            dmJson::Free(&doc);

            assert(top + 1 == lua_gettop(L));
            return 1;
        }
        dmJson::Free(&doc);

        assert(top == lua_gettop(L));
        return luaL_error(L, "Failed to parse json '%s' (%d).", json, r);
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
