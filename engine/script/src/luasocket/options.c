/*=========================================================================*\
* Common option interface
* LuaSocket toolkit
\*=========================================================================*/
#include <string.h>

#include <dmsdk/dlua/dlua.h>

#include "auxiliar.h"
#include "options.h"
#include "inet.h"


/*=========================================================================*\
* Internal functions prototypes
\*=========================================================================*/
static int opt_setmembership(dlua_State *L, p_socket ps, int level, int name);
static int opt_ip6_setmembership(dlua_State *L, p_socket ps, int level, int name);
static int opt_setboolean(dlua_State *L, p_socket ps, int level, int name);
static int opt_getboolean(dlua_State *L, p_socket ps, int level, int name);
static int opt_setint(dlua_State *L, p_socket ps, int level, int name);
static int opt_getint(dlua_State *L, p_socket ps, int level, int name);
static int opt_set(dlua_State *L, p_socket ps, int level, int name,
        void *val, int len);
static int opt_get(dlua_State *L, p_socket ps, int level, int name,
        void *val, int* len);

/*=========================================================================*\
* Exported functions
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Calls appropriate option handler
\*-------------------------------------------------------------------------*/
int opt_meth_setoption(dlua_State *L, p_opt opt, p_socket ps)
{
    const char *name = dluaL_checkstring(L, 2);      /* obj, name, ... */
    while (opt->name && strcmp(name, opt->name))
        opt++;
    if (!opt->func) {
        char msg[64];
        snprintf(msg, sizeof(msg), "unsupported option `%.35s'", name);
        dluaL_argerror(L, 2, msg);
    }
    return opt->func(L, ps);
}

int opt_meth_getoption(dlua_State *L, p_opt opt, p_socket ps)
{
    const char *name = dluaL_checkstring(L, 2);      /* obj, name, ... */
    while (opt->name && strcmp(name, opt->name))
        opt++;
    if (!opt->func) {
        char msg[64];
        snprintf(msg, sizeof(msg), "unsupported option `%.35s'", name);
        dluaL_argerror(L, 2, msg);
    }
    return opt->func(L, ps);
}

/* enables reuse of local address */
int opt_set_reuseaddr(dlua_State *L, p_socket ps)
{
    return opt_setboolean(L, ps, SOL_SOCKET, SO_REUSEADDR);
}

int opt_get_reuseaddr(dlua_State *L, p_socket ps)
{
    return opt_getboolean(L, ps, SOL_SOCKET, SO_REUSEADDR);
}

/* enables reuse of local port */
int opt_set_reuseport(dlua_State *L, p_socket ps)
{
    return opt_setboolean(L, ps, SOL_SOCKET, SO_REUSEPORT);
}

int opt_get_reuseport(dlua_State *L, p_socket ps)
{
    return opt_getboolean(L, ps, SOL_SOCKET, SO_REUSEPORT);
}

/* disables the Naggle algorithm */
int opt_set_tcp_nodelay(dlua_State *L, p_socket ps)
{
    return opt_setboolean(L, ps, IPPROTO_TCP, TCP_NODELAY);
}

int opt_get_tcp_nodelay(dlua_State *L, p_socket ps)
{
    return opt_getboolean(L, ps, IPPROTO_TCP, TCP_NODELAY);
}

int opt_set_keepalive(dlua_State *L, p_socket ps)
{
    return opt_setboolean(L, ps, SOL_SOCKET, SO_KEEPALIVE);
}

int opt_get_keepalive(dlua_State *L, p_socket ps)
{
    return opt_getboolean(L, ps, SOL_SOCKET, SO_KEEPALIVE);
}

int opt_set_dontroute(dlua_State *L, p_socket ps)
{
    return opt_setboolean(L, ps, SOL_SOCKET, SO_DONTROUTE);
}

int opt_set_broadcast(dlua_State *L, p_socket ps)
{
    return opt_setboolean(L, ps, SOL_SOCKET, SO_BROADCAST);
}

#if !defined(DM_IPV6_UNSUPPORTED)
int opt_set_ip6_unicast_hops(dlua_State *L, p_socket ps)
{
  return opt_setint(L, ps, IPPROTO_IPV6, IPV6_UNICAST_HOPS);
}

int opt_get_ip6_unicast_hops(dlua_State *L, p_socket ps)
{
  return opt_getint(L, ps, IPPROTO_IPV6, IPV6_UNICAST_HOPS);
}

int opt_set_ip6_multicast_hops(dlua_State *L, p_socket ps)
{
  return opt_setint(L, ps, IPPROTO_IPV6, IPV6_MULTICAST_HOPS);
}

int opt_get_ip6_multicast_hops(dlua_State *L, p_socket ps)
{
  return opt_getint(L, ps, IPPROTO_IPV6, IPV6_MULTICAST_HOPS);
}
#endif //DM_IPV6_UNSUPPORTED

