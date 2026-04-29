/*=========================================================================*\
* Simple exception support
* LuaSocket toolkit
\*=========================================================================*/
#include <stdio.h>

#include <dmsdk/dlua/dlua.h>

#include "except.h"

/*=========================================================================*\
* Internal function prototypes.
\*=========================================================================*/
static int global_protect(dlua_State *L);
static int global_newtry(dlua_State *L);
static int protected_(dlua_State *L);
static int finalize(dlua_State *L);
static int do_nothing(dlua_State *L);

/* except functions */
static dluaL_Reg func[] = {
    {"newtry",    global_newtry},
    {"protect",   global_protect},
    {NULL,        NULL}
};

/*-------------------------------------------------------------------------*\
* Try factory
\*-------------------------------------------------------------------------*/
static void wrap(dlua_State *L) {
    dlua_newtable(L);
    dlua_pushnumber(L, 1);
    dlua_pushvalue(L, -3);
    dlua_settable(L, -3);
    dlua_insert(L, -2);
    dlua_pop(L, 1);
}

static int finalize(dlua_State *L) {
    if (!dlua_toboolean(L, 1)) {
        dlua_pushvalue(L, dlua_upvalueindex(1));
        dlua_pcall(L, 0, 0, 0);
        dlua_settop(L, 2);
        wrap(L);
        dlua_error(L);
        return 0;
    } else return dlua_gettop(L);
}

static int do_nothing(dlua_State *L) {
    (void) L;
    return 0;
}

static int global_newtry(dlua_State *L) {
    dlua_settop(L, 1);
    if (dlua_isnil(L, 1)) dlua_pushcfunction(L, do_nothing);
    dlua_pushcclosure(L, finalize, 1);
    return 1;
}

/*-------------------------------------------------------------------------*\
* Protect factory
\*-------------------------------------------------------------------------*/
static int unwrap(dlua_State *L) {
    if (dlua_istable(L, -1)) {
        dlua_pushnumber(L, 1);
        dlua_gettable(L, -2);
        dlua_pushnil(L);
        dlua_insert(L, -2);
        return 1;
    } else return 0;
}

static int protected_(dlua_State *L) {
    dlua_pushvalue(L, dlua_upvalueindex(1));
    dlua_insert(L, 1);
    if (dlua_pcall(L, dlua_gettop(L) - 1, DLUA_MULTRET, 0) != 0) {
        if (unwrap(L)) return 2;
        else dlua_error(L);
        return 0;
    } else return dlua_gettop(L);
}

static int global_protect(dlua_State *L) {
    dlua_pushcclosure(L, protected_, 1);
    return 1;
}

/*-------------------------------------------------------------------------*\
* Init module
\*-------------------------------------------------------------------------*/
int except_open(dlua_State *L) {
#if 0
    dluaL_setfuncs(L, func, 0);
#else
    dluaL_openlib(L, NULL, func, 0);
#endif
    return 0;
}
