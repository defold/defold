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

#include "res_buffer.h"

#include <dlib/buffer.h>
#include <dlib/endian.h>
#include <dlib/log.h>

#include <stdlib.h>
#include <string.h>

namespace dmGameSystem
{
    // BUFC files are little-endian: magic[4], version:u16, streams:u16,
    // elements:u32, stride:u32; then streams of hash:u64, offset:u32,
    // type:u8, components:u8, padding:u16; then the interleaved payload.
    enum
    {
        BUFFER_FORMAT_VERSION = 1,
        BUFFER_FILE_HEADER_SIZE = 16,
        BUFFER_FILE_STREAM_SIZE = 16,
    };

    struct BufferPreloadData
    {
        uint32_t m_ElementCount;
        uint32_t m_Stride;
        uint32_t m_DataOffset;
        uint32_t m_StreamCount;
        // Followed by m_StreamCount StreamDeclarations, owned by this allocation.
    };

    static uint64_t ReadInteger(const uint8_t* data, uint32_t size)
    {
        uint64_t value = 0;
        for (uint32_t i = 0; i < size; ++i)
            value |= (uint64_t)data[i] << (i * 8);
        return value;
    }

    static dmResource::Result ParseBuffer(const void* data, uint32_t size, BufferPreloadData** out)
    {
        *out = 0;
        if (size < BUFFER_FILE_HEADER_SIZE || memcmp(data, "BUFC", 4) != 0)
            return dmResource::RESULT_INVALID_DATA;

        const uint8_t* bytes = (const uint8_t*)data;
        uint32_t version = ReadInteger(bytes + 4, 2);
        if (version != BUFFER_FORMAT_VERSION)
        {
            dmLogError("Unsupported buffer format version %u (expected %u).", version, BUFFER_FORMAT_VERSION);
            return dmResource::RESULT_INVALID_DATA;
        }
        uint32_t stream_count = ReadInteger(bytes + 6, 2);
        if (stream_count == 0 || stream_count > 255)
            return dmResource::RESULT_INVALID_DATA;

        uint32_t data_offset = BUFFER_FILE_HEADER_SIZE + stream_count * BUFFER_FILE_STREAM_SIZE;
        uint32_t element_count = ReadInteger(bytes + 8, 4);
        uint32_t stride = ReadInteger(bytes + 12, 4);
        if (data_offset > size || data_offset + (uint64_t)element_count * stride != size)
            return dmResource::RESULT_INVALID_DATA;

        BufferPreloadData* preload = (BufferPreloadData*)malloc(sizeof(BufferPreloadData) + stream_count * sizeof(dmBuffer::StreamDeclaration));
        if (!preload)
            return dmResource::RESULT_OUT_OF_MEMORY;

        dmBuffer::StreamDeclaration* streams = (dmBuffer::StreamDeclaration*)(preload + 1);
        uint32_t offsets[255];
        for (uint32_t i = 0; i < stream_count; ++i)
        {
            const uint8_t* stream = bytes + BUFFER_FILE_HEADER_SIZE + i * BUFFER_FILE_STREAM_SIZE;
            if (stream[12] >= dmBuffer::MAX_VALUE_TYPE_COUNT || stream[13] == 0)
            {
                free(preload);
                return dmResource::RESULT_INVALID_DATA;
            }
            streams[i].m_Name = ReadInteger(stream, 8);
            streams[i].m_Type = (dmBuffer::ValueType)stream[12];
            streams[i].m_Count = stream[13];
            streams[i].m_Flags = 0;
            streams[i].m_Reserved = 0;
        }

        uint32_t struct_size;
        dmBuffer::CalcStructSize(stream_count, streams, &struct_size, offsets);
        if (struct_size != stride)
        {
            free(preload);
            return dmResource::RESULT_INVALID_DATA;
        }
        for (uint32_t i = 0; i < stream_count; ++i)
        {
            const uint8_t* stream = bytes + BUFFER_FILE_HEADER_SIZE + i * BUFFER_FILE_STREAM_SIZE;
            if (ReadInteger(stream + 8, 4) != offsets[i])
            {
                free(preload);
                return dmResource::RESULT_INVALID_DATA;
            }
        }

        preload->m_ElementCount = element_count;
        preload->m_Stride = stride;
        preload->m_DataOffset = data_offset;
        preload->m_StreamCount = stream_count;
        *out = preload;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResBufferPreload(const dmResource::ResourcePreloadParams* params)
    {
        BufferPreloadData* preload;
        dmResource::Result result = ParseBuffer(params->m_Buffer, params->m_BufferSize, &preload);
        *params->m_PreloadData = preload;
        return result;
    }

    static dmResource::Result BuildBuffer(const void* data, const BufferPreloadData* preload, BufferResource* resource)
    {
        const dmBuffer::StreamDeclaration* streams = (const dmBuffer::StreamDeclaration*)(preload + 1);
        dmBuffer::Result result = dmBuffer::Create(preload->m_ElementCount, streams, preload->m_StreamCount, &resource->m_Buffer);
        if (result != dmBuffer::RESULT_OK)
        {
            dmLogError("Unable to create buffer (%d)", result);
            return result == dmBuffer::RESULT_ALLOCATION_ERROR ? dmResource::RESULT_OUT_OF_MEMORY : dmResource::RESULT_INVALID_DATA;
        }

        void* bytes;
        uint32_t size;
        dmBuffer::GetBytes(resource->m_Buffer, &bytes, &size);
        memcpy(bytes, (const uint8_t*)data + preload->m_DataOffset, size);
#if DM_ENDIAN == DM_ENDIAN_BIG
        // Current targets are little-endian. Convert scalar bytes for big-endian hosts.
        for (uint32_t s = 0; s < preload->m_StreamCount; ++s)
        {
            uint32_t value_size = dmBuffer::GetSizeForValueType(streams[s].m_Type);
            uint32_t offset;
            dmBuffer::GetStreamOffset(resource->m_Buffer, s, &offset);
            for (uint32_t i = 0; i < preload->m_ElementCount; ++i)
            {
                for (uint32_t c = 0; c < streams[s].m_Count; ++c)
                {
                    uint8_t* value = (uint8_t*)bytes + i * preload->m_Stride + offset + c * value_size;
                    for (uint32_t b = 0; b < value_size / 2; ++b)
                    {
                        uint8_t temp = value[b];
                        value[b] = value[value_size - b - 1];
                        value[value_size - b - 1] = temp;
                    }
                }
            }
        }
#endif
        resource->m_ElementCount = preload->m_ElementCount;
        resource->m_Stride = preload->m_Stride;
        dmBuffer::GetContentVersion(resource->m_Buffer, &resource->m_Version);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResBufferCreate(const dmResource::ResourceCreateParams* params)
    {
        BufferPreloadData* preload = (BufferPreloadData*)params->m_PreloadData;
        if (!preload)
            return dmResource::RESULT_INVALID_DATA;

        BufferResource* resource = new BufferResource();
        memset(resource, 0, sizeof(BufferResource));
        resource->m_NameHash = dmHashString64(params->m_Filename);
        dmResource::Result result = BuildBuffer(params->m_Buffer, preload, resource);
        free(preload);
        if (result != dmResource::RESULT_OK)
        {
            delete resource;
            return result;
        }
        ResourceDescriptorSetResource(params->m_Resource, resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResBufferDestroy(const dmResource::ResourceDestroyParams* params)
    {
        BufferResource* resource = (BufferResource*)ResourceDescriptorGetResource(params->m_Resource);
        dmBuffer::Destroy(resource->m_Buffer);
        delete resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResBufferRecreate(const dmResource::ResourceRecreateParams* params)
    {
        BufferPreloadData* preload;
        dmResource::Result result = ParseBuffer(params->m_Buffer, params->m_BufferSize, &preload);
        if (result != dmResource::RESULT_OK)
            return result;

        BufferResource replacement = {};
        result = BuildBuffer(params->m_Buffer, preload, &replacement);
        free(preload);
        if (result != dmResource::RESULT_OK)
            return result;

        BufferResource* resource = (BufferResource*)ResourceDescriptorGetResource(params->m_Resource);
        dmBuffer::Destroy(resource->m_Buffer);
        resource->m_Buffer = replacement.m_Buffer;
        resource->m_ElementCount = replacement.m_ElementCount;
        resource->m_Stride = replacement.m_Stride;
        dmBuffer::UpdateContentVersion(resource->m_Buffer);
        return dmResource::RESULT_OK;
    }
}
