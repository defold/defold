
#include <dmsdk/dlib/buffer.h>

#include <dlib/log.h>
#include <dlib/memory.h>
#include <dlib/math.h>

#include <string.h>
#include <assert.h>

namespace dmBuffer
{
    static const uint8_t ADDR_ALIGNMENT = 16;
    static const uint8_t GUARD_VALUES[] = {
        0xD3, 0xF0, 0x1D, 0xFF,
        0xD3, 0xF0, 0x1D, 0xFF,
        0xD3, 0xF0, 0x1D, 0xFF,
        0xD3, 0xF0, 0x1D, 0xFF,
    };
    static const uint8_t GUARD_SIZE = sizeof(GUARD_VALUES);

    struct Buffer
    {
        struct Stream
        {
            dmhash_t    m_Name;
            uint32_t    m_Offset; // ~4gb addressable space
            uint8_t     m_ValueType;
            uint8_t     m_ValueCount;
        };

        void*    m_Data; // All stream data, including guard bytes after each stream and 16 byte aligned.
        Stream*  m_Streams;
        uint32_t m_NumElements;
        uint16_t m_Version;
        uint8_t  m_NumStreams;
    };

    struct BufferContext
    {
        // Holds available slots (quite few, so simple linear search should be fine when creating new buffers)
        // Realloc when it grows
        Buffer** m_Buffers;
        uint32_t m_Capacity;
        uint32_t m_Version;
    };

    static BufferContext* g_BufferContext = 0;

    /////////////////////////////////////////////////////////////////
    // Buffer context

    void NewContext()
    {
        assert(g_BufferContext == 0 && "Buffer context should be null");

        const uint32_t capacity = 128;
        g_BufferContext = (BufferContext*)malloc( sizeof(BufferContext) + capacity * sizeof(Buffer*) );
        g_BufferContext->m_Capacity = capacity;
        uint32_t size = capacity * sizeof(Buffer*);
        g_BufferContext->m_Buffers = (Buffer**)malloc( size );
        g_BufferContext->m_Version = 0;

        memset(g_BufferContext->m_Buffers, 0, size);
    }

    void DeleteContext()
    {
        if( g_BufferContext )
        {
            free( g_BufferContext->m_Buffers );
            free( (void*)g_BufferContext );
        }
        g_BufferContext = 0;
    }

    static uint32_t FindEmptySlot(BufferContext* ctx)
    {
        for( uint32_t i = 0; i < ctx->m_Capacity; ++i )
        {
            if( ctx->m_Buffers[i] == 0 )
            {
                return i;
            }
        }
        return 0xFFFFFFFF;
    }

    static void GrowPool(BufferContext* ctx, uint32_t count)
    {
        uint32_t new_capacity = ctx->m_Capacity + count;
        ctx->m_Buffers = (Buffer**)realloc(g_BufferContext->m_Buffers, new_capacity * sizeof(Buffer**));
        for( uint32_t i = ctx->m_Capacity; i < new_capacity; ++i )
        {
            ctx->m_Buffers[i] = 0;
        }
        ctx->m_Capacity = new_capacity;
    }

    static Buffer* GetBuffer(BufferContext* ctx, HBuffer hbuffer)
    {
        if(hbuffer == 0) {
            return 0;
        }
        uint32_t version = hbuffer >> 16;
        uint32_t index = hbuffer & 0xFFFF;
        Buffer* b = ctx->m_Buffers[index];
        if (b == 0 || version != b->m_Version)
        {
            return 0;
        }
        return b;
    }

    static HBuffer SetBuffer(BufferContext* ctx, uint32_t index, Buffer* buffer)
    {
        assert( index < ctx->m_Capacity );
        assert( ctx->m_Buffers[index] == 0 );
        if( ctx->m_Version == 0 ) {
            ctx->m_Version++;
        }
        uint16_t version = ctx->m_Version++;
        ctx->m_Buffers[index] = buffer;
        buffer->m_Version = version;
        return version << 16 | index;
    }

