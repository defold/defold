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

#ifndef DM_GAMESYS_MODELBUFFERS_H
#define DM_GAMESYS_MODELBUFFERS_H

#include <dmsdk/dlib/static_assert.h>
#include <stddef.h>
#include <stdint.h>

namespace dmGameSystem
{
    // DMMB is little-endian on disk, matching the supported compiler/runtime targets.
    // Headers and descriptors contain no pointers or compiler-dependent bitfields.
    enum
    {
        MODEL_BUFFERS_VERSION   = 2,
        MODEL_BUFFERS_ALIGNMENT = 16
    };
    static const char MODEL_BUFFERS_MAGIC[4] = { 'D', 'M', 'M', 'B' };

    // Attribute indices are also the DMMB mask bit positions.
    enum
    {
        ATTRIBUTE_POSITION     = 0,
        ATTRIBUTE_NORMAL       = 1,
        ATTRIBUTE_TANGENT      = 2,
        ATTRIBUTE_COLOR        = 3,
        ATTRIBUTE_TEXCOORD0    = 4,
        ATTRIBUTE_TEXCOORD1    = 5,
        ATTRIBUTE_WEIGHTS      = 6,
        ATTRIBUTE_BONE_INDICES = 7,
        ATTRIBUTE_COUNT
    };

    enum
    {
        ATTRIBUTE_MASK_SKINNING = (1 << ATTRIBUTE_WEIGHTS) | (1 << ATTRIBUTE_BONE_INDICES),
        ATTRIBUTE_MASK_ALL      = (1 << ATTRIBUTE_COUNT) - 1,
        STATIC_VERTEX_STRIDE    = 72,
        SKINNED_VERTEX_STRIDE   = 104
    };

    // Scalar counts and byte offsets in the fixed interleaved vertex layouts.
    // The mask records source presence for CPU recovery, not GPU layout packing.
    static const uint32_t ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_COUNT] = { 3, 3, 4, 4, 2, 2, 4, 4 };
    static const uint32_t ATTRIBUTE_BYTE_OFFSETS[ATTRIBUTE_COUNT] = { 0, 12, 24, 40, 56, 64, 72, 88 };

    // The descriptor array immediately follows this header. Payload offsets are
    // relative to the file start; payloads and vertex ranges align to 16 bytes.
    struct ModelBuffersHeader
    {
        char     m_Magic[4];
        uint32_t m_Version;
        uint32_t m_Compression; // Zero; archive compression belongs to the resource loader.
        uint32_t m_DecodedSize; // Vertex payload size + index payload size.
        uint32_t m_RangeCount;
        uint32_t m_VertexOffset;
        uint32_t m_VertexSize;
        uint32_t m_IndexOffset;
        uint32_t m_IndexSize;
    };

    // Range offsets are relative to their corresponding payload. Index ranges
    // align to 4 bytes. Bone indices occupy float32 slots in the skinned layout.
    struct ModelBufferDescriptor
    {
        uint32_t m_VertexOffset;
        uint32_t m_VertexCount;
        uint32_t m_Stride;
        uint32_t m_IndexOffset;
        uint32_t m_IndexCount;
        uint32_t m_IndexElementSize;
        uint32_t m_AttributeMask;
    };

    DM_STATIC_ASSERT(sizeof(ModelBuffersHeader) == 36, Invalid_model_buffer_header_size);
    DM_STATIC_ASSERT(offsetof(ModelBuffersHeader, m_RangeCount) == 16, Invalid_model_buffer_range_count_offset);
    DM_STATIC_ASSERT(offsetof(ModelBuffersHeader, m_IndexSize) == 32, Invalid_model_buffer_index_size_offset);
    DM_STATIC_ASSERT(sizeof(ModelBufferDescriptor) == 28, Invalid_model_buffer_descriptor_size);
    DM_STATIC_ASSERT(offsetof(ModelBufferDescriptor, m_AttributeMask) == 24, Invalid_model_buffer_mask_offset);
}

#endif // DM_GAMESYS_MODELBUFFERS_H
