// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <stdint.h>
#include <string.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/static_assert.h>
#include "script.h"
#include "script_private.h"

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
#else
typedef lua_Number lua_Number_4_align;
#endif

// custom type when writing negative numbers as keys
// the rest of the types used when serializing a table come from lua.h
// make sure this type has a value quite a bit higher than the types in lua.h
#define LUA_TNEGATIVENUMBER 64

namespace dmScript
{
    const int TABLE_MAGIC = 0x42544448;
    const uint32_t TABLE_VERSION_CURRENT = 4;

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
     *
     *    Version 2:
     *    Adds support for binary strings.
     *
     *    Version 3:
     *    Adds support for negative numeric keys. Always writes four bytes.
     *
     *    Version 4:
     *    Adds support for more than 65535 keys in a table.
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

        SUB_TYPE_MAX // See below
    };

    struct GlobalInit
    {
        GlobalInit() {
            // Make sure the struct sizes are in sync! Think of potential save files!
            DM_STATIC_ASSERT(SUB_TYPE_MAX==6, Must_Add_SubType_Size);
            DM_STATIC_ASSERT(sizeof(dmMessage::URL) == 32, Invalid_Struct_Size);
            DM_STATIC_ASSERT(sizeof(dmhash_t) == 8, Invalid_Struct_Size);
            DM_STATIC_ASSERT(sizeof(dmVMath::Vector3) == 16, Invalid_Struct_Size);
            DM_STATIC_ASSERT(sizeof(dmVMath::Vector4) == 16, Invalid_Struct_Size);
            DM_STATIC_ASSERT(sizeof(dmVMath::Quat)    == 16, Invalid_Struct_Size);
            DM_STATIC_ASSERT(sizeof(dmVMath::Matrix4) == 64, Invalid_Struct_Size);
        }

    } g_ScriptTableInit;

    static bool IsSupportedVersion(const TableHeader& header)
    {
        bool supported = false;
        switch(header.m_Version)
        {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            supported = true;
            break;
        default:
            break;
        }
        return supported;
    }

    static char* WriteEncodedIndex(lua_State* L, lua_Number index, const TableHeader& header, char* buffer, const char* buffer_end)
    {
        if (0 == header.m_Version)
        {
            if (buffer_end - buffer < 2)
                luaL_error(L, "table too large");
            if (index > 0xffff)
                luaL_error(L, "index out of bounds, max is %d", 0xffff);
            uint16_t key = (uint16_t)index;
            memcpy(buffer, &key, sizeof(uint16_t));
            buffer += sizeof(uint16_t);
        }
        else if ((1 == header.m_Version) || (2 == header.m_Version))
        {
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
        else if ((3 == header.m_Version) || (4 == header.m_Version))
        {
            if (buffer_end - buffer < 4)
                luaL_error(L, "table too large");
            if (index < 0)
                index = -index;
            if (index > 0xffffffff)
                luaL_error(L, "index out of bounds, max is %d", 0xffffffff);
            uint32_t key = (uint32_t)index;
            *buffer++ = (uint8_t)(key & 0xFF);
            *buffer++ = (uint8_t)((key >> 8) & 0xFF);
            *buffer++ = (uint8_t)((key >> 16) & 0xFF);
            *buffer++ = (uint8_t)((key >> 24) & 0xFF);
        }
        else
        {
            assert(0);
        }
        return buffer;
    }

    // When storing/packing lua data to a byte array, we now use the binary lua string interface
    static uint32_t SaveTSTRING(lua_State* L, int index, char* buffer, uint32_t buffer_size, const char* buffer_end, uint32_t count)
    {
        size_t value_len = 0;
        const char* value = lua_tolstring(L, index, &value_len);
        uint32_t total_size = value_len + sizeof(uint32_t);
        if (buffer_end - buffer < (intptr_t)total_size)
        {
            luaL_error(L, "buffer (%d bytes) too small for table, exceeded at '%s' for element #%d", buffer_size, value, count);
        }

        uint32_t len = (uint32_t)value_len;
        memcpy(buffer, &len, sizeof(uint32_t));
        buffer += sizeof(uint32_t);
        memcpy(buffer, value, value_len);
        return total_size;
    }

    // When loading older save games, we will use the old unpack method (with truncated c strings)
    static uint32_t LoadOldTSTRING(lua_State* L, const char* buffer, const char* buffer_end, uint32_t count, PushTableLogger& logger)
    {
        uint32_t total_size = strlen(buffer) + 1;
        if (buffer_end - buffer < (intptr_t)total_size)
        {
            char log_str[PUSH_TABLE_LOGGER_STR_SIZE];
            PushTableLogPrint(logger, log_str);
            luaL_error(L, "Reading outside of buffer at element #%d (string): wanted to read: %d bytes left: %d [BufStart: %p, BufSize: %lu]\n'%s'", count, total_size, (int)(buffer_end - buffer), logger.m_BufferStart, logger.m_BufferSize, log_str);
        }

        lua_pushstring(L, buffer);
        return total_size;
    }

    // When loading/unpacking messages/save games, we use pascal strings, and the Lua binary string api
    static uint32_t LoadTSTRING(lua_State* L, const char* buffer, const char* buffer_end, uint32_t count, PushTableLogger& logger)
    {
        uint32_t len;
        memcpy(&len, buffer, sizeof(uint32_t));
        size_t value_len = (size_t)len;
        uint32_t total_size = value_len + sizeof(uint32_t);
        if (buffer_end - buffer < (intptr_t)total_size)
        {
            char log_str[PUSH_TABLE_LOGGER_STR_SIZE];
            PushTableLogPrint(logger, log_str);
            char str[512];
            dmSnPrintf(str, sizeof(str), "Reading outside of buffer at element #%d (string) [value_len=%lu]: wanted to read: %d bytes left: %d [BufStart: %p, BufSize: %lu]\n'%s'", count, value_len, total_size, (uint32_t)(buffer_end - buffer), logger.m_BufferStart, logger.m_BufferSize, log_str);
            luaL_error(L, "%s", str);
        }

        lua_pushlstring(L, buffer + sizeof(uint32_t), value_len);
        return total_size;
    }

    uint32_t DoCheckTableSize(lua_State* L, int index)
    {
        int top = lua_gettop(L);
        (void)top;

        luaL_checktype(L, index, LUA_TTABLE);
        lua_pushvalue(L, index);
        lua_pushnil(L);

        uint32_t size = 0;

        // count
        size += 4;
        while (lua_next(L, -2) != 0)
        {
            int key_type = lua_type(L, -2);
            int value_type = lua_type(L, -1);
            if (key_type != LUA_TSTRING && key_type != LUA_TNUMBER)
            {
                luaL_error(L, "keys in table must be of type number or string (found %s)", lua_typename(L, key_type));
            }

            // key + value type
            size += 2;
            if (key_type == LUA_TSTRING)
            {
                size += sizeof(uint32_t) + lua_objlen(L, -2);
            }
            else if (key_type == LUA_TNUMBER)
            {
                size += 4;
            }

            switch (value_type)
            {
                case LUA_TBOOLEAN:
                {
                    size += 1;
                }
                break;

                case LUA_TNUMBER:
                {
                    int align = ((size + sizeof(float) - 1) & ~(sizeof(float) - 1)) - size;
                    size += align;
                    size += sizeof(lua_Number);
                }
                break;

                case LUA_TSTRING:
                {
                    size += sizeof(uint32_t) + lua_objlen(L, -1);
                }
                break;

                case LUA_TUSERDATA:
                {
                    // subtype
                    size += 1;

                    int align = ((size + sizeof(float) - 1) & ~(sizeof(float) - 1)) - size;
                    size += align;

                    if (IsVector3(L, -1))
                    {
                        size += sizeof(float) * 3;
                    }
                    else if (IsVector4(L, -1))
                    {
                        size += sizeof(float) * 4;
                    }
                    else if (IsQuat(L, -1))
                    {
                        size += sizeof(float) * 4;
                    }
                    else if (IsMatrix4(L, -1))
                    {
                        size += sizeof(float) * 16;
                    }
                    else if (IsHash(L, -1))
                    {
                        size += sizeof(dmhash_t);
                    }
                    else if (IsURL(L, -1))
                    {
                        size += sizeof(dmMessage::URL);
                    }
                    else
                    {
                        luaL_error(L, "unsupported value type in table: %s", lua_typename(L, value_type));
                    }
                }
                break;

                case LUA_TTABLE:
                {
                    size += DoCheckTableSize(L, -1);
                }
                break;

                default:
                    luaL_error(L, "unsupported value type in table: %s", lua_typename(L, value_type));
                    break;
            }

            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        return size;
    }

    uint32_t CheckTableSize(lua_State* L, int index)
    {
        return sizeof(TableHeader) + DoCheckTableSize(L, index);
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

        if (buffer_size < 4)
        {
            luaL_error(L, "table too large");
        }
        // Make room for count (4 bytes)
        buffer += 4;

        uint32_t count = 0;
        while (lua_next(L, -2) != 0)
        {
            // Check overflow
            if (count == (uint32_t)0xffffffff)
            {
                luaL_error(L, "too many values in table, %d is max", 0xffffffff);
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

            if (key_type == LUA_TSTRING)
            {
                (*buffer++) = (char) LUA_TSTRING;
                (*buffer++) = (char) value_type;
                buffer += SaveTSTRING(L, -2, buffer, buffer_size, buffer_end, count);
            }
            else if (key_type == LUA_TNUMBER)
            {
                lua_Number key = lua_tonumber(L, -2);
                (*buffer++) = (char) (key >= 0 ? LUA_TNUMBER : LUA_TNEGATIVENUMBER);
                (*buffer++) = (char) value_type;
                buffer = WriteEncodedIndex(L, key, header, buffer, buffer_end);
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
                    buffer += SaveTSTRING(L, -1, buffer, buffer_size, buffer_end, count);
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
                    dmVMath::Vector3* v3;
                    dmVMath::Vector4* v4;
                    dmVMath::Quat* q;
                    dmVMath::Matrix4* m;
                    if ((v3 = ToVector3(L, -1)))
                    {
                        if (buffer_end - buffer < int32_t(sizeof(float) * 3))
                        {
                            luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                        }

                        *sub_type = (char) SUB_TYPE_VECTOR3;
                        *f++ = v3->getX();
                        *f++ = v3->getY();
                        *f++ = v3->getZ();

                        buffer += sizeof(float) * 3;
                    }
                    else if ((v4 = ToVector4(L, -1)))
                    {
                        if (buffer_end - buffer < int32_t(sizeof(float) * 4))
                        {
                            luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                        }

                        *sub_type = (char) SUB_TYPE_VECTOR4;
                        *f++ = v4->getX();
                        *f++ = v4->getY();
                        *f++ = v4->getZ();
                        *f++ = v4->getW();

                        buffer += sizeof(float) * 4;
                    }
                    else if ((q = ToQuat(L, -1)))
                    {
                        if (buffer_end - buffer < int32_t(sizeof(float) * 4))
                        {
                            luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                        }

                        *sub_type = (char) SUB_TYPE_QUAT;
                        *f++ = q->getX();
                        *f++ = q->getY();
                        *f++ = q->getZ();
                        *f++ = q->getW();

                        buffer += sizeof(float) * 4;
                    }
                    else if ((m = ToMatrix4(L, -1)))
                    {
                        if (buffer_end - buffer < int32_t(sizeof(float) * 16))
                        {
                            luaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, lua_typename(L, key_type), count);
                        }

                        *sub_type = (char) SUB_TYPE_MATRIX4;
                        for (uint32_t i = 0; i < 4; ++i)
                            for (uint32_t j = 0; j < 4; ++j)
                                *f++ = m->getElem(i, j);

                        buffer += sizeof(float) * 16;
                    }
                    else if (IsHash(L, -1))
                    {
                        dmhash_t hash = *(dmhash_t*)lua_touserdata(L, -1);
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
                        dmMessage::URL* url = (dmMessage::URL*)lua_touserdata(L, -1);
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

        memcpy(buffer_start, &count, sizeof(uint32_t));

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

    static const char* ReadEncodedIndex(lua_State* L, char key_type, const TableHeader& header, const char* buffer)
    {
        if (0 == header.m_Version)
        {
            if (key_type != LUA_TNUMBER)
            {
                luaL_error(L, "Unknown key type %d", key_type);
            }
            uint16_t value;
            memcpy(&value, buffer, sizeof(uint16_t));
            lua_pushnumber(L, value);
            buffer += sizeof(uint16_t);
        }
        else if ((1 == header.m_Version) || (2 == header.m_Version))
        {
            if (key_type != LUA_TNUMBER)
            {
                luaL_error(L, "Unknown key type %d", key_type);
            }
            uint32_t index;
            if(DecodeMSB(index, buffer))
            {
                lua_pushnumber(L, index);
            }
            else
            {
                luaL_error(L, "Invalid number encoding");
            }
        }
        else if ((3 == header.m_Version) || (4 == header.m_Version))
        {
            if (key_type != LUA_TNUMBER && key_type != LUA_TNEGATIVENUMBER)
            {
                luaL_error(L, "Unknown key type %d", key_type);
            }
            uint8_t b1 = (uint8_t)*buffer++;
            uint8_t b2 = (uint8_t)*buffer++;
            uint8_t b3 = (uint8_t)*buffer++;
            uint8_t b4 = (uint8_t)*buffer++;
            uint32_t index = b4 << 24 | b3 << 16 | b2 << 8 | b1;
            lua_Number number = index;
            if (key_type == LUA_TNEGATIVENUMBER)
            {
                number = -number;
            }
            lua_pushnumber(L, number);
        }
        else
        {
            assert(0);
        }
        return buffer;
    }

    void PushTableLogChar(PushTableLogger& logger, char c)
    {
        logger.m_Log[logger.m_Cursor++] = c;
        if (logger.m_Cursor > logger.m_Size) {
            logger.m_Size = logger.m_Cursor;
        }
        logger.m_Cursor = logger.m_Cursor % PUSH_TABLE_LOGGER_CAPACITY;
    }

    void PushTableLogString(PushTableLogger& logger, const char* s)
    {
        size_t len = strlen(s);
        for (size_t i = 0; i < len; i++)
        {
            PushTableLogChar(logger, s[i]);
        }
    }

    void PushTableLogFormat(PushTableLogger& logger, const char *format, ...)
    {
        char buffer[128];
        size_t count = sizeof(buffer);
        va_list argp;
        va_start(argp, format);
#if defined(_WIN32)
        _vsnprintf_s(buffer, count, _TRUNCATE, format, argp);
#else
        vsnprintf(buffer, count, format, argp);
#endif
        va_end(argp);
        PushTableLogString(logger, buffer);
    }

    void PushTableLogPrint(PushTableLogger& logger, char out[PUSH_TABLE_LOGGER_STR_SIZE])
    {
        memset(out, 0x0, PUSH_TABLE_LOGGER_STR_SIZE);
        if (logger.m_Size == 0) {
            return;
        }

        int t_c = ((int)logger.m_Cursor) - 1;
        for (int32_t i = 0; i < logger.m_Size;)
        {
            if (t_c < 0)
                t_c = PUSH_TABLE_LOGGER_CAPACITY + t_c;
            t_c = t_c % PUSH_TABLE_LOGGER_CAPACITY;

            int b = logger.m_Size - 1 - i++;
            out[b] = logger.m_Log[t_c--];
        }
    }

#define CHECK_PUSHTABLE_OOB(ELEMTYPE, LOGGER, BUFFER, BUFFER_END, COUNT, DEPTH) \
    if (BUFFER > BUFFER_END) { \
        char log_str[PUSH_TABLE_LOGGER_STR_SIZE]; \
        PushTableLogPrint(LOGGER, log_str); \
        char str[512]; \
        dmSnPrintf(str, sizeof(str), "Reading outside of buffer after %s element #%d (depth: #%d) [BufStart: %p, Cursor: %p, End: %p, BufSize: %lu, Bytes OOB: %d].\n'%s'", ELEMTYPE, COUNT, DEPTH, LOGGER.m_BufferStart, BUFFER, BUFFER_END, LOGGER.m_BufferSize, (int)(BUFFER_END - BUFFER), log_str); \
        return luaL_error(L, "%s", str); \
    }

    int DoPushTable(lua_State*L, PushTableLogger& logger, const TableHeader& header, const char* original_buffer, const char* buffer, uint32_t buffer_size, uint32_t depth)
    {
        int top = lua_gettop(L);
        (void)top;

        const char* buffer_start = buffer;
        const char* buffer_end = buffer + buffer_size;
        CHECK_PUSHTABLE_OOB("table header", logger, buffer+2, buffer_end, 0, depth);

        uint32_t count = 0;
        if (header.m_Version <= 3)
        {
            memcpy(&count, buffer, sizeof(uint16_t));
            buffer += 2;
        }
        else
        {
            memcpy(&count, buffer, sizeof(uint32_t));
            buffer += 4;
        }

        PushTableLogFormat(logger, "{%d|", (uint32_t)count);

        if (buffer > buffer_end) {
            char log_str[PUSH_TABLE_LOGGER_STR_SIZE];
            PushTableLogPrint(logger, log_str);
            char str[512]; \
            dmSnPrintf(str, sizeof(str), "Reading outside of buffer at before element [BufStart: %p, Cursor: %p, End: %p, BufSize: %lu, Bytes OOB: %d].\n'%s'", logger.m_BufferStart, buffer, buffer_end, logger.m_BufferSize, (int)(buffer_end - buffer), log_str); \
            return luaL_error(L, "%s", str);
        }

        lua_newtable(L);

        for (uint32_t i = 0; i < count; ++i)
        {
            CHECK_PUSHTABLE_OOB("key-value tags", logger, buffer+2, buffer_end, count, depth);

            char key_type = (*buffer++);
            char value_type = (*buffer++);

            if (key_type == LUA_TSTRING)
            {
                PushTableLogString(logger, "KS");

                if (header.m_Version <= 1)
                    buffer += LoadOldTSTRING(L, buffer, buffer_end, count, logger);
                else
                    buffer += LoadTSTRING(L, buffer, buffer_end, count, logger);

                CHECK_PUSHTABLE_OOB("key string", logger, buffer, buffer_end, count, depth);
            }
            else if (key_type == LUA_TNUMBER || key_type == LUA_TNEGATIVENUMBER)
            {
                PushTableLogString(logger, "KN");

                buffer = ReadEncodedIndex(L, key_type, header, buffer);
                CHECK_PUSHTABLE_OOB("key number", logger, buffer, buffer_end, count, depth);
            }

            switch (value_type)
            {
                case LUA_TBOOLEAN:
                {
                    PushTableLogString(logger, "VB");

                    lua_pushboolean(L, *buffer++);
                    CHECK_PUSHTABLE_OOB("value bool", logger, buffer, buffer_end, count, depth);
                }
                break;

                case LUA_TNUMBER:
                {
                    PushTableLogString(logger, "VN");

                    // NOTE: We align lua_Number to sizeof(float) even if lua_Number probably is of double type
                    intptr_t offset = buffer - original_buffer;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;
                    buffer += align_size;
                    // Sanity-check. At least 4 bytes alignment (de facto)
                    assert((((intptr_t) buffer) & 3) == 0);

                    lua_pushnumber(L, *((lua_Number_4_align *) buffer));
                    buffer += sizeof(lua_Number);

                    CHECK_PUSHTABLE_OOB("value number", logger, buffer, buffer_end, count, depth);
                }
                break;

                case LUA_TSTRING:
                {
                    PushTableLogString(logger, "VS");

                    if (header.m_Version <= 1)
                        buffer += LoadOldTSTRING(L, buffer, buffer_end, count, logger);
                    else
                        buffer += LoadTSTRING(L, buffer, buffer_end, count, logger);

                    CHECK_PUSHTABLE_OOB("value string", logger, buffer, buffer_end, count, depth);
                }
                break;

                case LUA_TUSERDATA:
                {
                    PushTableLogString(logger, "VU");

                    char sub_type = *buffer++;

                    // NOTE: We align lua_Number to sizeof(float) even if lua_Number probably is of double type
                    intptr_t offset = buffer - original_buffer;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;
                    buffer += align_size;
                    // Sanity-check. At least 4 bytes alignment (de facto)
                    assert((((intptr_t) buffer) & 3) == 0);

                    CHECK_PUSHTABLE_OOB("descriptor for udata", logger, buffer, buffer_end, count, depth);

                    if (sub_type == (char) SUB_TYPE_VECTOR3)
                    {
                        PushTableLogString(logger, "V3");

                        float* f = (float*) buffer;
                        dmScript::PushVector3(L, dmVMath::Vector3(f[0], f[1], f[2]));
                        buffer += sizeof(float) * 3;
                        CHECK_PUSHTABLE_OOB("udata vec3", logger, buffer, buffer_end, count, depth);
                    }
                    else if (sub_type == (char) SUB_TYPE_VECTOR4)
                    {
                        PushTableLogString(logger, "V4");

                        float* f = (float*) buffer;
                        dmScript::PushVector4(L, dmVMath::Vector4(f[0], f[1], f[2], f[3]));
                        buffer += sizeof(float) * 4;
                        CHECK_PUSHTABLE_OOB("udata vec4", logger, buffer, buffer_end, count, depth);
                    }
                    else if (sub_type == (char) SUB_TYPE_QUAT)
                    {
                        PushTableLogString(logger, "Q4");

                        float* f = (float*) buffer;
                        dmScript::PushQuat(L, dmVMath::Quat(f[0], f[1], f[2], f[3]));
                        buffer += sizeof(float) * 4;
                        CHECK_PUSHTABLE_OOB("udata quat", logger, buffer, buffer_end, count, depth);
                    }
                    else if (sub_type == (char) SUB_TYPE_MATRIX4)
                    {
                        PushTableLogString(logger, "M4");

                        float* f = (float*) buffer;
                        dmVMath::Matrix4 m;
                        for (uint32_t i = 0; i < 4; ++i)
                            for (uint32_t j = 0; j < 4; ++j)
                                m.setElem(i, j, f[i * 4 + j]);
                        dmScript::PushMatrix4(L, m);
                        buffer += sizeof(float) * 16;
                        CHECK_PUSHTABLE_OOB("udata mat4", logger, buffer, buffer_end, count, depth);
                    }
                    else if (sub_type == (char) SUB_TYPE_HASH)
                    {
                        PushTableLogString(logger, "H");

                        dmhash_t hash;
                        uint32_t hash_size = sizeof(dmhash_t);
                        memcpy(&hash, buffer, hash_size);
                        dmScript::PushHash(L, hash);
                        buffer += hash_size;
                        CHECK_PUSHTABLE_OOB("udata hash", logger, buffer, buffer_end, count, depth);
                    }
                    else if (sub_type == (char) SUB_TYPE_URL)
                    {
                        PushTableLogString(logger, "URL");

                        dmMessage::URL url;
                        uint32_t url_size = sizeof(dmMessage::URL);
                        memcpy(&url, buffer, url_size);
                        dmScript::PushURL(L, url);
                        buffer += url_size;
                        CHECK_PUSHTABLE_OOB("udata url", logger, buffer, buffer_end, count, depth);
                    }
                    else
                    {
                        return luaL_error(L, "Table contains invalid UserData subtype (%s) at element #%d: %s", lua_typename(L, key_type), i, buffer);
                    }
                }
                break;
                case LUA_TTABLE:
                {
                    int n_consumed = DoPushTable(L, logger, header, original_buffer, buffer, buffer_size, depth+1);
                    buffer += n_consumed;
                    CHECK_PUSHTABLE_OOB("table", logger, buffer, buffer_end, count, depth);
                }
                break;

                default:
                    return luaL_error(L, "Table contains invalid type (%s) at element #%d: %s", lua_typename(L, key_type), i, buffer);
                    break;
            }
            lua_settable(L, -3);

            CHECK_PUSHTABLE_OOB("loop end", logger, buffer, buffer_end, count, depth);
        }

        assert(top + 1 == lua_gettop(L));

        PushTableLogString(logger, "}");
        return buffer - buffer_start;
    }

#undef CHECK_PUSHTABLE_OOB

    void PushTable(lua_State*L, const char* buffer, uint32_t buffer_size)
    {
        TableHeader header;
        const char* original_buffer = buffer;

        // Check so that buffer has enough size to read header
        if (buffer_size < sizeof(TableHeader)) {
            char str[256];
            dmSnPrintf(str, sizeof(str), "Not enough data to read table header (buffer size: %u, header size: %lu)", buffer_size, sizeof(TableHeader));
            luaL_error(L, "%s", str);
        }

        buffer = ReadHeader(buffer, header);
        if (IsSupportedVersion(header))
        {
            buffer_size -= sizeof(TableHeader);
            PushTableLogger logger;
            logger.m_BufferStart = buffer;
            logger.m_BufferSize = buffer_size;
            DoPushTable(L, logger, header, original_buffer, buffer, buffer_size, 0);
        }
        else
        {
            char str[256];
            dmSnPrintf(str, sizeof(str), "Unsupported serialized table data: version = 0x%x (current = 0x%x)", header.m_Version, TABLE_VERSION_CURRENT);
            luaL_error(L, "%s", str);
        }
    }

}
