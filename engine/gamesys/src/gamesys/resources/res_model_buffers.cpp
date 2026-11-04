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
#include "../modelbuffers.h"

#include <dlib/log.h>
#include <rig/rig.h>
#include <stdlib.h>
#include <string.h>

namespace dmGameSystem
{
    DM_STATIC_ASSERT(sizeof(dmRig::RigModelVertex) == STATIC_VERTEX_STRIDE, Invalid_static_model_buffer_vertex_size);
    DM_STATIC_ASSERT(sizeof(dmRig::RigModelSkinnedVertex) == SKINNED_VERTEX_STRIDE, Invalid_skinned_model_buffer_vertex_size);

    struct ModelBufferRange
    {
        // First member: mesh render items hold this address.
        ModelResourceBuffers  m_Buffers;
        ModelBufferDescriptor m_Descriptor;
        ModelBuffersResource* m_Owner;
        dmRigDDF::Mesh        m_CPUMesh;
    };

    struct ModelBuffersResource
    {
        ModelBuffersHeader        m_Header;
        dmArray<ModelBufferRange> m_Ranges;
        dmGraphics::HVertexBuffer m_VertexBuffer;
        dmGraphics::HIndexBuffer  m_IndexBuffer;
        char*                     m_Path;
        void*                     m_CPUData;
        uint32_t                  m_CPUDataSize;
    };

    ModelResourceBuffers* GetModelBuffers(ModelBuffersResource* resource, uint32_t index)
    {
        return index < resource->m_Ranges.Size() ? &resource->m_Ranges[index].m_Buffers : 0;
    }

    dmResource::Result ResModelBuffersPreload(const dmResource::ResourcePreloadParams* params)
    {
        const ModelBuffersHeader* header = (const ModelBuffersHeader*)params->m_Buffer;
        if (!ValidateModelBuffers(header, params->m_BufferSize))
        {
            return dmResource::RESULT_INVALID_DATA;
        }

        ModelBuffersResource* resource = new ModelBuffersResource();
        resource->m_Header = *header;
        resource->m_Ranges.SetCapacity(header->m_RangeCount);
        resource->m_Ranges.SetSize(header->m_RangeCount);
        const ModelBufferDescriptor* ranges = (const ModelBufferDescriptor*)(header + 1);
        for (uint32_t i = 0; i < header->m_RangeCount; ++i)
        {
            const ModelBufferDescriptor& descriptor = ranges[i];
            bool skinned = (descriptor.m_AttributeMask & ATTRIBUTE_MASK_SKINNING) == ATTRIBUTE_MASK_SKINNING;
            ModelBufferRange& item = resource->m_Ranges[i];
            memset(&item, 0, sizeof(item));
            item.m_Descriptor = descriptor;
            item.m_Owner = resource;

            item.m_Buffers.m_VertexCount = descriptor.m_VertexCount;
            item.m_Buffers.m_IndexCount = descriptor.m_IndexCount;
            item.m_Buffers.m_VertexBufferOffset = descriptor.m_VertexOffset;
            item.m_Buffers.m_IndexBufferOffset = descriptor.m_IndexOffset;
            item.m_Buffers.m_IndexBufferElementType = descriptor.m_IndexElementSize == 2 ? dmGraphics::TYPE_UNSIGNED_SHORT : dmGraphics::TYPE_UNSIGNED_INT;
            item.m_Buffers.m_RigModelVertexFormat = skinned ? RIG_MODEL_VERTEX_FORMAT_SKINNED : RIG_MODEL_VERTEX_FORMAT_STATIC;
        }

        *params->m_PreloadData = resource;
        return dmResource::RESULT_OK;
    }

