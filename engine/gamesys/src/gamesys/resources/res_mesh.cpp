// Copyright 2020-2025 The Defold Foundation
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

    dmResource::Result AcquireResources(dmGraphics::HContext context, dmResource::HFactory factory, MeshResource* resource, const char* filename)
    {
        dmResource::Result result = dmResource::Get(factory, resource->m_MeshDDF->m_Material, (void**) &resource->m_Material);
        if (result != dmResource::RESULT_OK)
            return result;

        result = dmResource::Get(factory, resource->m_MeshDDF->m_Vertices, (void**) &resource->m_BufferResource);
        if (result != dmResource::RESULT_OK) {
            dmResource::Release(factory, (void*) resource->m_MeshDDF->m_Material);
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
            dmResource::Release(factory, (void*) resource->m_MeshDDF->m_Material);
            dmResource::Release(factory, (void*) resource->m_MeshDDF->m_Vertices);
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

        if (mesh_resource->m_BufferVersion != mesh_resource->m_BufferResource->m_Version)
        {
            if (!BuildVertices(mesh_resource))
            {
                dmLogWarning("Reloading the mesh failed, there might be rendering errors.");
            }
            mesh_resource->m_BufferVersion = mesh_resource->m_BufferResource->m_Version;
        }
    }

    static void ReleaseResources(dmResource::HFactory factory, MeshResource* resource)
    {
        if (resource->m_MeshDDF != 0x0)
            dmDDF::FreeMessage(resource->m_MeshDDF);

        if (resource->m_Material != 0x0)
            dmResource::Release(factory, resource->m_Material);

        if (resource->m_BufferResource != 0x0)
            dmResource::Release(factory, resource->m_BufferResource);

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
        }
    }

    dmResource::Result ResMeshPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmMeshDDF::MeshDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmMeshDDF_MeshDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(params->m_HintInfo, ddf->m_Material);
        dmResource::PreloadHint(params->m_HintInfo, ddf->m_Vertices);
        for (uint32_t i = 0; i < ddf->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            dmResource::PreloadHint(params->m_HintInfo, ddf->m_Textures[i]);
        }

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMeshCreate(const dmResource::ResourceCreateParams* params)
    {
        // FIXME: Not very nice to keep a global reference to the graphics context...
        // Needed by the reload callback since we need to rebuild the vertex declaration and vertbuffer.
        g_GraphicsContext = (dmGraphics::HContext) params->m_Context;

        MeshResource* mesh_resource = new MeshResource();
        memset(mesh_resource, 0, sizeof(MeshResource));
        mesh_resource->m_MeshDDF = (dmMeshDDF::MeshDesc*) params->m_PreloadData;
        dmResource::Result r = AcquireResources((dmGraphics::HContext) params->m_Context, params->m_Factory, mesh_resource, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, mesh_resource);
        }
        else
        {
            ReleaseResources(params->m_Factory, mesh_resource);
            delete mesh_resource;
        }

        mesh_resource->m_BufferVersion = mesh_resource->m_BufferResource->m_Version;

        dmResource::RegisterResourceReloadedCallback(params->m_Factory, ResourceReloadedCallback, mesh_resource);
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
        dmMeshDDF::MeshDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmMeshDDF_MeshDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        MeshResource* mesh_resource = (MeshResource*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, mesh_resource);
        mesh_resource->m_MeshDDF = ddf;
        return AcquireResources((dmGraphics::HContext) params->m_Context, params->m_Factory, mesh_resource, params->m_Filename);
    }
}
