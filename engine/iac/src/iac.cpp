#include <stdlib.h>
#include <assert.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include "iac.h"

// Extension internal platform specific functions
extern int IAC_PlatformSetListener(lua_State* L);


/*# set iac listener
 *
 * The listener callback has the following signature: function(self, payload, type) where payload is a table
 * with the iac payload and type is an iac.TYPE_<TYPE> enumeration.
 *
 * @name iac.set_listener
 * @param listener listener callback function (function)
 *
 * @examples
 * <p>
 * Set the iac listener.
 * </p>
 * <pre>
 * local function iac_listener(self, payload, type)
 *      if type == iac.TYPE_INVOCATION then
 *          -- This was an invocation
 *          print(payload.origin) -- origin may be empty string if it could not be resolved
 *          print(payload.url)
 *      end
 * end
 *
 * local init(self)
 *      ...
 *      iac.set_listener(iac_listener)
 * end
 *
 */
int IAC_SetListener(lua_State* L)
{
    return IAC_PlatformSetListener(L);
}


/*# encode url
 *
 * Encode to url based on parameters
 *
 * @name iac.encode_url
 * @param scheme string containing bundle id (platform dependent)
 * @param host string (or nil)
 * @param path string (or nil)
 * @param query table of attribute value pairs (or nil), i.e. query.x = y
 * @param fragment string (or nil)
 * @return url string
 *
 */