    static void FreeBuffer(BufferContext* ctx, HBuffer hbuffer)
    {
        uint16_t version = hbuffer >> 16;
        Buffer* b = ctx->m_Buffers[hbuffer & 0xffff];
        if (version != b->m_Version)
        {
            dmLogError("Stale buffer handle when freeing buffer");
            return;
        }
        ctx->m_Buffers[hbuffer & 0xffff] = 0;
        dmMemory::AlignedFree(b);
    }

    /////////////////////////////////////////////////////////////////
    // Buffer instances

    uint32_t GetSizeForValueType(ValueType value_type)
    {
        switch (value_type) {
            case VALUE_TYPE_UINT8:      return sizeof(uint8_t);
            case VALUE_TYPE_UINT16:     return sizeof(uint16_t);
            case VALUE_TYPE_UINT32:     return sizeof(uint32_t);
            case VALUE_TYPE_UINT64:     return sizeof(uint64_t);
            case VALUE_TYPE_INT8:       return sizeof(int8_t);
            case VALUE_TYPE_INT16:      return sizeof(int16_t);
            case VALUE_TYPE_INT32:      return sizeof(int32_t);
            case VALUE_TYPE_INT64:      return sizeof(int64_t);
            case VALUE_TYPE_FLOAT32:    return sizeof(float);
            case VALUE_TYPE_FLOAT64:    return sizeof(double);
            default:
                break;
        }

        // Should never happen, need to implement all value types above.
        assert(0 && "Unknown value type!");
        return 0;
    }

#define _TOSTRING(_NAME) case _NAME: return #_NAME;
    const char* GetResultString(Result result)
    {
        switch(result)
        {
            _TOSTRING(RESULT_OK)
            _TOSTRING(RESULT_GUARD_INVALID)
            _TOSTRING(RESULT_ALLOCATION_ERROR)
            _TOSTRING(RESULT_BUFFER_INVALID)
            _TOSTRING(RESULT_BUFFER_SIZE_ERROR)
            _TOSTRING(RESULT_STREAM_SIZE_ERROR)
            _TOSTRING(RESULT_STREAM_MISSING)
            _TOSTRING(RESULT_STREAM_TYPE_MISMATCH)
            _TOSTRING(RESULT_STREAM_COUNT_MISMATCH)
            default: return "buffer.cpp: Unknown result";
        }

    }

    const char* GetValueTypeString(ValueType value)
    {
        switch(value)
        {
            _TOSTRING(VALUE_TYPE_UINT8)
            _TOSTRING(VALUE_TYPE_UINT16)
            _TOSTRING(VALUE_TYPE_UINT32)
            _TOSTRING(VALUE_TYPE_UINT64)
            _TOSTRING(VALUE_TYPE_INT8)
            _TOSTRING(VALUE_TYPE_INT16)
            _TOSTRING(VALUE_TYPE_INT32)
            _TOSTRING(VALUE_TYPE_INT64)
            _TOSTRING(VALUE_TYPE_FLOAT32)
            _TOSTRING(VALUE_TYPE_FLOAT64)
            default: return "buffer.cpp: Unknown value type";
        }
    }
#undef _TOSTRING

    static void WriteGuard(void* ptr)
    {
        memcpy(ptr, GUARD_VALUES, GUARD_SIZE);
    }

    static bool ValidateStream(Buffer* buffer, const Buffer::Stream& stream)
    {
        const uintptr_t stream_buffer = (uintptr_t)buffer->m_Data + stream.m_Offset;
        uint32_t stream_size = stream.m_ValueCount * buffer->m_NumElements * GetSizeForValueType((dmBuffer::ValueType)stream.m_ValueType);
        return (memcmp((void*)(stream_buffer + stream_size), GUARD_VALUES, GUARD_SIZE) == 0);
    }

    Result ValidateBuffer(HBuffer hbuffer)
    {
        Buffer* buffer = GetBuffer(g_BufferContext, hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }

        // Check guards
        for (uint8_t i = 0; i < buffer->m_NumStreams; ++i) {
            if (!ValidateStream(buffer, buffer->m_Streams[i])) {
                return RESULT_GUARD_INVALID;
            }
        }
        return RESULT_OK;
    }

