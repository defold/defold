/* Lua CJSON - JSON support for Lua
 *
 * Copyright (c) 2010-2012  Mark Pulford <mark@kyne.com.au>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Caveats:
 * - JSON "null" values are represented as lightuserdata since Lua
 *   tables cannot contain "nil". Compare with cjson.null.
 * - Invalid UTF-8 characters are not detected and will be passed
 *   untouched. If required, UTF-8 error checking should be done
 *   outside this library.
 * - Javascript comments are not part of the JSON spec, and are not
 *   currently supported.
 *
 * Note: Decoding is slower than encoding. Lua spends significant
 *       time (30%) managing tables when parsing JSON since it is
 *       difficult to know object/array sizes ahead of time.
 */

#ifdef _MSC_VER
    //not #if defined(_WIN32) || defined(_WIN64) because we have strncasecmp in mingw
    #define strncasecmp _strnicmp
    #define strcasecmp _stricmp
#endif

#include <lua/lua.h>
#include <lua/lauxlib.h>

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "strbuf.h"
#include "fpconv.h"

#ifndef CJSON_MODNAME
    #define CJSON_MODNAME   "cjson"
#endif

#ifndef CJSON_VERSION
    #define CJSON_VERSION   "2.1.0"
#endif

#if !defined(isinf) && (defined(USE_INTERNAL_ISINF) || defined(MISSING_ISINF))
    #define isinf(x) (!isnan(x) && isnan((x) - (x)))
#endif

#define DEFAULT_SPARSE_CONVERT 0
#define DEFAULT_SPARSE_RATIO 2
#define DEFAULT_SPARSE_SAFE 10
#define DEFAULT_ENCODE_MAX_DEPTH 1000
#define DEFAULT_DECODE_MAX_DEPTH 1000
#define DEFAULT_ENCODE_INVALID_NUMBERS 0
#define DEFAULT_DECODE_INVALID_NUMBERS 1
#define DEFAULT_ENCODE_KEEP_BUFFER 1
#define DEFAULT_ENCODE_NUMBER_PRECISION 14

#ifdef DISABLE_INVALID_NUMBERS
    #undef DEFAULT_DECODE_INVALID_NUMBERS
    #define DEFAULT_DECODE_INVALID_NUMBERS 0
#endif

typedef enum {
    T_OBJ_BEGIN,
    T_OBJ_END,
    T_ARR_BEGIN,
    T_ARR_END,
    T_STRING,
    T_NUMBER,
    T_BOOLEAN,
    T_NULL,
    T_COLON,
    T_COMMA,
    T_END,
    T_WHITESPACE,
    T_ERROR,
    T_UNKNOWN
} json_token_type_t;

static const char *json_token_type_name[] =
{
    "T_OBJ_BEGIN",
    "T_OBJ_END",
    "T_ARR_BEGIN",
    "T_ARR_END",
    "T_STRING",
    "T_NUMBER",
    "T_BOOLEAN",
    "T_NULL",
    "T_COLON",
    "T_COMMA",
    "T_END",
    "T_WHITESPACE",
    "T_ERROR",
    "T_UNKNOWN",
    NULL
};

typedef struct {
    json_token_type_t ch2token[256];
    char escape2char[256];
    strbuf_t encode_buf;
    int encode_sparse_convert;
    int encode_sparse_ratio;
    int encode_sparse_safe;
    int encode_max_depth;
    int encode_invalid_numbers;
    int encode_number_precision;
    int encode_keep_buffer;
    int decode_invalid_numbers;
    int decode_max_depth;
} json_config_t;

typedef struct {
    const char *data;
    const char *ptr;
    strbuf_t *tmp;
    json_config_t *cfg;
    int current_depth;
} json_parse_t;

typedef struct {
    json_token_type_t type;
    int index;
    union {
        const char *string;
        double number;
        int boolean;
    } value;
    int string_len;
} json_token_t;

