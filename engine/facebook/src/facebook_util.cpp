#include "facebook.h"
#include "facebook_util.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <script/script.h>

#define MAX_DUPLICATE_TABLE_RECURSION 4

static int WriteString(char* dst, size_t dst_size, char* src, size_t src_size)
{
    if (dst_size < src_size) {
        return 0;
    }

    for (char* src_end = src + src_size; src != src_end; ++src) {
        *dst = *src;
        dst++;
    }

    return src_size;
}


static int LuaTableToJson(lua_State* L, int index, char* json_buffer, size_t json_buffer_size)
{
    assert(lua_istable(L, index));
    int top = lua_gettop(L);

    size_t cursor = 0;
    if (json_buffer_size < 1) {
        return 0;
    }

    bool is_array = dmFacebook::IsLuaArray(L, index);

    if (is_array) {
        json_buffer[cursor++] = '[';
    } else {
        json_buffer[cursor++] = '{';
    }

    lua_pushnil(L);
    int i = 0;
    while (lua_next(L, index) != 0) {

        // add commas
        if (i>0) {
            if (!WriteString(json_buffer+cursor, json_buffer_size-cursor, (char*)",", 1)) {
                lua_pop(L, 1);
                assert(top == lua_gettop(L));
                return 0;
            }
            cursor++;
        }

        // write key (skipped if this is an array)
        if (!is_array) {
            lua_pushvalue(L, -2);
            int r = dmFacebook::LuaValueToJson(L, lua_gettop(L), json_buffer+cursor, json_buffer_size-cursor);
            lua_pop(L, 1);
            cursor+=r;

            if (!WriteString(json_buffer+cursor, json_buffer_size-cursor, (char*)":", 1)) {
                lua_pop(L, 1);
                assert(top == lua_gettop(L));
                return 0;
            }
            cursor++;
        }

        // write value
        int r = dmFacebook::LuaValueToJson(L, lua_gettop(L), json_buffer+cursor, json_buffer_size-cursor);
        if (!r) {
            lua_pop(L, 1);
            assert(top == lua_gettop(L));
            return 0;
        }
        cursor+=r;

        lua_pop(L, 1);

        ++i;
    }

    // write ending of json object or array
    char* end = (char*)"}\0";
    if (is_array) {
        end = (char*)"]\0";
    }
    if (!WriteString(json_buffer+cursor, json_buffer_size-cursor, end, 2)) {
        assert(top == lua_gettop(L));
        return 0;
    }
    cursor+=1;

    assert(top == lua_gettop(L));
    return cursor;
}

bool dmFacebook::IsLuaArray(lua_State* L, int index)
{
    assert(lua_istable(L, index));
    int top = lua_gettop(L);

    lua_pushnil(L);
    int i = 1;
    bool table_is_array = true;
    while (lua_next(L, index) != 0) {

        // check key type
        if (LUA_TNUMBER != lua_type(L, -2) || i != (int)lua_tonumber(L, -2)) {
            table_is_array = false;
            lua_pop(L, 2);
            break;
        }
        i++;

        lua_pop(L, 1);
    }

    assert(top == lua_gettop(L));
    return table_is_array;
}

int dmFacebook::EscapeJsonString(const char* unescaped, char* escaped, char* end_ptr)
{
    if (escaped == NULL) {
        return 0;
    }
    // keep going through the unescaped until null terminating
    // always need at least 3 chars left in escaped buffer,
    // 2 for char expanding + 1 for null term
    char* esc_start = escaped;
    while (*unescaped && escaped <= end_ptr - 3) {

        switch (*unescaped) {

            case '\x22':
                *escaped++ = '\\';
                *escaped++ = '"';
                break;
            case '\x5C':
                *escaped++ = '\\';
                *escaped++ = '\\';
                break;
            case '\x08':
                *escaped++ = '\\';
                *escaped++ = '\b';
                break;
            case '\x0C':
                *escaped++ = '\\';
                *escaped++ = '\f';
                break;
            case '\x0A':
                *escaped++ = '\\';
                *escaped++ = '\n';
                break;
            case '\x0D':
                *escaped++ = '\\';
                *escaped++ = '\r';
                break;
            case '\x09':
                *escaped++ = '\\';
                *escaped++ = '\t';
                break;

            default:
                *escaped++ = *unescaped;
                break;

        }

        unescaped++;

    }

    *escaped = 0;

    return (escaped - esc_start);
}