    static void CreateStreams(Buffer* buffer, const StreamDeclaration* streams_decl)
    {
        uint32_t num_elements = buffer->m_NumElements;
        uintptr_t data_start = (uintptr_t)buffer->m_Data;
        uintptr_t ptr = data_start;
        for (uint8_t i = 0; i < buffer->m_NumStreams; ++i) {
            const StreamDeclaration& decl = streams_decl[i];
            ptr = DM_ALIGN(ptr, ADDR_ALIGNMENT);

            dmBuffer::Buffer::Stream& stream = buffer->m_Streams[i];
            stream.m_Name       = decl.m_Name;
            stream.m_ValueType  = decl.m_ValueType;
            stream.m_ValueCount = decl.m_ValueCount;
            stream.m_Offset     = ptr - data_start;

            // Write guard bytes after stream data
            uint32_t stream_size = num_elements * decl.m_ValueCount * GetSizeForValueType((dmBuffer::ValueType)decl.m_ValueType);
            ptr += stream_size;
            WriteGuard((void*)ptr);
            ptr += GUARD_SIZE;
        }
    }

    bool IsBufferValid(HBuffer hbuffer)
    {
        Buffer* buffer = GetBuffer(g_BufferContext, hbuffer);
        return buffer != 0;
    }

    Result Create(uint32_t num_elements, const StreamDeclaration* streams_decl, uint8_t streams_decl_count, HBuffer* out_buffer)
    {
        BufferContext* ctx = g_BufferContext;
        assert(ctx && "Buffer context not initialized");

        if (!streams_decl || !out_buffer) {
            return RESULT_ALLOCATION_ERROR;
        }

        // Verify element count and streams count
        if (num_elements == 0) {
            return RESULT_BUFFER_SIZE_ERROR;
        } else if (streams_decl_count == 0) {
            return RESULT_STREAM_SIZE_ERROR;
        }

        // Calculate total data allocation size needed
        uint32_t header_size = sizeof(Buffer) + sizeof(Buffer::Stream)*streams_decl_count;
        uint32_t buffer_size = header_size;
        for (uint8_t i = 0; i < streams_decl_count; ++i) {
            const StreamDeclaration& decl = streams_decl[i];

            // Make sure each stream is aligned
            buffer_size = DM_ALIGN(buffer_size, ADDR_ALIGNMENT);
            assert(buffer_size % ADDR_ALIGNMENT == 0);

            // Calculate size of stream buffer
            uint32_t stream_size = num_elements * decl.m_ValueCount * GetSizeForValueType(decl.m_ValueType);
            if (!stream_size) {
                return RESULT_STREAM_SIZE_ERROR;
            }

            // Add current total stream size to total buffer size
            buffer_size += stream_size + GUARD_SIZE;
        }

        if (buffer_size == header_size) {
            return RESULT_BUFFER_SIZE_ERROR;
        }

        uint32_t index = FindEmptySlot(ctx);
        if( index == 0xFFFFFFFF )
        {
            GrowPool(ctx, 64);
            index = FindEmptySlot(ctx);
            if( index == 0xFFFFFFFF ) {
                return RESULT_ALLOCATION_ERROR;
            }
        }

        // Allocate buffer to fit Buffer-struct, Stream-array and buffer data
        void* data_block = 0x0;
        dmMemory::Result r = dmMemory::AlignedMalloc((void**)&data_block, ADDR_ALIGNMENT, buffer_size);
        if (r != dmMemory::RESULT_OK) {
            return RESULT_ALLOCATION_ERROR;
        }

        // Get buffer from data block start
        Buffer* buffer = (Buffer*)data_block;
        buffer->m_NumElements = num_elements;
        buffer->m_NumStreams = streams_decl_count;
        buffer->m_Streams = (Buffer::Stream*)((uintptr_t)data_block + sizeof(Buffer));
        buffer->m_Data = (void*)((uintptr_t)buffer->m_Streams + sizeof(Buffer::Stream) * streams_decl_count);

        CreateStreams(buffer, streams_decl);

        *out_buffer = SetBuffer(ctx, index, buffer);
        return RESULT_OK;
    }

    void Destroy(HBuffer hbuffer)
    {
        if (hbuffer) {
            FreeBuffer(g_BufferContext, hbuffer);
        }
    }