static const char *char2escape[256] = {
    "\\u0000", "\\u0001", "\\u0002", "\\u0003",
    "\\u0004", "\\u0005", "\\u0006", "\\u0007",
    "\\b", "\\t", "\\n", "\\u000b",
    "\\f", "\\r", "\\u000e", "\\u000f",
    "\\u0010", "\\u0011", "\\u0012", "\\u0013",
    "\\u0014", "\\u0015", "\\u0016", "\\u0017",
    "\\u0018", "\\u0019", "\\u001a", "\\u001b",
    "\\u001c", "\\u001d", "\\u001e", "\\u001f",
    NULL, NULL, "\\\"", NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, "\\/",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, "\\\\", NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, "\\u007f",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

#if 0 // Defold: unused
static json_config_t *cfg;
static json_config_t *json_fetch_config(lua_State *l)
{
    // json_config_t *cfg;
    cfg = (json_config_t*)lua_touserdata(l, lua_upvalueindex(1));
    if (!cfg)
        luaL_error(l, "BUG: Unable to fetch CJSON configuration");
    return cfg;
}

static json_config_t *json_arg_init(lua_State *l, int args)
{
    luaL_argcheck(l, lua_gettop(l) <= args, args + 1,
                  "found too many arguments");
    while (lua_gettop(l) < args)
        lua_pushnil(l);
    return json_fetch_config(l);
}

static int json_integer_option(lua_State *l, int optindex, int *setting, int min, int max)
{
    char errmsg[64];
    int value;
    if (!lua_isnil(l, optindex))
    {
        value = luaL_checkinteger(l, optindex);
        snprintf(errmsg, sizeof(errmsg), "expected integer between %d and %d", min, max);
        luaL_argcheck(l, min <= value && value <= max, 1, errmsg);
        *setting = value;
    }
    lua_pushinteger(l, *setting);
    return 1;
}

static int json_enum_option(lua_State *l, int optindex, int *setting, const char **options, int bool_true)
{
    static const char *bool_options[] = { "off", "on", NULL };
    if (!options)
    {
        options = bool_options;
        bool_true = 1;
    }
    if (!lua_isnil(l, optindex))
    {
        if (bool_true && lua_isboolean(l, optindex))
            *setting = lua_toboolean(l, optindex) * bool_true;
        else
            *setting = luaL_checkoption(l, optindex, NULL, options);
    }
    if (bool_true && (*setting == 0 || *setting == bool_true))
        lua_pushboolean(l, *setting);
    else
        lua_pushstring(l, options[*setting]);
    return 1;
}

static int json_cfg_encode_sparse_array(lua_State *l)
{
    json_config_t *cfg = json_arg_init(l, 3);
    json_enum_option(l, 1, &cfg->encode_sparse_convert, NULL, 1);
    json_integer_option(l, 2, &cfg->encode_sparse_ratio, 0, INT_MAX);
    json_integer_option(l, 3, &cfg->encode_sparse_safe, 0, INT_MAX);
    return 3;
}

static int json_cfg_encode_max_depth(lua_State *l)
{
    json_config_t *cfg = json_arg_init(l, 1);
    return json_integer_option(l, 1, &cfg->encode_max_depth, 1, INT_MAX);
}

static int json_cfg_decode_max_depth(lua_State *l)
{
    json_config_t *cfg = json_arg_init(l, 1);
    return json_integer_option(l, 1, &cfg->decode_max_depth, 1, INT_MAX);
}

static int json_cfg_encode_number_precision(lua_State *l)
{
    json_config_t *cfg = json_arg_init(l, 1);
    return json_integer_option(l, 1, &cfg->encode_number_precision, 1, 14);
}

static int json_cfg_encode_keep_buffer(lua_State *l)
{
    json_config_t *cfg = json_arg_init(l, 1);
    int old_value;
    old_value = cfg->encode_keep_buffer;
    json_enum_option(l, 1, &cfg->encode_keep_buffer, NULL, 1);
    if (old_value ^ cfg->encode_keep_buffer)
    {
        if (cfg->encode_keep_buffer)
            strbuf_init(&cfg->encode_buf, 0);
        else
            strbuf_free(&cfg->encode_buf);
    }
    return 1;
}

#if defined(DISABLE_INVALID_NUMBERS) && !defined(USE_INTERNAL_FPCONV)
void json_verify_invalid_number_setting(lua_State *l, int *setting)
{
    if (*setting == 1)
    {
        *setting = 0;
        luaL_error(l, "Infinity, NaN, and/or hexadecimal numbers are not supported.");
    }
}
#else
#define json_verify_invalid_number_setting(l, s)    do { } while(0)
#endif

static int json_cfg_encode_invalid_numbers(lua_State *l)
{
    static const char *options[] = { "off", "on", "null", NULL };
    json_config_t *cfg = json_arg_init(l, 1);
    json_enum_option(l, 1, &cfg->encode_invalid_numbers, options, 1);
    json_verify_invalid_number_setting(l, &cfg->encode_invalid_numbers);
    return 1;
}

static int json_cfg_decode_invalid_numbers(lua_State *l)
{
    json_config_t *cfg = json_arg_init(l, 1);
    json_enum_option(l, 1, &cfg->decode_invalid_numbers, NULL, 1);
    json_verify_invalid_number_setting(l, &cfg->encode_invalid_numbers);
    return 1;
}

static int json_destroy_config(lua_State *l)
{
    // json_config_t *cfg;
    cfg = (json_config_t*)lua_touserdata(l, 1);
    if (cfg)
        strbuf_free(&cfg->encode_buf);
    cfg = NULL;
    return 0;
}
#endif

static int json_initialize_config(json_config_t* cfg)
{
	int i;
	cfg->encode_sparse_convert = DEFAULT_SPARSE_CONVERT;
    cfg->encode_sparse_ratio = DEFAULT_SPARSE_RATIO;
    cfg->encode_sparse_safe = DEFAULT_SPARSE_SAFE;
    cfg->encode_max_depth = DEFAULT_ENCODE_MAX_DEPTH;
    cfg->decode_max_depth = DEFAULT_DECODE_MAX_DEPTH;
    cfg->encode_invalid_numbers = DEFAULT_ENCODE_INVALID_NUMBERS;
    cfg->decode_invalid_numbers = DEFAULT_DECODE_INVALID_NUMBERS;
    cfg->encode_keep_buffer = DEFAULT_ENCODE_KEEP_BUFFER;
    cfg->encode_number_precision = DEFAULT_ENCODE_NUMBER_PRECISION;
#if DEFAULT_ENCODE_KEEP_BUFFER > 0
    if (!strbuf_init(&cfg->encode_buf, 0))
    {
        return 0;
    }
#endif
    for (i = 0; i < 256; i++)
        cfg->ch2token[i] = T_ERROR;
    cfg->ch2token['{'] = T_OBJ_BEGIN;
    cfg->ch2token['}'] = T_OBJ_END;
    cfg->ch2token['['] = T_ARR_BEGIN;
    cfg->ch2token[']'] = T_ARR_END;
    cfg->ch2token[','] = T_COMMA;
    cfg->ch2token[':'] = T_COLON;
    cfg->ch2token['\0'] = T_END;
    cfg->ch2token[' '] = T_WHITESPACE;
    cfg->ch2token['\t'] = T_WHITESPACE;
    cfg->ch2token['\n'] = T_WHITESPACE;
    cfg->ch2token['\r'] = T_WHITESPACE;
    cfg->ch2token['f'] = T_UNKNOWN;
    cfg->ch2token['i'] = T_UNKNOWN;
    cfg->ch2token['I'] = T_UNKNOWN;
    cfg->ch2token['n'] = T_UNKNOWN;
    cfg->ch2token['N'] = T_UNKNOWN;
    cfg->ch2token['t'] = T_UNKNOWN;
    cfg->ch2token['"'] = T_UNKNOWN;
    cfg->ch2token['+'] = T_UNKNOWN;
    cfg->ch2token['-'] = T_UNKNOWN;
    for (i = 0; i < 10; i++)
        cfg->ch2token['0' + i] = T_UNKNOWN;
    for (i = 0; i < 256; i++)
        cfg->escape2char[i] = 0;
    cfg->escape2char['"'] = '"';
    cfg->escape2char['\\'] = '\\';
    cfg->escape2char['/'] = '/';
    cfg->escape2char['b'] = '\b';
    cfg->escape2char['t'] = '\t';
    cfg->escape2char['n'] = '\n';
    cfg->escape2char['f'] = '\f';
    cfg->escape2char['r'] = '\r';
    cfg->escape2char['u'] = 'u';
    return 1;
}

#if 0 // Defold: unused
static void json_create_config(lua_State *l)
{
    // json_config_t *cfg;
    cfg = (json_config_t*) lua_newuserdata(l, sizeof(*cfg));

    lua_newtable(l);
    lua_pushcfunction(l, json_destroy_config);
    lua_setfield(l, -2, "__gc");
    lua_setmetatable(l, -2);

    // Defold change: We don't create config as userdata
    json_initialize_config(cfg);
}
#endif

#define WRAP_STR_BUF_GENERIC_ERROR(l, stmt) if (!(stmt)) luaL_error(l, "Error when encoding json");

static void json_encode_exception(lua_State *l, json_config_t *cfg, strbuf_t *json, int lindex, const char *reason)
{
    if (!cfg->encode_keep_buffer)
        strbuf_free(json);
    luaL_error(l, "Cannot serialise %s: %s", lua_typename(l, lua_type(l, lindex)), reason);
}

static void json_append_string(lua_State *l, strbuf_t *json, int lindex)
{
    const char *escstr;
    int i;
    const char *str;
    size_t len;
    str = lua_tolstring(l, lindex, &len);

    WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_ensure_empty_length(json, len * 6 + 2));
    strbuf_append_char_unsafe(json, '\"');
    for (i = 0; i < len; i++)
    {
        escstr = char2escape[(unsigned char)str[i]];
        if (escstr)
        {
            WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_string(json, escstr))
        }
        else
            strbuf_append_char_unsafe(json, str[i]);
    }
    strbuf_append_char_unsafe(json, '\"');
}

