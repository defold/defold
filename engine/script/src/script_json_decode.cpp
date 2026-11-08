// Copyright 2020-2026 The Defold Foundation
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

#include <dlib/json_lexer.h>
#include "script_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
    int lua_cjson_decode(lua_State* L, const char* json, size_t size, int options,
                         int protected_mode, char* error, size_t error_size);
}

namespace dmScript
{
    struct JsonReader
    {
        dmJson::Lexer        m_Lexer;
        const char*          m_Input;
        lua_State*           m_Lua;
        dmJson::StringBuffer m_Strings;
        bool                 m_NullAsUserdata;
        bool                 m_Failed;

        JsonReader(const char* text, uint32_t size, bool null_as_userdata)
        : m_Lexer((char*)text, (char*)text + size), m_Input(text), m_Lua(0)
        , m_NullAsUserdata(null_as_userdata), m_Failed(false)
        {
            m_Lexer.m_AllowNul = true;
            m_Lexer.m_RejectOverflow = false;
        }
        bool String()
        {
            char* text = m_Lexer.ReadString(true, &m_Strings);
            if (m_Strings.m_Failed)
            {
                return luaL_error(m_Lua, "Unable to allocate JSON string buffer") != 0;
            }
            if (!m_Lexer.m_Valid)
            {
                return false;
            }
            lua_pushlstring(m_Lua, text, m_Lexer.m_StringLength);
            return true;
        }
        bool Value(uint32_t depth)
        {
            m_Lexer.Space();
            if (depth > 64 || m_Lexer.m_Cursor == m_Lexer.m_End)
            {
                return false;
            }
            char c = *m_Lexer.m_Cursor;
            if (c == '[' || c == '{')
            {
                bool array = c == '[';
                char end = array ? ']' : '}';
                luaL_checkstack(m_Lua, 3, "JSON result exceeds the Lua stack");
                lua_newtable(m_Lua);
                ++m_Lexer.m_Cursor;
                uint32_t count = 0;
                m_Lexer.Space();
                if (m_Lexer.m_Cursor < m_Lexer.m_End && *m_Lexer.m_Cursor == end)
                {
                    ++m_Lexer.m_Cursor;
                    return true;
                }
                while (m_Lexer.m_Cursor < m_Lexer.m_End)
                {
                    m_Lexer.Space();
                    if (!array)
                    {
                        if (!String())
                        {
                            return false;
                        }
                        m_Lexer.Space();
                        if (m_Lexer.m_Cursor == m_Lexer.m_End || *m_Lexer.m_Cursor++ != ':')
                        {
                            return false;
                        }
                    }
                    if (!Value(depth + 1))
                    {
                        return false;
                    }
                    ++count;
                    if (array)
                    {
                        lua_rawseti(m_Lua, -2, count);
                    }
                    else
                    {
                        lua_rawset(m_Lua, -3);
                    }
                    m_Lexer.Space();
                    if (m_Lexer.m_Cursor == m_Lexer.m_End)
                    {
                        return false;
                    }
                    c = *m_Lexer.m_Cursor++;
                    if (c == end)
                    {
                        return true;
                    }
                    if (c != ',')
                    {
                        return false;
                    }
                }
                return false;
            }
            if (c == '"')
            {
                return String();
            }
            if (c == '-' || (c >= '0' && c <= '9'))
            {
                double value = m_Lexer.ReadNumber(true);
                if (!m_Lexer.m_Valid)
                {
                    return false;
                }
                lua_pushnumber(m_Lua, value);
                return true;
            }
            const char* literal = c == 't' ? "true" : c == 'f' ? "false" : "null";
            uint32_t size = c == 'f' ? 5 : 4;
            if ((uint32_t)(m_Lexer.m_End - m_Lexer.m_Cursor) < size || memcmp(m_Lexer.m_Cursor, literal, size))
            {
                return false;
            }
            m_Lexer.m_Cursor += size;
            if (c != 'n')
            {
                lua_pushboolean(m_Lua, c == 't');
            }
            else if (m_NullAsUserdata)
            {
                lua_pushlightuserdata(m_Lua, 0);
            }
            else
            {
                lua_pushnil(m_Lua);
            }
            return true;
        }
        bool Parse()
        {
            m_Lexer.m_Cursor = (char*)m_Input;
            m_Lexer.m_Valid = true;
            bool result = Value(0);
            m_Lexer.Space();
            return result && m_Lexer.m_Cursor == m_Lexer.m_End;
        }
    };

    static int DecodeProtected(lua_State* L)
    {
        JsonReader* reader = (JsonReader*)lua_touserdata(L, 1);
        reader->m_Lua = L;
        reader->m_Failed = !reader->Parse();
        return reader->m_Failed ? 0 : 1;
    }

    struct LegacyJson
    {
        const char* m_Text;
        size_t      m_Size;
        bool        m_NullAsUserdata;
    };

    static int DecodeLegacyProtected(lua_State* L)
    {
        LegacyJson* json = (LegacyJson*)lua_touserdata(L, 1);
        lua_newtable(L);
        lua_pushboolean(L, json->m_NullAsUserdata);
        lua_setfield(L, 2, "decode_null_as_userdata");
        return lua_cjson_decode(L, json->m_Text, json->m_Size, 2, 1, 0, 0);
    }

    // Keep allocations outside protected calls: Lua errors longjmp past C++
    // destructors. CJSON is retained for its legacy syntax and error messages;
    // Document's strict UTF-8, duplicate-key and depth policies are unchanged.
    int DecodeJson(lua_State* L, const char* text, size_t size, int options, int protected_mode, char* error, size_t error_size)
    {
        int top = lua_gettop(L);
        bool null_as_userdata = false;
        if (lua_istable(L, options))
        {
            lua_getfield(L, options, "decode_null_as_userdata");
            null_as_userdata = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
        }
        luaL_checkstack(L, 2, "JSON conversion requires two stack slots");
        lua_pushcfunction(L, DecodeProtected);
        int status = 0;
        bool legacy = false;
        if (size >= UINT32_MAX)
        {
            snprintf(error, error_size, "JSON input exceeds the addressable size");
            lua_settop(L, top);
            if (protected_mode)
            {
                return luaL_error(L, "%s", error);
            }
            return 0;
        }
        {
            JsonReader reader(text, (uint32_t)size, null_as_userdata);
            lua_pushlightuserdata(L, &reader);
            status = lua_pcall(L, 1, 1, 0);
            legacy = !status && reader.m_Failed;
        }
        if (legacy)
        {
            lua_settop(L, top);
            lua_pushcfunction(L, DecodeLegacyProtected);
            // The legacy scanner expects a terminator even for bounded C++ input.
            char* copy = (char*)malloc(size + 1);
            if (!copy)
            {
                snprintf(error, error_size, "Unable to allocate JSON input buffer");
                lua_settop(L, top);
                if (protected_mode)
                {
                    return luaL_error(L, "%s", error);
                }
                return 0;
            }
            memcpy(copy, text, size);
            copy[size] = 0;
            LegacyJson json = { copy, size, null_as_userdata };
            lua_pushlightuserdata(L, &json);
            status = lua_pcall(L, 1, 1, 0);
            free(copy);
        }
        if (!status)
        {
            return 1;
        }
        // Propagate the original Lua error only after releasing native buffers.
        if (protected_mode)
        {
            return luaL_error(L, "%s", lua_tostring(L, -1));
        }
        snprintf(error, error_size, "%s", lua_tostring(L, -1));
        lua_settop(L, top);
        return 0;
    }
}
