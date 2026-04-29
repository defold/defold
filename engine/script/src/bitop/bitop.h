#ifndef BITOP_H
#define BITOP_H

#include <dmsdk/dlua/dlua.h>

#define LUA_BITOP_VERSION   "1.0.2"

DLUA_API int luaopen_bit(dlua_State *L);

#endif /* BITOP_H */