static int lua_array_length(lua_State *l, json_config_t *cfg, strbuf_t *json)
{
    double k;
    int max;
    int items;
    max = 0;
    items = 0;
    lua_pushnil(l);
    while (lua_next(l, -2) != 0)
    {
        if (lua_type(l, -2) == LUA_TNUMBER && (k = lua_tonumber(l, -2)))
        {
            if (floor(k) == k && k >= 1)
            {
                if (k > max)
                    max = k;
                items++;
                lua_pop(l, 1);
                continue;
            }
        }
        lua_pop(l, 2);
        return -1;
    }
    if (cfg->encode_sparse_ratio > 0 &&
        max > items * cfg->encode_sparse_ratio &&
        max > cfg->encode_sparse_safe)
        {
            if (!cfg->encode_sparse_convert)
                json_encode_exception(l, cfg, json, -1, "excessively sparse array");
            return -1;
        }
    return max;
}

static void json_check_encode_depth(lua_State *l, json_config_t *cfg, int current_depth, strbuf_t *json)
{
    if (current_depth <= cfg->encode_max_depth && lua_checkstack(l, 3))
        return;
    if (!cfg->encode_keep_buffer)
        strbuf_free(json);
    luaL_error(l, "Cannot serialise, excessive nesting (%d)", current_depth);
}

