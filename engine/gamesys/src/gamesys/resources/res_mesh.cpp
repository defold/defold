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

#include "res_mesh.h"
#include "res_buffer.h"
#include "res_render_target.h"

#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/dstrings.h>
#include <dlib/memory.h>
#include <dlib/buffer.h>

#include "gamesys.h"
#include "gamesys_private.h"

namespace dmGameSystem
{
    static dmGraphics::HContext g_GraphicsContext = 0x0;

    struct MeshPreloadData
    {
        dmMeshDDF::MeshDesc* m_DDF;
        dmBufferDDF::BufferDesc* m_VertexBufferDDF;
        uint8_t*             m_IndexData;
        uint32_t             m_IndexDataSize;
    };

    static void FreePreloadData(MeshPreloadData* preload_data)
    {
        if (!preload_data)
            return;
        if (preload_data->m_DDF)
            dmDDF::FreeMessage(preload_data->m_DDF);
        if (preload_data->m_VertexBufferDDF)
            dmDDF::FreeMessage(preload_data->m_VertexBufferDDF);
        free(preload_data->m_IndexData);
        delete preload_data;
    }

    static dmResource::Result LoadMeshData(const void* buffer, uint32_t buffer_size, MeshPreloadData** out_preload_data)
    {
        if (buffer_size < sizeof(uint32_t))
            return dmResource::RESULT_FORMAT_ERROR;

        const uint8_t* bytes = (const uint8_t*) buffer;
        uint32_t header_size = (uint32_t) bytes[0] |
                               ((uint32_t) bytes[1] << 8) |
                               ((uint32_t) bytes[2] << 16) |
                               ((uint32_t) bytes[3] << 24);
        if (header_size > buffer_size - sizeof(uint32_t))
            return dmResource::RESULT_FORMAT_ERROR;

        dmMeshDDF::MeshDesc* ddf = 0;
        dmDDF::Result e = dmDDF::LoadMessage(bytes + sizeof(uint32_t), header_size, &dmMeshDDF_MeshDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
            return dmResource::RESULT_DDF_ERROR;

        uint32_t payload_offset = sizeof(uint32_t) + header_size;
        uint32_t payload_size = buffer_size - payload_offset;
        if (ddf->m_VertexBufferSize > payload_size)
        {
            dmDDF::FreeMessage(ddf);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmBufferDDF::BufferDesc* vertex_buffer_ddf = 0;
        e = dmDDF::LoadMessage(bytes + payload_offset, ddf->m_VertexBufferSize, &dmBufferDDF_BufferDesc_DESCRIPTOR, (void**) &vertex_buffer_ddf);
        if (e != dmDDF::RESULT_OK)
        {
            dmDDF::FreeMessage(ddf);
            return dmResource::RESULT_DDF_ERROR;
        }

        uint32_t index_size = ddf->m_IndexBufferFormat == dmMeshDDF::MeshDesc::INDEXBUFFER_FORMAT_16 ? sizeof(uint16_t) : sizeof(uint32_t);
        uint64_t expected_payload_size = (uint64_t) ddf->m_IndexCount * index_size;
        uint32_t index_payload_size = payload_size - ddf->m_VertexBufferSize;
        if (expected_payload_size != index_payload_size)
        {
            dmDDF::FreeMessage(ddf);
            dmDDF::FreeMessage(vertex_buffer_ddf);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        MeshPreloadData* preload_data = new MeshPreloadData();
        preload_data->m_DDF = ddf;
        preload_data->m_VertexBufferDDF = vertex_buffer_ddf;
        preload_data->m_IndexDataSize = index_payload_size;
        preload_data->m_IndexData = index_payload_size ? (uint8_t*) malloc(index_payload_size) : 0;
        if (index_payload_size)
            memcpy(preload_data->m_IndexData, bytes + payload_offset + ddf->m_VertexBufferSize, index_payload_size);
        *out_preload_data = preload_data;
        return dmResource::RESULT_OK;
    }

    static bool BuildVertexBufferResource(MeshResource* mesh_resource, dmBufferDDF::BufferDesc* vertex_buffer_ddf, const char* filename)
    {
        return CreateBufferResource(vertex_buffer_ddf, dmHashString64(filename), &mesh_resource->m_BufferResource);
    }

    static bool BuildIndexBufferResource(MeshResource* mesh_resource, const uint8_t* index_data, uint32_t index_data_size, const char* filename)
    {
        if (mesh_resource->m_MeshDDF->m_IndexCount == 0)
            return index_data_size == 0;

        dmBuffer::StreamDeclaration stream_decl;
        stream_decl.m_Name = dmHashString64(mesh_resource->m_MeshDDF->m_IndexStream);
        stream_decl.m_Type = mesh_resource->m_MeshDDF->m_IndexBufferFormat == dmMeshDDF::MeshDesc::INDEXBUFFER_FORMAT_16
                           ? dmBuffer::VALUE_TYPE_UINT16
                           : dmBuffer::VALUE_TYPE_UINT32;
        stream_decl.m_Count = 1;

        BufferResource* index_buffer_resource = new BufferResource();
        memset(index_buffer_resource, 0, sizeof(BufferResource));
        dmBuffer::Result result = dmBuffer::Create(mesh_resource->m_MeshDDF->m_IndexCount, &stream_decl, 1, &index_buffer_resource->m_Buffer);
        if (result != dmBuffer::RESULT_OK)
        {
            delete index_buffer_resource;
            return false;
        }

        void* stream_data = 0;
        uint32_t count = 0;
        uint32_t components = 0;
        uint32_t stride = 0;
        result = dmBuffer::GetStream(index_buffer_resource->m_Buffer, stream_decl.m_Name, &stream_data, &count, &components, &stride);
        if (result != dmBuffer::RESULT_OK || count != mesh_resource->m_MeshDDF->m_IndexCount || components != 1 || stride != 1)
        {
            dmBuffer::Destroy(index_buffer_resource->m_Buffer);
            delete index_buffer_resource;
            return false;
        }

        memcpy(stream_data, index_data, index_data_size);
        index_buffer_resource->m_NameHash = dmHashString64(filename);
        index_buffer_resource->m_ElementCount = count;
        index_buffer_resource->m_Stride = dmBuffer::GetStructSize(index_buffer_resource->m_Buffer);
        dmBuffer::GetContentVersion(index_buffer_resource->m_Buffer, &index_buffer_resource->m_Version);
        mesh_resource->m_IndexBufferResource = index_buffer_resource;
        return true;
    }

    static bool IsBufferTypeSupportedGraphicsType(dmBuffer::ValueType value_type) {
        if (value_type == dmBuffer::VALUE_TYPE_UINT64 ||
            value_type == dmBuffer::VALUE_TYPE_INT64) {
            return false;
        }

        return true;
    }

    static dmGraphics::Type BufferValueTypeToGraphicsType(dmBuffer::ValueType value_type)
    {
        switch (value_type)
        {
            case dmBuffer::VALUE_TYPE_UINT8:
                return dmGraphics::TYPE_UNSIGNED_BYTE;
            break;
            case dmBuffer::VALUE_TYPE_UINT16:
                return dmGraphics::TYPE_UNSIGNED_SHORT;
            break;
            case dmBuffer::VALUE_TYPE_UINT32:
                return dmGraphics::TYPE_UNSIGNED_INT;
            break;
            case dmBuffer::VALUE_TYPE_INT8:
                return dmGraphics::TYPE_BYTE;
            break;
            case dmBuffer::VALUE_TYPE_INT16:
                return dmGraphics::TYPE_SHORT;
            break;
            case dmBuffer::VALUE_TYPE_INT32:
                return dmGraphics::TYPE_INT;
            break;
            case dmBuffer::VALUE_TYPE_FLOAT32:
                return dmGraphics::TYPE_FLOAT;
            break;
            // case dmBuffer::VALUE_TYPE_UINT64:
            // case dmBuffer::VALUE_TYPE_INT64:
            default:
                return dmGraphics::TYPE_BYTE;
        }
    }

    static dmGraphics::PrimitiveType ToGraphicsPrimitiveType(dmMeshDDF::MeshDesc::PrimitiveType primitive_type)
    {
        switch (primitive_type)
        {
            case dmMeshDDF::MeshDesc::PRIMITIVE_LINES:          return dmGraphics::PRIMITIVE_LINES;
            case dmMeshDDF::MeshDesc::PRIMITIVE_TRIANGLES:      return dmGraphics::PRIMITIVE_TRIANGLES;
            case dmMeshDDF::MeshDesc::PRIMITIVE_TRIANGLE_STRIP: return dmGraphics::PRIMITIVE_TRIANGLE_STRIP;
            default:                                            assert(0 && "Unsupported primitive_type");
        }
        return (dmGraphics::PrimitiveType) -1;
    }

    bool BuildVertexDeclaration(BufferResource* buffer_resource, dmGraphics::HVertexDeclaration* out_vert_decl)
    {
        #define CHECK_BUFFER_RESULT_OR_RETURN(res) \
            if (res != dmBuffer::RESULT_OK) \
                return false;

        assert(buffer_resource);

        dmBuffer::HBuffer buffer = buffer_resource->m_Buffer;

        uint32_t stream_count;
        dmBuffer::Result buffer_res = dmBuffer::GetNumStreams(buffer, &stream_count);
        CHECK_BUFFER_RESULT_OR_RETURN(buffer_res);
        dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(g_GraphicsContext);

        for (uint32_t i = 0; i < stream_count; ++i)
        {
            dmhash_t stream_name;
            buffer_res = dmBuffer::GetStreamName(buffer, i, &stream_name);
            CHECK_BUFFER_RESULT_OR_RETURN(buffer_res);

            dmBuffer::ValueType stream_value_type;
            uint32_t stream_value_count;
            buffer_res = dmBuffer::GetStreamType(buffer, stream_name, &stream_value_type, &stream_value_count);
            CHECK_BUFFER_RESULT_OR_RETURN(buffer_res);

            if (!IsBufferTypeSupportedGraphicsType(stream_value_type)) {
                dmLogError("Value type for stream %s is not supported.", dmHashReverseSafe64(stream_name));
                dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);
                return false;
            }

            dmGraphics::AddVertexStream(stream_declaration, stream_name, stream_value_count, BufferValueTypeToGraphicsType(stream_value_type), false);
        }

        // Get correct "struct stride/size", since dmBuffer might align the structs etc.
        uint32_t stride = dmBuffer::GetStructSize(buffer);

        // Init vertex declaration
        *out_vert_decl = dmGraphics::NewVertexDeclaration(g_GraphicsContext, stream_declaration, stride);
        dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);

        // Update vertex declaration with exact offsets (since streams in buffers can be aligned).
        for (uint32_t i = 0; i < stream_count; ++i)
        {
            uint32_t offset = 0;
            buffer_res = dmBuffer::GetStreamOffset(buffer, i, &offset);
            CHECK_BUFFER_RESULT_OR_RETURN(buffer_res)

            bool b2 = dmGraphics::SetStreamOffset(*out_vert_decl, i, offset);
            assert(b2);
        }

        #undef CHECK_BUFFER_RESULT_OR_RETURN

        return true;
    }

    static bool BuildVertices(MeshResource* mesh_resource)
    {
        BufferResource* br = mesh_resource->m_BufferResource;
        assert(mesh_resource);
        assert(br);

        // Cleanup if we are rebuilding
        if (mesh_resource->m_VertexBuffer) {
            dmGraphics::DeleteVertexBuffer(mesh_resource->m_VertexBuffer);
            mesh_resource->m_VertexBuffer = 0x0;
        }
        if (mesh_resource->m_VertexDeclaration) {
            dmGraphics::DeleteVertexDeclaration(mesh_resource->m_VertexDeclaration);
            mesh_resource->m_VertexDeclaration = 0x0;
        }

        mesh_resource->m_PrimitiveType = ToGraphicsPrimitiveType(mesh_resource->m_MeshDDF->m_PrimitiveType);

        bool vert_decl_res = BuildVertexDeclaration(br, &mesh_resource->m_VertexDeclaration);
        if (!vert_decl_res) {
            dmLogError("Could not create vertex declaration from buffer resource.");
            return false;
        }

        // Get buffer data and upload/send to dmGraphics.
        uint8_t* bytes = 0x0;
        uint32_t size = 0;
        dmBuffer::Result r = dmBuffer::GetBytes(br->m_Buffer, (void**)&bytes, &size);
        if (r != dmBuffer::RESULT_OK) {
            dmLogError("Could not get bytes from buffer.");
            return false;
        }

        mesh_resource->m_VertexBuffer = dmGraphics::NewVertexBuffer(g_GraphicsContext, br->m_Stride * br->m_ElementCount, bytes, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

        return true;
    }

    static bool CopyVertexStreams(BufferResource* dst_resource, const BufferResource* src_resource)
    {
        dmBuffer::HBuffer dst_buffer = dst_resource->m_Buffer;
        dmBuffer::HBuffer src_buffer = src_resource->m_Buffer;
        uint32_t dst_count = 0;
        uint32_t src_count = 0;
        uint32_t stream_count = 0;
        if (dmBuffer::GetCount(dst_buffer, &dst_count) != dmBuffer::RESULT_OK ||
            dmBuffer::GetCount(src_buffer, &src_count) != dmBuffer::RESULT_OK ||
            dmBuffer::GetNumStreams(dst_buffer, &stream_count) != dmBuffer::RESULT_OK ||
            src_count < dst_count)
        {
            return false;
        }

        for (uint32_t i = 0; i < stream_count; ++i)
        {
            dmhash_t stream_name = 0;
            dmBuffer::ValueType dst_type;
            dmBuffer::ValueType src_type;
            uint32_t dst_components = 0;
            uint32_t src_components = 0;
            if (dmBuffer::GetStreamName(dst_buffer, i, &stream_name) != dmBuffer::RESULT_OK ||
                dmBuffer::GetStreamType(dst_buffer, stream_name, &dst_type, &dst_components) != dmBuffer::RESULT_OK ||
                dmBuffer::GetStreamType(src_buffer, stream_name, &src_type, &src_components) != dmBuffer::RESULT_OK ||
                dst_type != src_type || dst_components != src_components)
            {
                return false;
            }

            void* dst_data = 0;
            void* src_data = 0;
            uint32_t actual_dst_count = 0;
            uint32_t actual_src_count = 0;
            uint32_t actual_dst_components = 0;
            uint32_t actual_src_components = 0;
            uint32_t dst_stride = 0;
            uint32_t src_stride = 0;
            if (dmBuffer::GetStream(dst_buffer, stream_name, &dst_data, &actual_dst_count, &actual_dst_components, &dst_stride) != dmBuffer::RESULT_OK ||
                dmBuffer::GetStream(src_buffer, stream_name, &src_data, &actual_src_count, &actual_src_components, &src_stride) != dmBuffer::RESULT_OK ||
                actual_dst_count != dst_count || actual_src_count < dst_count ||
                actual_dst_components != dst_components || actual_src_components != dst_components)
            {
                return false;
            }

            const uint32_t value_size = dmBuffer::GetSizeForValueType(dst_type);
            const uint32_t element_size = value_size * dst_components;
            for (uint32_t element = 0; element < dst_count; ++element)
            {
                memcpy((uint8_t*) dst_data + element * dst_stride * value_size,
                       (const uint8_t*) src_data + element * src_stride * value_size,
                       element_size);
            }
        }

        dmBuffer::UpdateContentVersion(dst_buffer);
        dmBuffer::GetContentVersion(dst_buffer, &dst_resource->m_Version);
        return true;
    }

    bool SyncMeshVertexBuffer(MeshResource* mesh_resource)
    {
        if (!mesh_resource || !mesh_resource->m_SourceBufferResource ||
            !mesh_resource->m_SourceBufferResource->m_Buffer ||
            !mesh_resource->m_BufferResource || !mesh_resource->m_BufferResource->m_Buffer)
        {
            return false;
        }

        dmBuffer::HBuffer source_buffer = mesh_resource->m_SourceBufferResource->m_Buffer;
        uint32_t source_version = 0;
        if (dmBuffer::GetContentVersion(source_buffer, &source_version) != dmBuffer::RESULT_OK)
            return false;

        if (mesh_resource->m_SourceBuffer == source_buffer && mesh_resource->m_BufferVersion == source_version)
            return true;

        // Record the source state even on failure so an incompatible source
        // buffer does not produce the same warning every frame.
        mesh_resource->m_SourceBuffer = source_buffer;
        mesh_resource->m_BufferVersion = source_version;
        if (!CopyVertexStreams(mesh_resource->m_BufferResource, mesh_resource->m_SourceBufferResource))
        {
            dmLogWarning("Unable to synchronize the mesh vertex streams from '%s'.", mesh_resource->m_MeshDDF->m_Vertices);
            return false;
        }

        uint8_t* bytes = 0;
        uint32_t size = 0;
        if (dmBuffer::GetBytes(mesh_resource->m_BufferResource->m_Buffer, (void**) &bytes, &size) != dmBuffer::RESULT_OK)
        {
            dmLogWarning("Reading the synchronized mesh vertex buffer failed.");
            return false;
        }
        if (mesh_resource->m_VertexBuffer)
        {
            dmGraphics::SetVertexBufferData(mesh_resource->m_VertexBuffer,
                                            size,
                                            bytes,
                                            dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        }
        return true;
    }

    dmResource::Result AcquireResources(dmGraphics::HContext context, dmResource::HFactory factory, MeshResource* resource, const char* filename)
    {
        dmResource::Result result = dmResource::Get(factory, resource->m_MeshDDF->m_Material, (void**) &resource->m_Material);
        if (result != dmResource::RESULT_OK)
            return result;

        result = dmResource::Get(factory, resource->m_MeshDDF->m_Vertices, (void**) &resource->m_SourceBufferResource);
        if (result != dmResource::RESULT_OK) {
            dmResource::Release(factory, resource->m_Material);
            resource->m_Material = 0;
            return result;
        }

        TextureResource* textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(TextureResource*));

        RenderTargetResource* render_targets[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        memset(render_targets, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(RenderTargetResource*));

        for (uint32_t i = 0; i < resource->m_MeshDDF->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            const char* texture_path = resource->m_MeshDDF->m_Textures[i];
            if (*texture_path != 0)
            {
                TextureResource* texture_res;
                dmResource::Result r = dmResource::Get(factory, texture_path, (void**) &texture_res);

                dmRender::RenderResourceType render_res_type = ResourcePathToRenderResourceType(texture_path);

                if (render_res_type == dmRender::RENDER_RESOURCE_TYPE_RENDER_TARGET)
                {
                    render_targets[i] = (RenderTargetResource*) texture_res;
                    textures[i]       = render_targets[i]->m_ColorAttachmentResources[0];
                }
                else
                {
                    textures[i] = texture_res;
                }

                if (r != dmResource::RESULT_OK)
                {
                    if (result == dmResource::RESULT_OK)
                    {
                        result = r;
                    }
                }
                else
                {
                    r = dmResource::GetPath(factory, textures[i], &resource->m_TexturePaths[i]);
                    if (r != dmResource::RESULT_OK)
                    {
                       result = r;
                    }
                }
            }
        }
        if (result != dmResource::RESULT_OK)
        {
            dmResource::Release(factory, resource->m_Material);
            dmResource::Release(factory, resource->m_SourceBufferResource);
            resource->m_Material = 0;
            resource->m_SourceBufferResource = 0;
            for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            {
                if (textures[i])
                {
                    if (render_targets[i])
                    {
                        dmResource::Release(factory, (void*) render_targets[i]);
                    }
                    else
                    {
                        dmResource::Release(factory, (void*) textures[i]);
                    }
                }
            }
            return result;
        }
        memcpy(resource->m_Textures, textures, sizeof(TextureResource*) * dmRender::RenderObject::MAX_TEXTURE_COUNT);
        memcpy(resource->m_RenderTargets, render_targets, sizeof(RenderTargetResource*) * dmRender::RenderObject::MAX_TEXTURE_COUNT);

        // Buffer resources can be created with zero elements, in such case
        // the buffer will be null and we cannot create vertices.
        if (resource->m_BufferResource->m_Buffer) {
            BuildVertices(resource);
        }

        resource->m_PositionStreamId = dmHashString64(resource->m_MeshDDF->m_PositionStream);
        resource->m_NormalStreamId = dmHashString64(resource->m_MeshDDF->m_NormalStream);

        BufferResource* buffer_resource = resource->m_BufferResource;
        uint32_t stream_count = buffer_resource->m_BufferDDF->m_Streams.m_Count;
        for (uint32_t i = 0; i < stream_count; ++i)
        {
        	dmhash_t stream_id = dmHashString64(buffer_resource->m_BufferDDF->m_Streams[i].m_Name);
        	if (stream_id == resource->m_PositionStreamId) {
        		resource->m_PositionStreamType = buffer_resource->m_BufferDDF->m_Streams[i].m_ValueType;
        	} else if (stream_id == resource->m_NormalStreamId) {
                resource->m_NormalStreamType = buffer_resource->m_BufferDDF->m_Streams[i].m_ValueType;
            }
        }

        return result;
    }

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams* params)
    {
        MeshResource* mesh_resource = (MeshResource*) params->m_UserData;
        SyncMeshVertexBuffer(mesh_resource);
    }

    static void ReleaseResources(dmResource::HFactory factory, MeshResource* resource)
    {
        if (resource->m_MeshDDF != 0x0)
            dmDDF::FreeMessage(resource->m_MeshDDF);
        resource->m_MeshDDF = 0x0;

        if (resource->m_Material != 0x0)
            dmResource::Release(factory, resource->m_Material);
        resource->m_Material = 0x0;

        if (resource->m_SourceBufferResource != 0x0)
        {
            dmResource::Release(factory, resource->m_SourceBufferResource);
            resource->m_SourceBufferResource = 0x0;
        }

        if (resource->m_BufferResource != 0x0)
        {
            DestroyBufferResource(resource->m_BufferResource);
            resource->m_BufferResource = 0x0;
        }

        if (resource->m_IndexBufferResource != 0x0)
        {
            if (resource->m_IndexBufferResource->m_Buffer)
                dmBuffer::Destroy(resource->m_IndexBufferResource->m_Buffer);
            delete resource->m_IndexBufferResource;
            resource->m_IndexBufferResource = 0x0;
        }

        if (resource->m_VertexDeclaration)
        {
            dmGraphics::DeleteVertexDeclaration(resource->m_VertexDeclaration);
            resource->m_VertexDeclaration = 0;
        }

        if (resource->m_VertexBuffer)
        {
            dmGraphics::DeleteVertexBuffer(resource->m_VertexBuffer);
            resource->m_VertexBuffer = 0;
        }

        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (resource->m_Textures[i])
            {
                if (resource->m_RenderTargets[i])
                {
                    dmResource::Release(factory, (void*)resource->m_RenderTargets[i]);
                }
                else
                {
                    dmResource::Release(factory, (void*)resource->m_Textures[i]);
                }
            }
            resource->m_Textures[i] = 0x0;
            resource->m_RenderTargets[i] = 0x0;
            resource->m_TexturePaths[i] = 0;
        }
    }

    dmResource::Result ResMeshPreload(const dmResource::ResourcePreloadParams* params)
    {
        MeshPreloadData* preload_data = 0;
        dmResource::Result result = LoadMeshData(params->m_Buffer, params->m_BufferSize, &preload_data);
        if (result != dmResource::RESULT_OK)
            return result;
        dmMeshDDF::MeshDesc* ddf = preload_data->m_DDF;

        dmResource::PreloadHint(params->m_HintInfo, ddf->m_Material);
        dmResource::PreloadHint(params->m_HintInfo, ddf->m_Vertices);
        for (uint32_t i = 0; i < ddf->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            dmResource::PreloadHint(params->m_HintInfo, ddf->m_Textures[i]);
        }

        *params->m_PreloadData = preload_data;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMeshCreate(const dmResource::ResourceCreateParams* params)
    {
        // FIXME: Not very nice to keep a global reference to the graphics context...
        // Needed by the reload callback since we need to rebuild the vertex declaration and vertbuffer.
        g_GraphicsContext = (dmGraphics::HContext) params->m_Context;

        MeshResource* mesh_resource = new MeshResource();
        memset(mesh_resource, 0, sizeof(MeshResource));
        MeshPreloadData* preload_data = (MeshPreloadData*) params->m_PreloadData;
        mesh_resource->m_MeshDDF = preload_data->m_DDF;
        preload_data->m_DDF = 0;
        dmBufferDDF::BufferDesc* vertex_buffer_ddf = preload_data->m_VertexBufferDDF;
        preload_data->m_VertexBufferDDF = 0;
        bool vertex_buffer_result = BuildVertexBufferResource(mesh_resource, vertex_buffer_ddf, params->m_Filename);
        bool index_buffer_result = vertex_buffer_result && BuildIndexBufferResource(mesh_resource, preload_data->m_IndexData, preload_data->m_IndexDataSize, params->m_Filename);
        FreePreloadData(preload_data);
        dmResource::Result r = vertex_buffer_result && index_buffer_result
                             ? AcquireResources((dmGraphics::HContext) params->m_Context, params->m_Factory, mesh_resource, params->m_Filename)
                             : dmResource::RESULT_INVALID_DATA;
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, mesh_resource);
        }
        else
        {
            ReleaseResources(params->m_Factory, mesh_resource);
            delete mesh_resource;
        }

        if (r == dmResource::RESULT_OK)
        {
            mesh_resource->m_SourceBuffer = mesh_resource->m_SourceBufferResource->m_Buffer;
            dmBuffer::GetContentVersion(mesh_resource->m_SourceBuffer, &mesh_resource->m_BufferVersion);
            dmResource::RegisterResourceReloadedCallback(params->m_Factory, ResourceReloadedCallback, mesh_resource);
        }
        return r;
    }

