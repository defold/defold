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

#include "modelimporter_buffers.h"

#include <gamesys/modelbuffers.h>
#include <dlib/hash.h>
#include <stdlib.h>
#include <string.h>

namespace dmModelImporter
{
    using namespace dmGameSystem;

    struct BufferRange
    {
        Mesh*    m_Mesh;
        uint64_t m_Hash;
        uint32_t m_Mask;
    };

    static uint64_t Align(uint64_t value, uint32_t alignment)
    {
        return (value + alignment - 1) & ~(uint64_t)(alignment - 1);
    }

    static uint32_t IndexElementSize(const Mesh* mesh)
    {
        return mesh->m_VertexCount >= 65536 ? 4 : 2;
    }

    static uint32_t VertexStride(uint32_t mask)
    {
        return (mask & ATTRIBUTE_MASK_SKINNING) == ATTRIBUTE_MASK_SKINNING ? SKINNED_VERTEX_STRIDE : STATIC_VERTEX_STRIDE;
    }

    // Presence distinguishes an absent stream from an explicitly zero stream:
    // their GPU bytes match, but CPU recovery must restore the authored streams.
    static BufferRange MakeRange(Mesh* mesh)
    {
        const dmArray<float>* streams[] = {
            &mesh->m_Positions, &mesh->m_Normals, &mesh->m_Tangents,
            &mesh->m_Colors, &mesh->m_TexCoords0, &mesh->m_TexCoords1,
            &mesh->m_Weights
        };
        BufferRange range;
        range.m_Mesh = mesh;
        range.m_Mask = 1 << ATTRIBUTE_POSITION;

        HashState64 hash;
        dmHashInit64(&hash, false);
        for (uint32_t a = 0; a < ATTRIBUTE_BONE_INDICES; ++a)
        {
            uint32_t count = streams[a]->Size();
            dmHashUpdateBuffer64(&hash, &count, sizeof(count));
            if (count)
            {
                range.m_Mask |= 1 << a;
                dmHashUpdateBuffer64(&hash, streams[a]->Begin(), count * sizeof(float));
            }
        }

        uint32_t counts[] = { mesh->m_Bones.Size(), mesh->m_Indices.Size(), IndexElementSize(mesh) };
        dmHashUpdateBuffer64(&hash, counts, sizeof(counts));
        if (mesh->m_Bones.Size())
        {
            range.m_Mask |= 1 << ATTRIBUTE_BONE_INDICES;
            dmHashUpdateBuffer64(&hash, mesh->m_Bones.Begin(), mesh->m_Bones.Size() * sizeof(uint32_t));
        }
        if (mesh->m_Indices.Size())
        {
            dmHashUpdateBuffer64(&hash, mesh->m_Indices.Begin(), mesh->m_Indices.Size() * sizeof(uint32_t));
        }
        range.m_Hash = dmHashFinal64(&hash);
        return range;
    }

    static int CompareRanges(const void* left, const void* right)
    {
        uint64_t a = ((const BufferRange*)left)->m_Hash;
        uint64_t b = ((const BufferRange*)right)->m_Hash;
        return a < b ? -1 : a > b ? 1 : 0;
    }

    static inline void WriteAttribute(float* vertex, const float* values, uint32_t vertex_index, uint32_t attribute, float default_value)
    {
        uint32_t components = ATTRIBUTE_COMPONENT_COUNTS[attribute];
        const float* source = values ? values + vertex_index * components : 0;
        float* target = vertex + ATTRIBUTE_BYTE_OFFSETS[attribute] / sizeof(float);
        for (uint32_t c = 0; c < components; ++c)
        {
            target[c] = source ? source[c] : default_value;
        }
    }