static void json_append_data(lua_State *l, json_config_t *cfg, int current_depth, strbuf_t *json);
static void json_append_array(lua_State *l, json_config_t *cfg, int current_depth, strbuf_t *json, int array_length)
{
    int comma, i;
    WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_char(json, '['));
    comma = 0;
    for (i = 1; i <= array_length; i++)
    {
        if (comma)
        {
            WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_char(json, ','));
        }
        else
            comma = 1;
        lua_rawgeti(l, -1, i);
        json_append_data(l, cfg, current_depth, json);
        lua_pop(l, 1);
    }

    WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_char(json, ']'));
}

static void json_append_number(lua_State *l, json_config_t *cfg, strbuf_t *json, int lindex)
{
    double num = lua_tonumber(l, lindex);
    int len;
    if (cfg->encode_invalid_numbers == 0)
    {
        if (isinf(num) || isnan(num))
            json_encode_exception(l, cfg, json, lindex, "must not be NaN or Inf");
    } else if (cfg->encode_invalid_numbers == 1)
    {
        if (isnan(num))
        {
            WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_mem(json, "nan", 3));
            return;
        }
    } else
    {
        if (isinf(num) || isnan(num))
        {
            WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_mem(json, "null", 4));
            return;
        }
    }
    WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_ensure_empty_length(json, FPCONV_G_FMT_BUFSIZE));
    len = fpconv_g_fmt(strbuf_empty_ptr(json), num, cfg->encode_number_precision);
    strbuf_extend_length(json, len);
}