int opt_set_ip_multicast_loop(dlua_State *L, p_socket ps)
{
    return opt_setboolean(L, ps, IPPROTO_IP, IP_MULTICAST_LOOP);
}

int opt_get_ip_multicast_loop(dlua_State *L, p_socket ps)
{
    return opt_getboolean(L, ps, IPPROTO_IP, IP_MULTICAST_LOOP);
}

#if !defined(DM_IPV6_UNSUPPORTED)
int opt_set_ip6_multicast_loop(dlua_State *L, p_socket ps)
{
    return opt_setboolean(L, ps, IPPROTO_IPV6, IPV6_MULTICAST_LOOP);
}

int opt_get_ip6_multicast_loop(dlua_State *L, p_socket ps)
{
    return opt_getboolean(L, ps, IPPROTO_IPV6, IPV6_MULTICAST_LOOP);
}
#endif // DM_IPV6_UNSUPPORTED

int opt_set_linger(dlua_State *L, p_socket ps)
{
    struct linger li;                      /* obj, name, table */
    if (!dlua_istable(L, 3)) auxiliar_typeerror(L,3,dlua_typename(L, DLUA_TTABLE));
    dlua_pushliteral(L, "on");
    dlua_gettable(L, 3);
    if (!dlua_isboolean(L, -1))
        dluaL_argerror(L, 3, "boolean 'on' field expected");
    li.l_onoff = (u_short) dlua_toboolean(L, -1);
    dlua_pushliteral(L, "timeout");
    dlua_gettable(L, 3);
    if (!dlua_isnumber(L, -1))
        dluaL_argerror(L, 3, "number 'timeout' field expected");
    li.l_linger = (u_short) dlua_tonumber(L, -1);
    return opt_set(L, ps, SOL_SOCKET, SO_LINGER, (char *) &li, sizeof(li));
}

int opt_get_linger(dlua_State *L, p_socket ps)
{
    struct linger li;                      /* obj, name */
    int len = sizeof(li);
    int err = opt_get(L, ps, SOL_SOCKET, SO_LINGER, (char *) &li, &len);
    if (err)
        return err;
    dlua_newtable(L);
    dlua_pushboolean(L, li.l_onoff);
    dlua_setfield(L, -2, "on");
    dlua_pushinteger(L, li.l_linger);
    dlua_setfield(L, -2, "timeout");
    return 1;
}

int opt_set_ip_multicast_ttl(dlua_State *L, p_socket ps)
{
    return opt_setint(L, ps, IPPROTO_IP, IP_MULTICAST_TTL);
}

int opt_set_ip_multicast_if(dlua_State *L, p_socket ps)
{
    const char *address = dluaL_checkstring(L, 3);    /* obj, name, ip */
    struct in_addr val;
    val.s_addr = htonl(INADDR_ANY);
    if (strcmp(address, "*") && !inet_aton(address, &val))
        dluaL_argerror(L, 3, "ip expected");
    return opt_set(L, ps, IPPROTO_IP, IP_MULTICAST_IF,
        (char *) &val, sizeof(val));
}

int opt_get_ip_multicast_if(dlua_State *L, p_socket ps)
{
    struct in_addr val;
    socklen_t len = sizeof(val);
    if (getsockopt(*ps, IPPROTO_IP, IP_MULTICAST_IF, (char *) &val, &len) < 0) {
        dlua_pushnil(L);
        dlua_pushliteral(L, "getsockopt failed");
        return 2;
    }
    dlua_pushstring(L, inet_ntoa(val));
    return 1;
}

int opt_set_ip_add_membership(dlua_State *L, p_socket ps)
{
    return opt_setmembership(L, ps, IPPROTO_IP, IP_ADD_MEMBERSHIP);
}

int opt_set_ip_drop_membersip(dlua_State *L, p_socket ps)
{
    return opt_setmembership(L, ps, IPPROTO_IP, IP_DROP_MEMBERSHIP);
}

#if !defined(DM_IPV6_UNSUPPORTED)
int opt_set_ip6_add_membership(dlua_State *L, p_socket ps)
{
    return opt_ip6_setmembership(L, ps, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP);
}

int opt_set_ip6_drop_membersip(dlua_State *L, p_socket ps)
{
    return opt_ip6_setmembership(L, ps, IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP);
}

int opt_get_ip6_v6only(dlua_State *L, p_socket ps)
{
    return opt_getboolean(L, ps, IPPROTO_IPV6, IPV6_V6ONLY);
}

int opt_set_ip6_v6only(dlua_State *L, p_socket ps)
{
    return opt_setboolean(L, ps, IPPROTO_IPV6, IPV6_V6ONLY);
}
#endif // DM_IPV6_UNSUPPORTED

