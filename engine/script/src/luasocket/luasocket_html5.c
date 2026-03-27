/*=========================================================================*\
* LuaSocket toolkit
* HTML5 registration for Defold
\*=========================================================================*/

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

#include "lua.h"
#include "lauxlib.h"

#include "luasocket.h"
#include "except.h"
#include "timeout.h"

/*-------------------------------------------------------------------------*\
* Internal function prototypes
\*-------------------------------------------------------------------------*/
static int global_skip(lua_State *L);
static int global_unload(lua_State *L);
static int global_unsupported(lua_State *L);
static int global_dns_unsupported(lua_State *L);
static int base_open(lua_State *L);
static int html5_unsupported_open(lua_State *L);
static int html5_dns_open(lua_State *L);

/*-------------------------------------------------------------------------*\
* Modules and functions
\*-------------------------------------------------------------------------*/
static const luaL_Reg mod[] = {
    {"except", except_open},
    {"timeout", timeout_open},
    {"html5_dns", html5_dns_open},
    {"html5_unsupported", html5_unsupported_open},
    {NULL, NULL}
};

static luaL_Reg func[] = {
    {"skip",      global_skip},
    {"__unload",  global_unload},
    {NULL,        NULL}
};

/*-------------------------------------------------------------------------*\
* Skip a few arguments
\*-------------------------------------------------------------------------*/
static int global_skip(lua_State *L) {
    int amount = luaL_checkint(L, 1);
    int ret = lua_gettop(L) - amount - 1;
    return ret >= 0 ? ret : 0;
}

/*-------------------------------------------------------------------------*\
* Unloads the library
\*-------------------------------------------------------------------------*/
static int global_unload(lua_State *L) {
    (void) L;
    return 0;
}

static int global_unsupported(lua_State *L) {
    (void) L;
    lua_pushnil(L);
    lua_pushliteral(L, "LuaSocket networking is not supported on HTML5");
    return 2;
}

static int global_dns_unsupported(lua_State *L) {
    (void) L;
    lua_pushnil(L);
    lua_pushliteral(L, "LuaSocket DNS is not supported on HTML5");
    return 2;
}

static int html5_unsupported_open(lua_State *L) {
    static const luaL_Reg unsupported[] = {
        {"connect", global_unsupported},
        {"tcp", global_unsupported},
        {"tcp6", global_unsupported},
        {"udp", global_unsupported},
        {"udp6", global_unsupported},
        {"select", global_unsupported},
        {NULL, NULL}
    };
#if LUA_VERSION_NUM > 501 && !defined(LUA_COMPAT_MODULE)
    luaL_setfuncs(L, unsupported, 0);
#else
    luaL_openlib(L, NULL, unsupported, 0);
#endif
    return 0;
}

static int html5_dns_open(lua_State *L) {
    static const luaL_Reg dns[] = {
        {"toip", global_dns_unsupported},
        {"getaddrinfo", global_dns_unsupported},
        {"tohostname", global_dns_unsupported},
        {"getnameinfo", global_dns_unsupported},
        {"gethostname", global_dns_unsupported},
        {NULL, NULL}
    };

    lua_pushliteral(L, "dns");
    lua_newtable(L);
#if LUA_VERSION_NUM > 501 && !defined(LUA_COMPAT_MODULE)
    luaL_setfuncs(L, dns, 0);
#else
    luaL_openlib(L, NULL, dns, 0);
#endif
    lua_settable(L, -3);
    return 0;
}

/*-------------------------------------------------------------------------*\
* Setup basic stuff.
\*-------------------------------------------------------------------------*/
static int base_open(lua_State *L) {
    {
        /* export functions (and leave namespace table on top of stack) */
#if LUA_VERSION_NUM > 501 && !defined(LUA_COMPAT_MODULE)
        lua_newtable(L);
        luaL_setfuncs(L, func, 0);
#else
        luaL_openlib(L, "socket", func, 0);
#endif
#ifdef LUASOCKET_DEBUG
        lua_pushliteral(L, "_DEBUG");
        lua_pushboolean(L, 1);
        lua_rawset(L, -3);
#endif
        lua_pushliteral(L, "_VERSION");
        lua_pushstring(L, LUASOCKET_VERSION);
        lua_rawset(L, -3);
        return 1;
    }
}

/*-------------------------------------------------------------------------*\
* Initializes the HTML5-safe library modules.
\*-------------------------------------------------------------------------*/
LUASOCKET_API int luaopen_socket_core(lua_State *L) {
    int i;
    base_open(L);
    for (i = 0; mod[i].name; i++) mod[i].func(L);
    return 1;
}
