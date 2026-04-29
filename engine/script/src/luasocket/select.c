/*=========================================================================*\
* Select implementation
* LuaSocket toolkit
\*=========================================================================*/
#include <string.h>

#include <dmsdk/dlua/dlua.h>

#include "socket.h"
#include "timeout.h"
#include "select.h"

/*=========================================================================*\
* Internal function prototypes.
\*=========================================================================*/
static t_socket getfd(dlua_State *L);
static int dirty(dlua_State *L);
static void collect_fd(dlua_State *L, int tab, int itab,
        fd_set *set, t_socket *max_fd);
static int check_dirty(dlua_State *L, int tab, int dtab, fd_set *set);
static void return_fd(dlua_State *L, fd_set *set, t_socket max_fd,
        int itab, int tab, int start);
static void make_assoc(dlua_State *L, int tab);
static int global_select(dlua_State *L);

/* functions in library namespace */
static dluaL_Reg func[] = {
    {"select", global_select},
    {NULL,     NULL}
};

/*=========================================================================*\
* Exported functions
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Initializes module
\*-------------------------------------------------------------------------*/
int select_open(dlua_State *L) {
    dlua_pushliteral(L, "_SETSIZE");
    dlua_pushnumber(L, FD_SETSIZE);
    dlua_rawset(L, -3);
#if 0
    dluaL_setfuncs(L, func, 0);
#else
    dluaL_openlib(L, NULL, func, 0);
#endif
    return 0;
}

/*=========================================================================*\
* Global Lua functions
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Waits for a set of sockets until a condition is met or timeout.
\*-------------------------------------------------------------------------*/
static int global_select(dlua_State *L) {
    int rtab, wtab, itab, ret, ndirty;
    t_socket max_fd = SOCKET_INVALID;
    fd_set rset, wset;
    t_timeout tm;
    double t = dluaL_optnumber(L, 3, -1);
    FD_ZERO(&rset); FD_ZERO(&wset);
    dlua_settop(L, 3);
    dlua_newtable(L); itab = dlua_gettop(L);
    dlua_newtable(L); rtab = dlua_gettop(L);
    dlua_newtable(L); wtab = dlua_gettop(L);
    collect_fd(L, 1, itab, &rset, &max_fd);
    collect_fd(L, 2, itab, &wset, &max_fd);
    ndirty = check_dirty(L, 1, rtab, &rset);
    t = ndirty > 0? 0.0: t;
    timeout_init(&tm, t, -1);
    timeout_markstart(&tm);
    ret = socket_select(max_fd+1, &rset, &wset, NULL, &tm);
    if (ret > 0 || ndirty > 0) {
        return_fd(L, &rset, max_fd+1, itab, rtab, ndirty);
        return_fd(L, &wset, max_fd+1, itab, wtab, 0);
        make_assoc(L, rtab);
        make_assoc(L, wtab);
        return 2;
    } else if (ret == 0) {
        dlua_pushliteral(L, "timeout");
        return 3;
    } else {
        dluaL_error(L, "select failed");
        return 3;
    }
}

/*=========================================================================*\
* Internal functions
\*=========================================================================*/
static t_socket getfd(dlua_State *L) {
    t_socket fd = SOCKET_INVALID;
    dlua_pushliteral(L, "getfd");
    dlua_gettable(L, -2);
    if (!dlua_isnil(L, -1)) {
        dlua_pushvalue(L, -2);
        dlua_call(L, 1, 1);
        if (dlua_isnumber(L, -1)) {
            double numfd = dlua_tonumber(L, -1);
            fd = (numfd >= 0.0)? (t_socket) numfd: SOCKET_INVALID;
        }
    }
    dlua_pop(L, 1);
    return fd;
}

static int dirty(dlua_State *L) {
    int is = 0;
    dlua_pushliteral(L, "dirty");
    dlua_gettable(L, -2);
    if (!dlua_isnil(L, -1)) {
        dlua_pushvalue(L, -2);
        dlua_call(L, 1, 1);
        is = dlua_toboolean(L, -1);
    }
    dlua_pop(L, 1);
    return is;
}

static void collect_fd(dlua_State *L, int tab, int itab,
        fd_set *set, t_socket *max_fd) {
    int i = 1, n = 0;
    /* nil is the same as an empty table */
    if (dlua_isnil(L, tab)) return;
    /* otherwise we need it to be a table */
    dluaL_checktype(L, tab, DLUA_TTABLE);
    for ( ;; ) {
        t_socket fd;
        dlua_pushnumber(L, i);
        dlua_gettable(L, tab);
        if (dlua_isnil(L, -1)) {
            dlua_pop(L, 1);
            break;
        }
        /* getfd figures out if this is a socket */
        fd = getfd(L);
        if (fd != SOCKET_INVALID) {
            /* make sure we don't overflow the fd_set */
#ifdef _WIN32
            if (n >= FD_SETSIZE)
                dluaL_argerror(L, tab, "too many sockets");
#else
            if (fd >= FD_SETSIZE)
                dluaL_argerror(L, tab, "descriptor too large for set size");
#endif
            FD_SET(fd, set);
            n++;
            /* keep track of the largest descriptor so far */
            if (*max_fd == SOCKET_INVALID || *max_fd < fd)
                *max_fd = fd;
            /* make sure we can map back from descriptor to the object */
            dlua_pushnumber(L, (dlua_Number) fd);
            dlua_pushvalue(L, -2);
            dlua_settable(L, itab);
        }
        dlua_pop(L, 1);
        i = i + 1;
    }
}

static int check_dirty(dlua_State *L, int tab, int dtab, fd_set *set) {
    int ndirty = 0, i = 1;
    if (dlua_isnil(L, tab))
        return 0;
    for ( ;; ) {
        t_socket fd;
        dlua_pushnumber(L, i);
        dlua_gettable(L, tab);
        if (dlua_isnil(L, -1)) {
            dlua_pop(L, 1);
            break;
        }
        fd = getfd(L);
        if (fd != SOCKET_INVALID && dirty(L)) {
            dlua_pushnumber(L, ++ndirty);
            dlua_pushvalue(L, -2);
            dlua_settable(L, dtab);
            FD_CLR(fd, set);
        }
        dlua_pop(L, 1);
        i = i + 1;
    }
    return ndirty;
}

static void return_fd(dlua_State *L, fd_set *set, t_socket max_fd,
        int itab, int tab, int start) {
    t_socket fd;
    for (fd = 0; fd < max_fd; fd++) {
        if (FD_ISSET(fd, set)) {
            dlua_pushnumber(L, ++start);
            dlua_pushnumber(L, (dlua_Number) fd);
            dlua_gettable(L, itab);
            dlua_settable(L, tab);
        }
    }
}

static void make_assoc(dlua_State *L, int tab) {
    int i = 1, atab;
    dlua_newtable(L); atab = dlua_gettop(L);
    for ( ;; ) {
        dlua_pushnumber(L, i);
        dlua_gettable(L, tab);
        if (!dlua_isnil(L, -1)) {
            dlua_pushnumber(L, i);
            dlua_pushvalue(L, -2);
            dlua_settable(L, atab);
            dlua_pushnumber(L, i);
            dlua_settable(L, atab);
        } else {
            dlua_pop(L, 1);
            break;
        }
        i = i+1;
    }
}