/*=========================================================================*\
* Auxiliar functions
\*=========================================================================*/
static int opt_setmembership(dlua_State *L, p_socket ps, int level, int name)
{
    struct ip_mreq val;                   /* obj, name, table */
    if (!dlua_istable(L, 3)) auxiliar_typeerror(L,3,dlua_typename(L, DLUA_TTABLE));
    dlua_pushliteral(L, "multiaddr");
    dlua_gettable(L, 3);
    if (!dlua_isstring(L, -1))
        dluaL_argerror(L, 3, "string 'multiaddr' field expected");
    if (!inet_aton(dlua_tostring(L, -1), &val.imr_multiaddr))
        dluaL_argerror(L, 3, "invalid 'multiaddr' ip address");
    dlua_pushliteral(L, "interface");
    dlua_gettable(L, 3);
    if (!dlua_isstring(L, -1))
        dluaL_argerror(L, 3, "string 'interface' field expected");
    val.imr_interface.s_addr = htonl(INADDR_ANY);
    if (strcmp(dlua_tostring(L, -1), "*") &&
            !inet_aton(dlua_tostring(L, -1), &val.imr_interface))
        dluaL_argerror(L, 3, "invalid 'interface' ip address");
    return opt_set(L, ps, level, name, (char *) &val, sizeof(val));
}

#if !defined(DM_IPV6_UNSUPPORTED)
static int opt_ip6_setmembership(dlua_State *L, p_socket ps, int level, int name)
{
    struct ipv6_mreq val;                   /* obj, opt-name, table */
    memset(&val, 0, sizeof(val));
    if (!dlua_istable(L, 3)) auxiliar_typeerror(L,3,dlua_typename(L, DLUA_TTABLE));
    dlua_pushliteral(L, "multiaddr");
    dlua_gettable(L, 3);
    if (!dlua_isstring(L, -1))
        dluaL_argerror(L, 3, "string 'multiaddr' field expected");
    if (!inet_pton(AF_INET6, dlua_tostring(L, -1), &val.ipv6mr_multiaddr))
        dluaL_argerror(L, 3, "invalid 'multiaddr' ip address");
    dlua_pushliteral(L, "interface");
    dlua_gettable(L, 3);
    /* By default we listen to interface on default route
     * (sigh). However, interface= can override it. We should
     * support either number, or name for it. Waiting for
     * windows port of if_nametoindex */
    if (!dlua_isnil(L, -1)) {
        if (dlua_isnumber(L, -1)) {
            val.ipv6mr_interface = (unsigned int) dlua_tonumber(L, -1);
        } else
          dluaL_argerror(L, -1, "number 'interface' field expected");
    }
    return opt_set(L, ps, level, name, (char *) &val, sizeof(val));
}
#endif // DM_IPV6_UNSUPPORTED

static
int opt_get(dlua_State *L, p_socket ps, int level, int name, void *val, int* len)
{
    socklen_t socklen = *len;
    if (getsockopt(*ps, level, name, (char *) val, &socklen) < 0) {
        dlua_pushnil(L);
        dlua_pushliteral(L, "getsockopt failed");
        return 2;
    }
    *len = socklen;
    return 0;
}

static
int opt_set(dlua_State *L, p_socket ps, int level, int name, void *val, int len)
{
/// DEFOLD BEGIN
    if (1
#if !defined(__EMSCRIPTEN__)
        && setsockopt(*ps, level, name, (char *) val, len) < 0
#endif
        ) {
/// DEFOLD END
        dlua_pushnil(L);
        dlua_pushliteral(L, "setsockopt failed");
        return 2;
    }
    dlua_pushnumber(L, 1);
    return 1;
}

static int opt_getboolean(dlua_State *L, p_socket ps, int level, int name)
{
    int val = 0;
    int len = sizeof(val);
    int err = opt_get(L, ps, level, name, (char *) &val, &len);
    if (err)
        return err;
    dlua_pushboolean(L, val);
    return 1;
}

int opt_get_error(dlua_State *L, p_socket ps)
{
    int val = 0;
    socklen_t len = sizeof(val);
    if (getsockopt(*ps, SOL_SOCKET, SO_ERROR, (char *) &val, &len) < 0) {
        dlua_pushnil(L);
        dlua_pushliteral(L, "getsockopt failed");
        return 2;
    }
    dlua_pushstring(L, socket_strerror(val));
    return 1;
}

static int opt_setboolean(dlua_State *L, p_socket ps, int level, int name)
{
    int val = auxiliar_checkboolean(L, 3);             /* obj, name, bool */
    return opt_set(L, ps, level, name, (char *) &val, sizeof(val));
}

static int opt_getint(dlua_State *L, p_socket ps, int level, int name)
{
    int val = 0;
    int len = sizeof(val);
    int err = opt_get(L, ps, level, name, (char *) &val, &len);
    if (err)
        return err;
    dlua_pushnumber(L, val);
    return 1;
}

static int opt_setint(dlua_State *L, p_socket ps, int level, int name)
{
    int val = (int) dlua_tonumber(L, 3);             /* obj, name, int */
    return opt_set(L, ps, level, name, (char *) &val, sizeof(val));
}