    static void PackVertices(const BufferRange& range, uint8_t* output)
    {
        const Mesh* mesh = range.m_Mesh;
        const float* streams[] = {
            mesh->m_Positions.Begin(), mesh->m_Normals.Begin(), mesh->m_Tangents.Begin(),
            mesh->m_Colors.Begin(), mesh->m_TexCoords0.Begin(), mesh->m_TexCoords1.Begin(),
            mesh->m_Weights.Begin()
        };
        for (uint32_t a = 0; a < ATTRIBUTE_BONE_INDICES; ++a)
        {
            if (!(range.m_Mask & (1 << a)))
            {
                streams[a] = 0;
            }
        }

        uint32_t stride = VertexStride(range.m_Mask);
        for (uint32_t v = 0; v < mesh->m_VertexCount; ++v)
        {
            float* vertex = (float*)(output + v * stride);
            WriteAttribute(vertex, streams[ATTRIBUTE_POSITION], v, ATTRIBUTE_POSITION, 0.0f);
            WriteAttribute(vertex, streams[ATTRIBUTE_NORMAL], v, ATTRIBUTE_NORMAL, 0.0f);
            WriteAttribute(vertex, streams[ATTRIBUTE_TANGENT], v, ATTRIBUTE_TANGENT, 0.0f);
            WriteAttribute(vertex, streams[ATTRIBUTE_COLOR], v, ATTRIBUTE_COLOR, 1.0f);
            WriteAttribute(vertex, streams[ATTRIBUTE_TEXCOORD0], v, ATTRIBUTE_TEXCOORD0, 0.0f);
            WriteAttribute(vertex, streams[ATTRIBUTE_TEXCOORD1], v, ATTRIBUTE_TEXCOORD1, 0.0f);
            if (stride == SKINNED_VERTEX_STRIDE)
            {
                WriteAttribute(vertex, streams[ATTRIBUTE_WEIGHTS], v, ATTRIBUTE_WEIGHTS, 0.0f);
                float* bones = vertex + ATTRIBUTE_BYTE_OFFSETS[ATTRIBUTE_BONE_INDICES] / sizeof(float);
                for (uint32_t c = 0; c < ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_BONE_INDICES]; ++c)
                {
                    bones[c] = (float)mesh->m_Bones[v * ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_BONE_INDICES] + c];
                }
            }
        }
    }

    // Descriptors already contain range sizes; finish aligned offsets and allocate
    // the complete file before writing either payload.
    static bool AllocateBuffers(dmArray<ModelBufferDescriptor>& descriptors, dmArray<uint8_t>& output)
    {
        uint64_t vertex_size = 0;
        uint64_t index_size = 0;
        for (uint32_t i = 0; i < descriptors.Size(); ++i)
        {
            ModelBufferDescriptor& descriptor = descriptors[i];
            uint64_t vertex_offset = Align(vertex_size, MODEL_BUFFERS_ALIGNMENT);
            uint64_t index_offset = Align(index_size, 4);
            vertex_size = vertex_offset + (uint64_t)descriptor.m_VertexCount * descriptor.m_Stride;
            index_size = index_offset + (uint64_t)descriptor.m_IndexCount * descriptor.m_IndexElementSize;
            if (vertex_size > 0xffffffffu || index_size > 0xffffffffu)
            {
                return false;
            }
            descriptor.m_VertexOffset = (uint32_t)vertex_offset;
            descriptor.m_IndexOffset = (uint32_t)index_offset;
        }

        vertex_size = Align(vertex_size, MODEL_BUFFERS_ALIGNMENT);
        uint64_t vertex_start = Align(sizeof(ModelBuffersHeader) + (uint64_t)descriptors.Size() * sizeof(ModelBufferDescriptor), MODEL_BUFFERS_ALIGNMENT);
        uint64_t index_start = vertex_start + vertex_size;
        uint64_t size = index_start + index_size;
        if (size > 0x7fffffffu)
        {
            return false; // The result must also fit in a Java byte array.
        }

        output.SetCapacity((uint32_t)size);
        output.SetSize((uint32_t)size);
        memset(output.Begin(), 0, output.Size());
        ModelBuffersHeader* header = (ModelBuffersHeader*)output.Begin();
        memcpy(header->m_Magic, MODEL_BUFFERS_MAGIC, sizeof(header->m_Magic));
        header->m_Version = MODEL_BUFFERS_VERSION;
        header->m_Compression = 0;
        header->m_DecodedSize = (uint32_t)(vertex_size + index_size);
        header->m_RangeCount = descriptors.Size();
        header->m_VertexOffset = (uint32_t)vertex_start;
        header->m_VertexSize = (uint32_t)vertex_size;
        header->m_IndexOffset = (uint32_t)index_start;
        header->m_IndexSize = (uint32_t)index_size;
        if (descriptors.Size())
        {
            memcpy(header + 1, descriptors.Begin(), descriptors.Size() * sizeof(ModelBufferDescriptor));
        }
        return true;
    }

    bool CompileModelBuffers(Mesh** meshes, uint32_t count, dmArray<uint8_t>& output)
    {
        dmArray<BufferRange> ranges;
        ranges.SetCapacity(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            ranges.Push(MakeRange(meshes[i]));
        }
        if (count > 1)
        {
            qsort(ranges.Begin(), count, sizeof(BufferRange), CompareRanges);
        }

        uint32_t unique_count = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            BufferRange range = ranges[i];
            if (!unique_count || range.m_Hash != ranges[unique_count - 1].m_Hash)
            {
                ranges[unique_count++] = range;
            }
            range.m_Mesh->m_BufferIndex = unique_count - 1;
        }
        ranges.SetSize(unique_count);

        dmArray<ModelBufferDescriptor> descriptors;
        descriptors.SetCapacity(unique_count);
        descriptors.SetSize(unique_count);
        for (uint32_t i = 0; i < unique_count; ++i)
        {
            const BufferRange& range = ranges[i];
            ModelBufferDescriptor& descriptor = descriptors[i];
            descriptor.m_VertexCount = range.m_Mesh->m_VertexCount;
            descriptor.m_Stride = VertexStride(range.m_Mask);
            descriptor.m_IndexCount = range.m_Mesh->m_Indices.Size();
            descriptor.m_IndexElementSize = IndexElementSize(range.m_Mesh);
            descriptor.m_AttributeMask = range.m_Mask;
        }
        if (!AllocateBuffers(descriptors, output))
        {
            return false;
        }

        const ModelBuffersHeader* header = (const ModelBuffersHeader*)output.Begin();
        for (uint32_t i = 0; i < unique_count; ++i)
        {
            const BufferRange& range = ranges[i];
            const ModelBufferDescriptor& descriptor = descriptors[i];
            PackVertices(range, output.Begin() + header->m_VertexOffset + descriptor.m_VertexOffset);
            uint8_t* indices = output.Begin() + header->m_IndexOffset + descriptor.m_IndexOffset;
            if (descriptor.m_IndexElementSize == 4)
            {
                memcpy(indices, range.m_Mesh->m_Indices.Begin(), descriptor.m_IndexCount * sizeof(uint32_t));
            }
            else
            {
                for (uint32_t j = 0; j < descriptor.m_IndexCount; ++j)
                {
                    ((uint16_t*)indices)[j] = (uint16_t)range.m_Mesh->m_Indices[j];
                }
            }
        }
        return true;
    }

    bool CompileModelBuffers(Scene* scene)
    {
        dmArray<Mesh*> meshes;
        uint32_t count = 0;
        for (uint32_t i = 0; i < scene->m_Models.Size(); ++i)
        {
            count += scene->m_Models[i].m_Meshes.Size();
        }
        meshes.SetCapacity(count);
        for (uint32_t i = 0; i < scene->m_Models.Size(); ++i)
        {
            for (uint32_t j = 0; j < scene->m_Models[i].m_Meshes.Size(); ++j)
            {
                meshes.Push(&scene->m_Models[i].m_Meshes[j]);
            }
        }
        return CompileModelBuffers(meshes.Begin(), meshes.Size(), scene->m_ModelBuffers);
    }

    bool SelectModelBuffers(const void* data, const uint32_t* indices, uint32_t count, dmArray<uint8_t>& output)
    {
        const ModelBuffersHeader* source = (const ModelBuffersHeader*)data;
        const ModelBufferDescriptor* ranges = (const ModelBufferDescriptor*)(source + 1);
        dmArray<ModelBufferDescriptor> descriptors;
        descriptors.SetCapacity(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            descriptors.Push(ranges[indices[i]]);
        }
        if (!AllocateBuffers(descriptors, output))
        {
            return false;
        }

        const ModelBuffersHeader* header = (const ModelBuffersHeader*)output.Begin();
        for (uint32_t i = 0; i < count; ++i)
        {
            const ModelBufferDescriptor& range = ranges[indices[i]];
            const ModelBufferDescriptor& target = descriptors[i];
            memcpy(output.Begin() + header->m_VertexOffset + target.m_VertexOffset, (const uint8_t*)data + source->m_VertexOffset + range.m_VertexOffset, (size_t)range.m_VertexCount * range.m_Stride);
            memcpy(output.Begin() + header->m_IndexOffset + target.m_IndexOffset, (const uint8_t*)data + source->m_IndexOffset + range.m_IndexOffset, (size_t)range.m_IndexCount * range.m_IndexElementSize);
        }
        return true;
    }
}
