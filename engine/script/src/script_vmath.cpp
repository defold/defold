#include "script.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <dlib/vmath.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    /*# Vector math API documentation
     *
     * Functions for mathematical operations on vectors, matrices and quaternions.
     *
     * - The vector types (`vmath.vector3` and vmath.vector4`) supports addition, subtraction,
     * negation and multiplication with numbers and other vectors of the same type.
     * - The quaternion type (`vmath.quat`) supports multiplication with other quaternions.
     * - The matrix type (`vmath.matrix4`) can be multiplied with numbers, other matrices and `vmath.vector4` values.
     * - All types performs equality comparison by each component value.
     *
     * @document
     * @name Vector math
     * @namespace vmath
     */

#define SCRIPT_LIB_NAME "vmath"
#define SCRIPT_TYPE_NAME_VECTOR "vector" // Vector of unspecific length
#define SCRIPT_TYPE_NAME_VECTOR3 "vector3"
#define SCRIPT_TYPE_NAME_VECTOR4 "vector4"
#define SCRIPT_TYPE_NAME_QUAT "quat"
#define SCRIPT_TYPE_NAME_MATRIX4 "matrix4"

    bool IsVector(lua_State *L, int index)
    {
        void *p = lua_touserdata(L, index);
        bool result = false;
        if (p != 0x0)
        {
            if (lua_getmetatable(L, index))
            {
                lua_getfield(L, LUA_REGISTRYINDEX, SCRIPT_TYPE_NAME_VECTOR);
                if (lua_rawequal(L, -1, -2))
                {
                    result = true;
                }
                lua_pop(L, 2);
            }
        }
        return result;
    }

    static const luaL_reg Vector_methods[] =
    {
        {0,0}
    };

    static int Vector_len(lua_State *L)
    {
        dmVMath::FloatVector* v = CheckVector(L, 1);

        lua_pushnumber(L, v->size);
        return 1;
    }

    static int Vector_index(lua_State *L)
    {
        dmVMath::FloatVector* v = CheckVector(L, 1);

        int key = luaL_checkinteger(L, 2);
        if (key > 0 && key <= v->size)
        {
            lua_pushnumber(L, v->values[key-1]);
            return 1;
        }
        else
        {
            if (v->size > 0)
            {
                return luaL_error(L, "%s.%s only has valid indices between 1 and %d.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_VECTOR, v->size);
            }

            return luaL_error(L, "%s.%s has no addressable indices, size is 0.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_VECTOR);
        }
    }

    static int Vector_newindex(lua_State *L)
    {
        dmVMath::FloatVector* v = CheckVector(L, 1);

        int key = luaL_checkinteger(L, 2);
        if (key > 0 && key <= v->size)
        {
            v->values[key-1] = (float)luaL_checknumber(L, 3);
        }
        else
        {
            if (v->size > 0)
            {
                return luaL_error(L, "%s.%s only has valid indices between 1 and %d.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_VECTOR, v->size);
            }

            return luaL_error(L, "%s.%s has no addressable indices, size is 0.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_VECTOR);
        }
        return 0;
    }

    static int Vector_tostring(lua_State *L)
    {
        dmVMath::FloatVector* v = CheckVector(L, 1);
        lua_pushfstring(L, "%s.%s (size: %d)", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_VECTOR, v->size);
        return 1;
    }

    static int Vector_gc(lua_State *L)
    {
        dmVMath::FloatVector* v = CheckVector(L, 1);
        delete v;
        return 0;
    }

    static const luaL_reg Vector_meta[] =
    {
        {"__gc",        Vector_gc},
        {"__tostring",  Vector_tostring},
        {"__len",       Vector_len},
        {"__index",     Vector_index},
        {"__newindex",  Vector_newindex},
        {0,0}
    };

    bool IsVector3(lua_State *L, int index)
    {
        void *p = lua_touserdata(L, index);
        bool result = false;
        if (p != 0x0)
        {  /* value is a userdata? */
            if (lua_getmetatable(L, index))
            {  /* does it have a metatable? */
                lua_getfield(L, LUA_REGISTRYINDEX, SCRIPT_TYPE_NAME_VECTOR3);  /* get correct metatable */
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
        lua_pushfstring(L, "vmath.%s(%f, %f, %f)", SCRIPT_TYPE_NAME_VECTOR3, v->getX(), v->getY(), v->getZ());
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
            return luaL_error(L, "%s.%s only has fields x, y, z.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_VECTOR3);
        }
    }

    static int Vector3_newindex(lua_State *L)
    {
        Vectormath::Aos::Vector3* v = CheckVector3(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (key[0] == 'x')
        {
            v->setX((float) luaL_checknumber(L, 3));
        }
        else if (key[0] == 'y')
        {
            v->setY((float) luaL_checknumber(L, 3));
        }
        else if (key[0] == 'z')
        {
            v->setZ((float) luaL_checknumber(L, 3));
        }
        else
        {
            return luaL_error(L, "%s.%s only has fields x, y, z.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_VECTOR3);
        }
        return 0;
    }

    static int Vector3_add(lua_State *L)
    {
        Vectormath::Aos::Vector3* v1 = CheckVector3(L, 1);
        Vectormath::Aos::Vector3* v2 = CheckVector3(L, 2);
        PushVector3(L, *v1 + *v2);
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
        Vectormath::Aos::Vector3* v;
        float s;
        if (IsVector3(L, 1))
        {
            v = CheckVector3(L, 1);
            s = (float) luaL_checknumber(L, 2);
        }
        else
        {
            s = (float) luaL_checknumber(L, 1);
            v = CheckVector3(L, 2);
        }
        PushVector3(L, *v * s);
        return 1;
    }

    static int Vector3_unm(lua_State *L)
    {
        Vectormath::Aos::Vector3* v = CheckVector3(L, 1);
        PushVector3(L, - *v);
        return 1;
    }

    static int Vector3_concat(lua_State *L)
    {
        const char* s = luaL_checkstring(L, 1);
        Vectormath::Aos::Vector3* v = CheckVector3(L, 2);
        size_t size = 48 + strlen(s);
        char* buffer = new char[size];
        DM_SNPRINTF(buffer, size, "%s[%f, %f, %f]", s, v->getX(), v->getY(), v->getZ());
        lua_pushstring(L, buffer);
        delete [] buffer;
        return 1;
    }

    static int Vector3_eq(lua_State *L)
    {
        Vectormath::Aos::Vector3* v1 = CheckVector3(L, 1);
        Vectormath::Aos::Vector3* v2 = CheckVector3(L, 2);
        lua_pushboolean(L, v1->getX() == v2->getX() && v1->getY() == v2->getY() && v1->getZ() == v2->getZ());
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
        {"__concat", Vector3_concat},
        {"__eq", Vector3_eq},
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
                lua_getfield(L, LUA_REGISTRYINDEX, SCRIPT_TYPE_NAME_VECTOR4);  /* get correct metatable */
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
        lua_pushfstring(L, "vmath.%s(%f, %f, %f, %f)", SCRIPT_TYPE_NAME_VECTOR4, v->getX(), v->getY(), v->getZ(), v->getW());
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
            return luaL_error(L, "%s.%s only has fields x, y, z, w.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_VECTOR4);
        }
    }

    static int Vector4_newindex(lua_State *L)
    {
        Vectormath::Aos::Vector4* v = CheckVector4(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (key[0] == 'x')
        {
            v->setX((float) luaL_checknumber(L, 3));
        }
        else if (key[0] == 'y')
        {
            v->setY((float) luaL_checknumber(L, 3));
        }
        else if (key[0] == 'z')
        {
            v->setZ((float) luaL_checknumber(L, 3));
        }
        else if (key[0] == 'w')
        {
            v->setW((float) luaL_checknumber(L, 3));
        }
        else
        {
            return luaL_error(L, "%s.%s only has fields x, y, z, w.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_VECTOR4);
        }
        return 0;
    }

    static int Vector4_add(lua_State *L)
    {
        Vectormath::Aos::Vector4* v1 = CheckVector4(L, 1);
        Vectormath::Aos::Vector4* v2 = CheckVector4(L, 2);
        PushVector4(L, *v1 + *v2);
        return 1;
    }

    static int Vector4_sub(lua_State *L)
    {
        Vectormath::Aos::Vector4* v1 = CheckVector4(L, 1);
        Vectormath::Aos::Vector4* v2 = CheckVector4(L, 2);
        PushVector4(L, *v1 - *v2);
        return 1;
    }

    static int Vector4_mul(lua_State *L)
    {
        Vectormath::Aos::Vector4* v;
        float s;
        if (IsVector4(L, 1))
        {
            v = CheckVector4(L, 1);
            s = (float) luaL_checknumber(L, 2);
        }
        else
        {
            s = (float) luaL_checknumber(L, 1);
            v = CheckVector4(L, 2);
        }
        PushVector4(L, *v * s);
        return 1;
    }

    static int Vector4_unm(lua_State *L)
    {
        Vectormath::Aos::Vector4* v = CheckVector4(L, 1);
        PushVector4(L, - *v);
        return 1;
    }

    static int Vector4_concat(lua_State *L)
    {
        const char* s = luaL_checkstring(L, 1);
        Vectormath::Aos::Vector4* v = CheckVector4(L, 2);
        size_t size = 64 + strlen(s);
        char* buffer = new char[size];
        DM_SNPRINTF(buffer, size, "%s[%f, %f, %f, %f]", s, v->getX(), v->getY(), v->getZ(), v->getW());
        lua_pushstring(L, buffer);
        delete [] buffer;
        return 1;
    }

    static int Vector4_eq(lua_State *L)
    {
        Vectormath::Aos::Vector4* v1 = CheckVector4(L, 1);
        Vectormath::Aos::Vector4* v2 = CheckVector4(L, 2);
        lua_pushboolean(L, v1->getX() == v2->getX() && v1->getY() == v2->getY() && v1->getZ() == v2->getZ() && v1->getW() == v2->getW());
        return 1;
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
        {"__add",       Vector4_add},
        {"__sub",       Vector4_sub},
        {"__mul",       Vector4_mul},
        {"__unm",       Vector4_unm},
        {"__concat",    Vector4_concat},
        {"__eq",        Vector4_eq},
        {0,0}
    };

    bool IsQuat(lua_State *L, int index)
    {
        void *p = lua_touserdata(L, index);
        bool result = false;
        if (p != 0x0)
        {  /* value is a userdata? */
            if (lua_getmetatable(L, index))
            {  /* does it have a metatable? */
                lua_getfield(L, LUA_REGISTRYINDEX, SCRIPT_TYPE_NAME_QUAT);  /* get correct metatable */
                if (lua_rawequal(L, -1, -2))
                {  /* does it have the correct mt? */
                    result = true;
                }
                lua_pop(L, 2);  /* remove both metatables */
            }
        }
        return result;
    }

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
        lua_pushfstring(L, "vmath.%s(%f, %f, %f, %f)", SCRIPT_TYPE_NAME_QUAT, q->getX(), q->getY(), q->getZ(), q->getW());
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
            return luaL_error(L, "%s.%s only has fields x, y, z, w.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_QUAT);
        }
        return 0;
    }

    static int Quat_newindex(lua_State *L)
    {
        Vectormath::Aos::Quat* q = CheckQuat(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (key[0] == 'x')
        {
            q->setX((float) luaL_checknumber(L, -1));
        }
        else if (key[0] == 'y')
        {
            q->setY((float) luaL_checknumber(L, -1));
        }
        else if (key[0] == 'z')
        {
            q->setZ((float) luaL_checknumber(L, -1));
        }
        else if (key[0] == 'w')
        {
            q->setW((float) luaL_checknumber(L, -1));
        }
        else
        {
            return luaL_error(L, "%s.%s only has fields x, y, z, w.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_QUAT);
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

    static int Quat_concat(lua_State *L)
    {
        const char* s = luaL_checkstring(L, 1);
        Vectormath::Aos::Quat* q = CheckQuat(L, 2);
        size_t size = 64 + strlen(s);
        char* buffer = new char[size];
        DM_SNPRINTF(buffer, size, "%s[%f, %f, %f, %f]", s, q->getX(), q->getY(), q->getZ(), q->getW());
        lua_pushstring(L, buffer);
        delete [] buffer;
        return 1;
    }

    static int Quat_eq(lua_State *L)
    {
        Vectormath::Aos::Quat* q1 = CheckQuat(L, 1);
        Vectormath::Aos::Quat* q2 = CheckQuat(L, 2);
        lua_pushboolean(L, q1->getX() == q2->getX() && q1->getY() == q2->getY() && q1->getZ() == q2->getZ() && q1->getW() == q2->getW());
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
        {"__concat",    Quat_concat},
        {"__eq",        Quat_eq},
        {0,0}
    };

    bool IsMatrix4(lua_State *L, int index)
    {
        void *p = lua_touserdata(L, index);
        bool result = false;
        if (p != 0x0)
        {  /* value is a userdata? */
            if (lua_getmetatable(L, index))
            {  /* does it have a metatable? */
                lua_getfield(L, LUA_REGISTRYINDEX, SCRIPT_TYPE_NAME_MATRIX4);  /* get correct metatable */
                if (lua_rawequal(L, -1, -2))
                {  /* does it have the correct mt? */
                    result = true;
                }
                lua_pop(L, 2);  /* remove both metatables */
            }
        }
        return result;
    }

    static int Matrix4_gc(lua_State *L)
    {
        Vectormath::Aos::Matrix4* m = CheckMatrix4(L, 1);
        memset(m, 0, sizeof(*m));
        (void) m;
        assert(m);
        return 0;
    }

    static int Matrix4_tostring(lua_State *L)
    {
        Vectormath::Aos::Matrix4* m = CheckMatrix4(L, 1);
        lua_pushfstring(L, "vmath.%s(%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f)", SCRIPT_TYPE_NAME_MATRIX4,
            m->getElem(0, 0), m->getElem(1, 0), m->getElem(2, 0), m->getElem(3, 0),
            m->getElem(0, 1), m->getElem(1, 1), m->getElem(2, 1), m->getElem(3, 1),
            m->getElem(0, 2), m->getElem(1, 2), m->getElem(2, 2), m->getElem(3, 2),
            m->getElem(0, 3), m->getElem(1, 3), m->getElem(2, 3), m->getElem(3, 3));
        return 1;
    }

    static int Matrix4_index(lua_State *L)
    {
        Vectormath::Aos::Matrix4* m = CheckMatrix4(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (strlen(key) == 3)
        {
            int row = key[1] - (char)'0';
            int col = key[2] - (char)'0';
            if (0 <= row && row < 4 && 0 <= col && col < 4)
            {
                lua_pushnumber(L, m->getElem(col, row));
                return 1;
            }
        }
        else if (strlen(key) == 2)
        {
            int col = key[1] - (char)'0';
            if (0 <= col && col < 4)
            {
                PushVector4(L, m->getCol(col));
                return 1;
            }
        }
        return luaL_error(L, "%s.%s only has fields c0, ..., c3 and m00, m01, ..., m10, ..., m33.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_MATRIX4);
    }

    static int Matrix4_newindex(lua_State *L)
    {
        Vectormath::Aos::Matrix4* m = CheckMatrix4(L, 1);

        const char* key = luaL_checkstring(L, 2);
        if (strlen(key) == 3)
        {
            int row = key[1] - (char)'0';
            int col = key[2] - (char)'0';
            if (0 <= row && row < 4 && 0 <= col && col < 4)
            {
                m->setElem(col, row, (float) luaL_checknumber(L, -1));
                return 0;
            }
        }
        else if (strlen(key) == 2)
        {
            int col = key[1] - (char)'0';
            if (0 <= col && col < 4)
            {
                Vectormath::Aos::Vector4* vec = CheckVector4(L, -1);
                m->setCol(col, *vec);
                return 0;
            }
        }
        return luaL_error(L, "%s.%s only has fields c0, ..., c3 and m00, m01, ..., m10, ..., m33.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_MATRIX4);
    }

    static int Matrix4_mul(lua_State *L)
    {
        Vectormath::Aos::Matrix4 m1;
        if (lua_isnumber(L, 1))
        {
            float f = (float) lua_tonumber(L, 1);
            m1 = *CheckMatrix4(L, 2);
            PushMatrix4(L, m1 * f);
        }
        else
        {
            m1 = *CheckMatrix4(L, 1);
            if (IsMatrix4(L, 2))
            {
                Vectormath::Aos::Matrix4 m2 = *CheckMatrix4(L, 2);
                PushMatrix4(L, m1 * m2);
            }
            else if (IsVector4(L, 2))
            {
                Vectormath::Aos::Vector4 v = *CheckVector4(L, 2);
                PushVector4(L, m1 * v);
            }
            else if (lua_isnumber(L, 2))
            {
                float f = (float) luaL_checknumber(L, 2);
                PushMatrix4(L, m1 * f);
            }
            else
            {
                return luaL_error(L, "%s.%s can only be multiplied with a number, another %s or a %s.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_MATRIX4, SCRIPT_TYPE_NAME_MATRIX4, SCRIPT_TYPE_NAME_VECTOR4);
            }
        }
        return 1;
    }

    static int Matrix4_concat(lua_State *L)
    {
        const char* s = luaL_checkstring(L, 1);
        Vectormath::Aos::Matrix4* m = CheckMatrix4(L, 2);
        size_t size = 256 + strlen(s);
        char* buffer = new char[size];
        DM_SNPRINTF(buffer, size, "%s[%f, %f, %f, %f| %f, %f, %f, %f| %f, %f, %f, %f| %f, %f, %f, %f]", s,
            m->getElem(0, 0), m->getElem(1, 0), m->getElem(2, 0), m->getElem(3, 0),
            m->getElem(0, 1), m->getElem(1, 1), m->getElem(2, 1), m->getElem(3, 1),
            m->getElem(0, 2), m->getElem(1, 2), m->getElem(2, 2), m->getElem(3, 2),
            m->getElem(0, 3), m->getElem(1, 3), m->getElem(2, 3), m->getElem(3, 3));
        lua_pushstring(L, buffer);
        delete [] buffer;
        return 1;
    }

    static int Matrix4_eq(lua_State *L)
    {
        Vectormath::Aos::Matrix4* m1 = CheckMatrix4(L, 1);
        Vectormath::Aos::Matrix4* m2 = CheckMatrix4(L, 2);
        lua_pushboolean(L,
            m1->getElem(0, 0) == m2->getElem(0, 0) && m1->getElem(1, 0) == m2->getElem(1, 0) && m1->getElem(2, 0) == m2->getElem(2, 0) && m1->getElem(3, 0) == m2->getElem(3, 0) &&
            m1->getElem(0, 1) == m2->getElem(0, 1) && m1->getElem(1, 1) == m2->getElem(1, 1) && m1->getElem(2, 1) == m2->getElem(2, 1) && m1->getElem(3, 1) == m2->getElem(3, 1) &&
            m1->getElem(0, 2) == m2->getElem(0, 2) && m1->getElem(1, 2) == m2->getElem(1, 2) && m1->getElem(2, 2) == m2->getElem(2, 2) && m1->getElem(3, 2) == m2->getElem(3, 2) &&
            m1->getElem(0, 3) == m2->getElem(0, 3) && m1->getElem(1, 3) == m2->getElem(1, 3) && m1->getElem(2, 3) == m2->getElem(2, 3) && m1->getElem(3, 3) == m2->getElem(3, 3));
        return 1;
    }

    static const luaL_reg Matrix4_methods[] =
    {
        {0,0}
    };
    static const luaL_reg Matrix4_meta[] =
    {
        {"__gc",        Matrix4_gc},
        {"__tostring",  Matrix4_tostring},
        {"__index",     Matrix4_index},
        {"__newindex",  Matrix4_newindex},
        {"__mul",       Matrix4_mul},
        {"__concat",    Matrix4_concat},
        {"__eq",        Matrix4_eq},
        {0,0}
    };

    /*# create a new vector from a table of values
     * Creates a vector of arbitrary size. The vector is initialized
     * with numeric values from a table.
     *
     * [icon:attention] The table values are converted to floating point
     * values. If a value cannot be converted, a 0 is stored in that
     * value position in the vector.
     *
     * @name vmath.vector
     * @param t [type:table] table of numbers
     * @return v [type:vector] new vector
     * @examples
     *
     * How to create a vector with custom data to be used for animation easing:
     *
     * ```lua
     * local values = { 0, 0.5, 0 }
     * local vec = vmath.vector(values)
     * print(vec) --> vmath.vector (size: 3)
     * ```
     */
    static int Vector_new(lua_State* L)
    {
        dmVMath::FloatVector *v;
        if (lua_gettop(L) == 0)
        {
            v = new dmVMath::FloatVector(0);
        }
        else
        {
            luaL_checktype(L, 1, LUA_TTABLE);
            int array_size = lua_objlen(L, 1);
            v = new dmVMath::FloatVector(array_size);

            for (int i = 0; i < array_size; i++)
            {
                lua_pushnumber(L, i+1);
                lua_gettable(L, 1);
                v->values[i] = (float) lua_tonumber(L, -1);
                lua_pop(L, 1);
            }
        }

        PushVector(L, v);
        return 1;
    }

    /*# creates a new zero vector
     *
     * Creates a new zero vector with all components set to 0.
     *
     * @name vmath.vector3
     * @return v [type:vector3] new zero vector
     * @examples
     *
     * ```lua
     * local vec = vmath.vector3()
     * print(vec) --> vmath.vector3(0, 0, 0)
     * ```
     */

    /*# creates a new vector from scalar value
     *
     * Creates a new vector with all components set to the
     * supplied scalar value.
     *
     * @name vmath.vector3
     * @param n [type:number] scalar value to splat
     * @return v [type:vector3] new vector
     * @examples
     *
     * ```lua
     * local vec = vmath.vector3(1.0)
     * print(vec) --> vmath.vector3(1, 1, 1)
     * ```
     */

    /*# creates a new vector from another existing vector
     *
     * Creates a new vector with all components set to the
     * corresponding values from the supplied vector. I.e.
     * This function creates a copy of the given vector.
     *
     * @name vmath.vector3
     * @param v1 [type:vector3] existing vector
     * @return v [type:vector3] new vector
     * @examples
     *
     * ```lua
     * local vec1 = vmath.vector3(1.0)
     * local vec2 = vmath.vector3(vec1)
     * if vec1 == vec2 then
     *     -- yes, they are equal
     *     print(vec2) --> vmath.vector3(1, 1, 1)
     * end
     * ```
     */

    /*# creates a new vector from its coordinates
     *
     * Creates a new vector with the components set to the
     * supplied values.
     *
     * @name vmath.vector3
     * @param x [type:number] x coordinate
     * @param y [type:number] y coordinate
     * @param z [type:number] z coordinate
     * @return v [type:vector3] new vector
     * @examples
     *
     * ```lua
     * local vec = vmath.vector3(1.0, 2.0, 3.0)
     * print(vec) --> vmath.vector3(1, 2, 3)
     * print(-vec) --> vmath.vector3(-1, -2, -3)
     * print(vec * 2) --> vmath.vector3(2, 4, 6)
     * print(vec + vmath.vector3(2.0)) --> vmath.vector3(3, 4, 5)
     * print(vec - vmath.vector3(2.0)) --> vmath.vector3(-1, 0, 1)
     * ```
     */
    static int Vector3_new(lua_State* L)
    {
        Vectormath::Aos::Vector3 v;
        if (lua_gettop(L) == 0)
        {
            v = Vectormath::Aos::Vector3(0.0f, 0.0f, 0.0f);
        }
        else if (lua_gettop(L) == 1)
        {
            int type = lua_type(L, -1);
            if (type == LUA_TNUMBER)
            {
                float x = (float) lua_tonumber(L, -1);
                v = Vectormath::Aos::Vector3(x, x, x);
            }
            else
            {
                // If not number assume vector3
                v = *CheckVector3(L, -1);
            }
        }
        else
        {
            v.setX((float) luaL_checknumber(L, 1));
            v.setY((float) luaL_checknumber(L, 2));
            v.setZ((float) luaL_checknumber(L, 3));
        }
        PushVector3(L, v);
        return 1;
    }

    /*# creates a new zero vector
     *
     * Creates a new zero vector with all components set to 0.
     *
     * @name vmath.vector4
     * @return v [type:vector4] new zero vector
     * @examples
     *
     * ```lua
     * local vec = vmath.vector4()
     * print(vec) --> vmath.vector4(0, 0, 0, 0)
     * ```
     */

    /*# creates a new vector from scalar value
     *
     * Creates a new vector with all components set to the
     * supplied scalar value.
     *
     * @name vmath.vector4
     * @param n [type:number] scalar value to splat
     * @return v [type:vector4] new vector
     * @examples
     *
     * ```lua
     * local vec = vmath.vector4(1.0)
     * print(vec) --> vmath.vector4(1, 1, 1, 1)
     * ```
     */

    /*# creates a new vector from another existing vector
     *
     * Creates a new vector with all components set to the
     * corresponding values from the supplied vector. I.e.
     * This function creates a copy of the given vector.
     *
     * @name vmath.vector4
     * @param v1 [type:vector4] existing vector
     * @return v [type:vector4] new vector
     * @examples
     *
     * ```lua
     * local vect1 = vmath.vector4(1.0)
     * local vect2 = vmath.vector4(vec1)
     * if vec1 == vec2 then
     *     -- yes, they are equal
     *     print(vec2) --> vmath.vector4(1, 1, 1, 1)
     * end
     * ```
     */

    /*# creates a new vector from its coordinates
     *
     * Creates a new vector with the components set to the
     * supplied values.
     *
     * @name vmath.vector4
     * @param x [type:number] x coordinate
     * @param y [type:number] y coordinate
     * @param z [type:number] z coordinate
     * @param w [type:number] w coordinate
     * @return v [type:vector4] new vector
     * @examples
     *
     * ```lua
     * local vec = vmath.vector4(1.0, 2.0, 3.0, 4.0)
     * print(vec) --> vmath.vector4(1, 2, 3, 4)
     * print(-vec) --> vmath.vector4(-1, -2, -3, -4)
     * print(vec * 2) --> vmath.vector4(2, 4, 6, 8)
     * print(vec + vmath.vector4(2.0)) --> vmath.vector4(3, 4, 5, 6)
     * print(vec - vmath.vector4(2.0)) --> vmath.vector4(-1, 0, 1, 2)
     * ```
     */
    static int Vector4_new(lua_State* L)
    {
        Vectormath::Aos::Vector4 v;
        // NOTE: The following comment is obsolete
        // "No empty constructor, since the value of w matters"
        // Don't understand why. Every component matters :-)
        // Empty constructor added.
        if (lua_gettop(L) == 0)
        {
            v = Vectormath::Aos::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        }
        else if (lua_gettop(L) == 1)
        {
            int type = lua_type(L, -1);
            if (type == LUA_TNUMBER)
            {
                float x = (float) lua_tonumber(L, -1);
                v = Vectormath::Aos::Vector4(x, x, x, x);
            }
            else
            {
                // If not number assume vector4
                v = *CheckVector4(L, -1);
            }
        }
        else
        {
            v.setX((float) luaL_checknumber(L, 1));
            v.setY((float) luaL_checknumber(L, 2));
            v.setZ((float) luaL_checknumber(L, 3));
            v.setW((float) luaL_checknumber(L, 4));
        }
        PushVector4(L, v);
        return 1;
    }

    /*# creates a new identity quaternion
     *
     * Creates a new identity quaternion. The identity 
     * quaternion is equal to:
     *
     * `vmath.quat(0, 0, 0, 1)`
     *
     * @name vmath.quat
     * @return q [type:quaternion] new identity quaternion
     * @examples
     *
     * ```lua
     * local quat = vmath.quat()
     * print(quat) --> vmath.quat(0, 0, 0, 1)
     * ```
     */

    /*# creates a new quaternion from another existing quaternion
     *
     * Creates a new quaternion with all components set to the
     * corresponding values from the supplied quaternion. I.e.
     * This function creates a copy of the given quaternion.
     *
     * @name vmath.quat
     * @param q1 [type:quaternion] existing quaternion
     * @return q [type:quaternion] new quaternion
     * @examples
     *
     * ```lua
     * local quat1 = vmath.quat(1, 2, 3, 4)
     * local quat2 = vmath.quat(quat1)
     * if quat1 == quat2 then
     *     -- yes, they are equal
     *     print(quat2) --> vmath.quat(1, 2, 3, 4)
     * end
     * ```
     */

    /*# creates a new quaternion from its coordinates
     *
     * Creates a new quaternion with the components set
     * according to the supplied parameter values.
     *
     * @name vmath.quat
     * @param x [type:number] x coordinate
     * @param y [type:number] y coordinate
     * @param z [type:number] z coordinate
     * @param w [type:number] w coordinate
     * @return q [type:quaternion] new quaternion
     * @examples
     *
     * ```lua
     * local quat = vmath.quat(1, 2, 3, 4)
     * print(quat) --> vmath.quat(1, 2, 3, 4)
     * ```
     */
    static int Quat_new(lua_State* L)
    {
        Vectormath::Aos::Quat q;
        if (lua_gettop(L) == 0)
        {
            q = Vectormath::Aos::Quat::identity();
        }
        else if (lua_gettop(L) == 1)
        {
            q = *CheckQuat(L, -1);
        }
        else
        {
            q.setX((float) luaL_checknumber(L, 1));
            q.setY((float) luaL_checknumber(L, 2));
            q.setZ((float) luaL_checknumber(L, 3));
            q.setW((float) luaL_checknumber(L, 4));
        }
        PushQuat(L, q);
        return 1;
    }

    /*# creates a quaternion to rotate between two unit vectors
     *
     * The resulting quaternion describes the rotation that,
     * if applied to the first vector, would rotate the first
     * vector to the second. The two vectors must be unit
     * vectors (of length 1).
     *
     * [icon:attention] The result is undefined if the two vectors point in opposite directions
     *
     * @name vmath.quat_from_to
     * @param v1 [type:vector3] first unit vector, before rotation
     * @param v2 [type:vector3] second unit vector, after rotation
     * @return q [type:quaternion] quaternion representing the rotation from first to second vector
     * @examples
     *
     * ```lua
     * local v1 = vmath.vector3(1, 0, 0)
     * local v2 = vmath.vector3(0, 1, 0)
     * local rot = vmath.quat_from_to(v1, v2)
     * print(vmath.rotate(rot, v1)) --> vmath.vector3(0, 0.99999994039536, 0)
     * ```
     */
    static int Quat_FromTo(lua_State* L)
    {
        Vectormath::Aos::Vector3* v1 = CheckVector3(L, 1);
        Vectormath::Aos::Vector3* v2 = CheckVector3(L, 2);
        PushQuat(L, Vectormath::Aos::Quat::rotation(*v1, *v2));
        return 1;
    }

    /*# creates a quaternion to rotate around a unit vector
     *
     * The resulting quaternion describes a rotation of `angle`
     * radians around the axis described by the unit vector `v`.
     *
     * @name vmath.quat_axis_angle
     * @param v [type:vector3] axis
     * @param angle [type:number] angle
     * @return q [type:quaternion] quaternion representing the axis-angle rotation
     * @examples
     *
     * ```lua
     * local axis = vmath.vector3(1, 0, 0)
     * local rot = vmath.quat_axis_angle(axis, 3.141592653)
     * local vec = vmath.vector3(1, 1, 0)
     * print(vmath.rotate(rot, vec)) --> vmath.vector3(1, -1, -8.7422776573476e-08)
     * ```
     */
    static int Quat_AxisAngle(lua_State* L)
    {
        Vectormath::Aos::Vector3* axis = CheckVector3(L, 1);
        float angle = (float) luaL_checknumber(L, 2);
        PushQuat(L, Vectormath::Aos::Quat::rotation(angle, *axis));
        return 1;
    }

    /*# creates a quaternion from three base unit vectors
     *
     * The resulting quaternion describes the rotation from the
     * identity quaternion (no rotation) to the coordinate system
     * as described by the given x, y and z base unit vectors.
     *
     * @name vmath.quat_basis
     * @param x [type:vector3] x base vector
     * @param y [type:vector3] y base vector
     * @param z [type:vector3] z base vector
     * @return q [type:quaternion] quaternion representing the rotation of the specified base vectors
     * @examples
     *
     * ```lua
     * -- Axis rotated 90 degrees around z.
     * local rot_x = vmath.vector3(0, -1, 0)
     * local rot_y = vmath.vector3(1, 0, 0)
     * local z = vmath.vector3(0, 0, 1)
     * local rot1 = vmath.quat_basis(rot_x, rot_y, z)
     * local rot2 = vmath.quat_from_to(vmath.vector3(0, 1, 0), vmath.vector3(1, 0, 0))
     * if rot1 == rot2 then
     *     -- These quaternions are equal!
     *     print(rot2) --> vmath.quat(0, 0, -0.70710676908493, 0.70710676908493)
     * end
     * ```
     */
    static int Quat_Basis(lua_State* L)
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

    /*# creates a quaternion from rotation around x-axis
     *
     * The resulting quaternion describes a rotation of `angle`
     * radians around the x-axis.
     *
     * @name vmath.quat_rotation_x
     * @param angle [type:number] angle in radians around x-axis
     * @return q [type:quaternion] quaternion representing the rotation around the x-axis
     * @examples
     *
     * ```lua
     * local rot = vmath.quat_rotation_x(3.141592653)
     * local vec = vmath.vector3(1, 1, 0)
     * print(vmath.rotate(rot, vec)) --> vmath.vector3(1, -1, -8.7422776573476e-08)
     * ```
     */
    static int Quat_RotationX(lua_State* L)
    {
        float angle = (float) luaL_checknumber(L, 1);
        PushQuat(L, Vectormath::Aos::Quat::rotationX(angle));
        return 1;
    }

    /*# creates a quaternion from rotation around y-axis
     *
     * The resulting quaternion describes a rotation of `angle`
     * radians around the y-axis.
     *
     * @name vmath.quat_rotation_y
     * @param angle [type:number] angle in radians around y-axis
     * @return q [type:quaternion] quaternion representing the rotation around the y-axis
     * @examples
     *
     * ```lua
     * local rot = vmath.quat_rotation_y(3.141592653)
     * local vec = vmath.vector3(1, 1, 0)
     * print(vmath.rotate(rot, vec)) --> vmath.vector3(-1, 1, 8.7422776573476e-08)
     * ```
     */
    static int Quat_RotationY(lua_State* L)
    {
        float angle = (float) luaL_checknumber(L, 1);
        PushQuat(L, Vectormath::Aos::Quat::rotationY(angle));
        return 1;
    }

    /*# creates a quaternion from rotation around z-axis
     *
     * The resulting quaternion describes a rotation of `angle`
     * radians around the z-axis.
     *
     * @name vmath.quat_rotation_z
     * @param angle [type:number] angle in radians around z-axis
     * @return q [type:quaternion] quaternion representing the rotation around the z-axis
     * @examples
     *
     * ```lua
     * local rot = vmath.quat_rotation_z(3.141592653)
     * local vec = vmath.vector3(1, 1, 0)
     * print(vmath.rotate(rot, vec)) --> vmath.vector3(-0.99999988079071, -1, 0)
     * ```
     */
    static int Quat_RotationZ(lua_State* L)
    {
        float angle = (float) luaL_checknumber(L, 1);
        PushQuat(L, Vectormath::Aos::Quat::rotationZ(angle));
        return 1;
    }

    /*# creates a new identity matrix
     *
     * The resulting identity matrix describes a transform with
     * no translation or rotation.
     *
     * @name vmath.matrix4
     * @return m [type:matrix4] identity matrix
     * @examples
     *
     * ```lua
     * local mat = vmath.matrix4()
     * print(mat) --> vmath.matrix4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
     * ```
     */

    /*# creates a new matrix from another existing matrix
     *
     * Creates a new matrix with all components set to the
     * corresponding values from the supplied matrix. I.e.
     * the function creates a copy of the given matrix.
     *
     * @name vmath.matrix4
     * @param m1 [type:matrix4] existing matrix
     * @return m [type:matrix4] matrix which is a copy of the specified matrix
     * @examples
     *
     * ```lua
     * local mat1 = vmath.matrix4_rotation_x(3.141592653)
     * local mat2 = vmath.matrix4(mat1)
     * if mat1 == mat2 then
     *     -- yes, they are equal
     *     print(mat2) --> vmath.matrix4(1, 0, 0, 0, 0, -1, 8.7422776573476e-08, 0, 0, -8.7422776573476e-08, -1, 0, 0, 0, 0, 1)
     * end
     * ```
     */
    static int Matrix4_new(lua_State* L)
    {
        Vectormath::Aos::Matrix4 m;
        if (lua_gettop(L) == 0)
        {
            m = Vectormath::Aos::Matrix4::identity();
        }
        else if (lua_gettop(L) == 1)
        {
            m = *CheckMatrix4(L, -1);
        }
        else
        {
            return luaL_error(L, "A %s.%s can only be constructed with empty argument list or from another %s.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_MATRIX4, SCRIPT_TYPE_NAME_MATRIX4);
        }
        PushMatrix4(L, m);
        return 1;
    }

    /*# creates a frustum matrix
     *
     * Constructs a frustum matrix from the given values. The left, right,
     * top and bottom coordinates of the view cone are expressed as distances
     * from the center of the near clipping plane. The near and far coordinates
     * are expressed as distances from the tip of the view frustum cone.
     *
     * @name vmath.matrix4_frustum
     * @param left [type:number] coordinate for left clipping plane
     * @param right [type:number] coordinate for right clipping plane
     * @param bottom [type:number] coordinate for bottom clipping plane
     * @param top [type:number] coordinate for top clipping plane
     * @param near [type:number] coordinate for near clipping plane
     * @param far [type:number] coordinate for far clipping plane
     * @return m [type:matrix4] matrix representing the frustum
     * @examples
     *
     * ```lua
     * -- Construct a projection frustum with a vertical and horizontal
     * -- FOV of 45 degrees. Useful for rendering a square view.
     * local proj = vmath.matrix4_frustum(-1, 1, -1, 1, 1, 1000)
     * render.set_projection(proj)
     * ```
     */
    static int Matrix4_Frustum(lua_State* L)
    {
        float left = (float) luaL_checknumber(L, 1);
        float right = (float) luaL_checknumber(L, 2);
        float bottom = (float) luaL_checknumber(L, 3);
        float top = (float) luaL_checknumber(L, 4);
        float near_z = (float) luaL_checknumber(L, 5);
        if(near_z == 0.0f)
        {
            luaL_where(L, 1);
            dmLogWarning("%sperspective projection invalid, znear = 0", lua_tostring(L,-1));
        }
        float far_z = (float) luaL_checknumber(L, 6);
        PushMatrix4(L, Vectormath::Aos::Matrix4::frustum(left, right, bottom, top, near_z, far_z));
        return 1;
    }

    /*# creates a look-at view matrix
     *
     * The resulting matrix is created from the supplied look-at parameters.
     * This is useful for constructing a view matrix for a camera or
     * rendering in general.
     *
     * @name vmath.matrix4_look_at
     * @param eye [type:vector3] eye position
     * @param look_at [type:vector3] look-at position
     * @param up [type:vector3] up vector
     * @return m [type:matrix4] look-at matrix
     * @examples
     *
     * ```lua
     * -- Set up a perspective camera at z 100 with 45 degrees (pi/2) FOV
     * -- Aspect ratio 4:3
     * local eye = vmath.vector3(0, 0, 100)
     * local look_at = vmath.vector3(0, 0, 0)
     * local up = vmath.vector3(0, 1, 0)
     * local view = vmath.matrix4_look_at(eye, look_at, up)
     * render.set_view(view)
     * local proj = vmath.matrix4_perspective(3.141592/2, 4/3, 1, 1000)
     * render.set_projection(proj)
     * ```
     */
    static int Matrix4_LookAt(lua_State* L)
    {
        PushMatrix4(L, Vectormath::Aos::Matrix4::lookAt(Vectormath::Aos::Point3(*CheckVector3(L, 1)), Vectormath::Aos::Point3(*CheckVector3(L, 2)), *CheckVector3(L, 3)));
        return 1;
    }

    /*# creates an orthographic projection matrix
     * Creates an orthographic projection matrix.
     * This is useful to construct a projection matrix for a camera or rendering in general.
     *
     * @name vmath.matrix4_orthographic
     * @param left [type:number] coordinate for left clipping plane
     * @param right [type:number] coordinate for right clipping plane
     * @param bottom [type:number] coordinate for bottom clipping plane
     * @param top [type:number] coordinate for top clipping plane
     * @param near [type:number] coordinate for near clipping plane
     * @param far [type:number] coordinate for far clipping plane
     * @return m [type:matrix4] orthographic projection matrix
     * @examples
     *
     * ```lua
     * -- Set up an orthographic projection based on the width and height
     * -- of the game window.
     * local w = render.get_width()
     * local h = render.get_height()
     * local proj = vmath.matrix4_orthographic(- w / 2, w / 2, -h / 2, h / 2, -1000, 1000)
     * render.set_projection(proj)
     * ```
     */
    static int Matrix4_Orthographic(lua_State* L)
    {
        float left = (float) luaL_checknumber(L, 1);
        float right = (float) luaL_checknumber(L, 2);
        float bottom = (float) luaL_checknumber(L, 3);
        float top = (float) luaL_checknumber(L, 4);
        float near_z = (float) luaL_checknumber(L, 5);
        float far_z = (float) luaL_checknumber(L, 6);
        PushMatrix4(L, Vectormath::Aos::Matrix4::orthographic(left, right, bottom, top, near_z, far_z));
        return 1;
    }

    /*# creates a perspective projection matrix
     * Creates a perspective projection matrix.
     * This is useful to construct a projection matrix for a camera or rendering in general.
     *
     * @name vmath.matrix4_perspective
     * @param fov [type:number] angle of the full vertical field of view in radians
     * @param aspect [type:number] aspect ratio
     * @param near [type:number] coordinate for near clipping plane
     * @param far [type:number] coordinate for far clipping plane
     * @return m [type:matrix4] perspective projection matrix
     * @examples
     *
     * ```lua
     * -- Set up a perspective camera at z 100 with 45 degrees (pi/2) FOV
     * -- Aspect ratio 4:3
     * local eye = vmath.vector3(0, 0, 100)
     * local look_at = vmath.vector3(0, 0, 0)
     * local up = vmath.vector3(0, 1, 0)
     * local view = vmath.matrix4_look_at(eye, look_at, up)
     * render.set_view(view)
     * local proj = vmath.matrix4_perspective(3.141592/2, 4/3, 1, 1000)
     * render.set_projection(proj)
     * ```
     */
    static int Matrix4_Perspective(lua_State* L)
    {
        float fov = (float) luaL_checknumber(L, 1);
        float aspect = (float) luaL_checknumber(L, 2);
        float near_z = (float) luaL_checknumber(L, 3);
        float far_z = (float) luaL_checknumber(L, 4);
        if(near_z == 0.0f)
        {
            luaL_where(L, 1);
            dmLogWarning("%sperspective projection invalid, znear = 0", lua_tostring(L,-1));
        }
        PushMatrix4(L, Vectormath::Aos::Matrix4::perspective(fov, aspect, near_z, far_z));
        return 1;
    }

    /*# creates a matrix from a quaternion
     * The resulting matrix describes the same rotation as the quaternion, but does not have any translation (also like the quaternion).
     *
     * @name vmath.matrix4_from_quat
     * @param q [type:quaternion] quaternion to create matrix from
     * @return m [type:matrix4] matrix represented by quaternion
     * @examples
     *
     * ```lua
     * local vec = vmath.vector4(1, 1, 0, 0)
     * local quat = vmath.quat_rotation_z(3.141592653)
     * local mat = vmath.matrix4_from_quat(quat)
     * print(mat * vec) --> vmath.matrix4_frustum(-1, 1, -1, 1, 1, 1000)
     * ```
     */
    static int Matrix4_FromQuat(lua_State* L)
    {
        PushMatrix4(L, Vectormath::Aos::Matrix4::rotation(*CheckQuat(L, 1)));
        return 1;
    }

    /*# creates a matrix from an axis and an angle
     * The resulting matrix describes a rotation around the axis by the specified angle.
     *
     * @name vmath.matrix4_axis_angle
     * @param v [type:vector3] axis
     * @param angle [type:number] angle in radians
     * @return m [type:matrix4] matrix represented by axis and angle
     * @examples
     *
     * ```lua
     * local vec = vmath.vector4(1, 1, 0, 0)
     * local axis = vmath.vector3(0, 0, 1) -- z-axis
     * local mat = vmath.matrix4_axis_angle(axis, 3.141592653)
     * print(mat * vec) --> vmath.vector4(-0.99999994039536, -1.0000001192093, 0, 0)
     * ```
     */
    static int Matrix4_AxisAngle(lua_State* L)
    {
        Vectormath::Aos::Vector3* axis = CheckVector3(L, 1);
        float angle = (float) luaL_checknumber(L, 2);
        PushMatrix4(L, Vectormath::Aos::Matrix4::rotation(angle, *axis));
        return 1;
    }

    /*# creates a matrix from rotation around x-axis
     * The resulting matrix describes a rotation around the x-axis
     * by the specified angle.
     *
     * @name vmath.matrix4_rotation_x
     * @param angle [type:number] angle in radians around x-axis
     * @return m [type:matrix4] matrix from rotation around x-axis
     * @examples
     *
     * ```lua
     * local vec = vmath.vector4(1, 1, 0, 0)
     * local mat = vmath.matrix4_rotation_x(3.141592653)
     * print(mat * vec) --> vmath.vector4(1, -1, -8.7422776573476e-08, 0)
     * ```
     */
    static int Matrix4_RotationX(lua_State* L)
    {
        PushMatrix4(L, Vectormath::Aos::Matrix4::rotationX((float) luaL_checknumber(L, 1)));
        return 1;
    }

    /*# creates a matrix from rotation around y-axis
     * The resulting matrix describes a rotation around the y-axis
     * by the specified angle.
     *
     * @name vmath.matrix4_rotation_y
     * @param angle [type:number] angle in radians around y-axis
     * @return m [type:matrix4] matrix from rotation around y-axis
     * @examples
     *
     * ```lua
     * local vec = vmath.vector4(1, 1, 0, 0)
     * local mat = vmath.matrix4_rotation_y(3.141592653)
     * print(mat * vec) --> vmath.vector4(-1, 1, 8.7422776573476e-08, 0)
     * ```
     */
    static int Matrix4_RotationY(lua_State* L)
    {
        PushMatrix4(L, Vectormath::Aos::Matrix4::rotationY((float) luaL_checknumber(L, 1)));
        return 1;
    }

    /*# creates a matrix from rotation around z-axis
     * The resulting matrix describes a rotation around the z-axis
     * by the specified angle.
     *
     * @name vmath.matrix4_rotation_z
     * @param angle [type:number] angle in radians around z-axis
     * @return m [type:matrix4] matrix from rotation around z-axis
     * @examples
     *
     * ```lua
     * local vec = vmath.vector4(1, 1, 0, 0)
     * local mat = vmath.matrix4_rotation_z(3.141592653)
     * print(mat * vec) --> vmath.vector4(-0.99999994039536, -1.0000001192093, 0, 0)
     * ```
     */
    static int Matrix4_RotationZ(lua_State* L)
    {
        PushMatrix4(L, Vectormath::Aos::Matrix4::rotationZ((float) luaL_checknumber(L, 1)));
        return 1;
    }

    /*# calculates the inverse matrix.
     * The resulting matrix is the inverse of the supplied matrix.
     *
     * [icon:attention] For ortho-normal matrices, e.g. regular object transformation,
     * use `vmath.ortho_inv()` instead.
     * The specialized inverse for ortho-normalized matrices is much faster
     * than the general inverse.
     *
     * @name vmath.inv
     * @param m1 [type:matrix4] matrix to invert
     * @return m [type:matrix4] inverse of the supplied matrix
     * @examples
     *
     * ```lua
     * local mat1 = vmath.matrix4_rotation_z(3.141592653)
     * local mat2 = vmath.inv(mat1)
     * -- M * inv(M) = identity matrix
     * print(mat1 * mat2) --> vmath.matrix4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
     * ```
     */
    static int Inverse(lua_State* L)
    {
        const Vectormath::Aos::Matrix4* m = CheckMatrix4(L, 1);
        Vectormath::Aos::Matrix4 mi = Vectormath::Aos::inverse(*m);
        PushMatrix4(L, mi);
        return 1;
    }

    /*# calculates the inverse of an ortho-normal matrix.
     * The resulting matrix is the inverse of the supplied matrix.
     * The supplied matrix has to be an ortho-normal matrix, e.g.
     * describe a regular object transformation.
     *
     * [icon:attention] For matrices that are not ortho-normal
     * use the general inverse `vmath.inv()` instead.
     *
     * @name vmath.ortho_inv
     * @param m1 [type:matrix4] ortho-normalized matrix to invert
     * @return m [type:matrix4] inverse of the supplied matrix
     * @examples
     *
     * ```lua
     * local mat1 = vmath.matrix4_rotation_z(3.141592653)
     * local mat2 = vmath.ortho_inv(mat1)
     * -- M * inv(M) = identity matrix
     * print(mat1 * mat2) --> vmath.matrix4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
     * ```
     */
    static int OrthoInverse(lua_State* L)
    {
        const Vectormath::Aos::Matrix4* m = CheckMatrix4(L, 1);
        Vectormath::Aos::Matrix4 mi = Vectormath::Aos::orthoInverse(*m);
        PushMatrix4(L, mi);
        return 1;
    }

    /*# calculates the dot-product of two vectors
     *
     * The returned value is a scalar defined as:
     *
     * <code>P &#x22C5; Q = |P| |Q| cos &#x03B8;</code>
     *
     * where &#x03B8; is the angle between the vectors P and Q.
     *
     * - If the dot product is positive then the angle between the vectors is below 90 degrees.
     * - If the dot product is zero the vectors are perpendicular (at right-angles to each other).
     * - If the dot product is negative then the angle between the vectors is more than 90 degrees.
     *
     * @name vmath.dot
     * @param v1 [type:vector3|vector4] first vector
     * @param v2 [type:vector3|vector4] second vector
     * @return n [type:number] dot product
     * @examples
     *
     * ```lua
     * if vmath.dot(vector1, vector2) == 0 then
     *     -- The two vectors are perpendicular (at right-angles to each other)
     *     ...
     * end
     * ```
     */
    static int Dot(lua_State* L)
    {
        if (IsVector4(L, 1) && IsVector4(L, 2))
        {
            Vectormath::Aos::Vector4* v1 = CheckVector4(L, 1);
            Vectormath::Aos::Vector4* v2 = CheckVector4(L, 2);
            lua_pushnumber(L, Vectormath::Aos::dot(*v1, *v2));
        }
        else
        {
            Vectormath::Aos::Vector3* v1 = CheckVector3(L, 1);
            Vectormath::Aos::Vector3* v2 = CheckVector3(L, 2);
            lua_pushnumber(L, Vectormath::Aos::dot(*v1, *v2));
        }
        return 1;
    }

    /*# calculates the squared vector length
     *
     * Returns the squared length of the supplied vector.
     *
     * @name vmath.length_sqr
     * @param v [type:vector3|vector4] vector of which to calculate the squared length
     * @return n [type:number] squared vector length
     * @examples
     *
     * ```lua
     * if vmath.length_sqr(vector1) == 1 then
     *     -- The vector is normalized
     *     ...
     * end
     * ```
     */
    static int LengthSqr(lua_State* L)
    {
        if (IsVector4(L, 1))
        {
            Vectormath::Aos::Vector4* v = CheckVector4(L, 1);
            lua_pushnumber(L, Vectormath::Aos::lengthSqr(*v));
        }
        else
        {
            Vectormath::Aos::Vector3* v = CheckVector3(L, 1);
            lua_pushnumber(L, Vectormath::Aos::lengthSqr(*v));
        }
        return 1;
    }

    /*# calculates the vector length
     *
     * Returns the length of the supplied vector.
     *
     * @name vmath.length
     * @param v [type:vector3|vector4] vector of which to calculate the length
     * @return n [type:number] vector length
     * @examples
     *
     * ```lua
     * if vmath.length(self.velocity) < max_velocity then
     *     -- The speed (velocity vector) is below max.
     *     ...
     * end
     * ```
     */
    static int Length(lua_State* L)
    {
        if (IsVector4(L, 1))
        {
            Vectormath::Aos::Vector4* v = CheckVector4(L, 1);
            lua_pushnumber(L, Vectormath::Aos::length(*v));
        }
        else
        {
            Vectormath::Aos::Vector3* v = CheckVector3(L, 1);
            lua_pushnumber(L, Vectormath::Aos::length(*v));
        }
        return 1;
    }

    /*# normalizes a vector
     *
     * Normalizes a vector, i.e. returns a new vector with the same
     * direction as the input vector, but with length 1.
     *
     * [icon:attention] The length of the vector must be above 0, otherwise a
     * division-by-zero will occur.
     *
     * @name vmath.normalize
     * @param v1 [type:vector3|vector4] vector to normalize
     * @return v [type:vector3|vector4] new normalized vector
     * @examples
     *
     * ```lua
     * local vec = vmath.vector3(1, 2, 3)
     * local norm_vec = vmath.normalize(vec)
     * print(norm_vec) --> vmath.vector3(0.26726123690605, 0.5345224738121, 0.80178368091583)
     * print(vmath.length(norm_vec)) --> 0.99999994039536
     * ```
     */
    static int Normalize(lua_State* L)
    {
        if (IsVector4(L, 1))
        {
            Vectormath::Aos::Vector4* v = CheckVector4(L, 1);
            PushVector4(L, Vectormath::Aos::normalize(*v));
        }
        else
        {
            Vectormath::Aos::Vector3* v = CheckVector3(L, 1);
            PushVector3(L, Vectormath::Aos::normalize(*v));
        }
        return 1;
    }

    /*# calculates the cross-product of two vectors
     *
     * Given two linearly independent vectors P and Q, the cross product,
     * P &#x00D7; Q, is a vector that is perpendicular to both P and Q and
     * therefore normal to the plane containing them.
     *
     * If the two vectors have the same direction (or have the exact
     * opposite direction from one another, i.e. are not linearly independent)
     * or if either one has zero length, then their cross product is zero.
     *
     * @name vmath.cross
     * @param v1 [type:vector3] first vector
     * @param v2 [type:vector3] second vector
     * @return v [type:vector3] a new vector representing the cross product
     * @examples
     *
     * ```lua
     * local vec1 = vmath.vector3(1, 0, 0)
     * local vec2 = vmath.vector3(0, 1, 0)
     * print(vmath.cross(vec1, vec2)) --> vmath.vector3(0, 0, 1)
     * local vec3 = vmath.vector3(-1, 0, 0)
     * print(vmath.cross(vec1, vec3)) --> vmath.vector3(0, -0, 0)
     * ```
     */
    static int Cross(lua_State* L)
    {
        Vectormath::Aos::Vector3* v1 = CheckVector3(L, 1);
        Vectormath::Aos::Vector3* v2 = CheckVector3(L, 2);
        PushVector3(L, Vectormath::Aos::cross(*v1, *v2));
        return 1;
    }

    /*# lerps between two vectors
     *
     * Linearly interpolate between two vectors. The function
     * treats the vectors as positions and interpolates between
     * the positions in a straight line. Lerp is useful to describe
     * transitions from one place to another over time.
     *
     * [icon:attention] The function does not clamp t between 0 and 1.
     *
     * @name vmath.lerp
     * @param t [type:number] interpolation parameter, 0-1
     * @param v1 [type:vector3|vector4] vector to lerp from
     * @param v2 [type:vector3|vector4] vector to lerp to
     * @return v [type:vector3|vector4] the lerped vector
     * @examples
     *
     * ```lua
     * function init(self)
     *     self.t = 0
     * end
     *
     * function update(self, dt)
     *     self.t = self.t + dt
     *     if self.t <= 1 then
     *         local startpos = vmath.vector3(0, 600, 0)
     *         local endpos = vmath.vector3(600, 0, 0)
     *         local pos = vmath.lerp(self.t, startpos, endpos)
     *         go.set_position(pos, "go")
     *     end
     * end
     * ```
     */

    /*# lerps between two quaternions
     *
     * Linearly interpolate between two quaternions. Linear
     * interpolation of rotations are only useful for small
     * rotations. For interpolations of arbitrary rotations,
     * [ref:vmath.slerp] yields much better results.
     *
     * [icon:attention] The function does not clamp t between 0 and 1.
     *
     * @name vmath.lerp
     * @param t [type:number] interpolation parameter, 0-1
     * @param q1 [type:quaternion] quaternion to lerp from
     * @param q2 [type:quaternion] quaternion to lerp to
     * @return q [type:quaternion] the lerped quaternion
     * @examples
     *
     * ```lua
     * function init(self)
     *     self.t = 0
     * end
     *
     * function update(self, dt)
     *     self.t = self.t + dt
     *     if self.t <= 1 then
     *         local startrot = vmath.quat_rotation_z(0)
     *         local endrot = vmath.quat_rotation_z(3.141592653)
     *         local rot = vmath.lerp(self.t, startrot, endrot)
     *         go.set_rotation(rot, "go")
     *     end
     * end
     * ```
     */

    /*# lerps between two numbers
     *
     * Linearly interpolate between two values. Lerp is useful
     * to describe transitions from one value to another over time.
     *
     * [icon:attention] The function does not clamp t between 0 and 1.
     *
     * @name vmath.lerp
     * @param t [type:number] interpolation parameter, 0-1
     * @param n1 [type:number] number to lerp from
     * @param n2 [type:number] number to lerp to
     * @return n [type:number] the lerped number
     * @examples
     *
     * ```lua
     * function init(self)
     *     self.t = 0
     * end
     *
     * function update(self, dt)
     *     self.t = self.t + dt
     *     if self.t <= 1 then
     *         local startx = 0
     *         local endx = 600
     *         local x = vmath.lerp(self.t, startx, endx)
     *         go.set_position(vmath.vector3(x, 100, 0), "go")
     *     end
     * end
     * ```
     */

    static int Lerp(lua_State* L)
    {
        float t = (float) luaL_checknumber(L, 1);
        if (IsVector4(L, 2) && IsVector4(L, 3))
        {
            Vectormath::Aos::Vector4* v1 = CheckVector4(L, 2);
            Vectormath::Aos::Vector4* v2 = CheckVector4(L, 3);
            PushVector4(L, Vectormath::Aos::lerp(t, *v1, *v2));
            return 1;
        }
        else if (IsVector3(L, 2) && IsVector3(L, 3))
        {
            Vectormath::Aos::Vector3* v1 = CheckVector3(L, 2);
            Vectormath::Aos::Vector3* v2 = CheckVector3(L, 3);
            PushVector3(L, Vectormath::Aos::lerp(t, *v1, *v2));
            return 1;
        }
        else if (IsQuat(L, 2) && IsQuat(L, 3))
        {
            Vectormath::Aos::Quat* q1 = CheckQuat(L, 2);
            Vectormath::Aos::Quat* q2 = CheckQuat(L, 3);
            PushQuat(L, Vectormath::Aos::lerp(t, *q1, *q2));
            return 1;
        }
        else if (lua_isnumber(L, 2) && lua_isnumber(L, 3))
        {
            lua_Number n1 = (float) luaL_checknumber(L, 2);
            lua_Number n2 = (float) luaL_checknumber(L, 3);
            lua_pushnumber(L, n1 + t * (n2 - n1));
            return 1;
        }
        return luaL_error(L, "%s.%s takes one number and a pair of either %s.%ss, %s.%ss, %s.%ss or numbers as arguments.", SCRIPT_LIB_NAME, "lerp", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_VECTOR3, SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_VECTOR4, SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_QUAT);
    }

    /*# slerps between two vectors
     *
     * Spherically interpolates between two vectors. The difference to
     * lerp is that slerp treats the vectors as directions instead of
     * positions in space.
     *
     * The direction of the returned vector is interpolated by the angle
     * and the magnitude is interpolated between the magnitudes of the
     * from and to vectors.
     *
     * [icon:attention] Slerp is computationally more expensive than lerp.
     * The function does not clamp t between 0 and 1.
     *
     * @name vmath.slerp
     * @param t [type:number] interpolation parameter, 0-1
     * @param v1 [type:vector3|vector4] vector to slerp from
     * @param v2 [type:vector3|vector4] vector to slerp to
     * @return v [type:vector3|vector4] the slerped vector
     * @examples
     *
     * ```lua
     * function init(self)
     *     self.t = 0
     * end
     *
     * function update(self, dt)
     *     self.t = self.t + dt
     *     if self.t <= 1 then
     *         local startpos = vmath.vector3(0, 600, 0)
     *         local endpos = vmath.vector3(600, 0, 0)
     *         local pos = vmath.slerp(self.t, startpos, endpos)
     *         go.set_position(pos, "go")
     *     end
     * end
     * ```
     */

    /*# slerps between two quaternions
     *
     * Slerp travels the torque-minimal path maintaining constant
     * velocity, which means it travels along the straightest path along
     * the rounded surface of a sphere. Slerp is useful for interpolation
     * of rotations.
     *
     * Slerp travels the torque-minimal path, which means it travels
     * along the straightest path the rounded surface of a sphere.
     *
     * [icon:attention] The function does not clamp t between 0 and 1.
     *
     * @name vmath.slerp
     * @param t [type:number] interpolation parameter, 0-1
     * @param q1 [type:quaternion] quaternion to slerp from
     * @param q2 [type:quaternion] quaternion to slerp to
     * @return q [type:quaternion] the slerped quaternion
     * @examples
     *
     * ```lua
     * function init(self)
     *     self.t = 0
     * end
     *
     * function update(self, dt)
     *     self.t = self.t + dt
     *     if self.t <= 1 then
     *         local startrot = vmath.quat_rotation_z(0)
     *         local endrot = vmath.quat_rotation_z(3.141592653)
     *         local rot = vmath.slerp(self.t, startrot, endrot)
     *         go.set_rotation(rot, "go")
     *     end
     * end
     * ```
     */
    static int Slerp(lua_State* L)
    {
        float t = (float) luaL_checknumber(L, 1);
        if (IsVector4(L, 2) && IsVector4(L, 3))
        {
            Vectormath::Aos::Vector4* v1 = CheckVector4(L, 2);
            Vectormath::Aos::Vector4* v2 = CheckVector4(L, 3);
            PushVector4(L, Vectormath::Aos::slerp(t, *v1, *v2));
            return 1;
        }
        else if (IsVector3(L, 2) && IsVector3(L, 3))
        {
            Vectormath::Aos::Vector3* v1 = CheckVector3(L, 2);
            Vectormath::Aos::Vector3* v2 = CheckVector3(L, 3);
            PushVector3(L, Vectormath::Aos::slerp(t, *v1, *v2));
            return 1;
        }
        else
        {
            Vectormath::Aos::Quat* q1 = (Vectormath::Aos::Quat*)luaL_checkudata(L, 2, SCRIPT_TYPE_NAME_QUAT);
            Vectormath::Aos::Quat* q2 = (Vectormath::Aos::Quat*)luaL_checkudata(L, 3, SCRIPT_TYPE_NAME_QUAT);
            PushQuat(L, Vectormath::Aos::slerp(t, *q1, *q2));
            return 1;
        }
        return luaL_error(L, "%s.%s takes one number and either two %s.%s or two %s.%s as arguments.", SCRIPT_LIB_NAME, "slerp", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_VECTOR3, SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_QUAT);
    }

    /*# calculates the conjugate of a quaternion
     *
     * Calculates the conjugate of a quaternion. The result is a
     * quaternion with the same magnitudes but with the sign of
     * the imaginary (vector) parts changed:
     *
     * <code>q<super>*</super> = [w, -v]</code>
     *
     * @name vmath.conj
     * @param q1 [type:quatertion] quaternion of which to calculate the conjugate
     * @return q [type:quatertion] the conjugate
     * @examples
     *
     * ```lua
     * local quat = vmath.quat(1, 2, 3, 4)
     * print(vmath.conj(quat)) --> vmath.quat(-1, -2, -3, 4)
     * ```
     */
    static int Conj(lua_State* L)
    {
        Vectormath::Aos::Quat* q = CheckQuat(L, 1);
        PushQuat(L, Vectormath::Aos::conj(*q));
        return 1;
    }

    /*# rotates a vector by a quaternion
     *
     * Returns a new vector from the supplied vector that is
     * rotated by the rotation described by the supplied
     * quaternion.
     *
     * @name vmath.rotate
     * @param q [type:quatertion] quaternion
     * @param v1 [type:vector3] vector to rotate
     * @return v [type:vector3] the rotated vector
     * @examples
     *
     * ```lua
     * local vec = vmath.vector3(1, 1, 0)
     * local rot = vmath.quat_rotation_z(3.141592563)
     * print(vmath.rotate(rot, vec)) --> vmath.vector3(-1.0000002384186, -0.99999988079071, 0)
     * ```
     */
    static int Rotate(lua_State* L)
    {
        Vectormath::Aos::Quat* q = CheckQuat(L, 1);
        Vectormath::Aos::Vector3* v = CheckVector3(L, 2);
        PushVector3(L, Vectormath::Aos::rotate(*q, *v));
        return 1;
    }

    /*# projects a vector onto another vector
     *
     * Calculates the extent the projection of the first vector onto the second.
     * The returned value is a scalar p defined as:
     *
     * <code>p = |P| cos &#x03B8; / |Q|</code>
     *
     * where &#x03B8; is the angle between the vectors P and Q.
     *
     * @name vmath.project
     * @param v1 [type:vector3] vector to be projected on the second
     * @param v2 [type:vector3] vector onto which the first will be projected, must not have zero length
     * @return n [type:number] the projected extent of the first vector onto the second
     * @examples
     *
     * ```lua
     * local v1 = vmath.vector3(1, 1, 0)
     * local v2 = vmath.vector3(2, 0, 0)
     * print(vmath.project(v1, v2)) --> 0.5
     * ```
     */
    static int Project(lua_State* L)
    {
        Vectormath::Aos::Vector3* v1 = CheckVector3(L, 1);
        Vectormath::Aos::Vector3* v2 = CheckVector3(L, 2);
        float sq_len = Vectormath::Aos::lengthSqr(*v2);
        if (sq_len == 0.0f)
            return luaL_error(L, "The second %s.%s to %s.%s must have a length bigger than 0.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_VECTOR3, SCRIPT_LIB_NAME, "project");
        lua_pushnumber(L, Vectormath::Aos::dot(*v1, *v2) / sq_len);
        return 1;
    }

    static const luaL_reg methods[] =
    {
        {SCRIPT_TYPE_NAME_VECTOR, Vector_new},
        {SCRIPT_TYPE_NAME_VECTOR3, Vector3_new},
        {SCRIPT_TYPE_NAME_VECTOR4, Vector4_new},
        {SCRIPT_TYPE_NAME_QUAT, Quat_new},
        {SCRIPT_TYPE_NAME_MATRIX4, Matrix4_new},
        {"quat_from_to", Quat_FromTo},
        {"quat_axis_angle", Quat_AxisAngle},
        {"quat_basis", Quat_Basis},
        {"quat_rotation_x", Quat_RotationX},
        {"quat_rotation_y", Quat_RotationY},
        {"quat_rotation_z", Quat_RotationZ},
        {"matrix4_frustum", Matrix4_Frustum},
        {"matrix4_look_at", Matrix4_LookAt},
        {"matrix4_orthographic", Matrix4_Orthographic},
        {"matrix4_perspective", Matrix4_Perspective},
        {"matrix4_from_quat", Matrix4_FromQuat},
        {"matrix4_axis_angle", Matrix4_AxisAngle},
        {"matrix4_rotation_x", Matrix4_RotationX},
        {"matrix4_rotation_y", Matrix4_RotationY},
        {"matrix4_rotation_z", Matrix4_RotationZ},
        {"dot", Dot},
        {"length_sqr", LengthSqr},
        {"length", Length},
        {"normalize", Normalize},
        {"cross", Cross},
        {"lerp", Lerp},
        {"slerp", Slerp},
        {"conj", Conj},
        {"rotate", Rotate},
        {"project", Project},
        {"inv", Inverse},
        {"ortho_inv", OrthoInverse},
        {0, 0}
    };

    void InitializeVmath(lua_State* L)
    {
        int top = lua_gettop(L);

        const uint32_t type_count = 5;
        struct
        {
            const char* m_Name;
            const luaL_reg* m_Methods;
            const luaL_reg* m_Metatable;
        } types[type_count] =
        {
            {SCRIPT_TYPE_NAME_VECTOR, Vector_methods, Vector_meta},
            {SCRIPT_TYPE_NAME_VECTOR3, Vector3_methods, Vector3_meta},
            {SCRIPT_TYPE_NAME_VECTOR4, Vector4_methods, Vector4_meta},
            {SCRIPT_TYPE_NAME_QUAT, Quat_methods, Quat_meta},
            {SCRIPT_TYPE_NAME_MATRIX4, Matrix4_methods, Matrix4_meta}
        };
        for (uint32_t i = 0; i < type_count; ++i)
        {
            // create methods table, add it to the globals
            luaL_register(L, types[i].m_Name, types[i].m_Methods);
            int methods_index = lua_gettop(L);
            // create metatable for the type, add it to the Lua registry
            luaL_newmetatable(L, types[i].m_Name);
            int metatable = lua_gettop(L);
            // fill metatable
            luaL_register(L, 0, types[i].m_Metatable);

            lua_pushliteral(L, "__metatable");
            lua_pushvalue(L, methods_index);// dup methods table
            lua_settable(L, metatable);

            lua_pop(L, 2);
        }
        luaL_register(L, SCRIPT_LIB_NAME, methods);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

    void PushVector(lua_State* L, dmVMath::FloatVector* v)
    {
        dmVMath::FloatVector** vp = (dmVMath::FloatVector**)lua_newuserdata(L, sizeof(dmVMath::FloatVector*));
        *vp = v;
        luaL_getmetatable(L, SCRIPT_TYPE_NAME_VECTOR);
        lua_setmetatable(L, -2);
    }

    dmVMath::FloatVector* CheckVector(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            return *((dmVMath::FloatVector**)luaL_checkudata(L, index, SCRIPT_TYPE_NAME_VECTOR));
        }
        luaL_typerror(L, index, SCRIPT_TYPE_NAME_VECTOR);
        return 0x0;
    }

    void PushVector3(lua_State* L, const Vectormath::Aos::Vector3& v)
    {
        Vectormath::Aos::Vector3* vp = (Vectormath::Aos::Vector3*)lua_newuserdata(L, sizeof(Vectormath::Aos::Vector3));
        *vp = v;
        luaL_getmetatable(L, SCRIPT_TYPE_NAME_VECTOR3);
        lua_setmetatable(L, -2);
    }

    Vectormath::Aos::Vector3* CheckVector3(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            return (Vectormath::Aos::Vector3*)luaL_checkudata(L, index, SCRIPT_TYPE_NAME_VECTOR3);
        }
        luaL_typerror(L, index, SCRIPT_TYPE_NAME_VECTOR3);
        return 0x0;
    }

    void PushVector4(lua_State* L, const Vectormath::Aos::Vector4& v)
    {
        Vectormath::Aos::Vector4* vp = (Vectormath::Aos::Vector4*)lua_newuserdata(L, sizeof(Vectormath::Aos::Vector4));
        *vp = v;
        luaL_getmetatable(L, SCRIPT_TYPE_NAME_VECTOR4);
        lua_setmetatable(L, -2);
    }

    Vectormath::Aos::Vector4* CheckVector4(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            return (Vectormath::Aos::Vector4*)luaL_checkudata(L, index, SCRIPT_TYPE_NAME_VECTOR4);
        }
        luaL_typerror(L, index, SCRIPT_TYPE_NAME_VECTOR4);
        return 0x0;
    }

    void PushQuat(lua_State* L, const Vectormath::Aos::Quat& q)
    {
        Vectormath::Aos::Quat* qp = (Vectormath::Aos::Quat*)lua_newuserdata(L, sizeof(Vectormath::Aos::Quat));
        *qp = q;
        luaL_getmetatable(L, SCRIPT_TYPE_NAME_QUAT);
        lua_setmetatable(L, -2);
    }

    Vectormath::Aos::Quat* CheckQuat(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            return (Vectormath::Aos::Quat*)luaL_checkudata(L, index, SCRIPT_TYPE_NAME_QUAT);
        }
        luaL_typerror(L, index, SCRIPT_TYPE_NAME_QUAT);
        return 0x0;
    }

    void PushMatrix4(lua_State* L, const Vectormath::Aos::Matrix4& m)
    {
        Vectormath::Aos::Matrix4* mp = (Vectormath::Aos::Matrix4*)lua_newuserdata(L, sizeof(Vectormath::Aos::Matrix4));
        *mp = m;
        luaL_getmetatable(L, SCRIPT_TYPE_NAME_MATRIX4);
        lua_setmetatable(L, -2);
    }

    Vectormath::Aos::Matrix4* CheckMatrix4(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            return (Vectormath::Aos::Matrix4*)luaL_checkudata(L, index, SCRIPT_TYPE_NAME_MATRIX4);
        }
        luaL_typerror(L, index, SCRIPT_TYPE_NAME_MATRIX4);
        return 0x0;
    }
}
