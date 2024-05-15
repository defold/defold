// Copyright 2020-2024 The Defold Foundation
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


#include <dlib/hash.h>
#include <dmsdk/dlib/buffer.h>

#include <dlib/log.h>
#include <dlib/memory.h>
#include <dlib/math.h>
#include <dmsdk/dlib/vmath.h>
#include <dlib/array.h>

#include <dlib/opaque_handle_container.h>

#include <string.h>
#include <assert.h>
#include <new>

#if defined(_WIN32)
#include <malloc.h>
#endif

#include <stdio.h>

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
            uint32_t    m_Offset;       // Offset from start of data segment (for non-interleaved buffers, gives ~4gb addressable space)
            uint8_t     m_ValueType;
            uint8_t     m_ValueCount;
        };

        struct MetaData
        {
            dmhash_t    m_Name;
            uint8_t     m_ValueType;
            uint8_t     m_ValueCount;
            void*       m_Data;
        };

        void*    m_Data;            // All stream data, including guard bytes after each stream and 16 byte aligned.
        Stream*  m_Streams;
        dmArray<MetaData*> m_MetaDataArray;
        uint32_t m_Stride;          // The struct size (in bytes)
        uint32_t m_Count;           // The number of "structs" in the buffer (e.g. vertex count)
        uint16_t m_ContentVersion;  // A running number, which user can use to signal content changes
        uint8_t  m_NumStreams;
    };

    typedef dmOpaqueHandleContainer<Buffer> BufferContext;

    static BufferContext* g_BufferContext = 0;

    /////////////////////////////////////////////////////////////////
    // Buffer context

    void NewContext()
    {
        const uint32_t initial_capacity = 128;
        g_BufferContext = new BufferContext(initial_capacity);
    }

    void DeleteContext()
    {
        delete g_BufferContext;
        g_BufferContext = 0;
    }

    static void FreeMetadata(Buffer* buffer)
    {
        for (uint32_t i=0; i<buffer->m_MetaDataArray.Size(); i++) {
            Buffer::MetaData* metadata = buffer->m_MetaDataArray[i];
            free(metadata->m_Data);
            free(metadata);
        }
        buffer->m_MetaDataArray.SetSize(0);
    }

    static void FreeBuffer(BufferContext* ctx, HBuffer hbuffer)
    {
        if (!IsBufferValid(hbuffer))
        {
            dmLogError("Invalid buffer when freeing buffer");
            return;
        }

        Buffer* b = ctx->Get(hbuffer);
        if (b == 0x0)
        {
            dmLogError("Stale buffer handle when freeing buffer");
            return;
        }

        ctx->Release(hbuffer);

        FreeMetadata(b);
        b->m_MetaDataArray.~dmArray(); // b->m_MetaDataArray was initialized with "placement new" operator. We have to destroy it manually

        dmMemory::AlignedFree(b);
    }

    /////////////////////////////////////////////////////////////////
    // Buffer instances


    uint32_t GetStructSize(HBuffer hbuffer)
    {
        Buffer* buffer = g_BufferContext->Get(hbuffer);
        if (!buffer) {
            return 0;
        }
        return buffer->m_Stride;
    }

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
            default:
                break;
        }

        // Should never happen, need to implement all value types above.
        assert(0 && "Unknown value type!");
        return 0;
    }

    // Calculates the size of a struct, specified by the stream declarations.
    // The sizes and offsets should mimic the sizes and offsets generated by C++
    Result CalcStructSize(uint32_t num_streams, const StreamDeclaration* streams, uint32_t* out_size, uint32_t* offsets)
    {
        uint32_t biggestsize = 1;
        uint32_t size = 0;
        for (uint32_t i = 0; i < num_streams; ++i) {
            if (streams[i].m_Count == 0) {
                return RESULT_STREAM_SIZE_ERROR;
            }

            uint32_t type_size = GetSizeForValueType(streams[i].m_Type);
            if (type_size > biggestsize) {
                biggestsize = type_size;
            }

            size = DM_ALIGN(size, type_size);
            if (offsets)
                offsets[i] = size;

            size += streams[i].m_Count * type_size;
        }

        size = DM_ALIGN(size, biggestsize);
        *out_size = size;
        return size != 0 ? RESULT_OK : RESULT_STREAM_SIZE_ERROR;
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
            _TOSTRING(RESULT_METADATA_INVALID)
            _TOSTRING(RESULT_METADATA_MISSING)
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
            default: return "buffer.cpp: Unknown value type";
        }
    }
#undef _TOSTRING