    uint32_t GetNumStreams(HBuffer hbuffer)
    {
        Buffer* buffer = GetBuffer(g_BufferContext, hbuffer);
        return buffer ? buffer->m_NumStreams : 0xFFFFFFFF;
    }

    Result GetStreamName(HBuffer hbuffer, uint32_t index, dmhash_t* stream_name)
    {
        Buffer* buffer = GetBuffer(g_BufferContext, hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }
        if (index >= buffer->m_NumStreams) {
            return RESULT_STREAM_MISSING;
        }
        *stream_name = buffer->m_Streams[index].m_Name;
        return RESULT_OK;
    }

    static Buffer::Stream* GetStream(Buffer* buffer, dmhash_t stream_name)
    {
        if (!buffer) {
            return 0x0;
        }

        for (uint8_t i = 0; i < buffer->m_NumStreams; ++i) {
            Buffer::Stream* stream = &buffer->m_Streams[i];
            if (stream_name == stream->m_Name) {
                return stream;
            }
        }

        return 0x0;
    }

    Result CheckStreamType(HBuffer hbuffer, dmhash_t stream_name, dmBuffer::ValueType type, uint32_t type_count)
    {
        Buffer* buffer = GetBuffer(g_BufferContext, hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }

        // Get stream
        Buffer::Stream* stream = GetStream(buffer, stream_name);
        if (stream == 0x0) {
            return RESULT_STREAM_MISSING;
        }

        // Validate expected type and value count
        if (stream->m_ValueType != type) {
            return dmBuffer::RESULT_STREAM_TYPE_MISMATCH;
        } else if (stream->m_ValueCount != type_count) {
            return dmBuffer::RESULT_STREAM_COUNT_MISMATCH;
        }
        return RESULT_OK;
    }

    Result GetStream(HBuffer hbuffer, dmhash_t stream_name, void **out_stream, uint32_t *out_size)
    {
        Buffer* buffer = GetBuffer(g_BufferContext, hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }

        // Get stream
        Buffer::Stream* stream = GetStream(buffer, stream_name);
        if (stream == 0x0) {
            return RESULT_STREAM_MISSING;
        }

        // Validate guards
        if (!dmBuffer::ValidateStream(buffer, *stream)) {
            return RESULT_GUARD_INVALID;
        }

        *out_size = buffer->m_NumElements * stream->m_ValueCount * GetSizeForValueType((dmBuffer::ValueType)stream->m_ValueType);
        *out_stream = (void*)((uintptr_t)buffer->m_Data + stream->m_Offset);

        return RESULT_OK;
    }


    Result GetBytes(HBuffer hbuffer, void** out_buffer, uint32_t* out_size)
    {
        Buffer* buffer = GetBuffer(g_BufferContext, hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }

        Buffer::Stream* stream = &buffer->m_Streams[0];

        // Validate guards
        if (!dmBuffer::ValidateStream(buffer, *stream)) {
            return RESULT_GUARD_INVALID;
        }

        uint32_t stream_size = stream->m_ValueCount * buffer->m_NumElements * GetSizeForValueType((dmBuffer::ValueType)stream->m_ValueType);

        *out_size = stream_size;
        *out_buffer = (void*)((uintptr_t)buffer->m_Data + stream->m_Offset);

        return RESULT_OK;
    }

    Result GetElementCount(HBuffer hbuffer, uint32_t* out_element_count)
    {
        Buffer* buffer = GetBuffer(g_BufferContext, hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }
        *out_element_count = buffer->m_NumElements;
        return RESULT_OK;
    }

    Result GetStreamType(HBuffer hbuffer, dmhash_t stream_name, dmBuffer::ValueType* type, uint32_t* type_count)
    {
        Buffer* buffer = GetBuffer(g_BufferContext, hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }

        Buffer::Stream* stream = GetStream(buffer, stream_name);
        if (stream == 0x0) {
            return RESULT_STREAM_MISSING;
        }

        *type = (dmBuffer::ValueType)stream->m_ValueType;
        *type_count = stream->m_ValueCount;
        return RESULT_OK;
    }
}
