#include <stdint.h>
#include <string.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include "script.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

// Emscripten requires data access to be aligned and setting __attribute__((aligned(X)) should force emscripten to
// make multiple access instructions and merge them.
// However, there's a bug in emscripten which causes __attribute__((aligned(X)) to sometimes fail
// To work around this, we can use a typedef
// The bug is tracked: https://github.com/kripken/emscripten/issues/2378
#ifdef __EMSCRIPTEN__
typedef lua_Number __attribute__((aligned(4))) lua_Number_4_align;
typedef uint16_t __attribute__((aligned(1))) uint16_t_1_align;
#else
typedef lua_Number lua_Number_4_align ;
typedef uint16_t uint16_t_1_align;
#endif

namespace dmScript
{
    const int TABLE_MAGIC = 0x42544448;
    const uint32_t TABLE_VERSION_CURRENT = 1;

    /*
     * Original table serialization format:
     *
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
     *
     *    Version 1 table serialization format:
     *
     *    Adds a header block to the table at the head of the input, containing a magic identifier
     *    and version information. Keys of type LUA_TNUMBER use a variable length encoding, with continuation
     *    between bytes signaled by the MSB.
     *
     *    The values of the magic word is chosen so that it will not match values that could be created using
     *    the previous serialization format: this is exploited in implementing backward compatibility when reading
     *    serialized tables.
     *
     *    For the sake of brevity, the header block is not repeated for tables that are nested within this serialized data:
     *    i.e. we assume that the version is uniform throughout any one data set.
     *
     *    LUA_TNUMBER keys are restricted to 32 bits in length, an increase of 16 bits from the original version.
     *
     *    In all other respects, data is serialized in the same way. In particular, note that although users may
     *    create sparse arrays using 32 bit numbers as keys, we remain limited to 65536 (0xffff) rows per table.
     *
     *    Outside of their uses as keys, numerical values are not encoded in the fashion described above.
     *    For the imagined use cases, we consider it likely that taking this approach with keys will lead to smaller files,
     *    since a typical key will fit within a single byte of data. Numerical values when used elsewhere are essentially random
     *    and so we cannot guarantee that this encoding method will yield smaller data in such cases.
     */

    struct TableHeader
    {
        uint32_t m_Magic;
        uint32_t m_Version;

        TableHeader() : m_Magic(0), m_Version(0)
        {
        }
    };

    static bool EncodeMSB(uint32_t value, char*& buffer, const char* buffer_end)
    {
        bool ok = true;
        while (0x7f < value)
        {
            if (buffer >= buffer_end)
            {
                break;
            }
            *buffer++ = ((uint8_t)(value & 0x7f)) | 0x80;
            value >>= 7;
        }
        if (buffer < buffer_end)
        {
            *buffer++ = ((uint8_t)value) & 0x7f;
        }
        else
        {
            ok = false;
        }
        return ok;
    }

    static bool DecodeMSB(uint32_t& value, const char*& buffer)
    {
        bool ok = true;
        const uint32_t MAX_CONSUMED = 5;
        uint32_t consumed = 0;
        uint32_t decoded = 0;
        uint8_t count = 0;
        while (true)
        {
            uint8_t current = (uint8_t)*buffer++;
            ++consumed;
            decoded |= (current & 0x7f) << (7 * count++);
            if (0 == (current & 0x80))
            {
                break;
            }
            else if (consumed > MAX_CONSUMED)
            {
                ok = false;
                break;
            }
        }
        value = decoded;
        return ok;
    }

    enum SubType
    {
        SUB_TYPE_VECTOR3    = 0,
        SUB_TYPE_VECTOR4    = 1,
        SUB_TYPE_QUAT       = 2,
        SUB_TYPE_MATRIX4    = 3,
        SUB_TYPE_HASH       = 4,
        SUB_TYPE_URL        = 5,
    };

    static bool IsSupportedVersion(const TableHeader& header)
    {
        bool supported = false;
        switch(header.m_Version)
        {
        case 0:
        case 1:
            supported = true;
            break;
        default:
            break;
        }
        return supported;
    }