#if !defined(NDEBUG)
    static void WriteGuard(void* ptr)
    {
        memcpy(ptr, GUARD_VALUES, GUARD_SIZE);
    }

    static bool ValidateGuard(void* ptr)
    {
        return memcmp(ptr, GUARD_VALUES, GUARD_SIZE) == 0;
    }

    static Result ValidateBuffer(Buffer* buffer)
    {
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }

        // Check guard
        uint8_t* ptr = (uint8_t*)buffer->m_Data + (buffer->m_Count * buffer->m_Stride);
        if (!ValidateGuard(ptr)) {
                return RESULT_GUARD_INVALID;
        }
        return RESULT_OK;
    }

    Result ValidateBuffer(HBuffer hbuffer)
    {
        return ValidateBuffer(g_BufferContext->Get(hbuffer));
    }
#else
    static inline void WriteGuard(void* ptr)
    {
        (void*)ptr;
    }

    static inline Result ValidateBuffer(Buffer* buffer)
    {
        (void*)buffer;
        return RESULT_OK;
    }

    Result ValidateBuffer(HBuffer hbuffer)
    {
        (void*)hbuffer;
        return RESULT_OK;
    }
#endif

    static void CreateStreamsInterleaved(Buffer* buffer, const StreamDeclaration* streams_decl, const uint32_t* offsets)
    {
        for (uint8_t i = 0; i < buffer->m_NumStreams; ++i) {
            const StreamDeclaration& decl = streams_decl[i];
            dmBuffer::Buffer::Stream& stream = buffer->m_Streams[i];
            stream.m_Name       = decl.m_Name;
            stream.m_ValueType  = decl.m_Type;
            stream.m_ValueCount = decl.m_Count;
            stream.m_Offset     = offsets[i];
        }

        // Write guard bytes after payload data
        uint8_t* ptr = (uint8_t*)buffer->m_Data + (buffer->m_Count * buffer->m_Stride);
        WriteGuard((void*)ptr);
    }

    bool IsBufferValid(HBuffer hbuffer)
    {
        Buffer* buffer = g_BufferContext->Get(hbuffer);
        return buffer != 0 && ValidateBuffer(buffer) == RESULT_OK;
    }

    Result Create(uint32_t count, const StreamDeclaration* streams_decl, uint8_t streams_decl_count, HBuffer* out_buffer)
    {
        BufferContext* ctx = g_BufferContext;
        assert(ctx && "Buffer context not initialized");

        if (!streams_decl || !out_buffer) {
            return RESULT_ALLOCATION_ERROR;
        }

        // Verify streams count
        if (streams_decl_count == 0) {
            return RESULT_STREAM_SIZE_ERROR;
        }

        // Calculate total data allocation size needed
        uint32_t header_size = sizeof(Buffer) + sizeof(Buffer::Stream)*streams_decl_count;
        uint32_t buffer_size = header_size;

        // Interleaved streams
        uint32_t struct_size = 0;
        uint32_t* offsets = (uint32_t*)alloca(streams_decl_count * sizeof(uint32_t));
        dmBuffer::Result res = CalcStructSize(streams_decl_count, streams_decl, &struct_size, offsets);
        if (res != RESULT_OK) {
            return res;
        }

        // Make sure the data is aligned at the start
        buffer_size = DM_ALIGN(buffer_size, ADDR_ALIGNMENT);
        assert(buffer_size % ADDR_ALIGNMENT == 0);

        buffer_size += struct_size * count;

        // Add some guard bytes at the end
        buffer_size += GUARD_SIZE;

        if (buffer_size == header_size) {
            return RESULT_BUFFER_SIZE_ERROR;
        }

        // TODO: Perhaps implement as an index pool
        if (ctx->Full())
        {
            if (!ctx->Allocate(64))
            {
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
        buffer->m_Count = count;
        buffer->m_NumStreams = streams_decl_count;
        buffer->m_Streams = (Buffer::Stream*)((uintptr_t)data_block + sizeof(Buffer));
        buffer->m_Data = (void*)((uintptr_t)data_block + header_size);
        buffer->m_Stride = struct_size;
        buffer->m_ContentVersion = 0;
        new (&buffer->m_MetaDataArray) dmArray<Buffer::MetaData*>();

        CreateStreamsInterleaved(buffer, streams_decl, offsets);

        *out_buffer = ctx->Put(buffer);

        assert(*out_buffer != INVALID_OPAQUE_HANDLE);

        return RESULT_OK;
    }

    Result Clone(const HBuffer src_buffer, HBuffer* out_buffer)
    {
        Buffer* buffer = g_BufferContext->Get(src_buffer);
        Result res = dmBuffer::ValidateBuffer(buffer);
        if (res != RESULT_OK)
        {
            return res;
        }

        dmBuffer::StreamDeclaration* streams_decl = (dmBuffer::StreamDeclaration*) alloca(buffer->m_NumStreams * sizeof(dmBuffer::StreamDeclaration));
        for (int i = 0; i < buffer->m_NumStreams; ++i)
        {
            streams_decl[i].m_Name  = buffer->m_Streams[i].m_Name;
            streams_decl[i].m_Type  = (ValueType) buffer->m_Streams[i].m_ValueType;
            streams_decl[i].m_Count = buffer->m_Streams[i].m_ValueCount;
        }

        HBuffer dst_buffer;
        res = dmBuffer::Create(buffer->m_Count, streams_decl, buffer->m_NumStreams, &dst_buffer);
        if (res != RESULT_OK)
        {
            return res;
        }

        dmBuffer::Copy(dst_buffer, src_buffer);

        // Clone the meta data entries
        for (uint32_t i = 0; i < buffer->m_MetaDataArray.Size(); i++)
        {
            res = SetMetaData(dst_buffer,
                buffer->m_MetaDataArray[i]->m_Name,
                buffer->m_MetaDataArray[i]->m_Data,
                buffer->m_MetaDataArray[i]->m_ValueCount,
                (ValueType) buffer->m_MetaDataArray[i]->m_ValueType);
            assert(res == RESULT_OK);
        }

        *out_buffer = dst_buffer;
        return RESULT_OK;
    }

    Result GetStreamOffset(HBuffer buffer_handle, uint32_t index, uint32_t* offset)
    {
        Buffer* buffer = g_BufferContext->Get(buffer_handle);
        *offset = buffer->m_Streams[index].m_Offset;
        return RESULT_OK;
    }

    Result Copy(const HBuffer dst_buffer_handle, const HBuffer src_buffer_handle)
    {
        const Buffer* dst_buffer = g_BufferContext->Get(dst_buffer_handle);
        const Buffer* src_buffer = g_BufferContext->Get(src_buffer_handle);

        // Verify stream declaration is 1:1
        if (src_buffer->m_NumStreams != dst_buffer->m_NumStreams) {
            return RESULT_STREAM_COUNT_MISMATCH;
        }

        for (uint8_t i = 0; i < dst_buffer->m_NumStreams; ++i)
        {
            const Buffer::Stream& dst_stream = dst_buffer->m_Streams[i];
            const Buffer::Stream& src_stream = src_buffer->m_Streams[i];
            if (src_stream.m_Name != dst_stream.m_Name ||
                src_stream.m_Offset != dst_stream.m_Offset ||
                src_stream.m_ValueType != dst_stream.m_ValueType ||
                src_stream.m_ValueCount != dst_stream.m_ValueCount)
            {
                dmLogError("Stream mismatch: src(name: %s, offset: %u, type: %s, count: %u) != dst(name: %s, offset: %u, type: %s, count: %u)",
                    dmHashReverseSafe64(src_stream.m_Name), src_stream.m_Offset, GetValueTypeString((dmBuffer::ValueType)src_stream.m_ValueType), src_stream.m_ValueCount,
                    dmHashReverseSafe64(dst_stream.m_Name), dst_stream.m_Offset, GetValueTypeString((dmBuffer::ValueType)dst_stream.m_ValueType), dst_stream.m_ValueCount);
                return RESULT_STREAM_MISMATCH;
            }
        }

        // Verify destination buffer has enough of element space
        if (src_buffer->m_Count > dst_buffer->m_Count) {
            return RESULT_BUFFER_SIZE_ERROR;
        }

        // Get buffer pointers for dst and src
        void* dst_bytes = 0x0;
        uint32_t dst_size = 0;
        dmBuffer::Result r = dmBuffer::GetBytes(dst_buffer_handle, &dst_bytes, &dst_size);
        if (r != RESULT_OK) {
            return r;
        }
        void* src_bytes = 0x0;
        uint32_t src_size = 0;
        r = dmBuffer::GetBytes(src_buffer_handle, &src_bytes, &src_size);
        if (r != RESULT_OK) {
            return r;
        }

        // Copy memory
        memcpy(dst_bytes, src_bytes, src_size);

        return RESULT_OK;
    }

    void Destroy(HBuffer hbuffer)
    {
        if (hbuffer) {
            FreeBuffer(g_BufferContext, hbuffer);
        }
    }

    Result GetNumStreams(HBuffer hbuffer, uint32_t* num_streams)
    {
        Buffer* buffer = g_BufferContext->Get(hbuffer);
        if (buffer) {
            *num_streams = buffer->m_NumStreams;
        }
        return buffer ? RESULT_OK : RESULT_BUFFER_INVALID;
    }

    Result GetStreamName(HBuffer hbuffer, uint32_t index, dmhash_t* stream_name)
    {
        Buffer* buffer = g_BufferContext->Get(hbuffer);
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
        for (uint8_t i = 0; i < buffer->m_NumStreams; ++i) {
            Buffer::Stream* stream = &buffer->m_Streams[i];
            if (stream_name == stream->m_Name) {
                return stream;
            }
        }
        return 0x0;
    }

    Result GetStream(HBuffer hbuffer, dmhash_t stream_name, void** out_stream, uint32_t* count, uint32_t* component_count, uint32_t* stride)
    {
        Buffer* buffer = g_BufferContext->Get(hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }

        // Get stream
        Buffer::Stream* stream = GetStream(buffer, stream_name);
        if (stream == 0x0) {
            return RESULT_STREAM_MISSING;
        }

        // Validate guards
        if (RESULT_OK != dmBuffer::ValidateBuffer(buffer)) {
            return RESULT_GUARD_INVALID;
        }

        *out_stream = (void*)((uintptr_t)buffer->m_Data + stream->m_Offset);
        if (count)
            *count = buffer->m_Count;
        if (component_count)
            *component_count = stream->m_ValueCount;
        if (stride)
            *stride = buffer->m_Stride / GetSizeForValueType((dmBuffer::ValueType)stream->m_ValueType);
        return RESULT_OK;
    }

    Result GetBytes(HBuffer hbuffer, void** out_buffer, uint32_t* out_size)
    {
        Buffer* buffer = g_BufferContext->Get(hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }

        // Validate guards
        if (RESULT_OK != dmBuffer::ValidateBuffer(buffer)) {
            return RESULT_GUARD_INVALID;
        }

        *out_size = buffer->m_Stride * buffer->m_Count;
        *out_buffer = (void*)buffer->m_Data;

        return RESULT_OK;
    }

    Result GetCount(HBuffer hbuffer, uint32_t* out_element_count)
    {
        Buffer* buffer = g_BufferContext->Get(hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }
        *out_element_count = buffer->m_Count;
        return RESULT_OK;
    }

    Result GetStreamType(HBuffer hbuffer, dmhash_t stream_name, dmBuffer::ValueType* type, uint32_t* type_count)
    {
        Buffer* buffer = g_BufferContext->Get(hbuffer);
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

    Result GetContentVersion(HBuffer hbuffer, uint32_t* version)
    {
        Buffer* buffer = g_BufferContext->Get(hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }
        *version = (uint32_t)buffer->m_ContentVersion;
        return RESULT_OK;
    }

    Result UpdateContentVersion(HBuffer hbuffer)
    {
        Buffer* buffer = g_BufferContext->Get(hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }
        buffer->m_ContentVersion++;
        return RESULT_OK;
    }

    // Returns a pointer to the metadata item or 0 if not found.  Assumes buffer is valid.
    static Buffer::MetaData* FindMetaDataItem(Buffer* buffer, dmhash_t name_hash) {
        dmArray<Buffer::MetaData*>& arr = buffer->m_MetaDataArray;
        for (uint32_t  i=0; i<arr.Size(); i++) {
            if (arr[i]->m_Name == name_hash) {
                return arr[i];
            }
        }
        return 0; // nothing found
    }


    Result GetMetaData(HBuffer hbuffer, dmhash_t name_hash, void** data, uint32_t* count, ValueType* type) {
        Buffer* buffer = g_BufferContext->Get(hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }

        Buffer::MetaData* item = FindMetaDataItem(buffer, name_hash);
        if (!item) {
            return RESULT_METADATA_MISSING;
        }

        *count = item->m_ValueCount;
        *type = (ValueType)item->m_ValueType;
        *data = item->m_Data;

        return RESULT_OK;
    }

    Result SetMetaData(HBuffer hbuffer, dmhash_t name_hash, const void* data, uint32_t count, ValueType type)
    {
        Buffer* buffer = g_BufferContext->Get(hbuffer);
        if (!buffer) {
            return RESULT_BUFFER_INVALID;
        }
        if (count == 0) {
            return RESULT_METADATA_INVALID;
        }

        Buffer::MetaData* item = FindMetaDataItem(buffer, name_hash);
        uint32_t values_block_size = GetSizeForValueType(type)*count; // do this once
        if (item) {
            // make sure the type and value count is right
            if (item->m_ValueCount != count || item->m_ValueType != type) {
                return RESULT_METADATA_INVALID;
            }
            memcpy(item->m_Data, data, values_block_size);
            return RESULT_OK;
        } else {
            dmArray<Buffer::MetaData*>& metadata_items = buffer->m_MetaDataArray;
            if (metadata_items.Full()) {
                metadata_items.OffsetCapacity(2);
            }
            item = (Buffer::MetaData*) malloc(sizeof(Buffer::MetaData));
            item->m_Name = name_hash;
            item->m_ValueCount = count;
            item->m_ValueType = type;
            item->m_Data = malloc(values_block_size);
            memcpy(item->m_Data, data, values_block_size);
            metadata_items.Push(item);
        }

        return RESULT_OK;
    }

}
