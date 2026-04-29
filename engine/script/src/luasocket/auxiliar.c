/*=========================================================================*\
* Auxiliar routines for class hierarchy manipulation
* LuaSocket toolkit
\*=========================================================================*/
#include <string.h>
#include <stdio.h>

#include "auxiliar.h"

/*=========================================================================*\
* Exported functions
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Initializes the module
\*-------------------------------------------------------------------------*/
int auxiliar_open(dlua_State *L) {
    (void) L;
    return 0;
}

/*-------------------------------------------------------------------------*\
* Creates a new class with given methods
* Methods whose names start with __ are passed directly to the metatable.
\*-------------------------------------------------------------------------*/
void auxiliar_newclass(dlua_State *L, const char *classname, dluaL_Reg *func) {
    dluaL_newmetatable(L, classname); /* mt */
    /* create __index table to place methods */
    dlua_pushliteral(L, "__index");   /* mt,"__index" */
    dlua_newtable(L);                 /* mt,"__index",it */
    /* put class name into class metatable */
    dlua_pushliteral(L, "class");     /* mt,"__index",it,"class" */
    dlua_pushstring(L, classname);    /* mt,"__index",it,"class",classname */
    dlua_rawset(L, -3);               /* mt,"__index",it */
    /* pass all methods that start with _ to the metatable, and all others
     * to the index table */
    for (; func->name; func++) {     /* mt,"__index",it */
        dlua_pushstring(L, func->name);
        dlua_pushcfunction(L, func->func);
        dlua_rawset(L, func->name[0] == '_' ? -5: -3);
    }
    dlua_rawset(L, -3);               /* mt */
    dlua_pop(L, 1);
}

/*-------------------------------------------------------------------------*\
* Prints the value of a class in a nice way
\*-------------------------------------------------------------------------*/
int auxiliar_tostring(dlua_State *L) {
    char buf[32];
    if (!dlua_getmetatable(L, 1)) goto error;
    dlua_pushliteral(L, "__index");
    dlua_gettable(L, -2);
    if (!dlua_istable(L, -1)) goto error;
    dlua_pushliteral(L, "class");
    dlua_gettable(L, -2);
    if (!dlua_isstring(L, -1)) goto error;
    sprintf(buf, "%p", dlua_touserdata(L, 1));
    dlua_pushfstring(L, "%s: %s", dlua_tostring(L, -1), buf);
    return 1;
error:
    dlua_pushliteral(L, "invalid object passed to 'auxiliar.c:__tostring'");
    dlua_error(L);
    return 1;
}

/*-------------------------------------------------------------------------*\
* Insert class into group
\*-------------------------------------------------------------------------*/
void auxiliar_add2group(dlua_State *L, const char *classname, const char *groupname) {
    dluaL_getmetatable(L, classname);
    dlua_pushstring(L, groupname);
    dlua_pushboolean(L, 1);
    dlua_rawset(L, -3);
    dlua_pop(L, 1);
}

/*-------------------------------------------------------------------------*\
* Make sure argument is a boolean
\*-------------------------------------------------------------------------*/
int auxiliar_checkboolean(dlua_State *L, int objidx) {
    if (!dlua_isboolean(L, objidx))
        auxiliar_typeerror(L, objidx, dlua_typename(L, DLUA_TBOOLEAN));
    return dlua_toboolean(L, objidx);
}

/*-------------------------------------------------------------------------*\
* Return userdata pointer if object belongs to a given class, abort with
* error otherwise
\*-------------------------------------------------------------------------*/
void *auxiliar_checkclass(dlua_State *L, const char *classname, int objidx) {
    void *data = auxiliar_getclassudata(L, classname, objidx);
    if (!data) {
        char msg[45];
        sprintf(msg, "%.35s expected", classname);
        dluaL_argerror(L, objidx, msg);
    }
    return data;
}

/*-------------------------------------------------------------------------*\
* Return userdata pointer if object belongs to a given group, abort with
* error otherwise
\*-------------------------------------------------------------------------*/
void *auxiliar_checkgroup(dlua_State *L, const char *groupname, int objidx) {
    void *data = auxiliar_getgroupudata(L, groupname, objidx);
    if (!data) {
        char msg[45];
        sprintf(msg, "%.35s expected", groupname);
        dluaL_argerror(L, objidx, msg);
    }
    return data;
}

/*-------------------------------------------------------------------------*\
* Set object class
\*-------------------------------------------------------------------------*/
void auxiliar_setclass(dlua_State *L, const char *classname, int objidx) {
    dluaL_getmetatable(L, classname);
    if (objidx < 0) objidx--;
    dlua_setmetatable(L, objidx);
}

/*-------------------------------------------------------------------------*\
* Get a userdata pointer if object belongs to a given group. Return NULL
* otherwise
\*-------------------------------------------------------------------------*/
void *auxiliar_getgroupudata(dlua_State *L, const char *groupname, int objidx) {
    if (!dlua_getmetatable(L, objidx))
        return NULL;
    dlua_pushstring(L, groupname);
    dlua_rawget(L, -2);
    if (dlua_isnil(L, -1)) {
        dlua_pop(L, 2);
        return NULL;
    } else {
        dlua_pop(L, 2);
        return dlua_touserdata(L, objidx);
    }
}

/*-------------------------------------------------------------------------*\
* Get a userdata pointer if object belongs to a given class. Return NULL
* otherwise
\*-------------------------------------------------------------------------*/
void *auxiliar_getclassudata(dlua_State *L, const char *classname, int objidx) {
    return dluaL_checkudata(L, objidx, classname);
}

/*-------------------------------------------------------------------------*\
* Throws error when argument does not have correct type.
* Used to be part of lauxlib in Lua 5.1, was dropped from 5.2.
\*-------------------------------------------------------------------------*/
int auxiliar_typeerror (dlua_State *L, int narg, const char *tname) {
  const char *msg = dlua_pushfstring(L, "%s expected, got %s", tname,
      dluaL_typename(L, narg));
  return dluaL_argerror(L, narg, msg);
}

