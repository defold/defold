#include <dmsdk/dlib/buffer.h>

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
        uint16_t m_RefCount;
        uint8_t  m_NumStreams;
    };

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

    static bool ValidateStream(HBuffer buffer, const Buffer::Stream& stream)
    {
        const uintptr_t stream_buffer = (uintptr_t)buffer->m_Data + stream.m_Offset;
        uint32_t stream_size = stream.m_ValueCount * buffer->m_NumElements * GetSizeForValueType((dmBuffer::ValueType)stream.m_ValueType);
        return (memcmp((void*)(stream_buffer + stream_size), GUARD_VALUES, GUARD_SIZE) == 0);
    }

    Result ValidateBuffer(HBuffer buffer)
    {
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

    static void CreateStreams(HBuffer buffer, const StreamDeclaration* streams_decl)
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

    Result Allocate(uint32_t num_elements, const StreamDeclaration* streams_decl, uint8_t streams_decl_count, HBuffer* out_buffer)
    {
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

        // Allocate buffer to fit Buffer-struct, Stream-array and buffer data
        void* data_block = 0x0;
        dmMemory::Result r = dmMemory::AlignedMalloc((void**)&data_block, ADDR_ALIGNMENT, buffer_size);
        if (r != dmMemory::RESULT_OK) {
            return RESULT_ALLOCATION_ERROR;
        }

        // Get buffer from data block start
        HBuffer buffer = (Buffer*)data_block;
        buffer->m_NumElements = num_elements;
        buffer->m_NumStreams = streams_decl_count;
        buffer->m_Streams = (Buffer::Stream*)((uintptr_t)data_block + sizeof(Buffer));
        buffer->m_Data = (void*)((uintptr_t)buffer->m_Streams + sizeof(Buffer::Stream) * streams_decl_count);

        CreateStreams(buffer, streams_decl);

        *out_buffer = buffer;
        return RESULT_OK;
    }

    void Free(HBuffer buffer)
    {
        if (buffer) {
            dmMemory::AlignedFree(buffer);
        }
    }

    uint32_t GetNumStreams(dmBuffer::HBuffer buffer)
    {
        return buffer ? buffer->m_NumStreams : 0xFFFFFFFF;
    }

    Result GetStreamName(dmBuffer::HBuffer buffer, uint32_t index, dmhash_t* stream_name)
    {
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }
        if (index >= buffer->m_NumStreams) {
            return RESULT_STREAM_MISSING;
        }
        *stream_name = buffer->m_Streams[index].m_Name;
        return RESULT_OK;
    }

    static Buffer::Stream* GetStream(HBuffer buffer, dmhash_t stream_name)
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

    Result CheckStreamType(HBuffer buffer, dmhash_t stream_name, dmBuffer::ValueType type, uint32_t type_count)
    {
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

    Result GetStream(HBuffer buffer, dmhash_t stream_name, void **out_stream, uint32_t *out_size)
    {
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


    Result GetBytes(HBuffer buffer, void** out_buffer, uint32_t* out_size)
    {
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

    Result GetElementCount(HBuffer buffer, uint32_t* out_element_count)
    {
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }
        *out_element_count = buffer->m_NumElements;
        return RESULT_OK;
    }

    Result GetStreamType(HBuffer buffer, dmhash_t stream_name, dmBuffer::ValueType* type, uint32_t* type_count)
    {
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
