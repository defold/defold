#ifndef DM_SCRIPT_ZLIB_H
#define DM_SCRIPT_ZLIB_H

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void InitializeZlib(lua_State* L);
}

#endif // DM_SCRIPT_ZLIB_H
