#include "facebook_util.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <script/script.h>

static int AppendToString(char* dst, char* src, size_t cursor, size_t dst_size, size_t src_size)
{
    if (dst_size - cursor < src_size) {
        return 0;
    }

    for (int i = 0; i < (int)src_size; ++i) {
        dst[cursor+i] = src[i];
    }

    return 1;
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

size_t dmFacebook::LuaStringCommaArray(lua_State* L, int index, char* buffer, size_t buffer_size)
{
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

    return out_buffer_size;
}

static int AppendJsonKeyValue(char* json, size_t json_size, int &cursor, char* key, size_t key_len, char* value, size_t value_len)
{
    // allocate buffers to hold the escaped key and value strings
    char *key_escaped   = (char*)malloc(1+key_len*2*sizeof(char));
    char *value_escaped = (char*)malloc(1+value_len*2*sizeof(char));

    // escape string characters such as "
    key_len   = dmFacebook::EscapeJsonString(key, key_escaped, key_escaped+(1+key_len*2*sizeof(char)));
    value_len = dmFacebook::EscapeJsonString(value, value_escaped, value_escaped+(1+value_len*2*sizeof(char)));

    // concat table entry into json format
    // allocate enough to include; "key": "value"\0
    size_t concat_entry_len = key_len+value_len+7; //
    char* concat_entry = (char*)malloc(concat_entry_len*sizeof(char));
    DM_SNPRINTF(concat_entry, concat_entry_len, "\"%s\": \"%s\"", key_escaped, value_escaped);

    if (key_escaped) {
        free(key_escaped);
    }
    if (value_escaped) {
        free(value_escaped);
    }

    // append string to our output buffer
    if (0 == AppendToString(json, concat_entry, cursor, json_size, concat_entry_len)) {
        if (concat_entry) {
            free(concat_entry);
        }
        return 0;
    }
    if (concat_entry) {
        free(concat_entry);
    }

    cursor += concat_entry_len-1; // Don't include null term in cursor pos

    return 1;
}

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
                value_len = LuaStringCommaArray(L, lua_gettop(L), tmp, json_max_length);
                value = tmp;
            }
        } else {
            value = (char*)lua_tolstring(L, -1, &value_len);
        }
        lua_pop(L, 1);

        // append a entry separator
        if (i > 0) {
            if (0 == AppendToString(json, (char*)", ", cursor, json_max_length, 2)) {
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

    if (0 == AppendToString(json, (char*)"}\0", cursor, json_max_length, 2)) {
        return 0;
    }

    return 1;
}
