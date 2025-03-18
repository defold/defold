// Copyright 2020-2025 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <float.h>

#include <dlib/dstrings.h>

#include "script.h"
#include <setjmp.h>

extern "C"
{
    #include <lua/lua.h>
    #include <lua/lauxlib.h>

    // Defined in luacjson/lua_cjson.c
    int lua_cjson_decode(lua_State* L, const char* json_string, size_t json_len);
    int lua_cjson_encode(lua_State* L, char** json_str, size_t* json_length);
}

#include "script_json.h"
#include "script_private.h"

////////////////////////////////////////////////////////////////
// Once we know this works on all platforms, we may move this to a more public header
static jmp_buf* g_CurrentJmpBuf = 0;

struct LuaPanicScope
{
    lua_State*          m_L;
    lua_CFunction       m_OldFn;
    jmp_buf             m_JmpBuf;
    LuaPanicScope(lua_State* L, lua_CFunction fn);
    ~LuaPanicScope();
};

LuaPanicScope::LuaPanicScope(lua_State* L, lua_CFunction fn)
: m_L(L)
{
    m_OldFn = lua_atpanic(m_L, fn);
    g_CurrentJmpBuf = &m_JmpBuf;
}

LuaPanicScope::~LuaPanicScope()
{
    lua_atpanic(m_L, m_OldFn);
    g_CurrentJmpBuf = 0;
}

////////////////////////////////////////////////////////////////

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

    static int JsonToLuaInternal(lua_State* L, const char* json, size_t json_len)
    {
        int top = lua_gettop(L);
        int ret = lua_cjson_decode(L, json, json_len);
        if (ret != 1)
        {
            lua_pop(L, lua_gettop(L) - top);
        }
        assert(top + 1 == lua_gettop(L));
        return ret;
    }

    static int FallbackLuaPanic(lua_State* L)
    {
        if (g_CurrentJmpBuf != 0)
        {
            longjmp(*g_CurrentJmpBuf, 1); // Note that longjmp will automatically convert any 0 to a 1 (see documentation for longjmp)
        }
        return 0;
    }

    // Called from C (i.e. unprotected)
    int JsonToLua(lua_State* L, const char* json, size_t json_len)
    {
        LuaPanicScope scope(L, FallbackLuaPanic);
        int top = lua_gettop(L);
        int jmpval = setjmp(*g_CurrentJmpBuf);
        if (jmpval == 0)
        {
            return JsonToLuaInternal(L, json, json_len);
        }

        // If the error value is !=0, it means we got here from the panic function
        // pop any old items from the stack
        int num_to_pop = lua_gettop(L) - top;
        lua_pop(L, num_to_pop);
        return 0;
    }

    int LuaToJson(lua_State* L, char** json, size_t* json_len)
    {
        return lua_cjson_encode(L, json, json_len);
    }

    /*# decode JSON from a string to a lua-table
     * Decode a string of JSON data into a Lua table.
     * A Lua error is raised for syntax errors.
     *
     * @name json.decode
     * @param json [type:string] json data
     * @param [options] [type:table] table with decode options
     *
     * - [type:bool] `decode_null_as_userdata`: wether to decode a JSON null value as json.null or nil (default is nil)
     *
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
    static int Json_Decode(lua_State* L)
    {
        int top = lua_gettop(L);
        if (top == 0)
        {
            luaL_error(L, "json.decode requires one argument.");
        }

        size_t json_len;
        const char* json = luaL_checklstring(L, 1, &json_len);
        return JsonToLuaInternal(L, json, json_len);
    }

    /*# encode a lua table to a JSON string
     * Encode a lua table to a JSON string.
     * A Lua error is raised for syntax errors.
     *
     * @name json.encode
     * @param tbl [type:table] lua table to encode
     * @param [options] [type:table] table with encode options
     *
     * - [type:string] `encode_empty_table_as_object`: wether to encode an empty table as an JSON object or array (default is object)
     *
     * @return json [type:string] encoded json
     *
     * @examples
     *
     * Convert a lua table to a JSON string:
     *
     * ```lua
     * function init(self)
     *      local tbl = {
     *           persons = {
     *                { name = "John Doe"},
     *                { name = "Darth Vader"}
     *           }
     *      }
     *      local jsonstring = json.encode(tbl)
     *      pprint(jsonstring)
     * end
     * ```
     *
     * Results in the following printout:
     *
     * ```
     * {"persons":[{"name":"John Doe"},{"name":"Darth Vader"}]}
     * ```
     */
    static int Json_Encode(lua_State* L)
    {
        int top = lua_gettop(L);
        if (top == 0)
        {
            luaL_error(L, "json.encode requires one argument.");
        }

        char* json = 0;
        size_t json_length = 0;
        if (LuaToJson(L, &json, &json_length))
        {
            lua_pushlstring(L, json, json_length);
            free(json);
        } else {
            lua_pushnil(L);
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# null
    * Represents the null primitive from a json file
    * @name json.null
    * @variable
    */

    static const luaL_reg ScriptJson_methods[] =
    {
        {"decode", Json_Decode},
        {"encode", Json_Encode},
        {0, 0}
    };

    void InitializeJson(lua_State* L)
    {
        int top = lua_gettop(L);

        lua_pushvalue(L, LUA_GLOBALSINDEX);
        luaL_register(L, "json", ScriptJson_methods);

        // From lua_cjson.c in order to do comparisons with the json null character
        lua_pushlightuserdata(L, NULL);
        lua_setfield(L, -2, "null");

        lua_pop(L, 2);

        assert(top == lua_gettop(L));
    }
}