int dmFacebook::WriteEscapedJsonString(char* json, size_t json_size, const char* value, size_t value_len)
{
    // allocate buffers to hold the escaped key and value strings
    char *value_escaped = (char*)malloc(1+value_len*2*sizeof(char));

    // escape string characters such as "
    value_len = dmFacebook::EscapeJsonString(value, value_escaped, value_escaped+(1+value_len*2*sizeof(char)));

    int wrote_len = 0;
    if (value_len+3 <= json_size) {
        wrote_len += WriteString(json, json_size, (char*)"\"", 1);
        wrote_len += WriteString(json+1, json_size-1, value_escaped, value_len);
        wrote_len += WriteString(json+1+value_len, json_size-1-value_len, (char*)"\"", 1);
    }

    free(value_escaped);
    return wrote_len;
}

size_t dmFacebook::LuaStringCommaArray(lua_State* L, int index, char* buffer, size_t buffer_size)
{
    int top = lua_gettop(L);
    lua_pushnil(L);
    *buffer = 0;
    size_t out_buffer_size = 0;
    while (lua_next(L, index) != 0)
    {
        if (!lua_isstring(L, -1))
            luaL_error(L, "array arguments can only be strings (not %s)", lua_typename(L, lua_type(L, -1)));
        if (*buffer != 0) {
            dmStrlCat(buffer, ",", buffer_size);
            out_buffer_size += 1;
        }
        size_t lua_str_size;
        const char* entry_str = lua_tolstring(L, -1, &lua_str_size);
        dmStrlCat(buffer, entry_str, buffer_size);
        out_buffer_size += lua_str_size;
        lua_pop(L, 1);
    }

    assert(top == lua_gettop(L));
    return out_buffer_size;
}

int dmFacebook::LuaValueToJson(lua_State* L, int index, char* buffer, size_t buffer_size)
{
    int value_type = lua_type(L, index);
    size_t len = 0;
    const char* v = 0;
    switch (value_type) {
        case LUA_TSTRING:
            // String entries need to be escaped and enclosed with citations
            v = lua_tolstring(L, index, &len);
            len = dmFacebook::WriteEscapedJsonString(buffer, buffer_size, v, len);
            break;
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, index)) {
                v = "true";
                len = 4;
            } else {
                v = "false";
                len = 5;
            }
            len = WriteString(buffer, buffer_size, (char*)v, len);
            break;
        case LUA_TNUMBER:
            v = lua_tolstring(L, index, &len);
            len = WriteString(buffer, buffer_size, (char*)v, len);
            break;
        case LUA_TTABLE:
            len = LuaTableToJson(L, index, buffer, buffer_size);
            break;
        case LUA_TNIL:
            len = WriteString(buffer, buffer_size, (char*)"null", 4);
            break;
        default:
            dmLogError("unserializeable entry: %s (%x)", lua_typename(L, -1), value_type);
            break;
    }

    buffer[len] = '\0';

    // TODO check buffer overflow

    return len;
}

int dmFacebook::DialogTableToEmscripten(lua_State* L, const char* dialog_type, int from_index, int to_index)
{
    int r = DuplicateLuaTable(L, from_index, to_index, 0);
    if (!r) {
        dmLogError("Could not create Emscripten dialog param table.");
        return 0;
    }

    if (strcmp(dialog_type, "apprequests") == 0)
    {
        // need to convert "to" field into comma separated list if table
        lua_getfield(L, to_index, "to");
        if (lua_type(L, lua_gettop(L)) == LUA_TTABLE) {
            char t_buf[2048];
            size_t t = LuaStringCommaArray(L, lua_gettop(L), t_buf, 2048);
            lua_pushstring(L, t_buf);
            lua_setfield(L, to_index, "to");
        }
        lua_pop(L, 1);

        // need to convert "filters" field from enum to array of string(s)
        lua_getfield(L, to_index, "filters");
        if (lua_type(L, lua_gettop(L)) == LUA_TNUMBER) {

            switch (lua_tointeger(L, lua_gettop(L))) {
                case GAMEREQUEST_FILTER_APPUSERS:
                    lua_newtable(L);
                    lua_pushnumber(L, 1);
                    lua_pushstring(L, "app_users");
                    lua_rawset(L, -3);
                    lua_setfield(L, to_index, "filters");
                    break;
                case GAMEREQUEST_FILTER_APPNONUSERS:
                    lua_newtable(L);
                    lua_pushnumber(L, 1);
                    lua_pushstring(L, "app_non_users");
                    lua_rawset(L, -3);
                    lua_setfield(L, to_index, "filters");
                    break;
            }
        }
        lua_pop(L, 1);
    }

    return 1;
}

