// Copyright 2020-2023 The Defold Foundation
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

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <dlib/buffer.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>

#include "script_buffer.h"
#include "../resources/res_buffer.h"
#include "../gamesys.h"

#include <dmsdk/script/script.h>
#include <dmsdk/gamesys/script.h>

#if defined(_WIN32)
#include <malloc.h>
#define alloca(_SIZE) _alloca(_SIZE)
#endif

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    static uint32_t SCRIPT_BUFFER_TYPE_HASH = 0;
    static uint32_t SCRIPT_BUFFERSTREAM_TYPE_HASH = 0;
}

namespace dmGameSystem
{
    static dmResource::HFactory g_Factory = 0x0;

    /*# Buffer API documentation
     *
     * Functions for manipulating buffers and streams
     *
     * @document
     * @name Buffer
     * @namespace buffer
     */

    /*# uint8
     * Unsigned integer, 1 byte
     * @name buffer.VALUE_TYPE_UINT8
     * @variable
    */
    /*# uint16
     * Unsigned integer, 2 bytes
     * @name buffer.VALUE_TYPE_UINT16
     * @variable
    */
    /*# uint32
     * Unsigned integer, 4 bytes
     * @name buffer.VALUE_TYPE_UINT32
     * @variable
    */
    /*# uint64
     * Unsigned integer, 8 bytes
     * @name buffer.VALUE_TYPE_UINT64
     * @variable
    */
    /*# int8
     * Signed integer, 1 byte
     * @name buffer.VALUE_TYPE_INT8
     * @variable
    */
    /*# int16
     * Signed integer, 2 bytes
     * @name buffer.VALUE_TYPE_INT16
     * @variable
    */
    /*# int32
     * Signed integer, 4 bytes
     * @name buffer.VALUE_TYPE_INT32
     * @variable
    */
    /*# int64
     * Signed integer, 8 bytes
     * @name buffer.VALUE_TYPE_INT64
     * @variable
    */
    /*# float32
     * Float, single precision, 4 bytes
     * @name buffer.VALUE_TYPE_FLOAT32
     * @variable
    */

#define SCRIPT_LIB_NAME "buffer"
#define SCRIPT_TYPE_NAME_BUFFER "buffer"
#define SCRIPT_TYPE_NAME_BUFFERSTREAM "bufferstream"

    typedef void (*FStreamSetter)(void* data, int index, lua_Number v);
    typedef lua_Number (*FStreamGetter)(void* data, int index);

    // The stream concept as a struct, only exists here in the Lua world
    struct BufferStream
    {
        dmBuffer::HBuffer   m_Buffer;
        dmhash_t            m_Name;     // The stream name
        void*               m_Data;     // Pointer to the first struct in the stream
        FStreamSetter       m_Set;
        FStreamGetter       m_Get;
        uint32_t            m_Count;    // Number of structs contained in the stream (or buffer)
        uint32_t            m_Stride;   // The stride of the pointer, measured in the units of the value type
        uint32_t            m_TypeCount;// number of components that make up an "element". E.g. 3 in a Vec3
        dmBuffer::ValueType m_Type;		// The type of elements in the array
        int                 m_BufferRef;// Holds a reference to the Lua object
    };

    dmBuffer::HBuffer UnpackLuaBuffer(dmScript::LuaHBuffer* lua_buffer)
    {
        if (lua_buffer->m_Owner == dmScript::OWNER_RES) {
            BufferResource* res = (BufferResource*)lua_buffer->m_BufferRes;
            return res->m_Buffer;
        } else {
            return lua_buffer->m_Buffer;
        }
    }

    // Unless we check that a buffer resource actually exists, the engine will crash
    // when unpacking. This can happen if a user destroys a resource but a LuaHBuffer still holds a reference to the resource
    static bool CanUnpackLuaBuffer(dmScript::LuaHBuffer* lua_buffer)
    {
        if (lua_buffer->m_Owner == dmScript::OWNER_RES)
        {
            uint64_t hash;
            return dmResource::RESULT_OK == dmResource::GetPath(g_Factory, lua_buffer->m_BufferRes, &hash);
        }
        return true;
    }

    static bool IsStream(lua_State *L, int index)
    {
        return dmScript::GetUserType(L, index) == dmScript::SCRIPT_BUFFERSTREAM_TYPE_HASH;
    }

    template<typename T>
    lua_Number GetStreamValue(void* data, int index)
    {
        return *((T*)data + index);
    }

    template<typename T>
    void SetStreamValue(void* data, int index, lua_Number v)
    {
        *((T*)data + index) = (T)v;
    }

    FStreamGetter GetGetter(dmBuffer::ValueType type)
    {
        switch(type)
        {
        case dmBuffer::VALUE_TYPE_UINT8:    return &GetStreamValue<uint8_t>;
        case dmBuffer::VALUE_TYPE_UINT16:   return &GetStreamValue<uint16_t>;
        case dmBuffer::VALUE_TYPE_UINT32:   return &GetStreamValue<uint32_t>;
        case dmBuffer::VALUE_TYPE_UINT64:   return &GetStreamValue<uint64_t>;
        case dmBuffer::VALUE_TYPE_INT8:     return &GetStreamValue<int8_t>;
        case dmBuffer::VALUE_TYPE_INT16:    return &GetStreamValue<int16_t>;
        case dmBuffer::VALUE_TYPE_INT32:    return &GetStreamValue<int32_t>;
        case dmBuffer::VALUE_TYPE_INT64:    return &GetStreamValue<int64_t>;
        case dmBuffer::VALUE_TYPE_FLOAT32:  return &GetStreamValue<float>;
        default:
            dmLogWarning("buffer.stream has unknown data type");
            return 0;
        }
    }

