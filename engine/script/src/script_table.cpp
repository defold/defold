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
     * uint16_t   count
     *
     * char   key_type (LUA_TSTRING or LUA_TNUMBER)
     * char   value_type (LUA_TXXX)
     * T      key (null terminated string or uint16_t)
     * T      value
     *
     * char   key_type (LUA_TSTRING or LUA_TNUMBER)
     * char   value_type (LUA_TXXX)
     * T      key (null terminated string or uint16_t)
     * T      value
     * ...
     * if value is of type Vector3, Vector4, Quat, Matrix4 or Hash ie LUA_TUSERDATA, the first byte in value is the SubType
     */

    enum SubType
    {
        SUB_TYPE_VECTOR3    = 0,
        SUB_TYPE_VECTOR4    = 1,
        SUB_TYPE_QUAT       = 2,
        SUB_TYPE_MATRIX4    = 3,
        SUB_TYPE_HASH       = 4,
        SUB_TYPE_URL        = 5,
    };

    uint32_t DoCheckTable(lua_State* L, const char* original_buffer, char* buffer, uint32_t buffer_size, int index, bool require_aligned_access)
    {
        int top = lua_gettop(L);
        (void)top;

        char* buffer_start = buffer;
        char* buffer_end = buffer + buffer_size;
        luaL_checktype(L, index, LUA_TTABLE);

        lua_pushvalue(L, index);
        lua_pushnil(L);

        if (buffer_size < 2)
        {
            luaL_error(L, "table too large");
        }
        // Make room for count (2 bytes)
        buffer += 2;

        uint16_t count = 0;
        while (lua_next(L, -2) != 0)
        {
            // Check overflow
            if (count == (uint16_t)0xffff)
                luaL_error(L, "too many values in table, %d is max", 0xffff);

            count++;

            int key_type = lua_type(L, -2);
            int value_type = lua_type(L, -1);
            if (key_type != LUA_TSTRING && key_type != LUA_TNUMBER)
            {
                luaL_error(L, "keys in table must be of type number or string");
            }

            if (buffer_end - buffer < 2)
                luaL_error(L, "table too large");

            (*buffer++) = (char) key_type;
            (*buffer++) = (char) value_type;

            if (key_type == LUA_TSTRING)
            {
                const char* key = lua_tostring(L, -2);
                uint32_t key_len = strlen(key) + 1;

                if (buffer_end - buffer < int32_t(1 + key_len))
                {
                    luaL_error(L, "table too large");
                }
                memcpy(buffer, key, key_len);
                buffer += key_len;
            }
            else if (key_type == LUA_TNUMBER)
            {
                if (buffer_end - buffer < 2)
                    luaL_error(L, "table too large");
                lua_Number index = lua_tonumber(L, -2);
                if (index > 0xffff)
                    luaL_error(L, "index out of bounds, max is %d", 0xffff);
                uint16_t key = (uint16_t)index;
                if (require_aligned_access && (((intptr_t)buffer & 0xff) != 0) ){
                    *buffer = key & 0xff; //Assumes little endian
                    *(buffer+1) = (key>>8);
                } else {
                    *((uint16_t*)buffer) = key;
                }
                buffer += 2;
            }

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
                    intptr_t offset = buffer - original_buffer;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;

                    if (buffer_end - buffer < align_size)
                        luaL_error(L, "table too large");

#ifndef NDEBUG
                    memset(buffer, 0, align_size);
#endif
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
                    intptr_t offset = buffer - original_buffer;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;

                    if (buffer_end - buffer < align_size)
                        luaL_error(L, "table too large");

#ifndef NDEBUG
                    memset(buffer, 0, align_size);
#endif
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
                    else if (IsMatrix4(L, -1))
                    {
                        Vectormath::Aos::Matrix4* v = CheckMatrix4(L, -1);

                        if (buffer_end - buffer < int32_t(sizeof(float) * 16))
                            luaL_error(L, "table too large");

                        *sub_type = (char) SUB_TYPE_MATRIX4;
                        for (uint32_t i = 0; i < 4; ++i)
                            for (uint32_t j = 0; j < 4; ++j)
                                *f++ = v->getElem(i, j);

                        buffer += sizeof(float) * 16;
                    }
                    else if (IsHash(L, -1))
                    {
                        dmhash_t hash = CheckHash(L, -1);
                        const uint32_t hash_size = sizeof(dmhash_t);

                        if (buffer_end - buffer < int32_t(hash_size))
                            luaL_error(L, "table too large");

                        *sub_type = (char) SUB_TYPE_HASH;

                        memcpy(buffer, (const void*)&hash, hash_size);
                        buffer += hash_size;
                    }
                    else if (IsURL(L, -1))
                    {
                        dmMessage::URL* url = CheckURL(L, -1);
                        const uint32_t url_size = sizeof(dmMessage::URL);

                        if (buffer_end - buffer < int32_t(url_size))
                            luaL_error(L, "table too large");

                        *sub_type = (char) SUB_TYPE_URL;

                        memcpy(buffer, (const void*)url, url_size);
                        buffer += url_size;
                    }
                    else
                    {
                        luaL_error(L, "unsupported value type in table: %s", lua_typename(L, value_type));
                    }
                }
                break;

                case LUA_TTABLE:
                {
                    uint32_t n_used = DoCheckTable(L, original_buffer, buffer, buffer_end - buffer, -1, require_aligned_access);
                    buffer += n_used;
                }
                break;

                default:
                    luaL_error(L, "unsupported value type in table: %s", lua_typename(L, value_type));
                    break;
            }

            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        *((uint16_t*)buffer_start) = count;

        assert(top == lua_gettop(L));

        return buffer - buffer_start;
    }

    uint32_t CheckTable(lua_State* L, char* buffer, uint32_t buffer_size, int index, AlignmentOptions alignment_options)
    {
        bool align = alignment_options == ALIGN_ON || (alignment_options == ALIGN_AUTO); //TODO Arch specific!

        return DoCheckTable(L, buffer, buffer, buffer_size, index, true);
    }

    int DoPushTable(lua_State*L, const char* original_buffer, const char* buffer, bool require_aligned_access)
    {
        int top = lua_gettop(L);
        (void)top;
        const char* buffer_start = buffer;
        uint32_t count = *(uint16_t*)buffer;
        buffer += 2;
        lua_newtable(L);

        for (uint32_t i = 0; i < count; ++i)
        {
            char key_type = (*buffer++);
            char value_type = (*buffer++);

            if (key_type == LUA_TSTRING)
            {
                lua_pushstring(L, buffer);
                buffer += strlen(buffer) + 1;
            }
            else if (key_type == LUA_TNUMBER)
            {
                uint16_t val;
                if (require_aligned_access && (((intptr_t)buffer & 0xff) != 0))
                {
                    val = *((uint8_t*)buffer+1) << 8 | *((uint8_t*)buffer); //Assumes little endian
                }
                else
                {
                    val = *((uint16_t*)buffer);
                }
                lua_pushnumber(L, val);
                buffer += 2;
            }

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
                    intptr_t offset = buffer - original_buffer;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;
                    buffer += align_size;
                    // Sanity-check. At least 4 bytes alignment (de facto)
                    assert((((intptr_t) buffer) & 3) == 0);

                    union
                    {
                        lua_Number x;
                        char buf[sizeof(lua_Number)];
                    };

                    memcpy(buf, buffer, sizeof(buf));
                    lua_pushnumber(L, x);
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
                    intptr_t offset = buffer - original_buffer;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;
                    buffer += align_size;
                    // Sanity-check. At least 4 bytes alignment (de facto)
                    assert((((intptr_t) buffer) & 3) == 0);

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
                    else if (sub_type == (char) SUB_TYPE_QUAT)
                    {
                        float* f = (float*) buffer;
                        dmScript::PushQuat(L, Vectormath::Aos::Quat(f[0], f[1], f[2], f[3]));
                        buffer += sizeof(float) * 4;
                    }
                    else if (sub_type == (char) SUB_TYPE_MATRIX4)
                    {
                        float* f = (float*) buffer;
                        Vectormath::Aos::Matrix4 m;
                        for (uint32_t i = 0; i < 4; ++i)
                            for (uint32_t j = 0; j < 4; ++j)
                                m.setElem(i, j, f[i * 4 + j]);
                        dmScript::PushMatrix4(L, m);
                        buffer += sizeof(float) * 16;
                    }
                    else if (sub_type == (char) SUB_TYPE_HASH)
                    {
                        dmhash_t hash;
                        uint32_t hash_size = sizeof(dmhash_t);
                        memcpy(&hash, buffer, hash_size);
                        dmScript::PushHash(L, hash);
                        buffer += hash_size;
                    }
                    else if (sub_type == (char) SUB_TYPE_URL)
                    {
                        dmMessage::URL url;
                        uint32_t url_size = sizeof(dmMessage::URL);
                        memcpy(&url, buffer, url_size);
                        dmScript::PushURL(L, url);
                        buffer += url_size;
                    }
                    else
                    {
                        luaL_error(L, "Invalid user data");
                    }
                }
                break;
                case LUA_TTABLE:
                {
                    int n_consumed = DoPushTable(L, original_buffer, buffer, require_aligned_access);
                    buffer += n_consumed;
                }
                break;

                default:
                    luaL_error(L, "Invalid table buffer");
                    break;
            }
            lua_settable(L, -3);
        }

        assert(top + 1 == lua_gettop(L));

        return buffer - buffer_start;
    }

    void PushTable(lua_State*L, const char* buffer, AlignmentOptions alignment_options)
    {
        bool align = alignment_options == ALIGN_ON || (alignment_options == ALIGN_AUTO);  //TODO Arch specific!

        DoPushTable(L, buffer, buffer, align);
    }

}


