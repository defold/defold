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

#include "modelbuffers.h"

#include <assert.h>
#include <string.h>

namespace dmGameSystem
{
    uint64_t AlignModelBufferOffset(uint64_t value, uint32_t alignment)
    {
        return (value + alignment - 1) & ~(uint64_t)(alignment - 1);
    }

    static bool HasModelBuffersHeader(const ModelBuffersHeader* header, uint32_t size)
    {
        return size >= sizeof(ModelBuffersHeader) && memcmp(header->m_Magic, MODEL_BUFFERS_MAGIC, sizeof(header->m_Magic)) == 0;
    }

    bool ValidateModelBuffers(const ModelBuffersHeader* file_header, uint32_t size)
    {
        if (!HasModelBuffersHeader(file_header, size))
        {
            return false;
        }

        assert(((uintptr_t)file_header & (sizeof(uint32_t) - 1)) == 0);
        const ModelBuffersHeader& header = *file_header;
        const ModelBufferDescriptor* ranges = (const ModelBufferDescriptor*)(&header + 1);
        if (header.m_Version != MODEL_BUFFERS_VERSION || header.m_Compression != 0)
        {
            return false;
        }

        uint64_t descriptor_end = sizeof(ModelBuffersHeader) + (uint64_t)header.m_RangeCount * sizeof(ModelBufferDescriptor);
        uint64_t vertex_payload_end = (uint64_t)header.m_VertexOffset + header.m_VertexSize;
        uint64_t index_payload_end = (uint64_t)header.m_IndexOffset + header.m_IndexSize;
        uint64_t decoded_size = (uint64_t)header.m_VertexSize + header.m_IndexSize;
        if (header.m_RangeCount > (size - sizeof(ModelBuffersHeader)) / sizeof(ModelBufferDescriptor) ||
            header.m_VertexOffset != AlignModelBufferOffset(descriptor_end, MODEL_BUFFERS_ALIGNMENT))
        {
            return false;
        }

        if (header.m_IndexOffset != vertex_payload_end || index_payload_end != size ||
            header.m_DecodedSize != decoded_size)
        {
            return false;
        }

        uint64_t vertex_end = 0;
        uint64_t index_end = 0;
        for (uint32_t i = 0; i < header.m_RangeCount; ++i)
        {
            const ModelBufferDescriptor& descriptor = ranges[i];

            uint32_t skinning_mask = descriptor.m_AttributeMask & ATTRIBUTE_MASK_SKINNING;
            bool skinned = skinning_mask == ATTRIBUTE_MASK_SKINNING;
            if (descriptor.m_AttributeMask > ATTRIBUTE_MASK_ALL ||
                !(descriptor.m_AttributeMask & (1u << ATTRIBUTE_POSITION)) ||
                (skinning_mask != 0 && !skinned))
            {
                return false;
            }

            uint32_t stride = skinned ? SKINNED_VERTEX_STRIDE : STATIC_VERTEX_STRIDE;
            if (descriptor.m_Stride != stride ||
                descriptor.m_VertexOffset != AlignModelBufferOffset(vertex_end, MODEL_BUFFERS_ALIGNMENT) ||
                descriptor.m_IndexOffset != AlignModelBufferOffset(index_end, 4) ||
                (descriptor.m_IndexElementSize != 2 && descriptor.m_IndexElementSize != 4))
            {
                return false;
            }

            vertex_end = (uint64_t)descriptor.m_VertexOffset + (uint64_t)descriptor.m_VertexCount * descriptor.m_Stride;
            index_end = (uint64_t)descriptor.m_IndexOffset + (uint64_t)descriptor.m_IndexCount * descriptor.m_IndexElementSize;
            if (vertex_end > header.m_VertexSize || index_end > header.m_IndexSize)
            {
                return false;
            }
        }

        if (AlignModelBufferOffset(vertex_end, MODEL_BUFFERS_ALIGNMENT) != header.m_VertexSize || index_end != header.m_IndexSize)
        {
            return false;
        }

        return true;
    }
}
