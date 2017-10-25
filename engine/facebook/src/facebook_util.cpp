#include "facebook_private.h"
#include "facebook_util.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <script/script.h>

static int WriteString(char* dst, size_t dst_size, const char* src, size_t src_size)
{
    if (!dst) {
        return src_size; // If we're only counting the number of characters needed
    }

    if (dst_size < src_size) {
        return 0;
    }

    for (size_t i = 0; i < src_size; ++i) {
        dst[i] = src[i];
    }

    return src_size;
}

void dmFacebook::JoinCStringArray(const char** array, uint32_t arrlen,
    char* buffer, uint32_t buflen, const char* delimiter)
{
    if (array == 0x0 || arrlen == 0 || buffer == 0x0 || buflen == 0)
    {
        return;
    }

    for (uint32_t i = 0; i < arrlen; ++i)
    {
        if (i > 0)
        {
            (void) dmStrlCat(buffer, delimiter, buflen);
        }

        (void) dmStrlCat(buffer, array[i], buflen);
    }
}

int dmFacebook::luaTableToCArray(lua_State* L, int index, char** buffer, uint32_t buffer_size)
{
    uint32_t entries = 0;

    if (L == 0x0 || buffer == 0x0 || buffer_size == 0)
    {
        return entries;
    }

    lua_pushnil(L);
    while (lua_next(L, index))
    {
        if (lua_isstring(L, -1))
        {
            if (entries < buffer_size)
            {
                const char* permission = lua_tostring(L, -1);

                uint32_t permission_buffer_len = strlen(permission) + 1;
                char* permission_buffer = (char*) malloc(permission_buffer_len);
                DM_SNPRINTF(permission_buffer, permission_buffer_len, "%s", permission);

                buffer[entries++] = permission_buffer;
            }
        }
        else
        {
            for (uint32_t i = 0; i < entries; ++i)
            {
                free(buffer[i]);
            }

            lua_pop(L, 1);
            return -1;
        }

        lua_pop(L, 1);
    }

    return entries;
}

