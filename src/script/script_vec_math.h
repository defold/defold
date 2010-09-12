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
    bool PullVector3(lua_State* L, Vectormath::Aos::Vector3& v_out);
    void PushQuat(lua_State* L, const Vectormath::Aos::Quat& v);
    bool PullQuat(lua_State* L, Vectormath::Aos::Quat& v_out);
}

#endif // DM_SCRIPT_VEC_MATH_H
