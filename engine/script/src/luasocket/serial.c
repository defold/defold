/*=========================================================================*\
* Serial stream
* LuaSocket toolkit
\*=========================================================================*/
#include <string.h>

#include <dmsdk/dlua/dlua.h>

#include "auxiliar.h"
#include "socket.h"
#include "options.h"
#include "unix.h"
#include <sys/un.h>

/*
Reuses userdata definition from unix.h, since it is useful for all
stream-like objects.

If we stored the serial path for use in error messages or userdata
printing, we might need our own userdata definition.

Group usage is semi-inherited from unix.c, but unnecessary since we
have only one object type.
*/

/*=========================================================================*\
* Internal function prototypes
\*=========================================================================*/
static int global_create(dlua_State *L);
static int meth_send(dlua_State *L);
static int meth_receive(dlua_State *L);
static int meth_close(dlua_State *L);
static int meth_settimeout(dlua_State *L);
static int meth_getfd(dlua_State *L);
static int meth_setfd(dlua_State *L);
static int meth_dirty(dlua_State *L);
static int meth_getstats(dlua_State *L);
static int meth_setstats(dlua_State *L);

/* serial object methods */
static dluaL_Reg serial_methods[] = {
    {"__gc",        meth_close},
    {"__tostring",  auxiliar_tostring},
    {"close",       meth_close},
    {"dirty",       meth_dirty},
    {"getfd",       meth_getfd},
    {"getstats",    meth_getstats},
    {"setstats",    meth_setstats},
    {"receive",     meth_receive},
    {"send",        meth_send},
    {"setfd",       meth_setfd},
    {"settimeout",  meth_settimeout},
    {NULL,          NULL}
};

/* our socket creation function */
/* this is an ad-hoc module that returns a single function
 * as such, do not include other functions in this array. */
static dluaL_Reg func[] = {
    {"serial", global_create},
    {NULL,          NULL}
};


/*-------------------------------------------------------------------------*\
* Initializes module
\*-------------------------------------------------------------------------*/
LUASOCKET_API int luaopen_socket_serial(dlua_State *L) {
    /* create classes */
    auxiliar_newclass(L, "serial{client}", serial_methods);
    /* create class groups */
    auxiliar_add2group(L, "serial{client}", "serial{any}");
#if 0
    dlua_pushcfunction(L, global_create);
    (void) func;
#else
    /* set function into socket namespace */
    dluaL_openlib(L, "socket", func, 0);
    dlua_pushcfunction(L, global_create);
#endif
    return 1;
}

/*=========================================================================*\
* Lua methods
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Just call buffered IO methods
\*-------------------------------------------------------------------------*/
static int meth_send(dlua_State *L) {
    p_unix un = (p_unix) auxiliar_checkclass(L, "serial{client}", 1);
    return buffer_meth_send(L, &un->buf);
}

static int meth_receive(dlua_State *L) {
    p_unix un = (p_unix) auxiliar_checkclass(L, "serial{client}", 1);
    return buffer_meth_receive(L, &un->buf);
}

static int meth_getstats(dlua_State *L) {
    p_unix un = (p_unix) auxiliar_checkclass(L, "serial{client}", 1);
    return buffer_meth_getstats(L, &un->buf);
}

static int meth_setstats(dlua_State *L) {
    p_unix un = (p_unix) auxiliar_checkclass(L, "serial{client}", 1);
    return buffer_meth_setstats(L, &un->buf);
}

/*-------------------------------------------------------------------------*\
* Select support methods
\*-------------------------------------------------------------------------*/
static int meth_getfd(dlua_State *L) {
    p_unix un = (p_unix) auxiliar_checkgroup(L, "serial{any}", 1);
    dlua_pushnumber(L, (int) un->sock);
    return 1;
}

/* this is very dangerous, but can be handy for those that are brave enough */
static int meth_setfd(dlua_State *L) {
    p_unix un = (p_unix) auxiliar_checkgroup(L, "serial{any}", 1);
    un->sock = (t_socket) dluaL_checknumber(L, 2);
    return 0;
}

static int meth_dirty(dlua_State *L) {
    p_unix un = (p_unix) auxiliar_checkgroup(L, "serial{any}", 1);
    dlua_pushboolean(L, !buffer_isempty(&un->buf));
    return 1;
}

/*-------------------------------------------------------------------------*\
* Closes socket used by object
\*-------------------------------------------------------------------------*/
static int meth_close(dlua_State *L)
{
    p_unix un = (p_unix) auxiliar_checkgroup(L, "serial{any}", 1);
    socket_destroy(&un->sock);
    dlua_pushnumber(L, 1);
    return 1;
}


/*-------------------------------------------------------------------------*\
* Just call tm methods
\*-------------------------------------------------------------------------*/
static int meth_settimeout(dlua_State *L) {
    p_unix un = (p_unix) auxiliar_checkgroup(L, "serial{any}", 1);
    return timeout_meth_settimeout(L, &un->tm);
}

/*=========================================================================*\
* Library functions
\*=========================================================================*/


/*-------------------------------------------------------------------------*\
* Creates a serial object
\*-------------------------------------------------------------------------*/
static int global_create(dlua_State *L) {
    const char* path = dluaL_checkstring(L, 1);

    /* allocate unix object */
    p_unix un = (p_unix) dlua_newuserdata(L, sizeof(t_unix));

    /* open serial device */
    t_socket sock = open(path, O_NOCTTY|O_RDWR);

    /*printf("open %s on %d\n", path, sock);*/

    if (sock < 0)  {
        dlua_pushnil(L);
        dlua_pushstring(L, socket_strerror(errno));
        dlua_pushnumber(L, errno);
        return 3;
    }
    /* set its type as client object */
    auxiliar_setclass(L, "serial{client}", -1);
    /* initialize remaining structure fields */
    socket_setnonblocking(&sock);
    un->sock = sock;
    io_init(&un->io, (p_send) socket_write, (p_recv) socket_read,
            (p_error) socket_ioerror, &un->sock);
    timeout_init(&un->tm, -1, -1);
    buffer_init(&un->buf, &un->io, &un->tm);
    return 1;
}