    dmResource::Result ResMeshDestroy(const dmResource::ResourceDestroyParams* params)
    {
        MeshResource* mesh_resource = (MeshResource*)dmResource::GetResource(params->m_Resource);
        dmResource::UnregisterResourceReloadedCallback(params->m_Factory, ResourceReloadedCallback, mesh_resource);
        ReleaseResources(params->m_Factory, mesh_resource);
        delete mesh_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMeshRecreate(const dmResource::ResourceRecreateParams* params)
    {
        MeshPreloadData* preload_data = 0;
        dmResource::Result result = LoadMeshData(params->m_Buffer, params->m_BufferSize, &preload_data);
        if (result != dmResource::RESULT_OK)
            return result;
        MeshResource* mesh_resource = (MeshResource*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, mesh_resource);
        mesh_resource->m_MeshDDF = preload_data->m_DDF;
        preload_data->m_DDF = 0;
        dmBufferDDF::BufferDesc* vertex_buffer_ddf = preload_data->m_VertexBufferDDF;
        preload_data->m_VertexBufferDDF = 0;
        bool vertex_buffer_result = BuildVertexBufferResource(mesh_resource, vertex_buffer_ddf, params->m_Filename);
        bool index_buffer_result = vertex_buffer_result && BuildIndexBufferResource(mesh_resource, preload_data->m_IndexData, preload_data->m_IndexDataSize, params->m_Filename);
        FreePreloadData(preload_data);
        if (!vertex_buffer_result || !index_buffer_result)
            return dmResource::RESULT_INVALID_DATA;
        result = AcquireResources((dmGraphics::HContext) params->m_Context, params->m_Factory, mesh_resource, params->m_Filename);
        if (result == dmResource::RESULT_OK)
        {
            mesh_resource->m_SourceBuffer = mesh_resource->m_SourceBufferResource->m_Buffer;
            dmBuffer::GetContentVersion(mesh_resource->m_SourceBuffer, &mesh_resource->m_BufferVersion);
        }
        return result;
    }
}