static void json_append_object(lua_State *l, json_config_t *cfg, int current_depth, strbuf_t *json)
{
    int comma, keytype;
    WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_char(json, '{'));
    lua_pushnil(l);
    comma = 0;
    while (lua_next(l, -2) != 0)
    {
        if (comma)
            strbuf_append_char(json, ',');
        else
            comma = 1;
        keytype = lua_type(l, -2);
        if (keytype == LUA_TNUMBER)
        {
            WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_char(json, '"'));
            json_append_number(l, cfg, json, -2);
            strbuf_append_mem(json, "\":", 2);
        } else if (keytype == LUA_TSTRING)
        {
            json_append_string(l, json, -2);
            WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_char(json, ':'));
        } else
        {
            json_encode_exception(l, cfg, json, -2, "table key must be a number or string");
        }
        json_append_data(l, cfg, current_depth, json);
        lua_pop(l, 1);
    }
    WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_char(json, '}'));
}

static void json_append_data(lua_State *l, json_config_t *cfg, int current_depth, strbuf_t *json)
{
    int len;
    switch (lua_type(l, -1))
    {
        case LUA_TSTRING:
            json_append_string(l, json, -1);
            break;
        case LUA_TNUMBER:
            json_append_number(l, cfg, json, -1);
            break;
        case LUA_TBOOLEAN:
            if (lua_toboolean(l, -1))
            {
                WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_mem(json, "true", 4));
            }
            else
            {
                WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_mem(json, "false", 5));
            }
            break;
        case LUA_TTABLE:
            current_depth++;
            json_check_encode_depth(l, cfg, current_depth, json);
            len = lua_array_length(l, cfg, json);
            if (len > 0)
                json_append_array(l, cfg, current_depth, json, len);
            else
                json_append_object(l, cfg, current_depth, json);
            break;
        case LUA_TNIL:
            WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_mem(json, "null", 4));
            break;
        case LUA_TUSERDATA:
        case LUA_TFUNCTION:
        case LUA_TTHREAD:
            {
                unsigned long long pointer_addr = (unsigned long long)lua_topointer(l, -1);
                char str_buffer[16];
                int len = snprintf(str_buffer, 16, "%llx", pointer_addr);
                WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_mem(json, "\"object at 0x", 13));
                WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_mem(json, str_buffer, len));
                WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_mem(json, "\"", 1));
                break;
            }
        case LUA_TLIGHTUSERDATA:
            if (lua_touserdata(l, -1) == NULL)
            {
                WRAP_STR_BUF_GENERIC_ERROR(l, strbuf_append_mem(json, "null", 4));
                break;
            }
        default:
            /* Remaining types (LUA_TLIGHTUSERDATA) cannot be serialised */
            json_encode_exception(l, cfg, json, -1, "type not supported");
            /* never returns */
    }
}

#if 0 // Defold: unused
static int json_encode(lua_State *l)
{
    json_config_t *cfg = json_fetch_config(l);
    strbuf_t local_encode_buf;
    strbuf_t *encode_buf;
    char *json;
    int len;
    luaL_argcheck(l, lua_gettop(l) == 1, 1, "expected 1 argument");
    if (!cfg->encode_keep_buffer)
    {
        encode_buf = &local_encode_buf;
        strbuf_init(encode_buf, 0);
    } else
    {
        encode_buf = &cfg->encode_buf;
        strbuf_reset(encode_buf);
    }
    json_append_data(l, cfg, 0, encode_buf);
    json = strbuf_string(encode_buf, &len);
    lua_pushlstring(l, json, len);
    if (!cfg->encode_keep_buffer)
        strbuf_free(encode_buf);
    return 1;
}
#endif

static void json_process_value(lua_State *l, json_parse_t *json, json_token_t *token);
static int hexdigit2int(char hex)
{
    if ('0' <= hex  && hex <= '9')
        return hex - '0';
    hex |= 0x20;
    if ('a' <= hex && hex <= 'f')
        return 10 + hex - 'a';
    return -1;
}

static int decode_hex4(const char *hex)
{
    int digit[4];
    int i;
    for (i = 0; i < 4; i++)
    {
        digit[i] = hexdigit2int(hex[i]);
        if (digit[i] < 0)
        {
            return -1;
        }
    }
    return (digit[0] << 12) +
           (digit[1] << 8) +
           (digit[2] << 4) +
            digit[3];
}

