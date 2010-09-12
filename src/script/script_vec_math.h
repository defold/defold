#ifndef DM_SCRIPT_VEC_MATH_H
#define DM_SCRIPT_VEC_MATH_H

#include <vectormath/cpp/vectormath_aos.h>

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    void RegisterVecMathLibs(lua_State* L);

    void PushVector3(lua_State* L, const Vectormath::Aos::Vector3& v);
    Vectormath::Aos::Vector3* CheckVector3(lua_State* L, int index);
    void PushQuat(lua_State* L, const Vectormath::Aos::Quat& v);
    Vectormath::Aos::Quat* CheckQuat(lua_State* L, int index);
}

#endif // DM_SCRIPT_VEC_MATH_H
