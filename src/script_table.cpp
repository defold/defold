#include <stdint.h>
#include <string.h>
#include "script.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    /*
     * Table serialization format:
     * char   count
     *
     * char   value_type (LUA_TXXX)
     * char[] key (null terminated string)
     * T      value
     *
     * char   value_type (LUA_TXXX)
     * char[] key (null terminated string)
     * T      value
     * ...
     * if value is of type Point3, Vector3, Vector4 or Quat, ie LUA_TUSERDATA, the first byte in value is the SubType
     */

    enum SubType
    {
        SUB_TYPE_VECTOR3 = 0,
        SUB_TYPE_VECTOR4 = 1,
        SUB_TYPE_POINT3  = 2,
        SUB_TYPE_QUAT    = 3,
    };

    uint32_t CheckTable(lua_State* L, char* buffer, uint32_t buffer_size, int index)
    {
        char* buffer_start = buffer;
        char* buffer_end = buffer + buffer_size;
        luaL_checktype(L, index, LUA_TTABLE);

        lua_pushvalue(L, index);
        lua_pushnil(L);

        if (buffer_size == 0)
        {
            luaL_error(L, "table too large");
        }
        // Make room for count
        buffer++;

        char count = 0;
        while (lua_next(L, -2) != 0)
        {
            count++;

            // Arbitrary number. More than 32 seems to be excessive.
            if (count >= 32)
                luaL_error(L, "too many values in table");

            int key_type = lua_type(L, -2);
            int value_type = lua_type(L, -1);
            if (key_type != LUA_TSTRING)
            {
                luaL_error(L, "keys in table must be of type string");
            }
            const char* key = lua_tostring(L, -2);
            uint32_t key_len = strlen(key) + 1;

            if (buffer_end - buffer < int32_t(1 + key_len))
            {
                luaL_error(L, "table too large");
            }

            (*buffer++) = (char) value_type;
            memcpy(buffer, key, key_len);
            buffer += key_len;

            switch (value_type)
            {
                case LUA_TBOOLEAN:
                {
                    if (buffer_end - buffer < 1)
                        luaL_error(L, "table too large");
                    (*buffer++) = (char) lua_toboolean(L, -1);
                }
                break;

                case LUA_TNUMBER:
                {
                    // NOTE: We align lua_Number to sizeof(float) even if lua_Number probably is of double type
                    intptr_t offset = buffer - buffer_start;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;

                    if (buffer_end - buffer < align_size)
                        luaL_error(L, "table too large");

                    buffer += align_size;

                    if (buffer_end - buffer < int32_t(sizeof(lua_Number)) || buffer_end - buffer < align_size)
                        luaL_error(L, "table too large");

                    union
                    {
                        lua_Number x;
                        char buf[sizeof(lua_Number)];
                    };

                    x = lua_tonumber(L, -1);
                    memcpy(buffer, buf, sizeof(lua_Number));
                    buffer += sizeof(lua_Number);
                }
                break;

                case LUA_TSTRING:
                {
                    const char* value = lua_tostring(L, -1);
                    uint32_t value_len = strlen(value) + 1;
                    if (buffer_end - buffer < int32_t(value_len))
                        luaL_error(L, "table too large");
                    memcpy(buffer, value, value_len);
                    buffer += value_len;
                }
                break;

                case LUA_TUSERDATA:
                {
                    if (buffer_end - buffer < 1)
                        luaL_error(L, "table too large");

                    char* sub_type = buffer++;

                    // NOTE: We align lua_Number to sizeof(float) even if lua_Number probably is of double type
                    intptr_t offset = buffer - buffer_start;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;

                    if (buffer_end - buffer < align_size)
                        luaL_error(L, "table too large");

                    buffer += align_size;

                    float* f = (float*) (buffer);
                    if (IsVector3(L, -1))
                    {
                        Vectormath::Aos::Vector3* v = CheckVector3(L, -1);

                        if (buffer_end - buffer < int32_t(sizeof(float) * 3))
                            luaL_error(L, "table too large");

                        *sub_type = (char) SUB_TYPE_VECTOR3;
                        *f++ = v->getX();
                        *f++ = v->getY();
                        *f++ = v->getZ();

                        buffer += sizeof(float) * 3;
                    }
                    else if (IsVector4(L, -1))
                    {
                        Vectormath::Aos::Vector4* v = CheckVector4(L, -1);

                        if (buffer_end - buffer < int32_t(sizeof(float) * 4))
                            luaL_error(L, "table too large");

                        *sub_type = (char) SUB_TYPE_VECTOR4;
                        *f++ = v->getX();
                        *f++ = v->getY();
                        *f++ = v->getZ();
                        *f++ = v->getW();

                        buffer += sizeof(float) * 4;
                    }
                    else if (IsQuat(L, -1))
                    {
                        Vectormath::Aos::Quat* v = CheckQuat(L, -1);

                        if (buffer_end - buffer < int32_t(sizeof(float) * 4))
                            luaL_error(L, "table too large");

                        *sub_type = (char) SUB_TYPE_QUAT;
                        *f++ = v->getX();
                        *f++ = v->getY();
                        *f++ = v->getZ();
                        *f++ = v->getW();

                        buffer += sizeof(float) * 4;
                    }
                    else if (IsPoint3(L, -1))
                    {
                        Vectormath::Aos::Point3* v = CheckPoint3(L, -1);

                        if (buffer_end - buffer < int32_t(sizeof(float) * 3))
                            luaL_error(L, "table too large");

                        *sub_type = (char) SUB_TYPE_POINT3;
                        *f++ = v->getX();
                        *f++ = v->getY();
                        *f++ = v->getZ();

                        buffer += sizeof(float) * 3;
                    }

                    else
                    {
                        luaL_error(L, "unsupported value type in table", lua_typename(L, value_type));
                    }
                }
                break;

                default:
                    luaL_error(L, "unsupported value type in table", lua_typename(L, value_type));
            }

            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        *buffer_start = count;

        return buffer - buffer_start;
    }

    void PushTable(lua_State*L, const char* buffer)
    {
        const char* buffer_start = buffer;
        uint32_t count = (uint32_t) (*buffer++);
        lua_newtable(L);

        for (uint32_t i = 0; i < count; ++i)
        {
            char value_type = (*buffer++);
            uint32_t key_len = strlen(buffer) + 1;
            const char* key = buffer;
            buffer += key_len;

            switch (value_type)
            {
                case LUA_TBOOLEAN:
                {
                    lua_pushboolean(L, *buffer++);
                }
                break;

                case LUA_TNUMBER:
                {
                    // NOTE: We align lua_Number to sizeof(float) even if lua_Number probably is of double type
                    intptr_t offset = buffer - buffer_start;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;

                    buffer += align_size;

                    lua_pushnumber(L, *((lua_Number*) buffer));
                    buffer += sizeof(lua_Number);
                }
                break;

                case LUA_TSTRING:
                {
                    uint32_t value_len = strlen(buffer) + 1;
                    lua_pushstring(L, buffer);
                    buffer += value_len;
                }
                break;

                case LUA_TUSERDATA:
                {
                    char sub_type = *buffer++;

                    // NOTE: We align lua_Number to sizeof(float) even if lua_Number probably is of double type
                    intptr_t offset = buffer - buffer_start;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;
                    buffer += align_size;

                    if (sub_type == (char) SUB_TYPE_VECTOR3)
                    {
                        float* f = (float*) buffer;
                        dmScript::PushVector3(L, Vectormath::Aos::Vector3(f[0], f[1], f[2]));
                        buffer += sizeof(float) * 3;
                    }
                    else if (sub_type == (char) SUB_TYPE_VECTOR4)
                    {
                        float* f = (float*) buffer;
                        dmScript::PushVector4(L, Vectormath::Aos::Vector4(f[0], f[1], f[2], f[3]));
                        buffer += sizeof(float) * 4;
                    }
                    else if (sub_type == (char) SUB_TYPE_POINT3)
                    {
                        float* f = (float*) buffer;
                        dmScript::PushPoint3(L, Vectormath::Aos::Point3(f[0], f[1], f[2]));
                        buffer += sizeof(float) * 3;
                    }
                    else if (sub_type == (char) SUB_TYPE_QUAT)
                    {
                        float* f = (float*) buffer;
                        dmScript::PushQuat(L, Vectormath::Aos::Quat(f[0], f[1], f[2], f[3]));
                        buffer += sizeof(float) * 4;
                    }
                    else
                    {
                        assert(0);
                    }
                }
                break;

                default:
                    assert(0);
            }
            lua_setfield(L, -2, key);
        }
    }
}


