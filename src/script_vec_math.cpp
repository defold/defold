#include "script.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
#define LIB_NAME "vec_math"
#define TYPE_NAME_VECTOR3 "vector3"
#define TYPE_NAME_VECTOR4 "vector4"
#define TYPE_NAME_POINT3 "point3"
#define TYPE_NAME_QUAT "quat"

    bool IsVector3(lua_State *L, int index)
    {
        void *p = lua_touserdata(L, index);
        bool result = false;
        if (p != 0x0)
        {  /* value is a userdata? */
            if (lua_getmetatable(L, index))
            {  /* does it have a metatable? */
                lua_getfield(L, LUA_REGISTRYINDEX, TYPE_NAME_VECTOR3);  /* get correct metatable */
                if (lua_rawequal(L, -1, -2))
                {  /* does it have the correct mt? */
                    result = true;
                }
                lua_pop(L, 2);  /* remove both metatables */
            }
        }
        return result;
    }

    static int Vector3_gc(lua_State *L)
    {
        Vectormath::Aos::Vector3* v = CheckVector3(L, 1);
        memset(v, 0, sizeof(*v));
        (void) v;
        assert(v);
        return 0;
    }

    static int Vector3_tostring(lua_State *L)
    {
        Vectormath::Aos::Vector3* v = CheckVector3(L, 1);
        lua_pushfstring(L, "%s: [%f, %f, %f]", TYPE_NAME_VECTOR3, v->getX(), v->getY(), v->getZ());
        return 1;
    }

    static int Vector3_index(lua_State *L)
    {
        Vectormath::Aos::Vector3* v = CheckVector3(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (key[0] == 'x')
        {
            lua_pushnumber(L, v->getX());
            return 1;
        }
        else if (key[0] == 'y')
        {
            lua_pushnumber(L, v->getY());
            return 1;
        }
        else if (key[0] == 'z')
        {
            lua_pushnumber(L, v->getZ());
            return 1;
        }
        else
        {
            return luaL_error(L, "%s.%s only has fields x, y, z.", LIB_NAME, TYPE_NAME_VECTOR3);
        }
    }

    static int Vector3_newindex(lua_State *L)
    {
        Vectormath::Aos::Vector3* v = CheckVector3(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (key[0] == 'x')
        {
            v->setX(luaL_checknumber(L, 3));
        }
        else if (key[0] == 'y')
        {
            v->setY(luaL_checknumber(L, 3));
        }
        else if (key[0] == 'z')
        {
            v->setZ(luaL_checknumber(L, 3));
        }
        else
        {
            return luaL_error(L, "%s.%s only has fields x, y, z.", LIB_NAME, TYPE_NAME_VECTOR3);
        }
        return 0;
    }

    static int Vector3_add(lua_State *L)
    {
        if (IsPoint3(L,2))
        {
            Vectormath::Aos::Vector3* v1 = CheckVector3(L, 1);
            Vectormath::Aos::Point3* v2 = CheckPoint3(L, 2);
            PushPoint3(L, *v1 + *v2);
        }
        else
        {
            Vectormath::Aos::Vector3* v1 = CheckVector3(L, 1);
            Vectormath::Aos::Vector3* v2 = CheckVector3(L, 2);
            PushVector3(L, *v1 + *v2);
        }
        return 1;
    }

    static int Vector3_sub(lua_State *L)
    {
        Vectormath::Aos::Vector3* v1 = CheckVector3(L, 1);
        Vectormath::Aos::Vector3* v2 = CheckVector3(L, 2);
        PushVector3(L, *v1 - *v2);
        return 1;
    }

    static int Vector3_mul(lua_State *L)
    {
        Vectormath::Aos::Vector3* v = CheckVector3(L, 1);
        float s = luaL_checknumber(L, 2);
        PushVector3(L, *v * s);
        return 1;
    }

    static int Vector3_unm(lua_State *L)
    {
        Vectormath::Aos::Vector3* v = CheckVector3(L, 1);
        PushVector3(L, - *v);
        return 1;
    }

    static const luaL_reg Vector3_methods[] =
    {
        {0,0}
    };
    static const luaL_reg Vector3_meta[] =
    {
        {"__gc",        Vector3_gc},
        {"__tostring",  Vector3_tostring},
        {"__index",     Vector3_index},
        {"__newindex",  Vector3_newindex},
        {"__add", Vector3_add},
        {"__sub", Vector3_sub},
        {"__mul", Vector3_mul},
        {"__unm", Vector3_unm},
        {0,0}
    };

    bool IsVector4(lua_State *L, int index)
    {
        void *p = lua_touserdata(L, index);
        bool result = false;
        if (p != 0x0)
        {  /* value is a userdata? */
            if (lua_getmetatable(L, index))
            {  /* does it have a metatable? */
                lua_getfield(L, LUA_REGISTRYINDEX, TYPE_NAME_VECTOR4);  /* get correct metatable */
                if (lua_rawequal(L, -1, -2))
                {  /* does it have the correct mt? */
                    result = true;
                }
                lua_pop(L, 2);  /* remove both metatables */
            }
        }
        return result;
    }

    static int Vector4_gc(lua_State *L)
    {
        Vectormath::Aos::Vector4* v = CheckVector4(L, 1);
        memset(v, 0, sizeof(*v));
        (void) v;
        assert(v);
        return 0;
    }

    static int Vector4_tostring(lua_State *L)
    {
        Vectormath::Aos::Vector4* v = CheckVector4(L, 1);
        lua_pushfstring(L, "%s: [%f, %f, %f, %f]", TYPE_NAME_VECTOR4, v->getX(), v->getY(), v->getZ(), v->getW());
        return 1;
    }

    static int Vector4_index(lua_State *L)
    {
        Vectormath::Aos::Vector4* v = CheckVector4(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (key[0] == 'x')
        {
            lua_pushnumber(L, v->getX());
            return 1;
        }
        else if (key[0] == 'y')
        {
            lua_pushnumber(L, v->getY());
            return 1;
        }
        else if (key[0] == 'z')
        {
            lua_pushnumber(L, v->getZ());
            return 1;
        }
        else if (key[0] == 'w')
        {
            lua_pushnumber(L, v->getW());
            return 1;
        }
        else
        {
            return luaL_error(L, "%s.%s only has fields x, y, z, w.", LIB_NAME, TYPE_NAME_VECTOR4);
        }
    }

    static int Vector4_newindex(lua_State *L)
    {
        Vectormath::Aos::Vector4* v = CheckVector4(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (key[0] == 'x')
        {
            v->setX(luaL_checknumber(L, 3));
        }
        else if (key[0] == 'y')
        {
            v->setY(luaL_checknumber(L, 3));
        }
        else if (key[0] == 'z')
        {
            v->setZ(luaL_checknumber(L, 3));
        }
        else if (key[0] == 'w')
        {
            v->setW(luaL_checknumber(L, 3));
        }
        else
        {
            return luaL_error(L, "%s.%s only has fields x, y, z, w.", LIB_NAME, TYPE_NAME_VECTOR4);
        }
        return 0;
    }

    static const luaL_reg Vector4_methods[] =
    {
        {0,0}
    };
    static const luaL_reg Vector4_meta[] =
    {
        {"__gc",        Vector4_gc},
        {"__tostring",  Vector4_tostring},
        {"__index",     Vector4_index},
        {"__newindex",  Vector4_newindex},
        {0,0}
    };

    bool IsPoint3(lua_State *L, int index)
    {
        void *p = lua_touserdata(L, index);
        bool result = false;
        if (p != 0x0)
        {  /* value is a userdata? */
            if (lua_getmetatable(L, index))
            {  /* does it have a metatable? */
                lua_getfield(L, LUA_REGISTRYINDEX, TYPE_NAME_POINT3);  /* get correct metatable */
                if (lua_rawequal(L, -1, -2))
                {  /* does it have the correct mt? */
                    result = true;
                }
                lua_pop(L, 2);  /* remove both metatables */
            }
        }
        return result;
    }

    static int Point3_gc(lua_State *L)
    {
        Vectormath::Aos::Point3* v = CheckPoint3(L, 1);
        memset(v, 0, sizeof(*v));
        (void) v;
        assert(v);
        return 0;
    }

    static int Point3_tostring(lua_State *L)
    {
        Vectormath::Aos::Point3* v = CheckPoint3(L, 1);
        lua_pushfstring(L, "%s: [%f, %f, %f]", TYPE_NAME_VECTOR4, v->getX(), v->getY(), v->getZ());
        return 1;
    }

    static int Point3_index(lua_State *L)
    {
        Vectormath::Aos::Point3* v = CheckPoint3(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (key[0] == 'x')
        {
            lua_pushnumber(L, v->getX());
            return 1;
        }
        else if (key[0] == 'y')
        {
            lua_pushnumber(L, v->getY());
            return 1;
        }
        else if (key[0] == 'z')
        {
            lua_pushnumber(L, v->getZ());
            return 1;
        }
        else
        {
            return luaL_error(L, "%s.%s only has fields x, y, z.", LIB_NAME, TYPE_NAME_POINT3);
        }
    }

    static int Point3_newindex(lua_State *L)
    {
        Vectormath::Aos::Point3* v = CheckPoint3(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (key[0] == 'x')
        {
            v->setX(luaL_checknumber(L, 3));
        }
        else if (key[0] == 'y')
        {
            v->setY(luaL_checknumber(L, 3));
        }
        else if (key[0] == 'z')
        {
            v->setZ(luaL_checknumber(L, 3));
        }
        else
        {
            return luaL_error(L, "%s.%s only has fields x, y, z.", LIB_NAME, TYPE_NAME_POINT3);
        }
        return 0;
    }

    static int Point3_add(lua_State *L)
    {
        Vectormath::Aos::Point3* v1 = CheckPoint3(L, 1);
        Vectormath::Aos::Vector3* v2 = CheckVector3(L, 2);
        PushPoint3(L, *v1 + *v2);
        return 1;
    }

    static int Point3_sub(lua_State *L)
    {
        Vectormath::Aos::Point3* v1 = CheckPoint3(L, 1);
        Vectormath::Aos::Point3* v2 = CheckPoint3(L, 2);
        PushVector3(L, *v1 - *v2);
        return 1;
    }

    static const luaL_reg Point3_methods[] =
    {
        {0,0}
    };
    static const luaL_reg Point3_meta[] =
    {
        {"__gc",        Point3_gc},
        {"__tostring",  Point3_tostring},
        {"__index",     Point3_index},
        {"__newindex",  Point3_newindex},
        {"__add",       Point3_add},
        {"__sub",       Point3_sub},
        {0,0}
    };

    static int Quat_gc(lua_State *L)
    {
        Vectormath::Aos::Quat* q = CheckQuat(L, 1);
        memset(q, 0, sizeof(*q));
        (void) q;
        assert(q);
        return 0;
    }

    static int Quat_tostring(lua_State *L)
    {
        Vectormath::Aos::Quat* q = CheckQuat(L, 1);
        lua_pushfstring(L, "%s: [%f, %f, %f, %f]", TYPE_NAME_QUAT, q->getX(), q->getY(), q->getZ(), q->getW());
        return 1;
    }

    static int Quat_index(lua_State *L)
    {
        Vectormath::Aos::Quat* q = CheckQuat(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (key[0] == 'x')
        {
            lua_pushnumber(L, q->getX());
            return 1;
        }
        else if (key[0] == 'y')
        {
            lua_pushnumber(L, q->getY());
            return 1;
        }
        else if (key[0] == 'z')
        {
            lua_pushnumber(L, q->getZ());
            return 1;
        }
        else if (key[0] == 'w')
        {
            lua_pushnumber(L, q->getW());
            return 1;
        }
        else
        {
            return luaL_error(L, "%s.%s only has fields x, y, z, w.", LIB_NAME, TYPE_NAME_QUAT);
        }
        return 0;
    }

    static int Quat_newindex(lua_State *L)
    {
        Vectormath::Aos::Quat* q = CheckQuat(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (key[0] == 'x')
        {
            q->setX(luaL_checknumber(L, -1));
        }
        else if (key[0] == 'y')
        {
            q->setY(luaL_checknumber(L, -1));
        }
        else if (key[0] == 'z')
        {
            q->setZ(luaL_checknumber(L, -1));
        }
        else if (key[0] == 'w')
        {
            q->setW(luaL_checknumber(L, -1));
        }
        else
        {
            return luaL_error(L, "%s.%s only has fields x, y, z, w.", LIB_NAME, TYPE_NAME_QUAT);
        }
        return 0;
    }

    static int Quat_mul(lua_State *L)
    {
        Vectormath::Aos::Quat* q1 = CheckQuat(L, 1);
        Vectormath::Aos::Quat* q2 = CheckQuat(L, 2);
        PushQuat(L, *q1 * *q2);
        return 1;
    }

    static const luaL_reg Quat_methods[] =
    {
        {0,0}
    };
    static const luaL_reg Quat_meta[] =
    {
        {"__gc",        Quat_gc},
        {"__tostring",  Quat_tostring},
        {"__index",     Quat_index},
        {"__newindex",  Quat_newindex},
        {"__mul",       Quat_mul},
        {0,0}
    };

    static int Vector3_new(lua_State* L)
    {
        Vectormath::Aos::Vector3 v;
        if (IsPoint3(L, 1))
        {
            Vectormath::Aos::Point3* p = CheckPoint3(L, 1);
            v = Vectormath::Aos::Vector3(*p);
        }
        else
        {
            v.setX(luaL_checknumber(L, 1));
            v.setY(luaL_checknumber(L, 2));
            v.setZ(luaL_checknumber(L, 3));
        }
        PushVector3(L, v);
        return 1;
    }

    static int Vector4_new(lua_State* L)
    {
        Vectormath::Aos::Vector4 v;
        v.setX(luaL_checknumber(L, 1));
        v.setY(luaL_checknumber(L, 2));
        v.setZ(luaL_checknumber(L, 3));
        v.setW(luaL_checknumber(L, 4));
        PushVector4(L, v);
        return 1;
    }

    static int Point3_new(lua_State* L)
    {
        Vectormath::Aos::Point3 v;
        v.setX(luaL_checknumber(L, 1));
        v.setY(luaL_checknumber(L, 2));
        v.setZ(luaL_checknumber(L, 3));
        PushPoint3(L, v);
        return 1;
    }

    static int Quat_new(lua_State* L)
    {
        Vectormath::Aos::Quat q;
        q.setX(luaL_checknumber(L, 1));
        q.setY(luaL_checknumber(L, 2));
        q.setZ(luaL_checknumber(L, 3));
        q.setW(luaL_checknumber(L, 4));
        PushQuat(L, q);
        return 1;
    }

    static int Quat_FromStartToEnd(lua_State* L)
    {
        Vectormath::Aos::Vector3* v1 = CheckVector3(L, 1);
        Vectormath::Aos::Vector3* v2 = CheckVector3(L, 2);
        PushQuat(L, Vectormath::Aos::Quat::rotation(*v1, *v2));
        return 1;
    }

    static int Quat_FromAxisAngle(lua_State* L)
    {
        Vectormath::Aos::Vector3* axis = CheckVector3(L, 1);
        float angle = luaL_checknumber(L, 2);
        PushQuat(L, Vectormath::Aos::Quat::rotation(angle, *axis));
        return 1;
    }

    static int Quat_FromBasis(lua_State* L)
    {
        Vectormath::Aos::Vector3* x = CheckVector3(L, 1);
        Vectormath::Aos::Vector3* y = CheckVector3(L, 2);
        Vectormath::Aos::Vector3* z = CheckVector3(L, 3);
        Vectormath::Aos::Matrix3 m;
        m.setCol0(*x);
        m.setCol1(*y);
        m.setCol2(*z);
        PushQuat(L, Vectormath::Aos::Quat(m));
        return 1;
    }

    static int Quat_FromRotationX(lua_State* L)
    {
        float angle = luaL_checknumber(L, 1);
        PushQuat(L, Vectormath::Aos::Quat::rotationX(angle));
        return 1;
    }

    static int Quat_FromRotationY(lua_State* L)
    {
        float angle = luaL_checknumber(L, 1);
        PushQuat(L, Vectormath::Aos::Quat::rotationY(angle));
        return 1;
    }

    static int Quat_FromRotationZ(lua_State* L)
    {
        float angle = luaL_checknumber(L, 1);
        PushQuat(L, Vectormath::Aos::Quat::rotationZ(angle));
        return 1;
    }

    static int Dot(lua_State* L)
    {
        Vectormath::Aos::Vector3* v1 = CheckVector3(L, 1);
        Vectormath::Aos::Vector3* v2 = CheckVector3(L, 2);
        lua_pushnumber(L, Vectormath::Aos::dot(*v1, *v2));
        return 1;
    }

    static int LengthSqr(lua_State* L)
    {
        Vectormath::Aos::Vector3* v = CheckVector3(L, 1);
        lua_pushnumber(L, Vectormath::Aos::lengthSqr(*v));
        return 1;
    }

    static int Length(lua_State* L)
    {
        Vectormath::Aos::Vector3* v = CheckVector3(L, 1);
        lua_pushnumber(L, Vectormath::Aos::length(*v));
        return 1;
    }

    static int Normalize(lua_State* L)
    {
        Vectormath::Aos::Vector3* v = CheckVector3(L, 1);
        PushVector3(L, Vectormath::Aos::normalize(*v));
        return 1;
    }

    static int Cross(lua_State* L)
    {
        Vectormath::Aos::Vector3* v1 = CheckVector3(L, 1);
        Vectormath::Aos::Vector3* v2 = CheckVector3(L, 2);
        PushVector3(L, Vectormath::Aos::cross(*v1, *v2));
        return 1;
    }

    static int Lerp(lua_State* L)
    {
        float t = luaL_checknumber(L, 1);
        if (IsVector3(L, 2) && IsVector3(L, 3))
        {
            Vectormath::Aos::Vector3* v1 = (Vectormath::Aos::Vector3*)luaL_checkudata(L, 2, TYPE_NAME_VECTOR3);
            Vectormath::Aos::Vector3* v2 = (Vectormath::Aos::Vector3*)luaL_checkudata(L, 3, TYPE_NAME_VECTOR3);
            PushVector3(L, Vectormath::Aos::lerp(t, *v1, *v2));
            return 1;
        }
        else
        {
            Vectormath::Aos::Quat* q1 = (Vectormath::Aos::Quat*)luaL_checkudata(L, 2, TYPE_NAME_QUAT);
            Vectormath::Aos::Quat* q2 = (Vectormath::Aos::Quat*)luaL_checkudata(L, 3, TYPE_NAME_QUAT);
            PushQuat(L, Vectormath::Aos::lerp(t, *q1, *q2));
            return 1;
        }
        return luaL_error(L, "%s.%s takes one number and either two %s.%s or two %s.%s as arguments.", LIB_NAME, "lerp", LIB_NAME, TYPE_NAME_VECTOR3, LIB_NAME, TYPE_NAME_QUAT);
    }

    static int Slerp(lua_State* L)
    {
        float t = luaL_checknumber(L, 1);
        if (IsVector3(L, 2) && IsVector3(L, 3))
        {
            Vectormath::Aos::Vector3* v1 = CheckVector3(L, 2);
            Vectormath::Aos::Vector3* v2 = CheckVector3(L, 3);
            PushVector3(L, Vectormath::Aos::slerp(t, *v1, *v2));
            return 1;
        }
        else
        {
            Vectormath::Aos::Quat* q1 = (Vectormath::Aos::Quat*)luaL_checkudata(L, 2, TYPE_NAME_QUAT);
            Vectormath::Aos::Quat* q2 = (Vectormath::Aos::Quat*)luaL_checkudata(L, 3, TYPE_NAME_QUAT);
            PushQuat(L, Vectormath::Aos::slerp(t, *q1, *q2));
            return 1;
        }
        return luaL_error(L, "%s.%s takes one number and either two %s.%s or two %s.%s as arguments.", LIB_NAME, "slerp", LIB_NAME, TYPE_NAME_VECTOR3, LIB_NAME, TYPE_NAME_QUAT);
    }

    static int Conj(lua_State* L)
    {
        Vectormath::Aos::Quat* q = CheckQuat(L, 1);
        PushQuat(L, Vectormath::Aos::conj(*q));
        return 1;
    }

    static int Rotate(lua_State* L)
    {
        Vectormath::Aos::Quat* q = CheckQuat(L, 1);
        Vectormath::Aos::Vector3* v = CheckVector3(L, 2);
        PushVector3(L, Vectormath::Aos::rotate(*q, *v));
        return 1;
    }

    static const luaL_reg methods[] =
    {
        {TYPE_NAME_VECTOR3, Vector3_new},
        {TYPE_NAME_VECTOR4, Vector4_new},
        {TYPE_NAME_POINT3, Point3_new},
        {TYPE_NAME_QUAT, Quat_new},
        {"quat_from_start_to_end", Quat_FromStartToEnd},
        {"quat_from_axis_angle", Quat_FromAxisAngle},
        {"quat_from_basis", Quat_FromBasis},
        {"quat_from_rotation_x", Quat_FromRotationX},
        {"quat_from_rotation_y", Quat_FromRotationY},
        {"quat_from_rotation_z", Quat_FromRotationZ},
        {"dot", Dot},
        {"length_sqr", LengthSqr},
        {"length", Length},
        {"normalize", Normalize},
        {"cross", Cross},
        {"lerp", Lerp},
        {"slerp", Slerp},
        {"conj", Conj},
        {"rotate", Rotate},
        {0, 0}
    };

    void InitializeVecMath(lua_State* L)
    {
        int top = lua_gettop(L);

        const uint32_t type_count = 4;
        struct
        {
            const char* m_Name;
            const luaL_reg* m_Methods;
            const luaL_reg* m_Metatable;
        } types[type_count] =
        {
            {TYPE_NAME_VECTOR3, Vector3_methods, Vector3_meta},
            {TYPE_NAME_VECTOR4, Vector4_methods, Vector4_meta},
            {TYPE_NAME_POINT3, Point3_methods, Point3_meta},
            {TYPE_NAME_QUAT, Quat_methods, Quat_meta}
        };
        for (uint32_t i = 0; i < type_count; ++i)
        {
            // create methods table, add it to the globals
            luaL_register(L, types[i].m_Name, types[i].m_Methods);
            int methods = lua_gettop(L);
            // create metatable for the type, add it to the Lua registry
            luaL_newmetatable(L, types[i].m_Name);
            int metatable = lua_gettop(L);
            // fill metatable
            luaL_register(L, 0, types[i].m_Metatable);

            lua_pushliteral(L, "__metatable");
            lua_pushvalue(L, methods);// dup methods table
            lua_settable(L, metatable);

            lua_pop(L, 2);
        }
        luaL_register(L, LIB_NAME, methods);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

    void PushVector3(lua_State* L, const Vectormath::Aos::Vector3& v)
    {
        Vectormath::Aos::Vector3* vp = (Vectormath::Aos::Vector3*)lua_newuserdata(L, sizeof(Vectormath::Aos::Vector3));
        *vp = v;
        luaL_getmetatable(L, TYPE_NAME_VECTOR3);
        lua_setmetatable(L, -2);
    }

    Vectormath::Aos::Vector3* CheckVector3(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            return (Vectormath::Aos::Vector3*)luaL_checkudata(L, index, TYPE_NAME_VECTOR3);
        }
        luaL_typerror(L, index, TYPE_NAME_VECTOR3);
        return 0x0;
    }

    void PushVector4(lua_State* L, const Vectormath::Aos::Vector4& v)
    {
        Vectormath::Aos::Vector4* vp = (Vectormath::Aos::Vector4*)lua_newuserdata(L, sizeof(Vectormath::Aos::Vector4));
        *vp = v;
        luaL_getmetatable(L, TYPE_NAME_VECTOR4);
        lua_setmetatable(L, -2);
    }

    Vectormath::Aos::Vector4* CheckVector4(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            return (Vectormath::Aos::Vector4*)luaL_checkudata(L, index, TYPE_NAME_VECTOR4);
        }
        luaL_typerror(L, index, TYPE_NAME_VECTOR4);
        return 0x0;
    }

    void PushPoint3(lua_State* L, const Vectormath::Aos::Point3& v)
    {
        Vectormath::Aos::Point3* vp = (Vectormath::Aos::Point3*)lua_newuserdata(L, sizeof(Vectormath::Aos::Point3));
        *vp = v;
        luaL_getmetatable(L, TYPE_NAME_POINT3);
        lua_setmetatable(L, -2);
    }

    Vectormath::Aos::Point3* CheckPoint3(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            return (Vectormath::Aos::Point3*)luaL_checkudata(L, index, TYPE_NAME_POINT3);
        }
        luaL_typerror(L, index, TYPE_NAME_POINT3);
        return 0x0;
    }

    void PushQuat(lua_State* L, const Vectormath::Aos::Quat& q)
    {
        Vectormath::Aos::Quat* qp = (Vectormath::Aos::Quat*)lua_newuserdata(L, sizeof(Vectormath::Aos::Quat));
        *qp = q;
        luaL_getmetatable(L, TYPE_NAME_QUAT);
        lua_setmetatable(L, -2);
    }

    Vectormath::Aos::Quat* CheckQuat(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            return (Vectormath::Aos::Quat*)luaL_checkudata(L, index, TYPE_NAME_QUAT);
        }
        luaL_typerror(L, index, TYPE_NAME_QUAT);
        return 0x0;
    }
}