int IAC_EncodeURL(lua_State* L)
{
    int top = lua_gettop(L);
    const char* scheme = luaL_checkstring(L, 1);
    const char* host = lua_isnil(L, 2) ? 0x0 : luaL_checkstring(L, 2);
    const char* path = lua_isnil(L, 3) ? 0x0 : luaL_checkstring(L, 3);
    const char* fragment = lua_isnil(L, 5) ? 0x0 : luaL_checkstring(L, 5);

    const size_t MAX_BUFFER_SIZE = 2048;
    char buffer[MAX_BUFFER_SIZE];
    buffer[0] = 0;
    size_t buffer_index = 0;

#define INC_BUFFER_INDEX(x) \
    if((x < 0) || x >= (MAX_BUFFER_SIZE-buffer_index)) \
        return luaL_error(L, "Encoded buffer overflow (max size %d)", MAX_BUFFER_SIZE); \
    buffer_index += x

    INC_BUFFER_INDEX(DM_SNPRINTF(buffer, MAX_BUFFER_SIZE, "%s:", scheme));
    if(host)
    {
        INC_BUFFER_INDEX(DM_SNPRINTF(buffer+buffer_index, MAX_BUFFER_SIZE-buffer_index, "//%s", host));
    }
    if(path)
    {
        INC_BUFFER_INDEX(DM_SNPRINTF(buffer+buffer_index, MAX_BUFFER_SIZE-buffer_index, "/%s", path));
    }
    if(!lua_isnil(L, 4))
    {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua_pushvalue(L, 4);
        lua_pushnil(L);

        INC_BUFFER_INDEX(DM_SNPRINTF(buffer+buffer_index, MAX_BUFFER_SIZE-buffer_index, "?"));
        for(uint32_t count = 0; lua_next(L, -2) != 0; ++count)
        {
            int type = lua_type(L, -2);
            if (type != LUA_TSTRING)
            {
                return luaL_error(L, "keys in table must be of type string");
            }
            type = lua_type(L, -1);
            if (type != LUA_TSTRING)
            {
                return luaL_error(L, "values in table must be of type string");
            }
            const char* key = lua_tostring(L, -2);
            const char* value = lua_tostring(L, -1);

            INC_BUFFER_INDEX(DM_SNPRINTF(buffer+buffer_index, MAX_BUFFER_SIZE-buffer_index, count == 0 ? "%s=%s" : "&%s=%s", key, value));

            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    if(fragment)
    {
        INC_BUFFER_INDEX(DM_SNPRINTF(buffer+buffer_index, MAX_BUFFER_SIZE-buffer_index, "#%s", fragment));
    }

    lua_pushstring(L, buffer);
    assert(top+1 == lua_gettop(L));
    return 1;
#undef CHECK_BUFFER
}


/*# decode url
 *
 * <p>
 * Returns a table with the following members:
 * scheme, host, path, query (table), fragment.
 * Absent elements are represented by a nil value.
 * Url syntax: scheme:[//host][/][path][?query][#fragment]
 * </p>
 *
 * @name iac.decode_url
 * @param url url string
 * @return table with decoded information
 *
 */
int IAC_DecodeURL(lua_State* L)
{
    int top = lua_gettop(L);
    const char* url = luaL_checkstring(L, 1);
    const char* url_end = url + strlen(url);

    lua_newtable(L);

    // get scheme. Single segment
    lua_pushliteral(L, "scheme");
    const char* scheme_begin = url;
    const char* scheme_end = strrchr(scheme_begin, ':');
    if(scheme_end)
    {
        lua_pushlstring(L, scheme_begin, scheme_end-scheme_begin);
    }
    else
    {
        lua_pushnil(L);
        scheme_end = scheme_begin;
    }

    lua_rawset(L, -3);

    // get fragment. Single segment
    lua_pushliteral(L, "fragment");
    const char* fragment = strrchr(scheme_end, '#');
    if(fragment)
    {
        lua_pushstring(L, fragment+1);
    }
    else
    {
        lua_pushnil(L);
        fragment = url_end;
    }
    lua_rawset(L, -3);

    // get query. Multi segments
    lua_pushliteral(L, "query");
    const char* queries_end = fragment;
    const char* queries_begin = strchr(scheme_end, '?');
    if(queries_begin && (queries_begin < queries_end))
    {
        lua_newtable(L);
        const char* query_begin = queries_begin + 1;
        const char* query_end = queries_end;
        while(query_begin < queries_end)
        {
            query_end = strchr(query_begin, '&');
            if(!query_end)
            {
                query_end = queries_end;
            }
            const char* query_eq = strchr(query_begin, '=');
            if(!query_eq || (query_eq > query_end))
            {
                query_eq = query_end;
            }
            int32_t count = query_eq - query_begin;
            lua_pushlstring(L, query_begin, count);
            ++query_eq;
            count = query_end - query_eq;
            if(count < 1)
            {
                lua_pushnil(L);
            }
            else
            {
                lua_pushlstring(L, query_eq, count);
            }
            lua_rawset(L, -3);
            query_begin = query_end + 1;
        }
    }
    else
    {
        lua_pushnil(L);
        queries_begin = queries_end;
    }
    lua_rawset(L, -3);

    // get host. Single segment
    lua_pushliteral(L, "host");
    const char* host_begin = strchr(scheme_end, '/');
    const char* host_end = scheme_end;
    if(host_begin && host_begin[1] == '/')
    {
        host_begin += 2;
        host_end = strchr(host_begin, '/');
        if(!host_end)
        {
            host_end = queries_begin;
        }
        lua_pushlstring(L, host_begin, host_end-host_begin);
    }
    else
    {
        lua_pushnil(L);
        host_begin = host_end;
    }
    lua_rawset(L, -3);

    // get path. Multi segment
    lua_pushliteral(L, "path");
    const char* path_end = queries_begin;
    const char* path_begin = host_end;
    if(path_begin < path_end)
    {
        path_begin += path_begin[1] == '/' ? 2 : 1;
        lua_pushlstring(L, path_begin, path_end-path_begin);
    }
    else
    {
        lua_pushnil(L);
        path_begin = path_end;
    }
    lua_rawset(L, -3);

    assert(top + 1 == lua_gettop(L));
    return 1;
}


/*# iac type
 *
 * @name iac.TYPE_INVOCATION
 * @variable
 */

static const luaL_reg IAC_methods[] =
{
    {"set_listener", IAC_SetListener},
    {"encode_url", IAC_EncodeURL},
    {"decode_url", IAC_DecodeURL},
    {0, 0}
};


namespace dmIAC
{

    dmExtension::Result Initialize(dmExtension::Params* params)
    {
        lua_State*L = params->m_L;
        int top = lua_gettop(L);
        luaL_register(L, "iac", IAC_methods);

#define SETCONSTANT(name, val) \
            lua_pushnumber(L, (lua_Number) val); \
            lua_setfield(L, -2, #name);

        SETCONSTANT(TYPE_INVOCATION, DM_IAC_EXTENSION_TYPE_INVOCATION);

#undef SETCONSTANT

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return dmExtension::RESULT_OK;
    }


    dmExtension::Result Finalize(dmExtension::Params* params)
    {
        return dmExtension::RESULT_OK;
    }

}
