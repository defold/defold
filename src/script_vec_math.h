#ifndef DM_SCRIPT_VEC_MATH_H
#define DM_SCRIPT_VEC_MATH_H

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void InitializeVecMath(lua_State* L);
}

#endif // DM_SCRIPT_VEC_MATH_H
