#ifndef OPTIONS_H
#define OPTIONS_H
/*=========================================================================*\
* Common option interface
* LuaSocket toolkit
*
* This module provides a common interface to socket options, used mainly by
* modules UDP and TCP.
\*=========================================================================*/

#include <dmsdk/dlua/dlua.h>
#include "socket.h"

/* option registry */
typedef struct t_opt {
  const char *name;
  int (*func)(dlua_State *L, p_socket ps);
} t_opt;
typedef t_opt *p_opt;

/* supported options for setoption */
int opt_set_dontroute(dlua_State *L, p_socket ps);
int opt_set_broadcast(dlua_State *L, p_socket ps);
int opt_set_reuseaddr(dlua_State *L, p_socket ps);
int opt_set_tcp_nodelay(dlua_State *L, p_socket ps);
int opt_set_keepalive(dlua_State *L, p_socket ps);
int opt_set_linger(dlua_State *L, p_socket ps);
int opt_set_reuseaddr(dlua_State *L, p_socket ps);
int opt_set_reuseport(dlua_State *L, p_socket ps);
int opt_set_ip_multicast_if(dlua_State *L, p_socket ps);
int opt_set_ip_multicast_ttl(dlua_State *L, p_socket ps);
int opt_set_ip_multicast_loop(dlua_State *L, p_socket ps);
int opt_set_ip_add_membership(dlua_State *L, p_socket ps);
int opt_set_ip_drop_membersip(dlua_State *L, p_socket ps);
int opt_set_ip6_unicast_hops(dlua_State *L, p_socket ps);
int opt_set_ip6_multicast_hops(dlua_State *L, p_socket ps);
int opt_set_ip6_multicast_loop(dlua_State *L, p_socket ps);
int opt_set_ip6_add_membership(dlua_State *L, p_socket ps);
int opt_set_ip6_drop_membersip(dlua_State *L, p_socket ps);
int opt_set_ip6_v6only(dlua_State *L, p_socket ps);

/* supported options for getoption */
int opt_get_reuseaddr(dlua_State *L, p_socket ps);
int opt_get_tcp_nodelay(dlua_State *L, p_socket ps);
int opt_get_keepalive(dlua_State *L, p_socket ps);
int opt_get_linger(dlua_State *L, p_socket ps);
int opt_get_reuseaddr(dlua_State *L, p_socket ps);
int opt_get_ip_multicast_loop(dlua_State *L, p_socket ps);
int opt_get_ip_multicast_if(dlua_State *L, p_socket ps);
int opt_get_error(dlua_State *L, p_socket ps);
int opt_get_ip6_multicast_loop(dlua_State *L, p_socket ps);
int opt_get_ip6_multicast_hops(dlua_State *L, p_socket ps);
int opt_get_ip6_unicast_hops(dlua_State *L, p_socket ps);
int opt_get_ip6_v6only(dlua_State *L, p_socket ps);

/* invokes the appropriate option handler */
int opt_meth_setoption(dlua_State *L, p_opt opt, p_socket ps);
int opt_meth_getoption(dlua_State *L, p_opt opt, p_socket ps);

#endif