    static char* WriteEncodedNumber(lua_State* L, const TableHeader& header, char* buffer, const char* buffer_end)
    {
        if (0 == header.m_Version)
        {
            if (buffer_end - buffer < 2)
                luaL_error(L, "table too large");
            lua_Number index = lua_tonumber(L, -2);
            if (index > 0xffff)
                luaL_error(L, "index out of bounds, max is %d", 0xffff);
            uint16_t key = (uint16_t)index;
            *((uint16_t_1_align *)buffer) = key;
            buffer += 2;
        }
        else
        {
            lua_Number index = lua_tonumber(L, -2);
            if (index > 0xffffffff) {
                luaL_error(L, "index out of bounds, max is %d", 0xffffffff);
            }
            uint32_t key = (uint32_t)index;
            bool encoded = EncodeMSB(key, buffer, buffer_end);
            if (!encoded)
            {
                luaL_error(L, "table too large");
            }
        }
        return buffer;
    }

    uint32_t DoCheckTable(lua_State* L, const TableHeader& header, const char* original_buffer, char* buffer, uint32_t buffer_size, int index)
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
            {
                luaL_error(L, "too many values in table, %d is max", 0xffff);
            }

            count++;

            int key_type = lua_type(L, -2);
            int value_type = lua_type(L, -1);
            if (key_type != LUA_TSTRING && key_type != LUA_TNUMBER)
            {
                luaL_error(L, "keys in table must be of type number or string (found %s)", lua_typename(L, key_type));
            }

            if (buffer_end - buffer < 2)
            {
                luaL_error(L, "buffer (%d bytes) too small for table, exceeded at key for element #%d", buffer_size, count);
            }

            (*buffer++) = (char) key_type;
            (*buffer++) = (char) value_type;

            if (key_type == LUA_TSTRING)
            {
                const char* key = lua_tostring(L, -2);
                uint32_t key_len = strlen(key) + 1;

                if (buffer_end - buffer < int32_t(1 + key_len))
                {
                    luaL_error(L, "buffer (%d bytes) too small for table, exceeded at key for element #%d", buffer_size, count);
                }
                memcpy(buffer, key, key_len);
                buffer += key_len;
            }
            else if (key_type == LUA_TNUMBER)
            {
                buffer = WriteEncodedNumber(L, header, buffer, buffer_end);
            }