int dmFacebook::LuaValueToJsonValue(lua_State* L, int index, char* buffer, size_t buffer_size)
{
    // need space for null term
    if (buffer && buffer_size < 1) {
        return 0;
    }
    if( buffer ) {
        buffer_size--;
    }

    int top = lua_gettop(L);
    lua_pushvalue(L, index);
    index = lua_gettop(L);
    int value_type = lua_type(L, index);
    size_t len = 0;

    switch (value_type) {
        case LUA_TSTRING: {
            // String entries need to be escaped and enclosed with citations
            const char* v = lua_tolstring(L, index, &len);
            len = dmFacebook::WriteEscapedJsonString(buffer, buffer_size, v, len);
            break;
        }
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, index)) {
                len = WriteString(buffer, buffer_size, "true", 4);
            } else {
                len = WriteString(buffer, buffer_size, "false", 5);
            }
            break;
        case LUA_TNUMBER: {
            const char* v = lua_tolstring(L, index, &len);
            len = WriteString(buffer, buffer_size, v, len);
            break;
        }
        case LUA_TTABLE:
            len = dmFacebook::LuaTableToJson(L, index, buffer, buffer_size);
            break;
        case LUA_TNIL:
            len = WriteString(buffer, buffer_size, "null", 4);
            break;
        default:
            dmLogError("unserializeable entry: %s (%x)", lua_typename(L, -1), value_type);
            break;
    }

    if (buffer) {
        buffer[len] = '\0';
    }

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
    return len;
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
    if (unescaped == NULL || escaped == NULL) {
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
    if (value == NULL) {
        return 0;
    }

    // allocate buffers to hold the escaped key and value strings
    char *value_escaped = (char*)malloc((1+value_len*2)*sizeof(char));

    // escape string characters such as "
    value_len = dmFacebook::EscapeJsonString(value, value_escaped, value_escaped+((1+value_len*2)*sizeof(char)));

    int wrote_len = 0;
    if (json == 0) {
        wrote_len = value_len + 2;
    }
    else if (value_len+2 <= json_size) {
        wrote_len += WriteString(json, json_size, "\"", 1);
        wrote_len += WriteString(json+1, json_size-1, (const char*)value_escaped, value_len);
        wrote_len += WriteString(json+1+value_len, json_size-1-value_len, "\"", 1);
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

int dmFacebook::LuaTableToJson(lua_State* L, int index, char* json_buffer, size_t json_buffer_size)
{
    assert(lua_istable(L, index));
    int top = lua_gettop(L);

    size_t cursor = 0;
    if (json_buffer && json_buffer_size < 1) {
        return 0;
    }

    bool is_array = dmFacebook::IsLuaArray(L, index);

    if(json_buffer) {
        json_buffer[cursor] = is_array ? '[' : '{';
    }
    cursor++;

    lua_pushnil(L);
    int i = 0;
    while (lua_next(L, index) != 0) {

        // add commas
        if (i>0) {
            if (json_buffer && !WriteString(json_buffer+cursor, json_buffer_size-cursor, ",", 1)) {
                lua_pop(L, 2);
                assert(top == lua_gettop(L));
                return 0;
            }
            cursor++;
        }

        // write key (skipped if this is an array)
        if (!is_array) {
            lua_pushvalue(L, -2);
            int r = LuaValueToJsonValue(L, lua_gettop(L), json_buffer ? json_buffer+cursor : 0, json_buffer ? json_buffer_size-cursor : 0);
            lua_pop(L, 1);
            cursor+=r;

            if (json_buffer && !WriteString(json_buffer+cursor, json_buffer_size-cursor, ":", 1)) {
                lua_pop(L, 2);
                assert(top == lua_gettop(L));
                return 0;
            }
            cursor++;
        }

        // write value
        int r = LuaValueToJsonValue(L, lua_gettop(L), json_buffer ? json_buffer+cursor : 0, json_buffer ? json_buffer_size-cursor : 0);
        if (!r) {
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return 0;
        }
        cursor+=r;

        lua_pop(L, 1);

        ++i;
    }

    // write ending of json object or array
    const char* end = is_array ? (const char*)"]\0" : (const char*)"}\0";
    if (json_buffer && !WriteString(json_buffer+cursor, json_buffer_size-cursor, end, 2)) {
        assert(top == lua_gettop(L));
        return 0;
    }
    cursor+=1;

    assert(top == lua_gettop(L));
    return cursor;
}

void dmFacebook::SplitStringToTable(lua_State* L, int index, const char* str, char split)
{
    int i = 1;
    const char* it = str;
    while (true)
    {
        char c = *it;

        if (c == split || c == '\0')
        {
            lua_pushlstring(L, str, it - str);
            lua_rawseti(L, index, i++);
            if (c == '\0') {
                break;
            }

            str = it+1;
        }
        it++;
    }
}

int dmFacebook::DialogTableToAndroid(lua_State* L, const char* dialog_type, int from_index, int to_index)
{
    int top = lua_gettop(L);
    int r = DuplicateLuaTable(L, from_index, to_index, 4);
    if (!r) {
        dmLogError("Could not create Android specific dialog param table.");
        assert(top == lua_gettop(L));
        return 0;
    }

    if (strcmp(dialog_type, "apprequest") == 0 || strcmp(dialog_type, "apprequests") == 0)
    {
        // need to convert "to" field into comma separated list if table
        lua_getfield(L, to_index, "to");
        if (lua_type(L, lua_gettop(L)) == LUA_TTABLE) {
            char t_buf[2048];
            LuaStringCommaArray(L, lua_gettop(L), t_buf, sizeof(t_buf));
            lua_pushstring(L, t_buf);
            lua_setfield(L, to_index, "to");
        }
        lua_pop(L, 1);
    }

    assert(top == lua_gettop(L));
    return 1;
}

int dmFacebook::DialogTableToEmscripten(lua_State* L, const char* dialog_type, int from_index, int to_index)
{
    int top = lua_gettop(L);
    int r = DuplicateLuaTable(L, from_index, to_index, 4);
    if (!r) {
        dmLogError("Could not create Emscripten specific dialog param table.");
        assert(top == lua_gettop(L));
        return 0;
    }

    if (strcmp(dialog_type, "apprequest") == 0 || strcmp(dialog_type, "apprequests") == 0)
    {
        // need to convert "to" field into comma separated list if table
        lua_getfield(L, to_index, "to");
        if (lua_type(L, lua_gettop(L)) == LUA_TTABLE) {
            char t_buf[2048];
            LuaStringCommaArray(L, lua_gettop(L), t_buf, sizeof(t_buf));
            lua_pushstring(L, t_buf);
            lua_setfield(L, to_index, "to");
        }
        lua_pop(L, 1);

        // if recipients is a table, concat to comma separated list and override "to" field
        // (Since FB JS not have "recipient" field, we utilize the "to" field to have same
        // functionality and API as other platforms.)
        lua_getfield(L, to_index, "recipients");
        if (lua_type(L, lua_gettop(L)) == LUA_TTABLE) {
            char t_buf[2048];
            LuaStringCommaArray(L, lua_gettop(L), t_buf, sizeof(t_buf));
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

        // need to convert "action_type" field from enum to string
        lua_getfield(L, to_index, "action_type");
        if (lua_type(L, lua_gettop(L)) == LUA_TNUMBER) {
            switch (lua_tointeger(L, lua_gettop(L))) {
                case GAMEREQUEST_ACTIONTYPE_SEND:
                    lua_pushstring(L, "send");
                    lua_setfield(L, to_index, "action_type");
                    break;
                case GAMEREQUEST_ACTIONTYPE_ASKFOR:
                    lua_pushstring(L, "askfor");
                    lua_setfield(L, to_index, "action_type");
                    break;
                case GAMEREQUEST_ACTIONTYPE_TURN:
                    lua_pushstring(L, "turn");
                    lua_setfield(L, to_index, "action_type");
                    break;
            }
        }
        lua_pop(L, 1);
    }

    assert(top == lua_gettop(L));
    return 1;
}

int dmFacebook::DuplicateLuaTable(lua_State* L, int from_index, int to_index, unsigned int max_recursion_depth)
{
    assert(lua_istable(L, from_index));
    assert(lua_istable(L, to_index));
    if (max_recursion_depth == 0) {
        dmLogError("Max recursion depth reached when duplicating Lua table.");
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
                ret = DuplicateLuaTable(L, value_index, lua_gettop(L), max_recursion_depth-1);
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

size_t dmFacebook::CountStringArrayLength(lua_State* L, int table_index, size_t& entry_count)
{
    int top = lua_gettop(L);

    lua_pushnil(L);

    size_t needed_size = 0;
    while (lua_next(L, table_index) != 0)
    {
        if (!lua_isstring(L, -1)) {
            return luaL_error(L, "array arguments can only be strings (not %s)", lua_typename(L, lua_type(L, -1)));
        }

        size_t lua_str_size = 0;
        lua_tolstring(L, -1, &lua_str_size);
        needed_size += lua_str_size;
        entry_count++;
        lua_pop(L, 1);
    }

    assert(top == lua_gettop(L));

    return needed_size;
}