int dmFacebook::DuplicateLuaTable(lua_State* L, int from_index, int to_index, int recursion)
{
    assert(lua_istable(L, from_index));
    assert(lua_istable(L, to_index));
    if (recursion >= MAX_DUPLICATE_TABLE_RECURSION) {
        dmLogError("Max recursion depth reached (%d) when duplicating Lua table.", MAX_DUPLICATE_TABLE_RECURSION);
        return 0;
    }

    int top = lua_gettop(L);
    int ret = 1;

    lua_pushnil(L);
    while (lua_next(L, from_index) != 0 && ret) {

        // we need to be using absolute stack positions
        int t_top = lua_gettop(L);
        int key_index = t_top-1;
        int value_index = t_top;

        int key_type = lua_type(L, key_index);
        int value_type = lua_type(L, value_index);

        // copy key
        switch (key_type) {
            case LUA_TSTRING:
            case LUA_TNUMBER:
                lua_pushvalue(L, key_index);
                break;
            default:
                dmLogError("invalid key type: %s (%x)", lua_typename(L, key_type), key_type);
                lua_pushnil(L);
                ret = 0;
                break;
        }

        // copy value
        switch (value_type) {
            case LUA_TSTRING:
            case LUA_TNUMBER:
                lua_pushvalue(L, value_index);
                break;
            case LUA_TTABLE:
                lua_newtable(L);
                ret = DuplicateLuaTable(L, value_index, lua_gettop(L), recursion++);
                break;
            default:
                dmLogError("invalid value type: %s (%x)", lua_typename(L, value_type), value_type);
                lua_pushnil(L);
                ret = 0;
                break;
        }
        lua_rawset(L, to_index);

        lua_pop(L, 1);
    }

    assert(top == lua_gettop(L));
    return ret;
}

/*
int dmFacebook::LuaDialogParamsToJson(lua_State* L, int index, char* json, size_t json_max_length)
{

    int cursor = 0;
    if (json_max_length < 1) {
        return 0;
    }

    json[cursor++] = '{';

    lua_pushnil(L);
    int i = 0;
    while (lua_next(L, index) != 0) {

        size_t key_len = 0;
        size_t value_len = 0;
        char* key = (char*)lua_tolstring(L, -2, &key_len);
        char* value = NULL;

        // value, must be table (converts to comma separated string array) or convertable to string
        char* tmp = NULL;
        if (lua_istable(L, -1)) {
            tmp = (char*)malloc(json_max_length*sizeof(char));
            if (tmp != NULL) {
                // value_len = LuaStringCommaArray(L, lua_gettop(L), tmp, json_max_length);
                value = tmp;
            }
        } else {
            value = (char*)lua_tolstring(L, -1, &value_len);
        }
        lua_pop(L, 1);

        // append a entry separator
        if (i > 0) {
            if (0 == WriteString(json, (char*)", ", cursor, json_max_length, 2)) {
                if (tmp) {
                    free(tmp);
                }
                return 0;
            }
            cursor += 2;
        }

        // Create key-value-string pair and append to JSON string
        if (0 == AppendJsonKeyValue(json, json_max_length, cursor, key, key_len, value, value_len)) {
            if (tmp) {
                free(tmp);
            }
            return 0;
        }

        // free 'tmp' if it was allocated above
        if (tmp) {
            free(tmp);
        }

        ++i;
    }

    if (0 == WriteString(json, (char*)"}\0", cursor, json_max_length, 2)) {
        return 0;
    }

    return 1;
}
*/
