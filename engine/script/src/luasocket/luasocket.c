/*=========================================================================*\
* LuaSocket toolkit
* Networking support for the Lua language
* Diego Nehab
* 26/11/1999
*
* This library is part of an  effort to progressively increase the network
* connectivity  of  the Lua  language.  The  Lua interface  to  networking
* functions follows the Sockets API  closely, trying to simplify all tasks
* involved in setting up both  client and server connections. The provided
* IO routines, however, follow the Lua  style, being very similar  to the
* standard Lua read and write functions.
\*=========================================================================*/

/*=========================================================================*\
* Standard include files
\*=========================================================================*/
#include <dmsdk/dlua/dlua.h>


/*=========================================================================*\
* LuaSocket includes
\*=========================================================================*/
#include "luasocket.h"
#include "auxiliar.h"
#include "except.h"
#include "timeout.h"
#include "buffer.h"
#include "inet.h"
#include "tcp.h"
#include "udp.h"
#include "select.h"

/*-------------------------------------------------------------------------*\
* Internal function prototypes
\*-------------------------------------------------------------------------*/
static int global_skip(dlua_State *L);
static int global_unload(dlua_State *L);
static int base_open(dlua_State *L);

/*-------------------------------------------------------------------------*\
* Modules and functions
\*-------------------------------------------------------------------------*/
static const dluaL_Reg mod[] = {
    {"auxiliar", auxiliar_open},
    {"except", except_open},
    {"timeout", timeout_open},
    {"buffer", buffer_open},
    {"inet", inet_open},
    {"tcp", tcp_open},
    {"udp", udp_open},
    {"select", select_open},
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
    socket_close();
    return 0;
}

#if 0
int dluaL_typerror (dlua_State *L, int narg, const char *tname) {
  const char *msg = dlua_pushfstring(L, "%s expected, got %s",
                                    tname, dluaL_typename(L, narg));
  return dluaL_argerror(L, narg, msg);
}
#endif

/*-------------------------------------------------------------------------*\
* Setup basic stuff.
\*-------------------------------------------------------------------------*/
static int base_open(dlua_State *L) {
    if (socket_open()) {
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
        /* make version string available to scripts */
        dlua_pushliteral(L, "_VERSION");
        dlua_pushstring(L, LUASOCKET_VERSION);
        dlua_rawset(L, -3);
        return 1;
    } else {
        dlua_pushliteral(L, "unable to initialize library");
        dlua_error(L);
        return 0;
    }
}

/*-------------------------------------------------------------------------*\
* Initializes all library modules.
\*-------------------------------------------------------------------------*/
LUASOCKET_API int luaopen_socket_core(dlua_State *L) {
    int i;
    base_open(L);
    for (i = 0; mod[i].name; i++) mod[i].func(L);
    return 1;
}
