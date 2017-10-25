#include "script.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <dlib/buffer.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>

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
    /*# float64
     * Float, double precision, 8 bytes
     * @name buffer.VALUE_TYPE_FLOAT64
     * @variable
    */

#define SCRIPT_LIB_NAME "buffer"
#define SCRIPT_TYPE_NAME_BUFFER "buffer"
#define SCRIPT_TYPE_NAME_BUFFERSTREAM "bufferstream"

    // The stream concept as a struct, only exists here in the Lua world
    struct BufferStream
    {
        dmBuffer::HBuffer   m_Buffer;
        dmhash_t            m_Name;     // The stream name
        void*               m_Data;     // Easy access to the buffer data
        uint32_t            m_DataSize;	// Total data size in bytes
        uint32_t            m_Count;    // bytes / sizeof(type)
        uint32_t            m_TypeCount;// number of values that make up an "element". E.g. 3 in a Vec3
        dmBuffer::ValueType m_Type;		// The type of elements in the array
        int                 m_BufferRef;// Holds a reference to the Lua object
    };

    static bool IsBufferType(lua_State *L, int index, const char* type_name)
    {
        int top = lua_gettop(L);
        void *p = lua_touserdata(L, index);
        bool result = false;
        if (p != 0x0)
        {  /* value is a userdata? */
            if (lua_getmetatable(L, index))
            {  /* does it have a metatable? */
                lua_getfield(L, LUA_REGISTRYINDEX, type_name);  /* get correct metatable */
                if (lua_rawequal(L, -1, -2))
                {  /* does it have the correct mt? */
                    result = true;
                }
                lua_pop(L, 2);  /* remove both metatables */
            }
        }
        assert(top == lua_gettop(L));
        return result;
    }

    bool IsBuffer(lua_State *L, int index)
    {
        return IsBufferType(L, index, SCRIPT_TYPE_NAME_BUFFER);
    }

    void PushBuffer(lua_State* L, const dmScript::LuaHBuffer& v)
    {
        int top = lua_gettop(L);
        dmScript::LuaHBuffer* luabuf = (dmScript::LuaHBuffer*)lua_newuserdata(L, sizeof(dmScript::LuaHBuffer));
        luabuf->m_Buffer = v.m_Buffer;
        luabuf->m_UseLuaGC = v.m_UseLuaGC;
        luaL_getmetatable(L, SCRIPT_TYPE_NAME_BUFFER);
        lua_setmetatable(L, -2);
        assert(top + 1 == lua_gettop(L));
    }

    static dmScript::LuaHBuffer* CheckBufferNoError(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            dmScript::LuaHBuffer* buffer = (dmScript::LuaHBuffer*)luaL_checkudata(L, index, SCRIPT_TYPE_NAME_BUFFER);
            if( dmBuffer::IsBufferValid( buffer->m_Buffer ) ) {
                return buffer;
            }
        }
        return 0x0;
    }

    dmScript::LuaHBuffer* CheckBuffer(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            dmScript::LuaHBuffer* buffer = (dmScript::LuaHBuffer*)luaL_checkudata(L, index, SCRIPT_TYPE_NAME_BUFFER);
            if( dmBuffer::IsBufferValid( buffer->m_Buffer ) ) {
                return buffer;
            }
            luaL_error(L, "The buffer handle is invalid");
        }
        luaL_typerror(L, index, SCRIPT_TYPE_NAME_BUFFER);
        return 0x0;
    }

    static bool IsStream(lua_State *L, int index)
    {
        return IsBufferType(L, index, SCRIPT_TYPE_NAME_BUFFERSTREAM);
    }

    static int PushStream(lua_State* L, int bufferindex, dmBuffer::HBuffer buffer, dmhash_t stream_name)
    {
        dmBuffer::ValueType type;
        uint32_t typecount;
        dmBuffer::Result r = dmBuffer::GetStreamType(buffer, stream_name, &type, &typecount);
        if( r != dmBuffer::RESULT_OK )
        {
            return luaL_error(L, "Failed to get stream type: %s", dmBuffer::GetResultString(r));
        }

        void** data;
        uint32_t datasize;
        r = dmBuffer::GetStream(buffer, stream_name, (void**)&data, &datasize);
        if( r != dmBuffer::RESULT_OK )
        {
            return luaL_error(L, "Failed to get stream bytes: %s", dmBuffer::GetResultString(r));
        }

        int top = lua_gettop(L);
        BufferStream* p = (BufferStream*)lua_newuserdata(L, sizeof(BufferStream));
        p->m_Buffer = buffer;
        p->m_Name = stream_name;
        p->m_Data = data;
        p->m_DataSize = datasize;
        p->m_Type = type;
        p->m_TypeCount = typecount;
        p->m_Count = datasize / dmBuffer::GetSizeForValueType(type);

        // Push the Lua object and increase its ref count
        lua_pushvalue(L, bufferindex);
        p->m_BufferRef = dmScript::Ref(L, LUA_REGISTRYINDEX);

        luaL_getmetatable(L, SCRIPT_TYPE_NAME_BUFFERSTREAM);
        lua_setmetatable(L, -2);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static BufferStream* CheckStreamNoError(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            BufferStream* stream = (BufferStream*)luaL_checkudata(L, index, SCRIPT_TYPE_NAME_BUFFERSTREAM);
            if( dmBuffer::IsBufferValid( stream->m_Buffer ) ) {
                return stream;
            }
        }
        return 0x0;
    }

    static BufferStream* CheckStream(lua_State* L, int index)
    {
        if (lua_type(L, index) == LUA_TUSERDATA)
        {
            BufferStream* stream = (BufferStream*)luaL_checkudata(L, index, SCRIPT_TYPE_NAME_BUFFERSTREAM);
            if( dmBuffer::IsBufferValid( stream->m_Buffer ) ) {
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
        int top = lua_gettop(L);
        if( !lua_istable(L, index) )
        {
            return luaL_error(L, "buffer.create: Expected table, got %s", lua_typename(L, lua_type(L, index)));
        }

        lua_pushvalue(L, index);

        lua_pushnil(L);
        while (lua_next(L, -2) != 0)
        {
            if( lua_type(L, -2) != LUA_TSTRING )
            {
        		lua_pop(L, 3);
        		assert(top == lua_gettop(L));
                return luaL_error(L, "buffer.create: Unknown index type: %s - %s", lua_typename(L, lua_type(L, -2)), lua_tostring(L, -2));   
            }

            const char* key = lua_tostring(L, -2);

            if( strcmp(key, "name") == 0)
            {
                decl[current_decl].m_Name = dmScript::CheckHashOrString(L, -1);
            }
            else if( strcmp(key, "type") == 0)
            {
                decl[current_decl].m_ValueType = (dmBuffer::ValueType) luaL_checkint(L, -1);
            }
            else if( strcmp(key, "count") == 0)
            {
                decl[current_decl].m_ValueCount = (uint32_t) luaL_checkint(L, -1);
            }
            else
            {
        		lua_pop(L, 3);
        		assert(top == lua_gettop(L));
                return luaL_error(L, "buffer.create: Unknown index name: %s", key);
            }

            lua_pop(L, 1);
        }

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
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

        dmScript::LuaHBuffer luabuf = { buffer, true };
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
        int top = lua_gettop(L);
        dmScript::LuaHBuffer* buffer = dmScript::CheckBuffer(L, 1);
        dmhash_t stream_name = dmScript::CheckHashOrString(L, 2);
        PushStream(L, 1, buffer->m_Buffer, stream_name);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# copies data from one stream to another
     *
     * Copy a specified amount of data from one stream to another.
     *
     * [icon:attention] The value type must match between source and destination streams.
     * The source and destination streams can be the same.
     *
     * @name buffer.copy_stream
     * @param dst [type:bufferstream] the destination stream
     * @param dstoffset [type:number] the offset to start copying data to
     * @param src [type:bufferstream] the source data stream
     * @param srcoffset [type:number] the offset to start copying data from
     * @param count [type:number] the number of values to copy
     *
     * @examples
     * How to update a texture of a sprite:
     *
     * ```lua
     * -- copy entire stream
     * local srcstream = buffer.get_stream(srcbuffer, hash("luminance"))
     * local dststream = buffer.get_stream(dstbuffer, hash("a"))
     * buffer.copy_stream(dststream, 0, srcstream, 0, #srcstream)
     * ```
    */
    static int CopyStream(lua_State* L)
    {
        int top = lua_gettop(L);
        BufferStream* dststream = dmScript::CheckStream(L, 1);
        int dstoffset = luaL_checkint(L, 2);

        BufferStream* srcstream = 0;
        if( dmScript::IsStream(L, 3) )
        {
            srcstream = dmScript::CheckStream(L, 3);
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
                luaL_error(L, "The types of the streams differ. Expected 'buffer.%s', got 'buffer.%s'",
                                        dmBuffer::GetValueTypeString(dststream->m_Type), dmBuffer::GetValueTypeString(srcstream->m_Type) );
            }

            // Check overruns
            uint32_t valuesize = dmBuffer::GetSizeForValueType(dststream->m_Type);
            uint32_t bytes_to_copy      = valuesize * count;
            uint32_t dstoffset_in_bytes = dstoffset * valuesize;
            uint32_t srcoffset_in_bytes = srcoffset * valuesize;

            if( (dstoffset_in_bytes + bytes_to_copy) > dststream->m_DataSize )
            {
                luaL_error(L, "Trying to write too many values: Stream length: %d, Offset: %d, Values to copy: %d",
                        dststream->m_DataSize/valuesize, dstoffset, count);
            }
            if( (srcoffset_in_bytes + bytes_to_copy) > srcstream->m_DataSize )
            {
                luaL_error(L, "Trying to read too many values: Stream length: %d, Offset: %d, Values to copy: %d",
                        srcstream->m_DataSize/valuesize, srcoffset, count);
            }

            memmove( ((uint8_t*)dststream->m_Data) + dstoffset_in_bytes, ((uint8_t*)srcstream->m_Data) + srcoffset_in_bytes, bytes_to_copy);
        }

        assert(top == lua_gettop(L));
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
        int top = lua_gettop(L);

        dmScript::LuaHBuffer* _dstbuffer = dmScript::CheckBuffer(L, 1);
        dmScript::LuaHBuffer* _srcbuffer = dmScript::CheckBuffer(L, 3);
        dmBuffer::HBuffer dstbuffer = _dstbuffer->m_Buffer;
        dmBuffer::HBuffer srcbuffer = _srcbuffer->m_Buffer;
        int dstoffset = luaL_checkint(L, 2);
        int srcoffset = luaL_checkint(L, 4);
        int count = luaL_checkint(L, 5);

        // Validate first
        if( count <= 0 )
        {
            return luaL_error(L, "Invalid elements to copy: %u", count);
        }

        dmBuffer::Result r;
        uint32_t dstcount;
        uint32_t srccount;
        dmBuffer::GetElementCount(dstbuffer, &dstcount);
        dmBuffer::GetElementCount(srcbuffer, &srccount);
        if( (dstoffset + count) > dstcount )
        {
            return luaL_error(L, "Trying to write too many elements: Destination buffer length: %u, Offset: %u, Values to copy: %u", dstcount, dstoffset, count);
        }
        if( (srcoffset + count) > srccount )
        {
            return luaL_error(L, "Trying to read too many elements: Destination buffer length: %u, Offset: %u, Values to copy: %u", dstcount, dstoffset, count);
        }

        // Validate that target buffer has those stream names
        uint32_t num_streams = dmBuffer::GetNumStreams(srcbuffer);

        // Simple optimisation: Reusing this struct to hold some data between the validation step and the actual copy step
        BufferStream* stream_info = (BufferStream*)alloca( num_streams * 2 * sizeof(BufferStream) );

        for( uint32_t i = 0; i < num_streams; ++i )
        {
            BufferStream* dststream = &stream_info[i*2 + 0];
            BufferStream* srcstream = &stream_info[i*2 + 1];

            dmBuffer::GetStreamName(srcbuffer, i, &srcstream->m_Name);
            dmhash_t stream_name = srcstream->m_Name;

            r = dmBuffer::GetStream(dstbuffer, stream_name, (void**)&dststream->m_Data, &dststream->m_DataSize);
            if( r == dmBuffer::RESULT_STREAM_MISSING )
            {
                assert(top == lua_gettop(L));
                return luaL_error(L, "buffer.copy_buffer: Destination buffer has no stream named: %s", dmHashReverseSafe64(stream_name));
            }
            else if( r != dmBuffer::RESULT_OK )
            {
                assert(top == lua_gettop(L));
                return luaL_error(L, "buffer.copy_buffer: Failed getting destination byte array: %s", dmBuffer::GetResultString(r));
            }

            dmBuffer::GetStream(srcbuffer,  stream_name, (void**)&srcstream->m_Data, &srcstream->m_DataSize);
            GetStreamType(dstbuffer, stream_name, &dststream->m_Type, &dststream->m_TypeCount);
            GetStreamType(srcbuffer, stream_name, &srcstream->m_Type, &srcstream->m_TypeCount);

            if( dststream->m_Type != srcstream->m_Type )
            {
                assert(top == lua_gettop(L));
                return luaL_error(L, "buffer.copy_buffer: The streams (%s) have mismatching types: %s != %s", dmHashReverseSafe64(stream_name), dmBuffer::GetValueTypeString(dststream->m_Type), dmBuffer::GetValueTypeString(srcstream->m_Type));
            }

            if( dststream->m_TypeCount != srcstream->m_TypeCount )
            {
                assert(top == lua_gettop(L));
                return luaL_error(L, "buffer.copy_buffer: The streams (%s) have mismatching type count: %d != %d", dmHashReverseSafe64(stream_name), dststream->m_TypeCount, srcstream->m_TypeCount);
            }
        }

        // Now, do the copy
        for( uint32_t i = 0; i < num_streams; ++i )
        {
            BufferStream* dststream = &stream_info[i*2 + 0];
            BufferStream* srcstream = &stream_info[i*2 + 1];

            uint32_t valuesize = dmBuffer::GetSizeForValueType(dststream->m_Type) * dststream->m_TypeCount;
            uint32_t bytes_to_copy      = valuesize * count;
            uint32_t dstoffset_in_bytes = dstoffset * valuesize;
            uint32_t srcoffset_in_bytes = srcoffset * valuesize;

            memmove( ((uint8_t*)dststream->m_Data) + dstoffset_in_bytes, ((uint8_t*)srcstream->m_Data) + srcoffset_in_bytes, bytes_to_copy);
        }

        assert(top == lua_gettop(L));
        return 0;
    }


    /*# gets data from a stream
     *
     * Get all the bytes from a specified stream as a Lua string.
     *
     * @name buffer.get_bytes
     * @param buffer [type:buffer] the source buffer
     * @param stream_name [type:hash] the name of the stream
     * @return data [type:string] the buffer data as a Lua string
    */
    static int GetBytes(lua_State* L)
    {
        int top = lua_gettop(L);
        dmScript::LuaHBuffer* buffer = dmScript::CheckBuffer(L, 1);
        dmhash_t stream_name = dmScript::CheckHashOrString(L, -1);

        uint8_t* data;
        uint32_t datasize;
        dmBuffer::Result r = dmBuffer::GetStream(buffer->m_Buffer, stream_name, (void**)&data, &datasize);
        if( r != dmBuffer::RESULT_OK )
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "buffer.create: Failed creating buffer: %s", dmBuffer::GetResultString(r));
        }

        lua_pushlstring(L, (const char*)data, datasize);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    //////////////////////////////////////////////////////////////////
    // BUFFER

    static int Buffer_gc(lua_State *L)
    {
        dmScript::LuaHBuffer* buffer = CheckBufferNoError(L, 1);
        if( buffer && buffer->m_UseLuaGC )
        {
            dmBuffer::Destroy(buffer->m_Buffer);
        }
        return 0;
    }

    static int Buffer_tostring(lua_State *L)
    {
        int top = lua_gettop(L);
        dmScript::LuaHBuffer* buffer = CheckBuffer(L, 1);

        uint32_t num_streams = dmBuffer::GetNumStreams(buffer->m_Buffer);

        uint32_t out_element_count = 0;
        dmBuffer::Result r = dmBuffer::GetElementCount(buffer->m_Buffer, &out_element_count);
        if( r != dmBuffer::RESULT_OK )
        {
            lua_pushfstring(L, "buffer.%s(invalid)", SCRIPT_TYPE_NAME_BUFFER);
            assert(top + 1 == lua_gettop(L));
            return 1;
        }

        char buf[128];
        uint32_t maxlen = 64 + num_streams * sizeof(buf);
        char* s = (char*)alloca( maxlen );
        if( !s )
        {
            return luaL_error(L, "buffer.tostring: Failed creating temp memory (%u bytes)", maxlen);
        }

        *s = 0;
        DM_SNPRINTF(buf, sizeof(buf), "buffer.%s(count = %d, ", SCRIPT_TYPE_NAME_BUFFER, out_element_count);
        dmStrlCat(s, buf, maxlen);

        for( uint32_t i = 0; i < num_streams; ++i )
        {
            dmhash_t stream_name = 0;
            dmBuffer::GetStreamName(buffer->m_Buffer, i, &stream_name);

            dmBuffer::ValueType type;
            uint32_t type_count = 0;
            GetStreamType(buffer->m_Buffer, stream_name, &type, &type_count);

            const char* comma = i<(num_streams-1)?", ":"";
            const char* typestring = dmBuffer::GetValueTypeString(type);
            DM_SNPRINTF(buf, sizeof(buf), "{ hash(\"%s\"), buffer.%s, %d }%s", dmHashReverseSafe64(stream_name), typestring, type_count, comma );
            dmStrlCat(s, buf, maxlen);
        }
        dmStrlCat(s, ")", maxlen);

        lua_pushstring(L, s);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int Buffer_len(lua_State *L)
    {
        int top = lua_gettop(L);
        dmScript::LuaHBuffer* buffer = CheckBuffer(L, 1);
        uint32_t out_element_count = 0;
        dmBuffer::Result r = dmBuffer::GetElementCount(buffer->m_Buffer, &out_element_count);
        if (r != dmBuffer::RESULT_OK) {
            assert(top == lua_gettop(L));
            return luaL_error(L, "%s.%s could not get buffer length", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_BUFFER);
        }

        lua_pushnumber(L, out_element_count);
        assert(top + 1 == lua_gettop(L));
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
        lua_pushnumber(L, stream->m_Count);
        return 1;
    }

    static lua_Number GetStreamValue(BufferStream* stream, int index)
    {
        switch(stream->m_Type)
        {
        case dmBuffer::VALUE_TYPE_UINT8:    return ((uint8_t*)stream->m_Data)[index];
        case dmBuffer::VALUE_TYPE_UINT16:   return ((uint16_t*)stream->m_Data)[index];
        case dmBuffer::VALUE_TYPE_UINT32:   return ((uint32_t*)stream->m_Data)[index];
        case dmBuffer::VALUE_TYPE_UINT64:   return ((uint64_t*)stream->m_Data)[index];
        case dmBuffer::VALUE_TYPE_INT8:     return ((int8_t*)stream->m_Data)[index];
        case dmBuffer::VALUE_TYPE_INT16:    return ((int16_t*)stream->m_Data)[index];
        case dmBuffer::VALUE_TYPE_INT32:    return ((int32_t*)stream->m_Data)[index];
        case dmBuffer::VALUE_TYPE_INT64:    return ((int64_t*)stream->m_Data)[index];
        case dmBuffer::VALUE_TYPE_FLOAT32:  return ((float*)stream->m_Data)[index];
        case dmBuffer::VALUE_TYPE_FLOAT64:  return ((double*)stream->m_Data)[index];
        default:
            assert(false && "buffer.stream has unknown data type");
            break;
        }
        return 0;
    }


    static void SetStreamValue(BufferStream* stream, int index, lua_Number v)
    {
        switch(stream->m_Type)
        {
        case dmBuffer::VALUE_TYPE_UINT8:    ((uint8_t*)stream->m_Data)[index] = (uint8_t)v; break;
        case dmBuffer::VALUE_TYPE_UINT16:   ((uint16_t*)stream->m_Data)[index] = (uint16_t)v; break;
        case dmBuffer::VALUE_TYPE_UINT32:   ((uint32_t*)stream->m_Data)[index] = (uint32_t)v; break;
        case dmBuffer::VALUE_TYPE_UINT64:   ((uint64_t*)stream->m_Data)[index] = (uint64_t)v; break;
        case dmBuffer::VALUE_TYPE_INT8:     ((int8_t*)stream->m_Data)[index] = (int8_t)v; break;
        case dmBuffer::VALUE_TYPE_INT16:    ((int16_t*)stream->m_Data)[index] = (int16_t)v; break;
        case dmBuffer::VALUE_TYPE_INT32:    ((int32_t*)stream->m_Data)[index] = (int32_t)v; break;
        case dmBuffer::VALUE_TYPE_INT64:    ((int64_t*)stream->m_Data)[index] = (int64_t)v; break;
        case dmBuffer::VALUE_TYPE_FLOAT32:  ((float*)stream->m_Data)[index] = (float)v; break;
        case dmBuffer::VALUE_TYPE_FLOAT64:  ((double*)stream->m_Data)[index] = (double)v; break;
        default:
            assert(false && "buffer.stream has unknown data type");
            break;
        }
    }

    static int Stream_index(lua_State* L)
    {
        BufferStream* stream = CheckStream(L, 1);

        int key = luaL_checkinteger(L, 2);
        if (key > 0 && key <= stream->m_Count)
        {
            lua_pushnumber(L, GetStreamValue(stream, key - 1));
            return 1;
        }
        else
        {
            if (stream->m_Count > 0)
            {
                return luaL_error(L, "%s.%s only has valid indices between 1 and %d.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_BUFFERSTREAM, stream->m_Count);
            }
            return luaL_error(L, "%s.%s has no addressable indices, size is 0.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_BUFFERSTREAM);
        }
    }

    static int Stream_newindex(lua_State* L)
    {
        BufferStream* stream = CheckStream(L, 1);

        int key = luaL_checkinteger(L, 2);
        if (key > 0 && key <= stream->m_Count)
        {
            SetStreamValue(stream, key - 1, luaL_checknumber(L, 3));
        }
        else
        {
            if (stream->m_Count > 0)
            {
                return luaL_error(L, "%s.%s only has valid indices between 1 and %d.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_BUFFERSTREAM, stream->m_Count);
            }
            return luaL_error(L, "%s.%s has no addressable indices, size is 0.", SCRIPT_LIB_NAME, SCRIPT_TYPE_NAME_BUFFERSTREAM);
        }
        return 0;
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
        {0, 0}
    };

    struct BufferTypeStruct {
        const char* m_Name;
        const luaL_reg* m_Methods;
        const luaL_reg* m_Metatable;
    };

    void InitializeBuffer(lua_State* L)
    {
        int top = lua_gettop(L);

        const uint32_t type_count = 2;
        BufferTypeStruct types[type_count] =
        {
            {SCRIPT_TYPE_NAME_BUFFER, Buffer_methods, Buffer_meta},
            {SCRIPT_TYPE_NAME_BUFFERSTREAM, Stream_methods, Stream_meta},
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
        SETCONSTANT(VALUE_TYPE_FLOAT64);

#undef SETCONSTANT

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }

}
