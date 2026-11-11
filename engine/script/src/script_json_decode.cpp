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

#include <dlib/dstrings.h>
#include <dlib/json_lexer.h>
#include "script_json.h"
#include <stdlib.h>
#include <string.h>
extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
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

        JsonReader(const char* text, uint32_t size, bool null_as_userdata)
        : m_Lexer((char*)text, (char*)text + size), m_Input(text), m_Lua(0)
        , m_NullAsUserdata(null_as_userdata)
        {
            m_Lexer.m_AllowNul = true;
            m_Lexer.m_AllowControl = true;
            m_Lexer.m_RejectOverflow = false;
        }
        bool Word(const char* text, const char* word, uint32_t length) const
        {
            if ((uint32_t)(m_Lexer.m_End - text) < length)
            {
                return false;
            }
            for (uint32_t i = 0; i < length; ++i)
            {
                if ((text[i] | 32) != word[i])
                {
                    return false;
                }
            }
            return true;
        }
        static int Hex(char c)
        {
            if (c >= '0' && c <= '9')
            {
                return c - '0';
            }
            c |= 32;
            return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        }
        bool HexNumber(char* text, bool negative, double* value)
        {
            char* cursor = text + 2;
            uint64_t coefficient = 0;
            uint32_t digits = 0;
            int64_t power = 0;
            bool fraction = false, any = false, sticky = false;
            while (cursor < m_Lexer.m_End)
            {
                if (*cursor == '.' && !fraction)
                {
                    fraction = true;
                    ++cursor;
                    continue;
                }
                int digit = Hex(*cursor);
                if (digit < 0)
                {
                    break;
                }
                any = true;
                power -= fraction ? 4 : 0;
                if (coefficient || digit)
                {
                    if (digits < 16)
                    {
                        coefficient = (coefficient << 4) | digit;
                        ++digits;
                    }
                    else
                    {
                        power += 4;
                        sticky |= digit != 0;
                    }
                }
                ++cursor;
            }
            if (!any)
            {
                return false;
            }
            if (cursor < m_Lexer.m_End && (*cursor == 'p' || *cursor == 'P'))
            {
                char* exponent = cursor + 1;
                bool minus = exponent < m_Lexer.m_End && *exponent == '-';
                if (exponent < m_Lexer.m_End && (*exponent == '+' || *exponent == '-'))
                {
                    ++exponent;
                }
                char* begin = exponent;
                int64_t amount = 0;
                while (exponent < m_Lexer.m_End && *exponent >= '0' && *exponent <= '9')
                {
                    if (amount < INT64_C(10000000000))
                    {
                        amount = amount * 10 + *exponent - '0';
                    }
                    ++exponent;
                }
                if (exponent != begin)
                {
                    power += minus ? -amount : amount;
                    cursor = exponent;
                }
            }
            uint64_t bits = 0;
            if (coefficient && power > 2048)
            {
                bits = UINT64_C(0x7ff0000000000000);
            }
            else if (coefficient && power >= -2048)
            {
                // Retain at least 61 significant bits plus a sticky bit. The
                // shared integer rounding handles ties and subnormals exactly.
                uint64_t words[3] = { sticky ? UINT64_C(1) : 0, coefficient, 0 };
                bits = dmJson::RoundProduct(words, (int)power - 64);
            }
            bits |= negative ? UINT64_C(0x8000000000000000) : 0;
            memcpy(value, &bits, sizeof(bits));
            m_Lexer.m_Cursor = cursor;
            return true;
        }
        // Keep number scanning and conversion in the caller's loop. Avoiding
        // a call per value helps numeric arrays, particularly on Wasm.
        DM_FORCE_INLINE bool Number(double* value)
        {
            char* text = m_Lexer.m_Cursor;
            bool negative = *text == '-';
            if (*text == '-' || *text == '+')
            {
                ++text;
            }
            char first = text < m_Lexer.m_End ? *text : 0;
            if (first == '0' && m_Lexer.m_End - text >= 2 && (text[1] | 32) == 'x' && HexNumber(text, negative, value))
            {
                return true;
            }
            bool infinity = (first | 32) == 'i' && Word(text, "inf", 3);
            if (infinity || ((first | 32) == 'n' && Word(text, "nan", 3)))
            {
                char* cursor = text + (infinity && Word(text, "infinity", 8) ? 8 : 3);
                if (!infinity && cursor < m_Lexer.m_End && *cursor == '(')
                {
                    char* payload = cursor + 1;
                    while (payload < m_Lexer.m_End && *payload && *payload != ')')
                    {
#if !defined(__APPLE__)
                        // Darwin also accepts punctuation in NaN payloads.
                        if (!((*payload >= '0' && *payload <= '9') ||
                              ((*payload | 32) >= 'a' && (*payload | 32) <= 'z') || *payload == '_'))
                        {
                            break;
                        }
#endif
                        ++payload;
                    }
                    if (payload < m_Lexer.m_End && *payload == ')')
                    {
                        cursor = payload + 1;
                    }
                }
                uint64_t bits = infinity ? UINT64_C(0x7ff0000000000000) : UINT64_C(0x7ff8000000000000);
                bits |= negative ? UINT64_C(0x8000000000000000) : 0;
                memcpy(value, &bits, sizeof(bits));
                m_Lexer.m_Cursor = cursor;
                return true;
            }
            *value = m_Lexer.ReadNumber(true, true);
            return m_Lexer.m_Valid;
        }
        bool NumberStart() const
        {
            char* text = m_Lexer.m_Cursor;
            return text < m_Lexer.m_End && (*text == '-' || *text == '+' || (*text >= '0' && *text <= '9') ||
                                            Word(text, "inf", 3) || Word(text, "nan", 3));
        }
        // Classify strings only while reporting errors, keeping diagnostics
        // out of the shared successful-string path.
        const char* StringError(char* text, char** position) const
        {
            char* cursor = text + 1;
            while (cursor < m_Lexer.m_End && *cursor)
            {
                char* escape = cursor++;
                if (*escape == '"')
                {
                    *position = text;
                    return "T_STRING";
                }
                if (*escape != '\\')
                {
                    continue;
                }
                *position = escape;
                if (cursor == m_Lexer.m_End)
                {
                    return "invalid escape code";
                }
                char c = *cursor++;
                if (c != 'u')
                {
                    if (!c || !strchr("\"\\/bfnrt", c))
                    {
                        return "invalid escape code";
                    }
                    continue;
                }
                if (m_Lexer.m_End - cursor < 4)
                {
                    return "invalid unicode escape code";
                }
                int codepoint = dmJson::Unicode(cursor);
                cursor += 4;
                if (codepoint < 0 || (codepoint >= 0xdc00 && codepoint <= 0xdfff))
                {
                    return "invalid unicode escape code";
                }
                if (codepoint >= 0xd800 && codepoint <= 0xdbff)
                {
                    if (m_Lexer.m_End - cursor < 6 || cursor[0] != '\\' || cursor[1] != 'u')
                    {
                        return "invalid unicode escape code";
                    }
                    int low = dmJson::Unicode(cursor + 2);
                    if (low < 0xdc00 || low > 0xdfff)
                    {
                        return "invalid unicode escape code";
                    }
                    cursor += 6;
                }
            }
            *position = cursor;
            return "unexpected end of string";
        }
        bool Error(const char* expected, const char* found = 0, char* position = 0)
        {
            if (!position)
            {
                position = m_Lexer.m_Cursor;
            }
            if (!found)
            {
                char c = position < m_Lexer.m_End ? *position : 0;
                found = c == 0 ? "T_END" : c == '[' ? "T_ARR_BEGIN" : c == ']' ? "T_ARR_END" :
                        c == '{' ? "T_OBJ_BEGIN" : c == '}' ? "T_OBJ_END" : c == ':' ? "T_COLON" :
                        c == ',' ? "T_COMMA" : "invalid token";
                if (c == '"')
                {
                    found = StringError(position, &position);
                }
                else if (NumberStart())
                {
                    double value;
                    found = Number(&value) ? "T_NUMBER" : "invalid number";
                }
                else if ((m_Lexer.m_End - position >= 4 && !memcmp(position, "true", 4)) ||
                         (m_Lexer.m_End - position >= 5 && !memcmp(position, "false", 5)))
                {
                    found = "T_BOOLEAN";
                }
                else if (m_Lexer.m_End - position >= 4 && !memcmp(position, "null", 4))
                {
                    found = "T_NULL";
                }
            }
            char message[192];
            dmSnPrintf(message, sizeof(message), "Expected %s but found %s at character %u", expected, found,
                       (uint32_t)(position - m_Input) + 1);
            return luaL_error(m_Lua, "%s", message) != 0;
        }
        bool String(const char* expected)
        {
            if (m_Lexer.m_Cursor == m_Lexer.m_End || *m_Lexer.m_Cursor != '"')
            {
                return Error(expected);
            }
            char* begin = m_Lexer.m_Cursor;
            char* text = m_Lexer.ReadString(true, &m_Strings);
            if (m_Strings.m_Failed)
            {
                return luaL_error(m_Lua, "Unable to allocate JSON string buffer") != 0;
            }
            if (!m_Lexer.m_Valid)
            {
                char* position;
                const char* found = StringError(begin, &position);
                return Error(expected, found, position);
            }
            lua_pushlstring(m_Lua, text, m_Lexer.m_StringLength);
            return true;
        }
        bool Value(uint32_t depth)
        {
            m_Lexer.Space();
            if (m_Lexer.m_Cursor == m_Lexer.m_End || !*m_Lexer.m_Cursor)
            {
                return Error("value");
            }
            char c = *m_Lexer.m_Cursor;
            if (c == '[' || c == '{')
            {
                bool array = c == '[';
                char end = array ? ']' : '}';
                if (depth >= 1000 || !lua_checkstack(m_Lua, 3))
                {
                    char message[128];
                    dmSnPrintf(message, sizeof(message), "Found too many nested data structures (%u) at character %u",
                               depth + 1, (uint32_t)(m_Lexer.m_Cursor - m_Input) + 1);
                    return luaL_error(m_Lua, "%s", message) != 0;
                }
                ++m_Lexer.m_Cursor;
                uint32_t count = 0;
                m_Lexer.Space();
                if (m_Lexer.m_Cursor < m_Lexer.m_End && *m_Lexer.m_Cursor == end)
                {
                    lua_newtable(m_Lua);
                    ++m_Lexer.m_Cursor;
                    return true;
                }
                // The final length is unknown. Start nonempty arrays with two
                // slots and let Lua grow them normally; empty tables stay small.
                lua_createtable(m_Lua, array ? 2 : 0, 0);
                while (true)
                {
                    m_Lexer.Space();
                    if (!array)
                    {
                        if (!String("object key string"))
                        {
                            return false;
                        }
                        m_Lexer.Space();
                        if (m_Lexer.m_Cursor == m_Lexer.m_End || *m_Lexer.m_Cursor != ':')
                        {
                            return Error("colon");
                        }
                        ++m_Lexer.m_Cursor;
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
                    c = m_Lexer.m_Cursor < m_Lexer.m_End ? *m_Lexer.m_Cursor : 0;
                    if (c == end)
                    {
                        ++m_Lexer.m_Cursor;
                        return true;
                    }
                    if (c != ',')
                    {
                        return Error(array ? "comma or array end" : "comma or object end");
                    }
                    ++m_Lexer.m_Cursor;
                }
            }
            if (c == '"')
            {
                return String("value");
            }
            if (NumberStart())
            {
                char* begin = m_Lexer.m_Cursor;
                double value;
                if (!Number(&value))
                {
                    return Error("value", "invalid number", begin);
                }
                lua_pushnumber(m_Lua, value);
                return true;
            }
            const char* literal = c == 't' ? "true" : c == 'f' ? "false" : "null";
            uint32_t size = c == 'f' ? 5 : 4;
            if ((uint32_t)(m_Lexer.m_End - m_Lexer.m_Cursor) < size || memcmp(m_Lexer.m_Cursor, literal, size))
            {
                return Error("value");
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
            if (m_Lexer.m_End - m_Input >= 2 && (!m_Input[0] || !m_Input[1]))
            {
                return luaL_error(m_Lua, "JSON parser does not support UTF-16 or UTF-32") != 0;
            }
            m_Lexer.m_Cursor = (char*)m_Input;
            m_Lexer.m_Valid = true;
            bool result = Value(0);
            m_Lexer.Space();
            if (result && m_Lexer.m_Cursor < m_Lexer.m_End && *m_Lexer.m_Cursor)
            {
                return Error("the end");
            }
            return result;
        }
    };

    static int DecodeProtected(lua_State* L)
    {
        JsonReader* reader = (JsonReader*)lua_touserdata(L, 1);
        reader->m_Lua = L;
        return reader->Parse() ? 1 : 0;
    }

    // Keep allocations outside protected calls: Lua errors longjmp past C++
    // destructors. Lua's legacy syntax is handled here; Document's strict
    // UTF-8, duplicate-key and depth policies are unchanged.
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
        if (size >= UINT32_MAX)
        {
            dmSnPrintf(error, error_size, "JSON input exceeds the addressable size");
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
        dmSnPrintf(error, error_size, "%s", lua_tostring(L, -1));
        lua_settop(L, top);
        return 0;
    }
}