    FStreamSetter GetSetter(dmBuffer::ValueType type)
    {
        switch(type)
        {
        case dmBuffer::VALUE_TYPE_UINT8:    return &SetStreamValue<uint8_t>;
        case dmBuffer::VALUE_TYPE_UINT16:   return &SetStreamValue<uint16_t>;
        case dmBuffer::VALUE_TYPE_UINT32:   return &SetStreamValue<uint32_t>;
        case dmBuffer::VALUE_TYPE_UINT64:   return &SetStreamValue<uint64_t>;
        case dmBuffer::VALUE_TYPE_INT8:     return &SetStreamValue<int8_t>;
        case dmBuffer::VALUE_TYPE_INT16:    return &SetStreamValue<int16_t>;
        case dmBuffer::VALUE_TYPE_INT32:    return &SetStreamValue<int32_t>;
        case dmBuffer::VALUE_TYPE_INT64:    return &SetStreamValue<int64_t>;
        case dmBuffer::VALUE_TYPE_FLOAT32:  return &SetStreamValue<float>;
        default:
            dmLogWarning("buffer.stream has unknown data type");
            return 0;
        }
    }

    static int PushStream(lua_State* L, int bufferindex, dmBuffer::HBuffer buffer, dmhash_t stream_name)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmBuffer::ValueType type;
        uint32_t components;
        dmBuffer::Result r = dmBuffer::GetStreamType(buffer, stream_name, &type, &components);
        if( r != dmBuffer::RESULT_OK )
        {
            return DM_LUA_ERROR("Failed to get stream type: %s", dmBuffer::GetResultString(r));
        }

        void** data;
        uint32_t count = 0;
        uint32_t stride = 0;
        r = dmBuffer::GetStream(buffer, stream_name, (void**)&data, &count, &components, &stride);
        if( r != dmBuffer::RESULT_OK )
        {
            return DM_LUA_ERROR("Failed to get stream bytes: %s", dmBuffer::GetResultString(r));
        }

        FStreamGetter getter = GetGetter(type);
        FStreamSetter setter = GetSetter(type);
        if (getter == 0 || setter == 0)
        {
            return DM_LUA_ERROR("Failed to get stream getter and setter!");
        }

        BufferStream* p = (BufferStream*)lua_newuserdata(L, sizeof(BufferStream));
        p->m_Buffer = buffer;
        p->m_Name = stream_name;
        p->m_Data = data;
        p->m_Count = count;
        p->m_Stride = stride;
        p->m_Type = type;
        p->m_TypeCount = components;
        p->m_Set = setter;
        p->m_Get = getter;

        // Push the Lua object and increase its ref count
        lua_pushvalue(L, bufferindex);
        p->m_BufferRef = dmScript::Ref(L, LUA_REGISTRYINDEX);