static int codepoint_to_utf8(char *utf8, int codepoint)
{
    if (codepoint <= 0x7F)
    {
        utf8[0] = codepoint;
        return 1;
    }
    if (codepoint <= 0x7FF)
    {
        utf8[0] = (codepoint >> 6) | 0xC0;
        utf8[1] = (codepoint & 0x3F) | 0x80;
        return 2;
    }
    if (codepoint <= 0xFFFF)
    {
        utf8[0] = (codepoint >> 12) | 0xE0;
        utf8[1] = ((codepoint >> 6) & 0x3F) | 0x80;
        utf8[2] = (codepoint & 0x3F) | 0x80;
        return 3;
    }
    if (codepoint <= 0x1FFFFF)
    {
        utf8[0] = (codepoint >> 18) | 0xF0;
        utf8[1] = ((codepoint >> 12) & 0x3F) | 0x80;
        utf8[2] = ((codepoint >> 6) & 0x3F) | 0x80;
        utf8[3] = (codepoint & 0x3F) | 0x80;
        return 4;
    }
    return 0;
}

static int json_append_unicode_escape(json_parse_t *json)
{
    char utf8[4];
    int codepoint;
    int surrogate_low;
    int len;
    int escape_len = 6;
    codepoint = decode_hex4(json->ptr + 2);
    if (codepoint < 0)
        return -1;
    if ((codepoint & 0xF800) == 0xD800)
    {
        if (codepoint & 0x400)
            return -1;
        if (*(json->ptr + escape_len) != '\\' ||
            *(json->ptr + escape_len + 1) != 'u')
                return -1;
        surrogate_low = decode_hex4(json->ptr + 2 + escape_len);
        if (surrogate_low < 0)
            return -1;
        if ((surrogate_low & 0xFC00) != 0xDC00)
            return -1;
        codepoint = (codepoint & 0x3FF) << 10;
        surrogate_low &= 0x3FF;
        codepoint = (codepoint | surrogate_low) + 0x10000;
        escape_len = 12;
    }
    len = codepoint_to_utf8(utf8, codepoint);
    if (!len)
        return -1;
    strbuf_append_mem_unsafe(json->tmp, utf8, len);
    json->ptr += escape_len;
    return 0;
}

static void json_set_token_error(json_token_t *token, json_parse_t *json, const char *errtype)
{
    token->type = T_ERROR;
    token->index = json->ptr - json->data;
    token->value.string = errtype;
}

static void json_next_string_token(json_parse_t *json, json_token_t *token)
{
    char *escape2char = json->cfg->escape2char;
    char ch;
    assert(*json->ptr == '"');
    json->ptr++;
    strbuf_reset(json->tmp);
    while ((ch = *json->ptr) != '"')
    {
        if (!ch)
        {
            json_set_token_error(token, json, "unexpected end of string");
            return;
        }
        if (ch == '\\')
        {
            ch = *(json->ptr + 1);
            ch = escape2char[(unsigned char)ch];
            if (ch == 'u')
            {
                if (json_append_unicode_escape(json) == 0)
                    continue;
                json_set_token_error(token, json, "invalid unicode escape code");
                return;
            }
            if (!ch)
            {
                json_set_token_error(token, json, "invalid escape code");
                return;
            }
            json->ptr++;
        }
        strbuf_append_char_unsafe(json->tmp, ch);
        json->ptr++;
    }
    json->ptr++;
    strbuf_ensure_null(json->tmp);
    token->type = T_STRING;
    token->value.string = strbuf_string(json->tmp, &token->string_len);
}

static int json_is_invalid_number(json_parse_t *json)
{
    const char *p = json->ptr;
    if (*p == '+')
        return 1;
    if (*p == '-')
        p++;
    if (*p == '0')
    {
        int ch2 = *(p + 1);
        if ((ch2 | 0x20) == 'x' || ('0' <= ch2 && ch2 <= '9'))
            return 1;
        return 0;
    } else if (*p <= '9')
    {
        return 0;
    }
    if (!strncasecmp(p, "inf", 3))
        return 1;
    if (!strncasecmp(p, "nan", 3))
        return 1;
    return 0;
}

static void json_next_number_token(json_parse_t *json, json_token_t *token)
{
    char *endptr;
    token->type = T_NUMBER;
    token->value.number = fpconv_strtod(json->ptr, &endptr);
    if (json->ptr == endptr)
        json_set_token_error(token, json, "invalid number");
    else
        json->ptr = endptr;
    return;
}

