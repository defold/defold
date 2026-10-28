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

#include "res_model_buffers.h"

#include <dlib/log.h>
#include <rig/rig.h>
#include <stdlib.h>
#include <string.h>

namespace dmGameSystem
{
    // MBFC v2 is little-endian. See ModelBuffers.java for the build-time writer.
    enum
    {
        HEADER_SIZE = 36,
        RANGE_SIZE = 28
    };
    // Attribute indices are also the MBFC mask bit positions.
    enum
    {
        ATTRIBUTE_POSITION = 0,
        ATTRIBUTE_NORMAL = 1,
        ATTRIBUTE_TANGENT = 2,
        ATTRIBUTE_COLOR = 3,
        ATTRIBUTE_TEXCOORD0 = 4,
        ATTRIBUTE_TEXCOORD1 = 5,
        ATTRIBUTE_WEIGHTS = 6,
        ATTRIBUTE_BONE_INDICES = 7,
        ATTRIBUTE_COUNT
    };
    enum
    {
        ATTRIBUTE_MASK_SKINNING = (1 << ATTRIBUTE_WEIGHTS) | (1 << ATTRIBUTE_BONE_INDICES),
        ATTRIBUTE_MASK_ALL = (1 << ATTRIBUTE_COUNT) - 1
    };
    // Indexed by attribute-mask bit position: position, normal, tangent, color,
    // texcoord0, texcoord1, skin weights, bone indices. Counts are scalar values
    // per vertex; offsets are bytes from the start of an interleaved vertex.
    // The first six attributes use 72 bytes; skinning adds 32 bytes. See
    // ModelBuffers.compile() for the file layout and missing-attribute defaults.
    static const uint32_t ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_COUNT] = { 3, 3, 4, 4, 2, 2, 4, 4 };
    static const uint32_t ATTRIBUTE_BYTE_OFFSETS[ATTRIBUTE_COUNT] = { 0, 12, 24, 40, 56, 64, 72, 88 };

    // Decoded fields retained after prefix validation; this is not an on-disk struct.
    struct ModelBuffersHeader
    {
        uint32_t m_RangeCount;
        uint32_t m_VertexOffset;
        uint32_t m_VertexSize;
        uint32_t m_IndexOffset;
        uint32_t m_IndexSize;
    };

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

    struct ModelBufferRange
    {
        // First member: mesh render items hold this address.
        ModelResourceBuffers m_Buffers;
        ModelBufferDescriptor m_Descriptor;
        ModelBuffersResource* m_Owner;
        dmRigDDF::Mesh m_CPUMesh;
    };

    struct ModelBuffersResource
    {
        ModelBuffersHeader m_Header;
        dmArray<ModelBufferRange> m_Ranges;
        dmGraphics::HVertexBuffer m_VertexBuffer;
        dmGraphics::HIndexBuffer m_IndexBuffer;
        char* m_Path;
        void* m_CPUData;
        uint32_t m_CPUDataSize;
    };

    static uint32_t ReadInteger(const uint8_t* data)
    {
        return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
    }

    static uint64_t Align(uint64_t value, uint32_t alignment)
    {
        return (value + alignment - 1) & ~(uint64_t)(alignment - 1);
    }

    static dmResource::Result ParseBuffers(const void* data, uint32_t size, ModelBuffersResource* output, const ModelBuffersResource* expected)
    {
        const uint8_t* bytes = (const uint8_t*)data;
        if (size < HEADER_SIZE || memcmp(bytes, "MBFC", 4) != 0 || ReadInteger(bytes + 4) != 2 || ReadInteger(bytes + 8) != 0)
            return dmResource::RESULT_INVALID_DATA;
        ModelBuffersHeader header;
        header.m_RangeCount = ReadInteger(bytes + 16);
        header.m_VertexOffset = ReadInteger(bytes + 20);
        header.m_VertexSize = ReadInteger(bytes + 24);
        header.m_IndexOffset = ReadInteger(bytes + 28);
        header.m_IndexSize = ReadInteger(bytes + 32);
        if (header.m_RangeCount > (size - HEADER_SIZE) / RANGE_SIZE ||
            header.m_VertexOffset != Align(HEADER_SIZE + (uint64_t)header.m_RangeCount * RANGE_SIZE, 16) ||
            header.m_IndexOffset != (uint64_t)header.m_VertexOffset + header.m_VertexSize ||
            (uint64_t)header.m_IndexOffset + header.m_IndexSize != size ||
            ReadInteger(bytes + 12) != (uint64_t)header.m_VertexSize + header.m_IndexSize ||
            (expected && memcmp(&header, &expected->m_Header, sizeof(header)) != 0))
            return dmResource::RESULT_INVALID_DATA;
        if (output)
        {
            output->m_Header = header;
            output->m_Ranges.SetCapacity(header.m_RangeCount);
            output->m_Ranges.SetSize(header.m_RangeCount);
        }
        uint64_t vertex_end = 0;
        uint64_t index_end = 0;
        for (uint32_t i = 0; i < header.m_RangeCount; ++i)
        {
            const uint8_t* range = bytes + HEADER_SIZE + i * RANGE_SIZE;
            ModelBufferDescriptor descriptor;
            descriptor.m_VertexOffset = ReadInteger(range);
            descriptor.m_VertexCount = ReadInteger(range + 4);
            descriptor.m_Stride = ReadInteger(range + 8);
            descriptor.m_IndexOffset = ReadInteger(range + 12);
            descriptor.m_IndexCount = ReadInteger(range + 16);
            descriptor.m_IndexElementSize = ReadInteger(range + 20);
            descriptor.m_AttributeMask = ReadInteger(range + 24);
            bool skinned = (descriptor.m_AttributeMask & ATTRIBUTE_MASK_SKINNING) == ATTRIBUTE_MASK_SKINNING;
            if (descriptor.m_AttributeMask > ATTRIBUTE_MASK_ALL || !(descriptor.m_AttributeMask & (1u << ATTRIBUTE_POSITION)) ||
                ((descriptor.m_AttributeMask & ATTRIBUTE_MASK_SKINNING) != 0 && !skinned) ||
                descriptor.m_Stride != (skinned ? sizeof(dmRig::RigModelSkinnedVertex) : sizeof(dmRig::RigModelVertex)) ||
                descriptor.m_VertexOffset != Align(vertex_end, 16) || descriptor.m_IndexOffset != Align(index_end, 4) ||
                (descriptor.m_IndexElementSize != 2 && descriptor.m_IndexElementSize != 4))
                return dmResource::RESULT_INVALID_DATA;
            vertex_end = (uint64_t)descriptor.m_VertexOffset + (uint64_t)descriptor.m_VertexCount * descriptor.m_Stride;
            index_end = (uint64_t)descriptor.m_IndexOffset + (uint64_t)descriptor.m_IndexCount * descriptor.m_IndexElementSize;
            if (vertex_end > header.m_VertexSize || index_end > header.m_IndexSize ||
                (expected && memcmp(&descriptor, &expected->m_Ranges[i].m_Descriptor, sizeof(descriptor)) != 0))
                return dmResource::RESULT_INVALID_DATA;
            if (output)
            {
                ModelBufferRange& item = output->m_Ranges[i];
                memset(&item, 0, sizeof(item));
                item.m_Descriptor = descriptor;
                item.m_Owner = output;
                item.m_Buffers.m_VertexCount = descriptor.m_VertexCount;
                item.m_Buffers.m_IndexCount = descriptor.m_IndexCount;
                item.m_Buffers.m_VertexBufferOffset = descriptor.m_VertexOffset;
                item.m_Buffers.m_IndexBufferOffset = descriptor.m_IndexOffset;
                item.m_Buffers.m_IndexBufferElementType = descriptor.m_IndexElementSize == 2 ? dmGraphics::TYPE_UNSIGNED_SHORT : dmGraphics::TYPE_UNSIGNED_INT;
                item.m_Buffers.m_RigModelVertexFormat = skinned ? RIG_MODEL_VERTEX_FORMAT_SKINNED : RIG_MODEL_VERTEX_FORMAT_STATIC;
            }
        }
        return Align(vertex_end, 16) == header.m_VertexSize && index_end == header.m_IndexSize ? dmResource::RESULT_OK : dmResource::RESULT_INVALID_DATA;
    }

    ModelResourceBuffers* GetModelBuffers(ModelBuffersResource* resource, uint32_t index)
    {
        return index < resource->m_Ranges.Size() ? &resource->m_Ranges[index].m_Buffers : 0;
    }

    dmResource::Result ResModelBuffersPreload(const dmResource::ResourcePreloadParams* params)
    {
        ModelBuffersResource* resource = new ModelBuffersResource();
        dmResource::Result result = ParseBuffers(params->m_Buffer, params->m_BufferSize, resource, 0);
        if (result != dmResource::RESULT_OK)
        {
            delete resource;
            return result;
        }
        *params->m_PreloadData = resource;
        return dmResource::RESULT_OK;
    }

    static uint32_t GetResourceSize(const ModelBuffersResource* resource)
    {
        return sizeof(*resource) + resource->m_Ranges.Capacity() * sizeof(ModelBufferRange) + strlen(resource->m_Path) + 1 + resource->m_CPUDataSize;
    }

    dmResource::Result ResModelBuffersCreate(const dmResource::ResourceCreateParams* params)
    {
        ModelBuffersResource* resource = (ModelBuffersResource*)params->m_PreloadData;
        dmGraphics::HContext context = (dmGraphics::HContext)params->m_Context;
        for (uint32_t i = 0; i < resource->m_Ranges.Size(); ++i)
        {
            if (resource->m_Ranges[i].m_Descriptor.m_IndexElementSize == 4 && !dmGraphics::IsIndexBufferFormatSupported(context, dmGraphics::INDEXBUFFER_FORMAT_32))
            {
                dmLogError("The platform doesn't support 32 bit index buffers. See the setting 'model.split_large_meshes'");
                delete resource;
                return dmResource::RESULT_NOT_SUPPORTED;
            }
        }
        const ModelBuffersHeader& header = resource->m_Header;
        resource->m_VertexBuffer = dmGraphics::NewVertexBuffer(context, header.m_VertexSize, (const uint8_t*)params->m_Buffer + header.m_VertexOffset, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        if (header.m_IndexSize)
            resource->m_IndexBuffer = dmGraphics::NewIndexBuffer(context, header.m_IndexSize, (const uint8_t*)params->m_Buffer + header.m_IndexOffset, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        for (uint32_t i = 0; i < resource->m_Ranges.Size(); ++i)
        {
            resource->m_Ranges[i].m_Buffers.m_VertexBuffer = resource->m_VertexBuffer;
            resource->m_Ranges[i].m_Buffers.m_IndexBuffer = resource->m_IndexBuffer;
        }
        resource->m_Path = strdup(params->m_Filename);
        dmResource::SetResource(params->m_Resource, resource);
        dmResource::SetResourceSize(params->m_Resource, GetResourceSize(resource));
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResModelBuffersDestroy(const dmResource::ResourceDestroyParams* params)
    {
        ModelBuffersResource* resource = (ModelBuffersResource*)dmResource::GetResource(params->m_Resource);
        dmGraphics::DeleteVertexBuffer(resource->m_VertexBuffer);
        if (resource->m_IndexBuffer)
            dmGraphics::DeleteIndexBuffer(resource->m_IndexBuffer);
        free(resource->m_Path);
        free(resource->m_CPUData);
        delete resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result EnsureModelMeshData(dmResource::HFactory factory, ModelResourceBuffers* buffers, dmRigDDF::Mesh* mesh)
    {
        if (mesh->m_BufferIndex == ~0u || mesh->m_Positions.m_Count)
            return dmResource::RESULT_OK;
        ModelBuffersResource* resource = ((ModelBufferRange*)buffers)->m_Owner;
        if (!resource->m_CPUData)
        {
            void* data;
            uint32_t size;
            dmResource::Result result = dmResource::GetRaw(factory, resource->m_Path, &data, &size);
            if (result != dmResource::RESULT_OK)
                return result;
            result = ParseBuffers(data, size, 0, resource);
            if (result != dmResource::RESULT_OK)
            {
                free(data);
                return result;
            }
            uint64_t data_size = 0;
            for (uint32_t i = 0; i < resource->m_Ranges.Size(); ++i)
            {
                const ModelBufferDescriptor& descriptor = resource->m_Ranges[i].m_Descriptor;
                uint32_t components = 0;
                for (uint32_t a = 0; a < ATTRIBUTE_COUNT; ++a)
                    if (descriptor.m_AttributeMask & (1u << a))
                        components += ATTRIBUTE_COMPONENT_COUNTS[a];
                data_size = Align(data_size, 4) + (uint64_t)descriptor.m_VertexCount * components * sizeof(float) + (uint64_t)descriptor.m_IndexCount * descriptor.m_IndexElementSize;
            }
            if (data_size > 0xffffffffu || !(resource->m_CPUData = malloc((size_t)data_size)))
            {
                free(data);
                return dmResource::RESULT_OUT_OF_MEMORY;
            }
            resource->m_CPUDataSize = (uint32_t)data_size;
            uint8_t* write = (uint8_t*)resource->m_CPUData;
            for (uint32_t i = 0; i < resource->m_Ranges.Size(); ++i)
            {
                ModelBufferRange& item = resource->m_Ranges[i];
                const ModelBufferDescriptor& descriptor = item.m_Descriptor;
                write = (uint8_t*)resource->m_CPUData + Align(write - (uint8_t*)resource->m_CPUData, 4);
                dmRigDDF::Mesh& cpu_mesh = item.m_CPUMesh;
                float** streams[ATTRIBUTE_COUNT] = { &cpu_mesh.m_Positions.m_Data, &cpu_mesh.m_Normals.m_Data, &cpu_mesh.m_Tangents.m_Data, &cpu_mesh.m_Colors.m_Data, &cpu_mesh.m_Texcoord0.m_Data, &cpu_mesh.m_Texcoord1.m_Data, &cpu_mesh.m_Weights.m_Data, 0 };
                uint32_t* counts[ATTRIBUTE_COUNT] = { &cpu_mesh.m_Positions.m_Count, &cpu_mesh.m_Normals.m_Count, &cpu_mesh.m_Tangents.m_Count, &cpu_mesh.m_Colors.m_Count, &cpu_mesh.m_Texcoord0.m_Count, &cpu_mesh.m_Texcoord1.m_Count, &cpu_mesh.m_Weights.m_Count, &cpu_mesh.m_BoneIndices.m_Count };
                for (uint32_t a = 0; a < ATTRIBUTE_COUNT; ++a)
                {
                    if (!(descriptor.m_AttributeMask & (1u << a)))
                        continue;
                    if (a == ATTRIBUTE_BONE_INDICES)
                        cpu_mesh.m_BoneIndices.m_Data = (uint32_t*)write;
                    else
                        *streams[a] = (float*)write;
                    *counts[a] = descriptor.m_VertexCount * ATTRIBUTE_COMPONENT_COUNTS[a];
                    for (uint32_t v = 0; v < descriptor.m_VertexCount; ++v)
                    {
                        const uint8_t* vertex = (const uint8_t*)data + resource->m_Header.m_VertexOffset + descriptor.m_VertexOffset + v * descriptor.m_Stride + ATTRIBUTE_BYTE_OFFSETS[a];
                        for (uint32_t c = 0; c < ATTRIBUTE_COMPONENT_COUNTS[a]; ++c)
                        {
                            uint32_t bits = ReadInteger(vertex + c * sizeof(float));
                            float value;
                            memcpy(&value, &bits, sizeof(value));
                            if (a == ATTRIBUTE_BONE_INDICES)
                                ((uint32_t*)write)[c] = (uint32_t)value;
                            else
                                ((float*)write)[c] = value;
                        }
                        write += ATTRIBUTE_COMPONENT_COUNTS[a] * sizeof(float);
                    }
                }
                uint32_t index_size = descriptor.m_IndexCount * descriptor.m_IndexElementSize;
                cpu_mesh.m_Indices.m_Data = write;
                cpu_mesh.m_Indices.m_Count = index_size;
                memcpy(write, (const uint8_t*)data + resource->m_Header.m_IndexOffset + descriptor.m_IndexOffset, index_size);
                write += index_size;
            }
            free(data);
            dmResource::HDescriptor descriptor;
            dmResource::GetDescriptor(factory, resource->m_Path, &descriptor);
            dmResource::SetResourceSize(descriptor, GetResourceSize(resource));
        }
        const dmRigDDF::Mesh& cpu_mesh = ((ModelBufferRange*)buffers)->m_CPUMesh;
        mesh->m_Positions = cpu_mesh.m_Positions;
        mesh->m_Normals = cpu_mesh.m_Normals;
        mesh->m_Tangents = cpu_mesh.m_Tangents;
        mesh->m_Colors = cpu_mesh.m_Colors;
        mesh->m_Texcoord0 = cpu_mesh.m_Texcoord0;
        mesh->m_Texcoord1 = cpu_mesh.m_Texcoord1;
        mesh->m_Weights = cpu_mesh.m_Weights;
        mesh->m_BoneIndices = cpu_mesh.m_BoneIndices;
        mesh->m_Indices = cpu_mesh.m_Indices;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResModelBuffersRecreate(const dmResource::ResourceRecreateParams*)
    {
        // A geometry change produces a new content-addressed path.
        return dmResource::RESULT_NOT_SUPPORTED;
    }
} // namespace dmGameSystem