        luaL_getmetatable(L, SCRIPT_TYPE_NAME_BUFFERSTREAM);
        lua_setmetatable(L, -2);
        return 1;
    }

    static BufferStream* CheckStreamNoError(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            BufferStream* stream = (BufferStream*)dmScript::ToUserType(L, index, dmScript::SCRIPT_BUFFERSTREAM_TYPE_HASH);
            if (stream && dmBuffer::IsBufferValid(stream->m_Buffer))
            {
                return stream;
            }
        }
        return 0x0;
    }

    static BufferStream* CheckStream(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            BufferStream* stream = (BufferStream*)dmScript::CheckUserType(L, index, dmScript::SCRIPT_BUFFERSTREAM_TYPE_HASH, 0);
            if (stream && dmBuffer::IsBufferValid(stream->m_Buffer))
            {
                return stream;
            }
            luaL_error(L, "The buffer handle is invalid");
        }
        luaL_typerror(L, index, SCRIPT_TYPE_NAME_BUFFERSTREAM);
        return 0x0;
    }

    ////////////////////////////////////////////////////////
    // Buffer Module

    static int ParseStreamDeclaration(lua_State* L, int index, dmBuffer::StreamDeclaration* decl, int current_decl)
    {
        DM_LUA_STACK_CHECK(L, 0)
        if( !lua_istable(L, index) )
        {
            return DM_LUA_ERROR("buffer.create: Expected table, got %s", lua_typename(L, lua_type(L, index)));
        }

        lua_pushvalue(L, index);

        dmBuffer::ValueType value_type = dmBuffer::MAX_VALUE_TYPE_COUNT;
        lua_pushnil(L);
        while (lua_next(L, -2) != 0)
        {
            if( lua_type(L, -2) != LUA_TSTRING )
            {
        		lua_pop(L, 3);
                return DM_LUA_ERROR("buffer.create: Unknown index type: %s - %s", lua_typename(L, lua_type(L, -2)), lua_tostring(L, -2));
            }

            const char* key = lua_tostring(L, -2);

            if( strcmp(key, "name") == 0)
            {
                decl[current_decl].m_Name = dmScript::CheckHashOrString(L, -1);
            }
            else if( strcmp(key, "type") == 0)
            {
                value_type = (dmBuffer::ValueType) luaL_checkint(L, -1);
            }
            else if( strcmp(key, "count") == 0)
            {
                decl[current_decl].m_Count = (uint32_t) luaL_checkint(L, -1);
            }
            else
            {
        		lua_pop(L, 3);
                return DM_LUA_ERROR("buffer.create: Unknown index name: %s", key);
            }
            lua_pop(L, 1);
        }

        lua_pop(L, 1);

        if (value_type < 0 || value_type >= dmBuffer::MAX_VALUE_TYPE_COUNT)
        {
            return DM_LUA_ERROR("buffer.create: Invalid stream value type: %d. Must be between %d and %d. Is it a nil value in the declaration?", value_type, 0, dmBuffer::MAX_VALUE_TYPE_COUNT-1);
        }
        decl[current_decl].m_Type = value_type;

        return 0;
    }

    /*# creates a new buffer
     *
     * Create a new data buffer containing a specified set of streams. A data buffer
     * can contain one or more streams with typed data. This is useful for managing
     * compound data, for instance a vertex buffer could contain separate streams for
     * vertex position, color, normal etc.
     *
     * @name buffer.create
     * @param element_count [type:number] The number of elements the buffer should hold
     * @param declaration [type:table] A table where each entry (table) describes a stream
     *
     * - [type:hash|string] `name`: The name of the stream
     * - [type:constant] `type`: The data type of the stream
     * - [type:number] `count`: The number of values each element should hold
     *
     * @return buffer [type:buffer] the new buffer
     *
     * @examples
     * How to create and initialize a buffer
     *
     * ```lua
     * function init(self)
     *   local size = 128
     *   self.image = buffer.create( size * size, { {name=hash("rgb"), type=buffer.VALUE_TYPE_UINT8, count=3 } })
     *   self.imagestream = buffer.get_stream(self.image, hash("rgb"))
     *
     *   for y=0,self.height-1 do
     *      for x=0,self.width-1 do
     *          local index = y * self.width * 3 + x * 3 + 1
     *          self.imagestream[index + 0] = self.r
     *          self.imagestream[index + 1] = self.g
     *          self.imagestream[index + 2] = self.b
     *      end
     *   end
     * ```
     */
    static int Create(lua_State* L)
    {
        int top = lua_gettop(L);

        int num_elements = luaL_checkint(L, 1);
        if( num_elements < 1 )
        {
            return luaL_error(L, "buffer.create: Number of elements must be positive: %d", num_elements);
        }
        if( !lua_istable(L, 2) )
        {
            return luaL_error(L, "buffer.create: Second argument must be a table");
        }

        int num_decl = lua_objlen(L, 2);
        if( num_decl < 1 )
        {
            return luaL_error(L, "buffer.create: You must specify at least one stream declaration");
        }

        dmBuffer::StreamDeclaration* decl = (dmBuffer::StreamDeclaration*)alloca(num_decl * sizeof(dmBuffer::StreamDeclaration));
        if( !decl )
        {
            return luaL_error(L, "buffer.create: Failed to create memory for %d stream declarations", num_decl);
        }

        uint32_t count = 0;
        lua_pushvalue(L, 2);

        lua_pushnil(L);
        while (lua_next(L, -2) != 0)
        {
            ParseStreamDeclaration(L, -1, decl, count);
            count++;

            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        dmBuffer::HBuffer buffer = 0;
        dmBuffer::Result r = dmBuffer::Create((uint32_t)num_elements, decl, num_decl, &buffer);

        if( r != dmBuffer::RESULT_OK )
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "buffer.create: Failed creating buffer: %s", dmBuffer::GetResultString(r));
        }

        dmScript::LuaHBuffer luabuf(buffer, dmScript::OWNER_LUA);
        PushBuffer(L, luabuf);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# gets a stream from a buffer
     *
     * Get a specified stream from a buffer.
     *
     * @name buffer.get_stream
     * @param buffer [type:buffer] the buffer to get the stream from
     * @param stream_name [type:hash|string] the stream name
     * @return stream [type:bufferstream] the data stream
    */
    static int GetStream(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmBuffer::HBuffer hbuffer = dmScript::CheckBufferUnpack(L, 1);
        dmhash_t stream_name = dmScript::CheckHashOrString(L, 2);
        PushStream(L, 1, hbuffer, stream_name);
        return 1;
    }

    // Offsets and count is in "value type"
    template<typename T>
    static void CopyStreamInternalT(T* dst, uint32_t dstoffset, uint32_t dststride,
                                    const T* src, uint32_t srcoffset, uint32_t srcstride,
                                    uint32_t count, uint32_t components)
    {
        // convert from component offset into struct offset
        uint32_t dstcount = dstoffset / components;
        dstoffset = dstoffset % components;
        dst += dstcount * dststride;
        uint32_t srccount = srcoffset / components;
        srcoffset = srcoffset % components;
        src += srccount * srcstride;
        while( count > 0 )
        {
            dst[dstoffset] = src[srcoffset];
            dstoffset = (dstoffset+1) % components;
            srcoffset = (srcoffset+1) % components;
            if (dstoffset == 0)
            {
                dst += dststride;
            }
            if (srcoffset == 0)
            {
                src += srcstride;
            }
            count--;
        }
    }

    static bool CopyStreamInternal(BufferStream* dststream, uint32_t dstoffset,
                                    const BufferStream* srcstream, uint32_t srcoffset,
                                    uint32_t count)
    {
        #define DM_COPY_STREAM(_T_) CopyStreamInternalT<_T_>((_T_*)dststream->m_Data, dstoffset, dststream->m_Stride, \
                                                            (_T_*)srcstream->m_Data, srcoffset, srcstream->m_Stride, \
                                                            count, dststream->m_TypeCount)
            switch(dststream->m_Type)
            {
            case dmBuffer::VALUE_TYPE_UINT8:      DM_COPY_STREAM(uint8_t); break;
            case dmBuffer::VALUE_TYPE_UINT16:     DM_COPY_STREAM(uint16_t); break;
            case dmBuffer::VALUE_TYPE_UINT32:     DM_COPY_STREAM(uint32_t); break;
            case dmBuffer::VALUE_TYPE_UINT64:     DM_COPY_STREAM(uint64_t); break;
            case dmBuffer::VALUE_TYPE_INT8:       DM_COPY_STREAM(int8_t); break;
            case dmBuffer::VALUE_TYPE_INT16:      DM_COPY_STREAM(int16_t); break;
            case dmBuffer::VALUE_TYPE_INT32:      DM_COPY_STREAM(int32_t); break;
            case dmBuffer::VALUE_TYPE_INT64:      DM_COPY_STREAM(int64_t); break;
            case dmBuffer::VALUE_TYPE_FLOAT32:    DM_COPY_STREAM(float); break;
            default:
                return false;
            }
            return true;
    #undef DM_COPY_STREAM
    }

    /*# copies data from one stream to another
     *
     * Copy a specified amount of data from one stream to another.
     *
     * [icon:attention] The value type and size must match between source and destination streams.
     * The source and destination streams can be the same.
     *
     * @name buffer.copy_stream
     * @param dst [type:bufferstream] the destination stream
     * @param dstoffset [type:number] the offset to start copying data to (measured in value type)
     * @param src [type:bufferstream] the source data stream
     * @param srcoffset [type:number] the offset to start copying data from (measured in value type)
     * @param count [type:number] the number of values to copy (measured in value type)
     *
     * @examples
     * How to update a texture of a sprite:
     *
     * ```lua
     * -- copy entire stream
     * local srcstream = buffer.get_stream(srcbuffer, hash("xyz"))
     * local dststream = buffer.get_stream(dstbuffer, hash("xyz"))
     * buffer.copy_stream(dststream, 0, srcstream, 0, #srcstream)
     * ```
    */
    static int CopyStream(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        BufferStream* dststream = CheckStream(L, 1);
        int dstoffset = luaL_checkint(L, 2);

        BufferStream* srcstream = 0;
        if( IsStream(L, 3) )
        {
            srcstream = CheckStream(L, 3);
        }
        else
        {
            return luaL_typerror(L, 3, SCRIPT_TYPE_NAME_BUFFERSTREAM);
        }

        int srcoffset = luaL_checkint(L, 4);
        int count = luaL_checkint(L, 5);

        if(srcstream)
        {
            if( dststream->m_Type != srcstream->m_Type )
            {
                return DM_LUA_ERROR("The types of the streams differ. Expected 'buffer.%s', got 'buffer.%s'",
                                        dmBuffer::GetValueTypeString(dststream->m_Type), dmBuffer::GetValueTypeString(srcstream->m_Type) );
            }
            if( dststream->m_TypeCount != srcstream->m_TypeCount )
            {
                return DM_LUA_ERROR("The type count of the streams differ. Expected %u 'buffer.%s', got %u 'buffer.%s'",
                                        dststream->m_TypeCount, dmBuffer::GetValueTypeString(dststream->m_Type), srcstream->m_TypeCount, dmBuffer::GetValueTypeString(srcstream->m_Type) );
            }

            if( (uint32_t)(dstoffset + count) > dststream->m_Count * dststream->m_TypeCount )
            {
                return DM_LUA_ERROR("Trying to write too many values: Stream length: %d, Offset: %d, Values to copy: %d", dststream->m_Count, dstoffset, count);
            }
            if( (uint32_t)(srcoffset + count) > srcstream->m_Count * srcstream->m_TypeCount )
            {
                return DM_LUA_ERROR("Trying to read too many values: Stream length: %d, Offset: %d, Values to copy: %d", srcstream->m_Count, srcoffset, count);
            }

            if (!CopyStreamInternal(dststream, dstoffset, srcstream, srcoffset, count))
            {
                return DM_LUA_ERROR("Unknown stream value type: %d", dststream->m_Type);
            }
        }
        return 0;
    }

    /*# copies one buffer to another
     *
     * Copy all data streams from one buffer to another, element wise.
     *
     * [icon:attention] Each of the source streams must have a matching stream in the
     * destination buffer. The streams must match in both type and size.
     * The source and destination buffer can be the same.
     *
     * @name buffer.copy_buffer
     * @param dst [type:buffer] the destination buffer
     * @param dstoffset [type:number] the offset to start copying data to
     * @param src [type:buffer] the source data buffer
     * @param srcoffset [type:number] the offset to start copying data from
     * @param count [type:number] the number of elements to copy
     *
     * @examples
     * How to copy elements (e.g. vertices) from one buffer to another
     *
     * ```lua
     * -- copy entire buffer
     * buffer.copy_buffer(dstbuffer, 0, srcbuffer, 0, #srcbuffer)
     *
     * -- copy last 10 elements to the front of another buffer
     * buffer.copy_buffer(dstbuffer, 0, srcbuffer, #srcbuffer - 10, 10)
     * ```
    */
    static int CopyBuffer(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        dmBuffer::HBuffer dst_hbuffer = dmScript::CheckBufferUnpack(L, 1);
        dmBuffer::HBuffer src_hbuffer = dmScript::CheckBufferUnpack(L, 3);
        dmBuffer::HBuffer dstbuffer = dst_hbuffer;
        dmBuffer::HBuffer srcbuffer = src_hbuffer;
        int dstoffset = luaL_checkint(L, 2);
        int srcoffset = luaL_checkint(L, 4);
        int count = luaL_checkint(L, 5);

        // Validate first
        if( count <= 0 )
        {
            return DM_LUA_ERROR("Invalid elements to copy: %u", count);
        }

        dmBuffer::Result r;
        uint32_t dstcount;
        uint32_t srccount;
        dmBuffer::GetCount(dstbuffer, &dstcount);
        dmBuffer::GetCount(srcbuffer, &srccount);
        if( (dstoffset + count) > (int)dstcount )
        {
            return DM_LUA_ERROR("Trying to write too many elements: Destination buffer length: %u, Offset: %u, Values to copy: %u", dstcount, dstoffset, count);
        }
        if( (srcoffset + count) > (int)srccount )
        {
            return DM_LUA_ERROR("Trying to read too many elements: Destination buffer length: %u, Offset: %u, Values to copy: %u", dstcount, dstoffset, count);
        }

        // Validate that target buffer has those stream names
        uint32_t num_streams;
        dmBuffer::GetNumStreams(srcbuffer, &num_streams);

        // Simple optimisation: Reusing this struct to hold some data between the validation step and the actual copy step
        BufferStream* stream_info = (BufferStream*)alloca( num_streams * 2 * sizeof(BufferStream) );

        for( uint32_t i = 0; i < num_streams; ++i )
        {
            BufferStream* dststream = &stream_info[i*2 + 0];
            BufferStream* srcstream = &stream_info[i*2 + 1];

            dmBuffer::GetStreamName(srcbuffer, i, &srcstream->m_Name);
            dmhash_t stream_name = srcstream->m_Name;

            r = dmBuffer::GetStream(dstbuffer, stream_name, (void**)&dststream->m_Data, &dststream->m_Count, &dststream->m_TypeCount, &dststream->m_Stride);
            if( r == dmBuffer::RESULT_STREAM_MISSING )
            {
                return DM_LUA_ERROR("buffer.copy_buffer: Destination buffer has no stream named: %s", dmHashReverseSafe64(stream_name));
            }
            else if( r != dmBuffer::RESULT_OK )
            {
                return DM_LUA_ERROR("buffer.copy_buffer: Failed getting destination byte array: %s", dmBuffer::GetResultString(r));
            }

            dmBuffer::GetStream(srcbuffer,  stream_name, (void**)&srcstream->m_Data, &srcstream->m_Count, &srcstream->m_TypeCount, &srcstream->m_Stride);
            GetStreamType(dstbuffer, stream_name, &dststream->m_Type, &dststream->m_TypeCount);
            GetStreamType(srcbuffer, stream_name, &srcstream->m_Type, &srcstream->m_TypeCount);

            if( dststream->m_Type != srcstream->m_Type )
            {
                return DM_LUA_ERROR("buffer.copy_buffer: The streams (%s) have mismatching types: %s != %s", dmHashReverseSafe64(stream_name), dmBuffer::GetValueTypeString(dststream->m_Type), dmBuffer::GetValueTypeString(srcstream->m_Type));
            }

            if( dststream->m_TypeCount != srcstream->m_TypeCount )
            {
                return DM_LUA_ERROR("buffer.copy_buffer: The streams (%s) have mismatching type count: %d != %d", dmHashReverseSafe64(stream_name), dststream->m_TypeCount, srcstream->m_TypeCount);
            }
        }

        // Now, do the copy
        for( uint32_t i = 0; i < num_streams; ++i )
        {
            BufferStream* dststream = &stream_info[i*2 + 0];
            BufferStream* srcstream = &stream_info[i*2 + 1];

            if (!CopyStreamInternal(dststream, dstoffset * dststream->m_TypeCount, srcstream, srcoffset * dststream->m_TypeCount, count * dststream->m_TypeCount))
            {
                return DM_LUA_ERROR("Unknown stream value type: %d", dststream->m_Type);
            }
        }

        return 0;
    }


    /*# gets data from a stream
     *
     * Get a copy of all the bytes from a specified stream as a Lua string.
     *
     * @name buffer.get_bytes
     * @param buffer [type:buffer] the source buffer
     * @param stream_name [type:hash] the name of the stream
     * @return data [type:string] the buffer data as a Lua string
    */
    static int GetBytes(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmBuffer::HBuffer hbuffer = dmScript::CheckBufferUnpack(L, 1);

        uint8_t* data;
        uint32_t datasize;
        dmBuffer::Result r = dmBuffer::GetBytes(hbuffer, (void**)&data, &datasize);
        if( r != dmBuffer::RESULT_OK )
        {
            return DM_LUA_ERROR("buffer.create: Failed getting buffer: %s", dmBuffer::GetResultString(r));
        }

        lua_pushlstring(L, (const char*)data, datasize);
        return 1;
    }

    //////////////////////////////////////////////////////////////////
    // BUFFER

    static int Buffer_gc(lua_State *L)
    {
        dmScript::LuaHBuffer* buffer = dmScript::CheckBufferNoError(L, 1);
        if( buffer )
        {
            if (buffer->m_Owner == dmScript::OWNER_LUA)
            {
                dmBuffer::Destroy(buffer->m_Buffer);
            } else if (buffer->m_Owner == dmScript::OWNER_RES) {
                dmResource::Release(g_Factory, buffer->m_BufferRes);
            }

        }
        return 0;
    }

    static int Buffer_tostring(lua_State *L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        uint32_t num_streams;
        dmBuffer::HBuffer hbuffer = dmScript::CheckBufferUnpack(L, 1);
        dmBuffer::GetNumStreams(hbuffer, &num_streams);

        uint32_t out_element_count = 0;
        dmBuffer::Result r = dmBuffer::GetCount(hbuffer, &out_element_count);
        if( r != dmBuffer::RESULT_OK )
        {
            lua_pushfstring(L, "buffer.%s(invalid)", SCRIPT_TYPE_NAME_BUFFER);
            return 1;
        }

        char buf[128];
        uint32_t maxlen = 64 + num_streams * sizeof(buf);
        char* s = (char*)alloca( maxlen );
        if( !s )
        {
            return DM_LUA_ERROR("buffer.tostring: Failed creating temp memory (%u bytes)", maxlen);
        }

        *s = 0;
        uint32_t version = 0;
        dmBuffer::GetContentVersion(hbuffer, &version);
        dmSnPrintf(buf, sizeof(buf), "buffer.%s(count = %d, version = %u, handle = %u, ", SCRIPT_TYPE_NAME_BUFFER, out_element_count, version, (uint32_t) hbuffer);
        dmStrlCat(s, buf, maxlen);

        for( uint32_t i = 0; i < num_streams; ++i )
        {
            dmhash_t stream_name = 0;
            dmBuffer::GetStreamName(hbuffer, i, &stream_name);

            dmBuffer::ValueType type;
            uint32_t type_count = 0;
            GetStreamType(hbuffer, stream_name, &type, &type_count);

            const char* comma = i<(num_streams-1)?", ":"";
            const char* typestring = dmBuffer::GetValueTypeString(type);
            dmSnPrintf(buf, sizeof(buf), "{ hash(\"%s\"), buffer.%s, %d }%s", dmHashReverseSafe64(stream_name), typestring, type_count, comma );
            dmStrlCat(s, buf, maxlen);
        }
        dmStrlCat(s, ")", maxlen);

        lua_pushstring(L, s);
        return 1;
    }

    static int Buffer_len(lua_State *L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmBuffer::HBuffer hbuffer = dmScript::CheckBufferUnpack(L, 1);
        uint32_t count = 0;
        dmBuffer::Result r = dmBuffer::GetCount(hbuffer, &count);
        if (r != dmBuffer::RESULT_OK) {
            return DM_LUA_ERROR("%s.%s could not get buffer length", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_BUFFER);
        }

        lua_pushnumber(L, count);
        return 1;
    }

    static const luaL_reg Buffer_methods[] =
    {
        {0,0}
    };
    static const luaL_reg Buffer_meta[] =
    {
        {"__gc",        Buffer_gc},
        {"__tostring",  Buffer_tostring},
        {"__len",       Buffer_len},
        {0,0}
    };


    //////////////////////////////////////////////////////////////////
    // STREAM

    static int Stream_gc(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        BufferStream* stream = CheckStreamNoError(L, 1);
        if( stream )
        {
	        // decrease ref to buffer
	        dmScript::Unref(L, LUA_REGISTRYINDEX, stream->m_BufferRef);
	    }
        return 0;
    }

    static int Stream_tostring(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        BufferStream* stream = CheckStream(L, 1);
        dmBuffer::ValueType type;
        uint32_t type_count;
        dmBuffer::Result r = GetStreamType(stream->m_Buffer, stream->m_Name, &type, &type_count);
        if( r == dmBuffer::RESULT_OK )
            lua_pushfstring(L, "%s.%s({ hash(\"%s\"), buffer.%s, %d })", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_BUFFERSTREAM, dmHashReverseSafe64(stream->m_Name), dmBuffer::GetValueTypeString(type), type_count );
        else
            lua_pushfstring(L, "%s.%s({ hash(\"%s\"), unknown, unknown })", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_BUFFERSTREAM, dmHashReverseSafe64(stream->m_Name));
        return 1;
    }

    static int Stream_len(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        BufferStream* stream = CheckStream(L, 1);
        lua_pushnumber(L, stream->m_Count * stream->m_TypeCount);
        return 1;
    }

    static int Stream_index(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        BufferStream* stream = CheckStream(L, 1);
        int index = luaL_checkinteger(L, 2) - 1;
        if (index < 0 || index >= (int)(stream->m_Count * stream->m_TypeCount))
        {
            if (stream->m_Count > 0)
            {
                return DM_LUA_ERROR("%s.%s only has valid indices between 1 and %d.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_BUFFERSTREAM, stream->m_Count * stream->m_TypeCount);
            }
            return DM_LUA_ERROR("%s.%s has no addressable indices, size is 0.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_BUFFERSTREAM);
        }

        // convert contiguous index to stream space
        uint32_t count = index / stream->m_TypeCount;
        uint32_t component = index % stream->m_TypeCount;
        lua_pushnumber(L, stream->m_Get(stream->m_Data, count * stream->m_Stride + component));
        return 1;
    }

    static int Stream_newindex(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        BufferStream* stream = CheckStream(L, 1);
        int index = luaL_checkinteger(L, 2) - 1;
        if (index < 0 || index >= (int)(stream->m_Count * stream->m_TypeCount))
        {
            if (stream->m_Count > 0)
            {
                return DM_LUA_ERROR("%s.%s only has valid indices between 1 and %d.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_BUFFERSTREAM, stream->m_Count * stream->m_TypeCount);
            }
            return DM_LUA_ERROR("%s.%s has no addressable indices, size is 0.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_BUFFERSTREAM);
        }

        uint32_t count = index / stream->m_TypeCount;
        uint32_t component = index % stream->m_TypeCount;
        stream->m_Set(stream->m_Data, count * stream->m_Stride + component, luaL_checknumber(L, 3));
        dmBuffer::UpdateContentVersion(stream->m_Buffer);
        return 0;
    }


    // Allocates and fills up an array of ints/floats from a table at the top of the stack. It pops the table before returning.
    template <typename T>
    static T* LuaTableToArray(lua_State* L, uint32_t count, dmBuffer::ValueType valueType)
    {

        T* buffer = (T*) malloc(count*dmBuffer::GetSizeForValueType(valueType));

        uint32_t i = 0;
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (valueType == dmBuffer::VALUE_TYPE_FLOAT32)
            {
                buffer[i] = (T)luaL_checknumber(L, -1);
            } else
            {
                buffer[i] = (T)luaL_checkinteger(L, -1);
            }
            luaL_checkint(L, -2); // index of the value. Make sure it's an int

            i++;
            lua_pop(L, 1); // pop value, keep index for next iteration
        }

        lua_pop(L, 1); // pop the table itself

        return buffer;
    }

    /*# set a metadata entry on a buffer
     *
     * Creates or updates a metadata array entry on a buffer.
     *
     * [icon:attention] The value type and count given when updating the entry should match those used when first creating it.
     *
     * @name buffer.set_metadata
     * @param buf [type:buffer] the buffer to set the metadata on
     * @param metadata_name [type:hash|string] name of the metadata entry
     * @param values [type:table] actual metadata, an array of numeric values
     * @param value_type [type:constant] type of values when stored
     *
     * @examples
     * How to set a metadata entry on a buffer
     *
     * ```lua
     * -- create a new metadata entry with three floats
     * buffer.set_metadata(buf, hash("somefloats"), {1.5, 3.2, 7.9}, buffer.VALUE_TYPE_FLOAT32)
     * -- ...
     * -- update to a new set of values
     * buffer.set_metadata(buf, hash("somefloats"), {-2.5, 10.0, 32.2}, buffer.VALUE_TYPE_FLOAT32)
     * ```
    */
    static int SetMetadata(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        // get buffer
        dmBuffer::HBuffer hbuffer = dmScript::CheckBufferUnpack(L, 1);

        // get metadata entry name
        dmhash_t entry_name = dmScript::CheckHashOrString(L, 2);

        // get number type
        dmBuffer::ValueType valueType = (dmBuffer::ValueType) luaL_checkinteger(L, 4);

        // get array of values
        luaL_checktype(L, 3, LUA_TTABLE);
        uint32_t count = lua_objlen(L, 3);
        if (count > 0)
        {
            // validate valuetype early
            if (valueType < 0 || valueType >= dmBuffer::MAX_VALUE_TYPE_COUNT)
            {
                return DM_LUA_ERROR("invalid metadata value type supplied: %ld", (long) valueType);
            }
            if (valueType == dmBuffer::VALUE_TYPE_UINT64 || valueType == dmBuffer::VALUE_TYPE_INT64)
            {
                return DM_LUA_ERROR("64 bit integer metadata are not supported.");
            }

            void* values = 0;
            lua_pushvalue(L, 3);

            #define DM_LUA_TABLE_TO_ARRAY(_T_) values = (void*) LuaTableToArray<_T_>(L, count, valueType)
            switch(valueType)
            {
                case dmBuffer::VALUE_TYPE_FLOAT32:
                    DM_LUA_TABLE_TO_ARRAY(float);
                break;
                case dmBuffer::VALUE_TYPE_UINT8:
                    DM_LUA_TABLE_TO_ARRAY(uint8_t);
                break;
                case dmBuffer::VALUE_TYPE_UINT16:
                    DM_LUA_TABLE_TO_ARRAY(uint16_t);
                break;
                case dmBuffer::VALUE_TYPE_UINT32:
                    DM_LUA_TABLE_TO_ARRAY(uint32_t);
                break;
                case dmBuffer::VALUE_TYPE_INT8:
                    DM_LUA_TABLE_TO_ARRAY(int8_t);
                break;
                case dmBuffer::VALUE_TYPE_INT16:
                    DM_LUA_TABLE_TO_ARRAY(int16_t);
                break;
                case dmBuffer::VALUE_TYPE_INT32:
                    DM_LUA_TABLE_TO_ARRAY(int32_t);
                break;
                default:break;
            }
            #undef DM_LUA_TABLE_TO_ARRAY

            dmBuffer::Result r = dmBuffer::SetMetaData(hbuffer, entry_name, (void*) values, count, valueType);
            free(values);

            if( r != dmBuffer::RESULT_OK )
            {
                return DM_LUA_ERROR("cannot set metadata for buffer: %s", dmBuffer::GetResultString(r));
            }
        } else
        {
            return DM_LUA_ERROR("invalid metadata");
        }

        return 0;
    }

    // copies values from array to (empty) lua table on top of the stack
    template <typename T>
    static void ArrayToLuaTable(lua_State* L, const T* values, uint32_t count, dmBuffer::ValueType valueType) {
        if (valueType == dmBuffer::VALUE_TYPE_FLOAT32) {
            for (uint32_t i=0; i<count; i++)
            {
                T value = values[i];
                lua_pushnumber(L, value);
                lua_rawseti(L, -2, i+1); // lua indices start from 1
            }
        } else
        {
            for (uint32_t i=0; i<count; i++)
            {
                T value = values[i];
                lua_pushinteger(L, value);
                lua_rawseti(L, -2, i+1); // lua indices start from 1
            }
        }
    }

    /*# retrieve a metadata entry from a buffer
     *
     * Get a named metadata entry from a buffer along with its type.
     *
     * @name buffer.get_metadata
     * @param buf [type:buffer] the buffer to get the metadata from
     * @param metadata_name [type:hash|string] name of the metadata entry
     * @return values [type:table] table of metadata values or nil if the entry does not exist
     * @return value_type [type:constant] numeric type of values or nil
     *
     * @examples
     * How to get a metadata entry from a buffer
     *
     * ```lua
     * -- retrieve a metadata entry named "somefloats" and its nomeric type
     * local values, type = buffer.get_metadata(buf, hash("somefloats"))
     * if metadata then print(#metadata.." values in 'somefloats'") end
     * ```
    */
    static int GetMetadata(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);

        // get buffer
        dmBuffer::HBuffer hbuffer = dmScript::CheckBufferUnpack(L, 1);

        // get metadata entry name
        dmhash_t entry_name = dmScript::CheckHashOrString(L, 2);

        uint32_t count;
        dmBuffer::ValueType valueType;
        void* values;
        dmBuffer::Result r = dmBuffer::GetMetaData(hbuffer, entry_name, &values, &count, &valueType);
        if ( r == dmBuffer::RESULT_METADATA_MISSING)
        {
            lua_pushnil(L);  // nil for metadata entry
            lua_pushnil(L); // nil for numbertype
            return 2;
        }
        if ( r != dmBuffer::RESULT_OK )
        {
            return DM_LUA_ERROR("error getting metadata for buffer: %s", dmBuffer::GetResultString(r));
        }

        lua_newtable(L);
        #define DM_ARRAY_TO_LUA_TABLE(_T_) ArrayToLuaTable<_T_>(L, (_T_*) values, count, valueType)
        switch (valueType)
        {
            case dmBuffer::VALUE_TYPE_FLOAT32:
                DM_ARRAY_TO_LUA_TABLE(float);
            break;
            case dmBuffer::VALUE_TYPE_UINT8:
                DM_ARRAY_TO_LUA_TABLE(uint8_t);
            break;
            case dmBuffer::VALUE_TYPE_UINT16:
                DM_ARRAY_TO_LUA_TABLE(uint16_t);
            break;
            case dmBuffer::VALUE_TYPE_UINT32:
                DM_ARRAY_TO_LUA_TABLE(uint32_t);
            break;
            case dmBuffer::VALUE_TYPE_INT8:
                DM_ARRAY_TO_LUA_TABLE(int8_t);
            break;
            case dmBuffer::VALUE_TYPE_INT16:
                DM_ARRAY_TO_LUA_TABLE(int16_t);
            break;
            case dmBuffer::VALUE_TYPE_INT32:
                DM_ARRAY_TO_LUA_TABLE(int32_t);
            break;
            case dmBuffer::VALUE_TYPE_UINT64:
            case dmBuffer::VALUE_TYPE_INT64:
                return DM_LUA_ERROR("retrieving 64 bit integer metadata is not supported");
            break;
            default:
                // we shouldn't reach this point
                return DM_LUA_ERROR("invalid value type supplied: %d", valueType);
        }
        #undef DM_ARRAY_TO_LUA_TABLE
        lua_pushinteger(L, (uint32_t) valueType);

        return 2; // table with values and valueType in the stack
    }


    static const luaL_reg Stream_methods[] =
    {
        {0,0}
    };
    static const luaL_reg Stream_meta[] =
    {
        {"__gc",        Stream_gc},
        {"__tostring",  Stream_tostring},
        {"__len",       Stream_len},
        {"__index",     Stream_index},
        {"__newindex",  Stream_newindex},
        {0,0}
    };

    /////////////////////////////////////////////////////////////////////////////

    static const luaL_reg Module_methods[] =
    {
        {"create", Create},
        {"get_stream", GetStream},
        {"get_bytes", GetBytes},
        {"copy_stream", CopyStream},
        {"copy_buffer", CopyBuffer},
        {"set_metadata",SetMetadata},
        {"get_metadata",GetMetadata},
        {0, 0}
    };

    struct BufferTypeStruct {
        const char* m_Name;
        const luaL_reg* m_Methods;
        const luaL_reg* m_Metatable;
        uint32_t* m_TypeHash;
    };

    void ScriptBufferRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        g_Factory = context.m_Factory;
        int top = lua_gettop(L);

        const uint32_t type_count = 2;
        BufferTypeStruct types[type_count] =
        {
            {SCRIPT_TYPE_NAME_BUFFER, Buffer_methods, Buffer_meta, &dmScript::SCRIPT_BUFFER_TYPE_HASH},
            {SCRIPT_TYPE_NAME_BUFFERSTREAM, Stream_methods, Stream_meta, &dmScript::SCRIPT_BUFFERSTREAM_TYPE_HASH},
        };

        for (uint32_t i = 0; i < type_count; ++i)
        {
            *types[i].m_TypeHash = dmScript::RegisterUserType(L, types[i].m_Name, types[i].m_Methods, types[i].m_Metatable);
        }
        luaL_register(L, SCRIPT_LIB_NAME, Module_methods);

#define SETCONSTANT(name) \
        lua_pushnumber(L, (lua_Number) dmBuffer::name); \
        lua_setfield(L, -2, #name);\

        SETCONSTANT(VALUE_TYPE_UINT8);
        SETCONSTANT(VALUE_TYPE_UINT16);
        SETCONSTANT(VALUE_TYPE_UINT32);
        SETCONSTANT(VALUE_TYPE_UINT64);
        SETCONSTANT(VALUE_TYPE_INT8);
        SETCONSTANT(VALUE_TYPE_INT16);
        SETCONSTANT(VALUE_TYPE_INT32);
        SETCONSTANT(VALUE_TYPE_INT64);
        SETCONSTANT(VALUE_TYPE_FLOAT32);

#undef SETCONSTANT

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }

}

namespace dmScript
{
    static inline bool IsValidOwner(LuaBufferOwnership ownership)
    {
        return ownership == dmScript::OWNER_C || ownership == dmScript::OWNER_LUA || ownership == dmScript::OWNER_RES;
    }

    bool IsBuffer(lua_State *L, int index)
    {
        return dmScript::GetUserType(L, index) == SCRIPT_BUFFER_TYPE_HASH;
    }

    void PushBuffer(lua_State* L, const dmScript::LuaHBuffer& v)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmScript::LuaHBuffer* luabuf = (dmScript::LuaHBuffer*)lua_newuserdata(L, sizeof(dmScript::LuaHBuffer));
        luabuf->m_Buffer = v.m_Buffer;
        luabuf->m_BufferRes = v.m_BufferRes;
        luabuf->m_Owner = v.m_Owner;
        assert(IsValidOwner(luabuf->m_Owner));
        luaL_getmetatable(L, SCRIPT_TYPE_NAME_BUFFER);
        lua_setmetatable(L, -2);
    }

    // Note: the throw_error only controls this particular function, not the CheckUserType
    static dmBuffer::HBuffer CheckBufferUnpackInternal(lua_State* L, int index, bool throw_error, dmScript::LuaHBuffer** out_luabuffer)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            dmScript::LuaHBuffer* buffer = (dmScript::LuaHBuffer*)dmScript::CheckUserType(L, index, SCRIPT_BUFFER_TYPE_HASH, 0);
            if (!dmGameSystem::CanUnpackLuaBuffer(buffer))
            {
                if (throw_error)
                    luaL_error(L, "The buffer handle was stale");
                else
                    return 0;
            }

            dmBuffer::HBuffer hbuffer = dmGameSystem::UnpackLuaBuffer(buffer);
            if( dmBuffer::IsBufferValid( hbuffer ) ) {
                if (out_luabuffer)
                    *out_luabuffer = buffer;
                return hbuffer;
            }

            if (throw_error)
                luaL_error(L, "The buffer handle is invalid");
            else
                return 0;
        }
        luaL_typerror(L, index, SCRIPT_TYPE_NAME_BUFFER);
        return 0x0;
    }

    dmBuffer::HBuffer CheckBufferUnpack(lua_State* L, int index)
    {
        return CheckBufferUnpackInternal(L, index, true, 0);
    }

    dmBuffer::HBuffer CheckBufferUnpackNoError(lua_State* L, int index)
    {
        return CheckBufferUnpackInternal(L, index, false, 0);
    }

    dmScript::LuaHBuffer* CheckBuffer(lua_State* L, int index)
    {
        dmScript::LuaHBuffer* buffer = 0;
        dmBuffer::HBuffer hbuffer = CheckBufferUnpackInternal(L, index, true, &buffer);
        return hbuffer != 0 ? buffer : 0; // SHouldn't get here due to the lua_error
    }

    dmScript::LuaHBuffer* CheckBufferNoError(lua_State* L, int index)
    {
        dmScript::LuaHBuffer* buffer = 0;
        dmBuffer::HBuffer hbuffer = CheckBufferUnpackInternal(L, index, false, &buffer);
        return hbuffer != 0 ? buffer : 0;
    }
}