static void json_next_token(json_parse_t *json, json_token_t *token)
{
    const json_token_type_t *ch2token = json->cfg->ch2token;
    int ch;
    while (1)
    {
        ch = (unsigned char)*(json->ptr);
        token->type = ch2token[ch];
        if (token->type != T_WHITESPACE)
            break;
        json->ptr++;
    }
    token->index = json->ptr - json->data;
    if (token->type == T_ERROR)
    {
        json_set_token_error(token, json, "invalid token");
        return;
    }
    if (token->type == T_END)
        return;
    if (token->type != T_UNKNOWN)
    {
        json->ptr++;
        return;
    }
    if (ch == '"')
    {
        json_next_string_token(json, token);
        return;
    } else if (ch == '-' || ('0' <= ch && ch <= '9'))
    {
        if (!json->cfg->decode_invalid_numbers && json_is_invalid_number(json))
        {
            json_set_token_error(token, json, "invalid number");
            return;
        }
        json_next_number_token(json, token);
        return;
    } else if (!strncmp(json->ptr, "true", 4))
    {
        token->type = T_BOOLEAN;
        token->value.boolean = 1;
        json->ptr += 4;
        return;
    } else if (!strncmp(json->ptr, "false", 5))
    {
        token->type = T_BOOLEAN;
        token->value.boolean = 0;
        json->ptr += 5;
        return;
    } else if (!strncmp(json->ptr, "null", 4))
    {
        token->type = T_NULL;
        json->ptr += 4;
        return;
    } else if (json->cfg->decode_invalid_numbers && json_is_invalid_number(json))
    {
        json_next_number_token(json, token);
        return;
    }
    json_set_token_error(token, json, "invalid token");
}

static void json_throw_parse_error(lua_State *l, json_parse_t *json, const char *exp, json_token_t *token)
{
    const char *found;
    strbuf_free(json->tmp);
    if (token->type == T_ERROR)
        found = token->value.string;
    else
        found = json_token_type_name[token->type];
    luaL_error(l, "Expected %s but found %s at character %d", exp, found, token->index + 1);
}

static inline void json_decode_ascend(json_parse_t *json)
{
    json->current_depth--;
}

static void json_decode_descend(lua_State *l, json_parse_t *json, int slots)
{
    json->current_depth++;
    if (json->current_depth <= json->cfg->decode_max_depth &&
        lua_checkstack(l, slots)) {
        return;
    }
    strbuf_free(json->tmp);
    luaL_error(l, "Found too many nested data structures (%d) at character %ld", json->current_depth, json->ptr - json->data);
}

static void json_parse_object_context(lua_State *l, json_parse_t *json)
{
    json_token_t token;
    json_decode_descend(l, json, 3);
    lua_newtable(l);
    json_next_token(json, &token);
    if (token.type == T_OBJ_END)
    {
        json_decode_ascend(json);
        return;
    }
    while (1)
    {
        if (token.type != T_STRING)
            json_throw_parse_error(l, json, "object key string", &token);
        lua_pushlstring(l, token.value.string, token.string_len);
        json_next_token(json, &token);
        if (token.type != T_COLON)
            json_throw_parse_error(l, json, "colon", &token);
        json_next_token(json, &token);
        json_process_value(l, json, &token);
        lua_rawset(l, -3);
        json_next_token(json, &token);
        if (token.type == T_OBJ_END)
        {
            json_decode_ascend(json);
            return;
        }
        if (token.type != T_COMMA)
            json_throw_parse_error(l, json, "comma or object end", &token);
        json_next_token(json, &token);
    }
}

static void json_parse_array_context(lua_State *l, json_parse_t *json)
{
    json_token_t token;
    int i;
    json_decode_descend(l, json, 2);
    lua_newtable(l);
    json_next_token(json, &token);
    if (token.type == T_ARR_END)
    {
        json_decode_ascend(json);
        return;
    }
    for (i = 1; ; i++)
    {
        json_process_value(l, json, &token);
        lua_rawseti(l, -2, i);
        json_next_token(json, &token);
        if (token.type == T_ARR_END)
        {
            json_decode_ascend(json);
            return;
        }
        if (token.type != T_COMMA)
            json_throw_parse_error(l, json, "comma or array end", &token);
        json_next_token(json, &token);
    }
}