    static uint32_t GetResourceSize(const ModelBuffersResource* resource)
    {
        return sizeof(*resource) + resource->m_Ranges.Capacity() * sizeof(ModelBufferRange)
               + strlen(resource->m_Path) + 1 + resource->m_CPUDataSize;
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
        {
            resource->m_IndexBuffer = dmGraphics::NewIndexBuffer(context, header.m_IndexSize, (const uint8_t*)params->m_Buffer + header.m_IndexOffset, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        }
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
        {
            dmGraphics::DeleteIndexBuffer(resource->m_IndexBuffer);
        }

        free(resource->m_Path);
        free(resource->m_CPUData);
        delete resource;
        return dmResource::RESULT_OK;
    }

    static uint64_t GetCPUDataSize(const ModelBuffersResource* resource)
    {
        uint64_t size = 0;
        for (uint32_t i = 0; i < resource->m_Ranges.Size(); ++i)
        {
            const ModelBufferDescriptor& descriptor = resource->m_Ranges[i].m_Descriptor;
            uint32_t components = 0;
            for (uint32_t a = 0; a < ATTRIBUTE_COUNT; ++a)
            {
                if (descriptor.m_AttributeMask & (1u << a))
                {
                    components += ATTRIBUTE_COMPONENT_COUNTS[a];
                }
            }

            size = AlignModelBufferOffset(size, 4);
            size += (uint64_t)descriptor.m_VertexCount * components * sizeof(float);
            size += (uint64_t)descriptor.m_IndexCount * descriptor.m_IndexElementSize;
        }

        return size;
    }

    static uint8_t* UnpackMeshData(const ModelBufferDescriptor& descriptor, const uint8_t* vertices, const uint8_t* indices, dmRigDDF::Mesh* mesh, uint8_t* write)
    {
        float** streams[ATTRIBUTE_COUNT] = {
            &mesh->m_Positions.m_Data,
            &mesh->m_Normals.m_Data,
            &mesh->m_Tangents.m_Data,
            &mesh->m_Colors.m_Data,
            &mesh->m_Texcoord0.m_Data,
            &mesh->m_Texcoord1.m_Data,
            &mesh->m_Weights.m_Data,
            0
        };
        uint32_t* counts[ATTRIBUTE_COUNT] = {
            &mesh->m_Positions.m_Count,
            &mesh->m_Normals.m_Count,
            &mesh->m_Tangents.m_Count,
            &mesh->m_Colors.m_Count,
            &mesh->m_Texcoord0.m_Count,
            &mesh->m_Texcoord1.m_Count,
            &mesh->m_Weights.m_Count,
            &mesh->m_BoneIndices.m_Count
        };

        for (uint32_t a = 0; a < ATTRIBUTE_COUNT; ++a)
        {
            if (!(descriptor.m_AttributeMask & (1u << a)))
            {
                continue;
            }

            if (a == ATTRIBUTE_BONE_INDICES)
            {
                mesh->m_BoneIndices.m_Data = (uint32_t*)write;
            }
            else
            {
                *streams[a] = (float*)write;
            }
            *counts[a] = descriptor.m_VertexCount * ATTRIBUTE_COMPONENT_COUNTS[a];

            for (uint32_t v = 0; v < descriptor.m_VertexCount; ++v)
            {
                const uint8_t* vertex = vertices + v * descriptor.m_Stride + ATTRIBUTE_BYTE_OFFSETS[a];
                for (uint32_t c = 0; c < ATTRIBUTE_COMPONENT_COUNTS[a]; ++c)
                {
                    float value = ((const float*)vertex)[c];
                    if (a == ATTRIBUTE_BONE_INDICES)
                    {
                        ((uint32_t*)write)[c] = (uint32_t)value;
                    }
                    else
                    {
                        ((float*)write)[c] = value;
                    }
                }
                write += ATTRIBUTE_COMPONENT_COUNTS[a] * sizeof(float);
            }
        }

        uint32_t index_size = descriptor.m_IndexCount * descriptor.m_IndexElementSize;
        mesh->m_Indices.m_Data = write;
        mesh->m_Indices.m_Count = index_size;
        memcpy(write, indices, index_size);
        return write + index_size;
    }

    static dmResource::Result LoadCPUBufferData(dmResource::HFactory factory, ModelBuffersResource* resource)
    {
        void* data;
        uint32_t size;
        dmResource::Result result = dmResource::GetRaw(factory, resource->m_Path, &data, &size);
        if (result != dmResource::RESULT_OK)
        {
            return result;
        }

        // CPU recovery must use the same layout as the buffers already uploaded to the GPU.
        const ModelBuffersHeader* header = (const ModelBuffersHeader*)data;
        if (!ValidateModelBuffers(header, size) || memcmp(header, &resource->m_Header, sizeof(*header)) != 0)
        {
            free(data);
            return dmResource::RESULT_INVALID_DATA;
        }
        const ModelBufferDescriptor* ranges = (const ModelBufferDescriptor*)(header + 1);
        for (uint32_t i = 0; i < header->m_RangeCount; ++i)
        {
            if (memcmp(&ranges[i], &resource->m_Ranges[i].m_Descriptor, sizeof(ranges[i])) != 0)
            {
                free(data);
                return dmResource::RESULT_INVALID_DATA;
            }
        }

        uint64_t data_size = GetCPUDataSize(resource);
        if (data_size > 0xffffffffu)
        {
            free(data);
            return dmResource::RESULT_OUT_OF_MEMORY;
        }

        resource->m_CPUData = malloc((size_t)data_size);
        if (!resource->m_CPUData)
        {
            free(data);
            return dmResource::RESULT_OUT_OF_MEMORY;
        }
        resource->m_CPUDataSize = (uint32_t)data_size;

        uint8_t* base = (uint8_t*)resource->m_CPUData;
        uint8_t* write = base;
        for (uint32_t i = 0; i < resource->m_Ranges.Size(); ++i)
        {
            ModelBufferRange& range = resource->m_Ranges[i];
            const ModelBufferDescriptor& descriptor = range.m_Descriptor;
            const uint8_t* vertices = (const uint8_t*)data + resource->m_Header.m_VertexOffset + descriptor.m_VertexOffset;
            const uint8_t* indices = (const uint8_t*)data + resource->m_Header.m_IndexOffset + descriptor.m_IndexOffset;
            write = base + AlignModelBufferOffset(write - base, 4);
            write = UnpackMeshData(descriptor, vertices, indices, &range.m_CPUMesh, write);
        }
        free(data);

        dmResource::HDescriptor descriptor;
        dmResource::GetDescriptor(factory, resource->m_Path, &descriptor);
        dmResource::SetResourceSize(descriptor, GetResourceSize(resource));
        return dmResource::RESULT_OK;
    }

    dmResource::Result LoadModelMeshData(dmResource::HFactory factory, ModelResourceBuffers* buffers, dmRigDDF::Mesh* mesh)
    {
        if (mesh->m_Positions.m_Count)
        {
            return dmResource::RESULT_OK;
        }

        ModelBufferRange* range = (ModelBufferRange*)buffers;
        ModelBuffersResource* resource = range->m_Owner;
        if (!resource->m_CPUData)
        {
            dmResource::Result result = LoadCPUBufferData(factory, resource);
            if (result != dmResource::RESULT_OK)
            {
                return result;
            }
        }

        const dmRigDDF::Mesh& cpu_mesh = range->m_CPUMesh;
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
