#ifndef DM_SCRIPT_VMATH_H
#define DM_SCRIPT_VMATH_H

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void InitializeVmath(lua_State* L);
}

#endif // DM_SCRIPT_VMATH_H