static void json_process_value(lua_State *l, json_parse_t *json, json_token_t *token)
{
    switch (token->type) {
    case T_STRING:
        lua_pushlstring(l, token->value.string, token->string_len);
        break;;
    case T_NUMBER:
        lua_pushnumber(l, token->value.number);
        break;;
    case T_BOOLEAN:
        lua_pushboolean(l, token->value.boolean);
        break;;
    case T_OBJ_BEGIN:
        json_parse_object_context(l, json);
        break;;
    case T_ARR_BEGIN:
        json_parse_array_context(l, json);
        break;;
    case T_NULL:
        lua_pushlightuserdata(l, NULL);
        break;;
    default:
        json_throw_parse_error(l, json, "value", token);
    }
}

#if 0 // Defold: unused
static int json_decode(lua_State *l)
{
    json_parse_t json;
    json_token_t token;
    size_t json_len;
    luaL_argcheck(l, lua_gettop(l) == 1, 1, "expected 1 argument");
    json.cfg = json_fetch_config(l);
    json.data = luaL_checklstring(l, 1, &json_len);
    json.current_depth = 0;
    json.ptr = json.data;
    if (json_len >= 2 && (!json.data[0] || !json.data[1]))
        luaL_error(l, "JSON parser does not support UTF-16 or UTF-32");
    json.tmp = strbuf_new(json_len);
    json_next_token(&json, &token);
    json_process_value(l, &json, &token);
    json_next_token(&json, &token);
    if (token.type != T_END)
        json_throw_parse_error(l, &json, "the end", &token);
    strbuf_free(json.tmp);
    return 1;
}

/* ===== INITIALISATION ===== */

#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 502
static void luaL_setfuncs (lua_State *l, const luaL_Reg *reg, int nup)
{
    int i;

    luaL_checkstack(l, nup, "too many upvalues");
    for (; reg->name != NULL; reg++)
    {
        for (i = 0; i < nup; i++)
            lua_pushvalue(l, -nup);
        lua_pushcclosure(l, reg->func, nup);
        lua_setfield(l, -(nup + 2), reg->name);
    }
    lua_pop(l, nup);
}
#endif

static int json_protect_conversion(lua_State *l)
{
    int err;
    luaL_argcheck(l, lua_gettop(l) == 1, 1, "expected 1 argument");
    lua_pushvalue(l, lua_upvalueindex(1));
    lua_insert(l, 1);
    err = lua_pcall(l, 1, 1, 0);
    if (!err)
        return 1;
    if (err == LUA_ERRRUN)
    {
        lua_pushnil(l);
        lua_insert(l, -2);
        return 2;
    }
    return luaL_error(l, "Memory allocation error in CJSON protected call");
}

static int lua_cjson_new(lua_State *l)
{
    int top = lua_gettop(l);

    luaL_Reg reg[] =
    {
        { "encode", json_encode },
        { "decode", json_decode },
        { NULL, NULL }
    };
    // create table
    lua_newtable(l);

    json_create_config(l);

    // fill with funcs
    luaL_setfuncs(l, reg, 1);
    lua_pushlightuserdata(l, NULL);
    lua_setfield(l, -2, "null");

    // pop the created table
    lua_setglobal(l, CJSON_MODNAME);

    assert(top == lua_gettop(l));
    return 0;
}
#endif

////////////////////////////////////////////////////////////
// Defold: Expose needed functions to our script_json module
////////////////////////////////////////////////////////////

// The major difference between this function and the internal
// cjson json_encode function is that we don't use the config
// object created by cjson. Instead we initialize and create
// a default config here to avoid passing the config as
// a user data object.
int lua_cjson_encode(lua_State *L)
{
	json_config_t cfg;
    strbuf_t local_encode_buf;
    strbuf_t *encode_buf;
    char *json;
    int len;

    json_initialize_config(&cfg);
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "expected 1 argument");
    if (!cfg.encode_keep_buffer)
    {
        encode_buf = &local_encode_buf;
        WRAP_STR_BUF_GENERIC_ERROR(L, strbuf_init(encode_buf, 0));
    } else
    {
        encode_buf = &cfg.encode_buf;
        strbuf_reset(encode_buf);
    }
    json_append_data(L, &cfg, 0, encode_buf);
    json = strbuf_string(encode_buf, &len);
    lua_pushlstring(L, json, len);
    if (!cfg.encode_keep_buffer)
        strbuf_free(encode_buf);
    return 1;
}

#undef WRAP_STR_BUF_GENERIC_ERROR