            switch (value_type)
            {
                case LUA_TBOOLEAN:
                {
                    if (buffer_end - buffer < 1)
                    {
                        luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                    }
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
                    {
                        luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                    }

#ifndef NDEBUG
                    memset(buffer, 0, align_size);
#endif
                    buffer += align_size;

                    if (buffer_end - buffer < int32_t(sizeof(lua_Number)) || buffer_end - buffer < align_size)
                    {
                        luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                    }

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
                    size_t value_len = 0;
                    const char* value = lua_tolstring(L, -1, &value_len);
                    if (buffer_end - buffer < (value_len + sizeof(uint32_t)))
                    {
                        luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                    }

                    uint32_t* u32ptr = (uint32_t*) (buffer);
                    *u32ptr = (uint32_t)value_len;
                    buffer += sizeof(uint32_t);
                    memcpy(buffer, value, value_len);
                    buffer += value_len;
                }
                break;

                case LUA_TUSERDATA:
                {
                    if (buffer_end - buffer < 1)
                    {
                        luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                    }

                    char* sub_type = buffer++;

                    // NOTE: We align lua_Number to sizeof(float) even if lua_Number probably is of double type
                    intptr_t offset = buffer - original_buffer;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;

                    if (buffer_end - buffer < align_size)
                    {
                        luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                    }

#ifndef NDEBUG
                    memset(buffer, 0, align_size);
#endif
                    buffer += align_size;

                    float* f = (float*) (buffer);
                    if (IsVector3(L, -1))
                    {
                        Vectormath::Aos::Vector3* v = CheckVector3(L, -1);

                        if (buffer_end - buffer < int32_t(sizeof(float) * 3))
                        {
                            luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                        }

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
                        {
                            luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                        }

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
                        {
                            luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                        }

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
                        {
                            luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                        }

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
                        {
                            luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                        }

                        *sub_type = (char) SUB_TYPE_HASH;

                        memcpy(buffer, (const void*)&hash, hash_size);
                        buffer += hash_size;
                    }
                    else if (IsURL(L, -1))
                    {
                        dmMessage::URL* url = CheckURL(L, -1);
                        const uint32_t url_size = sizeof(dmMessage::URL);

                        if (buffer_end - buffer < int32_t(url_size))
                        {
                            luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                        }

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
                    uint32_t n_used = DoCheckTable(L, header, original_buffer, buffer, buffer_end - buffer, -1);
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

        *((uint16_t_1_align *)buffer_start) = count;

        assert(top == lua_gettop(L));

        return buffer - buffer_start;
    }

    uint32_t CheckTable(lua_State* L, char* buffer, uint32_t buffer_size, int index)
    {
        if (buffer_size > sizeof(TableHeader)) {
            char* original_buffer = buffer;

            TableHeader* header = (TableHeader*)buffer;
            header->m_Magic = TABLE_MAGIC;
            header->m_Version = TABLE_VERSION_CURRENT;
            buffer += sizeof(TableHeader);
            buffer_size -= (buffer - original_buffer);

            return sizeof(TableHeader) + DoCheckTable(L, *header, original_buffer, buffer, buffer_size, index);
        } else {
            luaL_error(L, "buffer (%d bytes) too small for header (%zu bytes)", buffer_size, sizeof(TableHeader));
            return 0;
        }
    }

    static const char* ReadHeader(const char* buffer, TableHeader& header)
    {
        TableHeader* buffered_header = (TableHeader*)buffer;
        if (TABLE_MAGIC == buffered_header->m_Magic) {
            buffer += sizeof(TableHeader);
            header = *buffered_header;
        }
        return buffer;
    }

    static const char* ReadEncodedNumber(lua_State* L, const TableHeader& header, const char* buffer)
    {
        if (0 == header.m_Version)
        {
            lua_pushnumber(L, *((uint16_t_1_align *)buffer));
            buffer += 2;
        }
        else
        {
            uint32_t value;
            if(DecodeMSB(value, buffer))
            {
                lua_pushnumber(L, value);
            }
            else
            {
                luaL_error(L, "Invalid number encoding");
            }
        }
        return buffer;
    }

    int DoPushTable(lua_State*L, const TableHeader& header, const char* original_buffer, const char* buffer)
    {
        int top = lua_gettop(L);
        (void)top;
        const char* buffer_start = buffer;
        uint32_t count = *(uint16_t_1_align *)buffer;
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
                buffer = ReadEncodedNumber(L, header, buffer);
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

                    lua_pushnumber(L, *((lua_Number_4_align *) buffer));
                    buffer += sizeof(lua_Number);
                }
                break;

                case LUA_TSTRING:
                {
                    uint32_t* u32ptr = (uint32_t*)buffer;
                    size_t value_len = (size_t)*u32ptr;
                    buffer += sizeof(uint32_t);
                    lua_pushlstring(L, buffer, value_len);
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
                        return luaL_error(L, "Table contains invalid UserData subtype (%s) at element #%d: %s", lua_typename(L, key_type), i, buffer);
                    }
                }
                break;
                case LUA_TTABLE:
                {
                    int n_consumed = DoPushTable(L, header, original_buffer, buffer);
                    buffer += n_consumed;
                }
                break;

                default:
                    return luaL_error(L, "Table contains invalid type (%s) at element #%d: %s", lua_typename(L, key_type), i, buffer);
                    break;
            }
            lua_settable(L, -3);
        }

        assert(top + 1 == lua_gettop(L));

        return buffer - buffer_start;
    }

    void PushTable(lua_State*L, const char* buffer)
    {
        TableHeader header;
        const char* original_buffer = buffer;
        buffer = ReadHeader(buffer, header);
        if (IsSupportedVersion(header))
        {
            DoPushTable(L, header, original_buffer, buffer);
        }
        else
        {
            char str[256];
            DM_SNPRINTF(str, sizeof(str), "Unsupported serialized table data: version = 0x%x (current = 0x%x)", header.m_Version, TABLE_VERSION_CURRENT);
            luaL_error(L, "%s", str);
        }
    }

}


