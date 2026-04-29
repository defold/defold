// Copyright 2020-2026 The Defold Foundation
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
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/static_assert.h>
#include "script.h"
#include "script_private.h"

extern "C"
{
#include <dmsdk/dlua/dlua.h>
}

// custom type when writing negative numbers or hashes as keys
// the rest of the types used when serializing a table come from dlua.h
// make sure these types have a value quite a bit higher than the types in dlua.h
#define DLUA_TNEGATIVENUMBER 64
#define DLUA_THASH 65

namespace dmScript
{
    const int TABLE_MAGIC = 0x42544448;
    const uint32_t TABLE_VERSION_CURRENT = 5;
    const uint32_t TABLE_VERSION_BASE = 4;
    const uint32_t TABLE_VERSION_HASH_KEYS_ADDED = TABLE_VERSION_CURRENT;

    /*
     * Version 0:
     *    Written without a header. Original table serialization format:
     *
     *      uint16_t   count
     *
     *      char   key_type (DLUA_TSTRING or DLUA_TNUMBER)
     *      char   value_type (DLUA_TXXX)
     *      T      key (null terminated string or uint16_t)
     *      T      value
     *
     *      char   key_type (DLUA_TSTRING or DLUA_TNUMBER)
     *      char   value_type (DLUA_TXXX)
     *      T      key (null terminated string or uint16_t)
     *      T      value
     *      ...
     *      if value is of type Vector3, Vector4, Quat, Matrix4 or Hash i.e. DLUA_TUSERDATA, the first byte in value is the SubType
     *
     * Version 1:
     *    Adds a header block to the table at the head of the input, containing a magic identifier
     *    and version information. Keys of type DLUA_TNUMBER use a variable length encoding, with continuation
     *    between bytes signaled by the MSB.
     *
     *    The values of the magic word is chosen so that it will not match values that could be created using
     *    the previous serialization format: this is exploited in implementing backward compatibility when reading
     *    serialized tables.
     *
     *    For the sake of brevity, the header block is not repeated for tables that are nested within this serialized data:
     *    i.e. we assume that the version is uniform throughout any one data set.
     *
     *    DLUA_TNUMBER keys are restricted to 32 bits in length, an increase of 16 bits from the original version.
     *
     *    In all other respects, data is serialized in the same way. In particular, note that although users may
     *    create sparse arrays using 32 bit numbers as keys, we remain limited to 65536 (0xffff) rows per table.
     *
     *    Outside of their uses as keys, numerical values are not encoded in the fashion described above.
     *    For the imagined use cases, we consider it likely that taking this approach with keys will lead to smaller files,
     *    since a typical key will fit within a single byte of data. Numerical values when used elsewhere are essentially random
     *    and so we cannot guarantee that this encoding method will yield smaller data in such cases.
     *
     * Version 2:
     *    Adds support for binary strings.
     *
     * Version 3:
     *    Adds support for negative numeric keys (type DLUA_TNEGATIVENUMBER). Always writes four bytes.
     *
     * Version 4:
     *    Adds support for more than 65535 keys in a table.
     * 
     * Version 5:
     *    Adds support for hash userdata (type DLUA_THASH) as keys.
     */

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
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            supported = true;
            break;
        default:
            break;
        }
        return supported;
    }

    static char* WriteEncodedIndex(dlua_State* L, dlua_Number index, const TableHeader& header, char* buffer, const char* buffer_end, dmArray<const void*>& table_stack)
    {
        if (header.m_Version == 0)
        {
            if (buffer_end - buffer < 2)
            {
                table_stack.SetCapacity(0);
                dluaL_error(L, "table too large");
            }
            if (index > 0xffff)
            {
                table_stack.SetCapacity(0);
                dluaL_error(L, "index out of bounds, max is %d", 0xffff);
            }
            uint16_t key = (uint16_t)index;
            memcpy(buffer, &key, sizeof(uint16_t));
            buffer += sizeof(uint16_t);
        }
        else if (header.m_Version <= 2)
        {
            if (index > 0xffffffff)
            {
                table_stack.SetCapacity(0);
                dluaL_error(L, "index out of bounds, max is %d", 0xffffffff);
            }
            uint32_t key = (uint32_t)index;
            bool encoded = EncodeMSB(key, buffer, buffer_end);
            if (!encoded)
            {
                table_stack.SetCapacity(0);
                dluaL_error(L, "table too large");
            }
        }
        else if (header.m_Version <= 5)
        {
            if (buffer_end - buffer < 4)
            {
                table_stack.SetCapacity(0);
                dluaL_error(L, "table too large");
            }
            if (index < 0)
                index = -index;
            if (index > 0xffffffff)
            {
                table_stack.SetCapacity(0);
                dluaL_error(L, "index out of bounds, max is %d", 0xffffffff);
            }
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
    static uint32_t SaveTSTRING(dlua_State* L, int index, char* buffer, uint32_t buffer_size, const char* buffer_end, uint32_t count, dmArray<const void*>& table_stack)
    {
        size_t value_len = 0;
        const char* value = dlua_tolstring(L, index, &value_len);
        uint32_t total_size = value_len + sizeof(uint32_t);
        if (buffer_end - buffer < (intptr_t)total_size)
        {
            table_stack.SetCapacity(0);
            dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at '%s' for element #%d", buffer_size, value, count);
        }

        uint32_t len = (uint32_t)value_len;
        memcpy(buffer, &len, sizeof(uint32_t));
        buffer += sizeof(uint32_t);
        memcpy(buffer, value, value_len);
        return total_size;
    }

    // When loading older save games, we will use the old unpack method (with truncated c strings)
    static uint32_t LoadOldTSTRING(dlua_State* L, const char* buffer, const char* buffer_end, uint32_t count, PushTableLogger& logger)
    {
        uint32_t total_size = strlen(buffer) + 1;
        if (buffer_end - buffer < (intptr_t)total_size)
        {
            char log_str[PUSH_TABLE_LOGGER_STR_SIZE];
            PushTableLogPrint(logger, log_str);
            dluaL_error(L, "Reading outside of buffer at element #%d (string): wanted to read: %d bytes left: %d [BufStart: %p, BufSize: %lu]\n'%s'", count, total_size, (int)(buffer_end - buffer), logger.m_BufferStart, logger.m_BufferSize, log_str);
        }

        dlua_pushstring(L, buffer);
        return total_size;
    }

    // When loading/unpacking messages/save games, we use pascal strings, and the Lua binary string api
    static uint32_t LoadTSTRING(dlua_State* L, const char* buffer, const char* buffer_end, uint32_t count, PushTableLogger& logger)
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
            dluaL_error(L, "%s", str);
        }

        dlua_pushlstring(L, buffer + sizeof(uint32_t), value_len);
        return total_size;
    }

    static void StackPush(dmArray<const void*>& table_stack, const void* p)
    {
        if (table_stack.Full())
            table_stack.OffsetCapacity(8);
        table_stack.Push(p);
    }

    static const void* StackPop(dmArray<const void*>& table_stack)
    {
        const void* p = table_stack.Back();
        table_stack.Pop();
        return p;
    }

    static bool StackContains(dmArray<const void*>& table_stack, const void* p)
    {
        uint32_t size = table_stack.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            if (table_stack[i] == p)
                return true;
        }
        return false;
    }

    uint32_t DoCheckTableSize(dlua_State* L, int index, int parent_offset, dmArray<const void*>& table_stack)
    {
        int top = dlua_gettop(L);
        (void)top;

        dluaL_checktype(L, index, DLUA_TTABLE);

        const void* table_data = (const void*)dlua_topointer(L, index);
        if (StackContains(table_stack, table_data))
        {
            table_stack.SetCapacity(0);
            return dluaL_error(L, "Save table is recursive!");
        }
        StackPush(table_stack, table_data);

        dlua_pushvalue(L, index);
        dlua_pushnil(L);

        uint32_t size = 0;

        // count
        size += 4;
        while (dlua_next(L, -2) != 0)
        {
            dlua_Type key_type = dlua_type(L, -2);
            dlua_Type value_type = dlua_type(L, -1);
            bool key_is_hash = IsHash(L, -2);

            if (key_type != DLUA_TSTRING && key_type != DLUA_TNUMBER && !key_is_hash)
            {
                dluaL_error(L, "keys in table must be of type number, string or hash (found %s)", dlua_typename(L, key_type));
            }

            // key + value type
            size += 2;
            if (key_type == DLUA_TSTRING)
            {
                size += sizeof(uint32_t) + dlua_objlen(L, -2);
            }
            else if (key_type == DLUA_TNUMBER)
            {
                size += 4;
            }
            else if (key_is_hash)
            {
                size += sizeof(dmhash_t);
            }

            switch (value_type)
            {
                case DLUA_TBOOLEAN:
                {
                    size += 1;
                }
                break;

                case DLUA_TNUMBER:
                {
                    int offset = parent_offset + size;
                    int aligned_offset = (offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    int align_size = aligned_offset - offset;
                    size += align_size;
                    size += sizeof(dlua_Number);
                }
                break;

                case DLUA_TSTRING:
                {
                    size += sizeof(uint32_t) + dlua_objlen(L, -1);
                }
                break;

                case DLUA_TUSERDATA:
                {
                    // subtype
                    size += 1;

                    int offset = parent_offset + size;
                    int aligned_offset = (offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    int align_size = aligned_offset - offset;
                    size += align_size;


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
                        dluaL_error(L, "unsupported value type in table: %s", dlua_typename(L, value_type));
                    }
                }
                break;

                case DLUA_TTABLE:
                {
                    size += DoCheckTableSize(L, -1, parent_offset + size, table_stack);
                }
                break;

                default:
                    dluaL_error(L, "unsupported value type in table: %s", dlua_typename(L, value_type));
                    break;
            }

            dlua_pop(L, 1);
        }
        dlua_pop(L, 1);

        const void* p = StackPop(table_stack);
        assert(p == table_data);
        return size;
    }

    uint32_t CheckTableSize(dlua_State* L, int index)
    {
        dmArray<const void*> table_stack;
        uint32_t size = sizeof(TableHeader) + DoCheckTableSize(L, index, 0, table_stack);
        return size;
    }

    static uint32_t DoCheckTable(dlua_State* L, TableHeader& header, const char* original_buffer, char* buffer, uint32_t buffer_size, int index, dmArray<const void*>& table_stack)
    {
        int top = dlua_gettop(L);
        (void)top;

        char* buffer_start = buffer;
        char* buffer_end = buffer + buffer_size;
        dluaL_checktype(L, index, DLUA_TTABLE);

        const void* table_data = (const void*)dlua_topointer(L, index);
        if (StackContains(table_stack, table_data))
        {
            table_stack.SetCapacity(0);
            return dluaL_error(L, "Save table is recursive!");
        }
        StackPush(table_stack, table_data);

        dlua_pushvalue(L, index);
        dlua_pushnil(L);

        if (buffer_size < 4)
        {
            table_stack.SetCapacity(0);
            dluaL_error(L, "table too large");
        }
        // Make room for count (4 bytes)
        buffer += 4;

        uint32_t count = 0;
        while (dlua_next(L, -2) != 0)
        {
            // Check overflow
            if (count == (uint32_t)0xffffffff)
            {
                table_stack.SetCapacity(0);
                dluaL_error(L, "too many values in table, %d is max", 0xffffffff);
            }

            count++;

            dlua_Type key_type = dlua_type(L, -2);
            dmhash_t* key_hash = ToHash(L, -2);
            dlua_Type value_type = dlua_type(L, -1);

            if (key_type != DLUA_TSTRING && key_type != DLUA_TNUMBER && !key_hash)
            {
                table_stack.SetCapacity(0);
                dluaL_error(L, "keys in table must be of type number, string or hash (found %s)", dlua_typename(L, key_type));
            }

            if (buffer_end - buffer < 2)
            {
                table_stack.SetCapacity(0);
                dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at key for element #%d", buffer_size, count);
            }

            if (key_type == DLUA_TSTRING)
            {
                (*buffer++) = (char) DLUA_TSTRING;
                (*buffer++) = (char) value_type;
                buffer += SaveTSTRING(L, -2, buffer, buffer_size, buffer_end, count, table_stack);
            }
            else if (key_type == DLUA_TNUMBER)
            {
                dlua_Number key = dlua_tonumber(L, -2);
                (*buffer++) = (char) (key >= 0 ? DLUA_TNUMBER : DLUA_TNEGATIVENUMBER);
                (*buffer++) = (char) value_type;
                buffer = WriteEncodedIndex(L, key, header, buffer, buffer_end, table_stack);
            }
            else if (key_hash)
            {
                header.m_Version = TABLE_VERSION_HASH_KEYS_ADDED;
                (*buffer++) = (char) DLUA_THASH;
                (*buffer++) = (char) value_type;
            
                const uint32_t hash_size = sizeof(dmhash_t);

                if (buffer_end - buffer < int32_t(hash_size))
                {
                    table_stack.SetCapacity(0);
                    dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at key (hash) for element #%d", buffer_size, count);
                }

                memcpy(buffer, (const void*)key_hash, hash_size);
                buffer += hash_size;
            }

            switch (value_type)
            {
                case DLUA_TBOOLEAN:
                {
                    if (buffer_end - buffer < 1)
                    {
                        table_stack.SetCapacity(0);
                        dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, dlua_typename(L, key_type), count);
                    }
                    (*buffer++) = (char) dlua_toboolean(L, -1);
                }
                break;

                case DLUA_TNUMBER:
                {
                    // NOTE: We align dlua_Number to sizeof(float) even if dlua_Number probably is of double type
                    intptr_t offset = buffer - original_buffer;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;

                    if (buffer_end - buffer < align_size)
                    {
                        table_stack.SetCapacity(0);
                        dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, dlua_typename(L, key_type), count);
                    }

#ifndef NDEBUG
                    memset(buffer, 0, align_size);
#endif
                    buffer += align_size;

                    if (buffer_end - buffer < int32_t(sizeof(dlua_Number)) || buffer_end - buffer < align_size)
                    {
                        table_stack.SetCapacity(0);
                        dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, dlua_typename(L, key_type), count);
                    }

                    union
                    {
                        dlua_Number x;
                        char buf[sizeof(dlua_Number)];
                    };

                    x = dlua_tonumber(L, -1);
                    memcpy(buffer, buf, sizeof(dlua_Number));
                    buffer += sizeof(dlua_Number);
                }
                break;

                case DLUA_TSTRING:
                {
                    buffer += SaveTSTRING(L, -1, buffer, buffer_size, buffer_end, count, table_stack);
                }
                break;

                case DLUA_TUSERDATA:
                {
                    if (buffer_end - buffer < 1)
                    {
                        table_stack.SetCapacity(0);
                        dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, dlua_typename(L, key_type), count);
                    }

                    char* sub_type = buffer++;

                    // NOTE: We align dlua_Number to sizeof(float) even if dlua_Number probably is of double type
                    intptr_t offset = buffer - original_buffer;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;

                    if (buffer_end - buffer < align_size)
                    {
                        table_stack.SetCapacity(0);
                        dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, dlua_typename(L, key_type), count);
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
                            table_stack.SetCapacity(0);
                            dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, dlua_typename(L, key_type), count);
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
                            table_stack.SetCapacity(0);
                            dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, dlua_typename(L, key_type), count);
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
                            table_stack.SetCapacity(0);
                            dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, dlua_typename(L, key_type), count);
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
                            table_stack.SetCapacity(0);
                            dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, dlua_typename(L, key_type), count);
                        }

                        *sub_type = (char) SUB_TYPE_MATRIX4;
                        for (uint32_t i = 0; i < 4; ++i)
                            for (uint32_t j = 0; j < 4; ++j)
                                *f++ = m->getElem(i, j);

                        buffer += sizeof(float) * 16;
                    }
                    else if (IsHash(L, -1))
                    {
                        dmhash_t hash = *(dmhash_t*)dlua_touserdata(L, -1);
                        const uint32_t hash_size = sizeof(dmhash_t);

                        if (buffer_end - buffer < int32_t(hash_size))
                        {
                            table_stack.SetCapacity(0);
                            dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, dlua_typename(L, key_type), count);
                        }

                        *sub_type = (char) SUB_TYPE_HASH;

                        memcpy(buffer, (const void*)&hash, hash_size);
                        buffer += hash_size;
                    }
                    else if (IsURL(L, -1))
                    {
                        dmMessage::URL* url = (dmMessage::URL*)dlua_touserdata(L, -1);
                        const uint32_t url_size = sizeof(dmMessage::URL);

                        if (buffer_end - buffer < int32_t(url_size))
                        {
                            table_stack.SetCapacity(0);
                            dluaL_error(L, "buffer (%d bytes) too small for table, exceeded at value (%s) for element #%d", buffer_size, dlua_typename(L, key_type), count);
                        }

                        *sub_type = (char) SUB_TYPE_URL;

                        memcpy(buffer, (const void*)url, url_size);
                        buffer += url_size;
                    }
                    else
                    {
                        table_stack.SetCapacity(0);
                        dluaL_error(L, "unsupported value type in table: %s", dlua_typename(L, value_type));
                    }
                }
                break;

                case DLUA_TTABLE:
                {
                    uint32_t n_used = DoCheckTable(L, header, original_buffer, buffer, buffer_end - buffer, -1, table_stack);
                    buffer += n_used;
                }
                break;

                default:
                    table_stack.SetCapacity(0);
                    dluaL_error(L, "unsupported value type in table: %s", dlua_typename(L, value_type));
                    break;
            }

            dlua_pop(L, 1);
        }
        dlua_pop(L, 1);

        const void* p = StackPop(table_stack);
        assert(p == table_data);

        memcpy(buffer_start, &count, sizeof(uint32_t));

        assert(top == dlua_gettop(L));
        return buffer - buffer_start;
    }

    uint32_t CheckTable(dlua_State* L, char* buffer, uint32_t buffer_size, int index)
    {
        assert((intptr_t)buffer % 16 == 0);
        if (buffer_size > sizeof(TableHeader)) {
            char* original_buffer = buffer;

            TableHeader* header = (TableHeader*)buffer;
            header->m_Magic = TABLE_MAGIC;
            header->m_Version = TABLE_VERSION_BASE;
            buffer += sizeof(TableHeader);
            buffer_size -= (buffer - original_buffer);

            dmArray<const void*> table_stack;
            return sizeof(TableHeader) + DoCheckTable(L, *header, original_buffer, buffer, buffer_size, index, table_stack);
        } else {
            dluaL_error(L, "buffer (%d bytes) too small for header (%zu bytes)", buffer_size, sizeof(TableHeader));
            return 0;
        }
    }

    const char* ReadHeader(const char* buffer, TableHeader& header)
    {
        TableHeader* buffered_header = (TableHeader*)buffer;
        if (TABLE_MAGIC == buffered_header->m_Magic) {
            buffer += sizeof(TableHeader);
            header = *buffered_header;
        }
        return buffer;
    }

    static const char* ReadEncodedIndex(dlua_State* L, char key_type, const TableHeader& header, const char* buffer)
    {
        if (header.m_Version == 0)
        {
            if (key_type != DLUA_TNUMBER)
            {
                dluaL_error(L, "Unknown key type %d", key_type);
            }
            uint16_t value;
            memcpy(&value, buffer, sizeof(uint16_t));
            dlua_pushnumber(L, value);
            buffer += sizeof(uint16_t);
        }
        else if (header.m_Version <= 2)
        {
            if (key_type != DLUA_TNUMBER)
            {
                dluaL_error(L, "Unknown key type %d", key_type);
            }
            uint32_t index;
            if(DecodeMSB(index, buffer))
            {
                dlua_pushnumber(L, index);
            }
            else
            {
                dluaL_error(L, "Invalid number encoding");
            }
        }
        else if (header.m_Version <= 5)
        {
            if (key_type != DLUA_TNUMBER && key_type != DLUA_TNEGATIVENUMBER)
            {
                dluaL_error(L, "Unknown key type %d", key_type);
            }
            uint8_t b1 = (uint8_t)*buffer++;
            uint8_t b2 = (uint8_t)*buffer++;
            uint8_t b3 = (uint8_t)*buffer++;
            uint8_t b4 = (uint8_t)*buffer++;
            uint32_t index = b4 << 24 | b3 << 16 | b2 << 8 | b1;
            dlua_Number number = index;
            if (key_type == DLUA_TNEGATIVENUMBER)
            {
                number = -number;
            }
            dlua_pushnumber(L, number);
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
        vsnprintf(buffer, count, format, argp);
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
        dmSnPrintf(str, sizeof(str), "Reading outside of buffer after %s element #%d (depth: #%d) [BufStart: %p, Cursor: %p, End: %p, BufSize: %u, Bytes OOB: %d].\n'%s'", ELEMTYPE, COUNT, DEPTH, LOGGER.m_BufferStart, BUFFER, BUFFER_END, (uint32_t)LOGGER.m_BufferSize, (int)(BUFFER_END - BUFFER), log_str); \
        return dluaL_error(L, "%s", str); \
    }

    int DoPushTable(dlua_State*L, PushTableLogger& logger, const TableHeader& header, const char* original_buffer, const char* buffer, uint32_t buffer_size, uint32_t depth)
    {
        int top = dlua_gettop(L);
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
            return dluaL_error(L, "%s", str);
        }

        dlua_newtable(L);

        for (uint32_t i = 0; i < count; ++i)
        {
            CHECK_PUSHTABLE_OOB("key-value tags", logger, buffer+2, buffer_end, count, depth);

            char key_type = (*buffer++);
            char value_type = (*buffer++);

            if (key_type == DLUA_TSTRING)
            {
                PushTableLogString(logger, "KS");

                if (header.m_Version <= 1)
                    buffer += LoadOldTSTRING(L, buffer, buffer_end, count, logger);
                else
                    buffer += LoadTSTRING(L, buffer, buffer_end, count, logger);

                CHECK_PUSHTABLE_OOB("key string", logger, buffer, buffer_end, count, depth);
            }
            else if (key_type == DLUA_TNUMBER || key_type == DLUA_TNEGATIVENUMBER)
            {
                PushTableLogString(logger, "KN");

                buffer = ReadEncodedIndex(L, key_type, header, buffer);
                CHECK_PUSHTABLE_OOB("key number", logger, buffer, buffer_end, count, depth);
            }
            else if (key_type == DLUA_THASH)
            {
                PushTableLogString(logger, "KH");

                dmhash_t hash;
                uint32_t hash_size = sizeof(dmhash_t);
                memcpy(&hash, buffer, hash_size);
                dmScript::PushHash(L, hash);
                buffer += hash_size;
                CHECK_PUSHTABLE_OOB("key hash", logger, buffer, buffer_end, count, depth);
            }

            switch (value_type)
            {
                case DLUA_TBOOLEAN:
                {
                    PushTableLogString(logger, "VB");

                    dlua_pushboolean(L, *buffer++);
                    CHECK_PUSHTABLE_OOB("value bool", logger, buffer, buffer_end, count, depth);
                }
                break;

                case DLUA_TNUMBER:
                {
                    PushTableLogString(logger, "VN");

                    // NOTE: We align dlua_Number to sizeof(float) even if dlua_Number probably is of double type
                    intptr_t offset = buffer - original_buffer;
                    intptr_t aligned_buffer = ((intptr_t) offset + sizeof(float)-1) & ~(sizeof(float)-1);
                    intptr_t align_size = aligned_buffer - (intptr_t) offset;
                    buffer += align_size;

                    union
                    {
                        dlua_Number x;
                        char x_buf[sizeof(dlua_Number)];
                    };
                    memcpy(x_buf, buffer, sizeof(dlua_Number));
                    dlua_pushnumber(L, x);
                    buffer += sizeof(dlua_Number);

                    CHECK_PUSHTABLE_OOB("value number", logger, buffer, buffer_end, count, depth);
                }
                break;

                case DLUA_TSTRING:
                {
                    PushTableLogString(logger, "VS");

                    if (header.m_Version <= 1)
                        buffer += LoadOldTSTRING(L, buffer, buffer_end, count, logger);
                    else
                        buffer += LoadTSTRING(L, buffer, buffer_end, count, logger);

                    CHECK_PUSHTABLE_OOB("value string", logger, buffer, buffer_end, count, depth);
                }
                break;

                case DLUA_TUSERDATA:
                {
                    PushTableLogString(logger, "VU");

                    char sub_type = *buffer++;

                    // NOTE: We align dlua_Number to sizeof(float) even if dlua_Number probably is of double type
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
                        return dluaL_error(L, "Table contains invalid UserData subtype (%d) at element #%d: %s", sub_type, i, buffer);
                    }
                }
                break;
                case DLUA_TTABLE:
                {
                    int n_consumed = DoPushTable(L, logger, header, original_buffer, buffer, buffer_size, depth+1);
                    buffer += n_consumed;
                    CHECK_PUSHTABLE_OOB("table", logger, buffer, buffer_end, count, depth);
                }
                break;

                default:
                    return dluaL_error(L, "Table contains invalid value type (%s) at element #%d: %s", dlua_typename(L, value_type), i, buffer);
                    break;
            }
            dlua_settable(L, -3);

            CHECK_PUSHTABLE_OOB("loop end", logger, buffer, buffer_end, count, depth);
        }

        assert(top + 1 == dlua_gettop(L));

        PushTableLogString(logger, "}");
        return buffer - buffer_start;
    }

#undef CHECK_PUSHTABLE_OOB

    void PushTable(dlua_State*L, const char* buffer, uint32_t buffer_size)
    {
        TableHeader header;
        const char* original_buffer = buffer;

        // Check so that buffer has enough size to read header
        if (buffer_size < sizeof(TableHeader)) {
            char str[256];
            dmSnPrintf(str, sizeof(str), "Not enough data to read table header (buffer size: %u, header size: %u)", buffer_size, (uint32_t)sizeof(TableHeader));
            dluaL_error(L, "%s", str);
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
            dluaL_error(L, "%s", str);
        }
    }

}
