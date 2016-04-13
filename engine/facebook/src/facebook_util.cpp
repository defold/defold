#include "facebook_util.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <script/script.h>

static int AppendToString(char* dst, char* src, size_t cursor, size_t dst_size, size_t src_size)
{
    if (dst_size - cursor < src_size) {
        return 0;
    }

    for (int i = 0; i < src_size; ++i) {
        dst[cursor+i] = src[i];
    }

    return 1;
}

int dmFacebook::EscapeJsonString(const char* unescaped, char* escaped, char* end_ptr)
{
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

        // key, must be convertable to string
        size_t kp_len;
        const char* kp = lua_tolstring(L, -2, &kp_len);

        // value, must be table (converts to comma separated string array) or convertable to string
        const char* vp;
        size_t vp_len;
        if (lua_istable(L, -1)) {
            char tmp[json_max_length];
            vp_len = LuaStringCommaArray(L, lua_gettop(L), tmp, json_max_length);
            vp = tmp;
        } else {
            vp = lua_tolstring(L, -1, &vp_len);
        }

        // escape string characters such as "
        char *kp_escaped = (char*)malloc(1+kp_len*2*sizeof(char));
        char *vp_escaped = (char*)malloc(1+vp_len*2*sizeof(char));
        kp_len = EscapeJsonString(kp, kp_escaped, kp_escaped+(1+kp_len*2*sizeof(char)));
        vp_len = EscapeJsonString(vp, vp_escaped, vp_escaped+(1+vp_len*2*sizeof(char)));

        // concat table entry into json format
        size_t tmp_len = kp_len+vp_len+7; // 8 chars: "": ""\0
        char *tmp;
        if (i > 0) {
            tmp_len += 2;
            tmp = (char*)malloc(tmp_len*sizeof(char));
            DM_SNPRINTF(tmp, tmp_len, ", \"%s\": \"%s\"", kp_escaped, vp_escaped);
        } else {
            tmp = (char*)malloc(tmp_len*sizeof(char));
            DM_SNPRINTF(tmp, tmp_len, "\"%s\": \"%s\"", kp_escaped, vp_escaped);
        }
        lua_pop(L, 1);

        // append json string to our output buffer
        if (0 == AppendToString(json, tmp, cursor, json_max_length, tmp_len)) {
            free(tmp);
            return 0;
        }
        free(tmp);

        cursor += tmp_len-1; // Don't include null term in cursor pos
        ++i;
    }

    if (0 == AppendToString(json, "}\0", cursor, json_max_length, 2)) {
        return 0;
    }

    return 1;
}