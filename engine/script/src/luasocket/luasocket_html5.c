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

#include <dmsdk/dlua/dlua.h>

#include "luasocket.h"
#include "except.h"
#include "timeout.h"

/*-------------------------------------------------------------------------*\
* Internal function prototypes
\*-------------------------------------------------------------------------*/
static int global_skip(dlua_State *L);
static int global_unload(dlua_State *L);
static int global_unsupported(dlua_State *L);
static int global_dns_unsupported(dlua_State *L);
static int base_open(dlua_State *L);
static int html5_unsupported_open(dlua_State *L);
static int html5_dns_open(dlua_State *L);

/*-------------------------------------------------------------------------*\
* Modules and functions
\*-------------------------------------------------------------------------*/
static const dluaL_Reg mod[] = {
    {"except", except_open},
    {"timeout", timeout_open},
    {"html5_dns", html5_dns_open},
    {"html5_unsupported", html5_unsupported_open},
    {NULL, NULL}
};

static dluaL_Reg func[] = {
    {"skip",      global_skip},
    {"__unload",  global_unload},
    {NULL,        NULL}
};

/*-------------------------------------------------------------------------*\
* Skip a few arguments
\*-------------------------------------------------------------------------*/
static int global_skip(dlua_State *L) {
    int amount = dluaL_checkint(L, 1);
    int ret = dlua_gettop(L) - amount - 1;
    return ret >= 0 ? ret : 0;
}

/*-------------------------------------------------------------------------*\
* Unloads the library
\*-------------------------------------------------------------------------*/
static int global_unload(dlua_State *L) {
    (void) L;
    return 0;
}

static int global_unsupported(dlua_State *L) {
    dlua_pushnil(L);
    dlua_pushliteral(L, "LuaSocket networking is not supported on HTML5");
    return 2;
}

static int global_dns_unsupported(dlua_State *L) {
    dlua_pushnil(L);
    dlua_pushliteral(L, "LuaSocket DNS is not supported on HTML5");
    return 2;
}

static int html5_unsupported_open(dlua_State *L) {
    static const dluaL_Reg unsupported[] = {
        {"connect", global_unsupported},
        {"tcp", global_unsupported},
        {"tcp6", global_unsupported},
        {"udp", global_unsupported},
        {"udp6", global_unsupported},
        {"select", global_unsupported},
        {NULL, NULL}
    };
#if 0
    dluaL_setfuncs(L, unsupported, 0);
#else
    dluaL_openlib(L, NULL, unsupported, 0);
#endif
    return 0;
}

static int html5_dns_open(dlua_State *L) {
    static const dluaL_Reg dns[] = {
        {"toip", global_dns_unsupported},
        {"getaddrinfo", global_dns_unsupported},
        {"tohostname", global_dns_unsupported},
        {"getnameinfo", global_dns_unsupported},
        {"gethostname", global_dns_unsupported},
        {NULL, NULL}
    };

    dlua_pushliteral(L, "dns");
    dlua_newtable(L);
#if 0
    dluaL_setfuncs(L, dns, 0);
#else
    dluaL_openlib(L, NULL, dns, 0);
#endif
    dlua_settable(L, -3);
    return 0;
}

/*-------------------------------------------------------------------------*\
* Setup basic stuff.
\*-------------------------------------------------------------------------*/
static int base_open(dlua_State *L) {
    {
        /* export functions (and leave namespace table on top of stack) */
#if 0
        dlua_newtable(L);
        dluaL_setfuncs(L, func, 0);
#else
        dluaL_openlib(L, "socket", func, 0);
#endif
#ifdef LUASOCKET_DEBUG
        dlua_pushliteral(L, "_DEBUG");
        dlua_pushboolean(L, 1);
        dlua_rawset(L, -3);
#endif
        dlua_pushliteral(L, "_VERSION");
        dlua_pushstring(L, LUASOCKET_VERSION);
        dlua_rawset(L, -3);
        return 1;
    }
}

/*-------------------------------------------------------------------------*\
* Initializes the HTML5-safe library modules.
\*-------------------------------------------------------------------------*/
LUASOCKET_API int luaopen_socket_core(dlua_State *L) {
    int i;
    base_open(L);
    for (i = 0; mod[i].name; i++) mod[i].func(L);
    return 1;
}
