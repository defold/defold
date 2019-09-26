#include "res_mesh.h"

#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/dstrings.h>
#include <dlib/memory.h>

#include "gamesys.h"

namespace dmGameSystem
{
    static dmGraphics::HContext g_GraphicsContext = 0x0;

    static dmGraphics::Type StreamTypeToGraphicsType(dmBufferDDF::ValueType value_type)
    {
        switch (value_type)
        {
            case dmBufferDDF::VALUE_TYPE_UINT8:
                return dmGraphics::TYPE_UNSIGNED_BYTE;
            break;
            case dmBufferDDF::VALUE_TYPE_UINT16:
                return dmGraphics::TYPE_UNSIGNED_SHORT;
            break;
            case dmBufferDDF::VALUE_TYPE_UINT32:
                return dmGraphics::TYPE_UNSIGNED_INT;
            break;
            case dmBufferDDF::VALUE_TYPE_INT8:
                return dmGraphics::TYPE_BYTE;
            break;
            case dmBufferDDF::VALUE_TYPE_INT16:
                return dmGraphics::TYPE_SHORT;
            break;
            case dmBufferDDF::VALUE_TYPE_INT32:
                return dmGraphics::TYPE_INT;
            break;
            case dmBufferDDF::VALUE_TYPE_FLOAT32:
                return dmGraphics::TYPE_FLOAT;
            break;

            // case dmBufferDDF::VALUE_TYPE_UINT64:
            // case dmBufferDDF::VALUE_TYPE_INT64:
            default:
                assert(false && "Unknown value type!");
                return dmGraphics::TYPE_BYTE;
        }
    }

    static size_t StreamTypeToSize(dmBufferDDF::ValueType value_type)
    {
        switch (value_type)
        {
            case dmBufferDDF::VALUE_TYPE_UINT8:
                return sizeof(uint8_t);
            break;
            case dmBufferDDF::VALUE_TYPE_UINT16:
                return sizeof(uint16_t);
            break;
            case dmBufferDDF::VALUE_TYPE_UINT32:
                return sizeof(uint32_t);
            break;
            case dmBufferDDF::VALUE_TYPE_INT8:
                return sizeof(int8_t);
            break;
            case dmBufferDDF::VALUE_TYPE_INT16:
                return sizeof(int16_t);
            break;
            case dmBufferDDF::VALUE_TYPE_INT32:
                return sizeof(int32_t);
            break;
            case dmBufferDDF::VALUE_TYPE_FLOAT32:
                return sizeof(float);
            break;

            // case dmBufferDDF::VALUE_TYPE_UINT64:
            // case dmBufferDDF::VALUE_TYPE_INT64:
            default:
                assert(false && "Unknown value type!");
                return 0;
        }
    }

    static bool BuildVertices(dmGraphics::HContext context,  MeshResource* mesh_resource)
    {
        assert(mesh_resource);
        assert(mesh_resource->m_BufferResource);
        const BufferResource* buffer_resource = mesh_resource->m_BufferResource;

        const uint32_t stream_count = buffer_resource->m_BufferDDF->m_Streams.m_Count;
        dmGraphics::VertexElement* vert_decls = (dmGraphics::VertexElement*)malloc(stream_count * sizeof(dmGraphics::VertexElement));

        uint32_t vert_size = 0;
        for (uint32_t i = 0; i < stream_count; ++i)
        {
            const dmBufferDDF::StreamDesc& ddf_stream = buffer_resource->m_BufferDDF->m_Streams[i];
            dmGraphics::VertexElement& vert_decl = vert_decls[i];
            vert_decl.m_Name = ddf_stream.m_Name;
            vert_decl.m_Stream = i;
            vert_decl.m_Size = ddf_stream.m_ValueCount;
            vert_decl.m_Type = StreamTypeToGraphicsType(ddf_stream.m_ValueType);
            vert_decl.m_Normalize = false;

            vert_size += StreamTypeToSize(ddf_stream.m_ValueType) * ddf_stream.m_ValueCount;
        }

        // Init vertex declaration
        mesh_resource->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(context, vert_decls, stream_count);
        free(vert_decls);

        // Update vertex declaration with exact offsets (since streams in buffers can be aligned).
        for (uint32_t i = 0; i < stream_count; ++i)
        {
            uint32_t offset = 0;
            dmBuffer::Result r = dmBuffer::GetStreamOffset(buffer_resource->m_Buffer, i, &offset);
            assert(r == dmBuffer::RESULT_OK);

            bool b2 = dmGraphics::SetStreamOffset(mesh_resource->m_VertexDeclaration, i, offset);
            assert(b2);
        }

        // Set correct "struct stride/size", since dmBuffer might align the structs etc.
        uint32_t stride = dmBuffer::GetStructSize(buffer_resource->m_Buffer);
        bool vert_stride_res = dmGraphics::SetVertexStride(mesh_resource->m_VertexDeclaration, stride);
        if (!vert_stride_res) {
            dmLogError("Could not get vertex element size for vertex buffer.");
            return false;
        }

        // We need to keep track of the exact vertex size (ie the "struct" size according to dmBuffer)
        // since for world space vertices we need to allocate a correct data buffer size for it.
        // We also use it when passing the vertsize*elemcount to dmGraphics.
        mesh_resource->m_VertSize = stride;

        // Get buffer data and upload/send to dmGraphics.
        uint8_t* bytes = 0x0;
        uint32_t size = 0;
        dmBuffer::Result r = dmBuffer::GetBytes(buffer_resource->m_Buffer, (void**)&bytes, &size);
        if (r != dmBuffer::RESULT_OK) {
            dmLogError("Could not get bytes from buffer!");
            return false;
        }

        mesh_resource->m_ElementCount = buffer_resource->m_ElementCount;
        mesh_resource->m_VertexBuffer = dmGraphics::NewVertexBuffer(context, mesh_resource->m_VertSize * buffer_resource->m_ElementCount, bytes, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

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

        dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(dmGraphics::HTexture));
        for (uint32_t i = 0; i < resource->m_MeshDDF->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            const char* texture_path = resource->m_MeshDDF->m_Textures[i];
            if (*texture_path != 0)
            {
                dmResource::Result r = dmResource::Get(factory, texture_path, (void**) &textures[i]);
                if (r != dmResource::RESULT_OK)
                {
                    if (result == dmResource::RESULT_OK) {
                        result = r;
                    }
                } else {
                    r = dmResource::GetPath(factory, textures[i], &resource->m_TexturePaths[i]);
                    if (r != dmResource::RESULT_OK) {
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
                if (textures[i]) dmResource::Release(factory, (void*) textures[i]);
            return result;
        }
        memcpy(resource->m_Textures, textures, sizeof(dmGraphics::HTexture) * dmRender::RenderObject::MAX_TEXTURE_COUNT);

        BuildVertices(context, resource);

        resource->m_PositionStreamId = dmHashString64(resource->m_MeshDDF->m_PositionStream);

        BufferResource* buffer_resource = resource->m_BufferResource;
        uint32_t stream_count = buffer_resource->m_BufferDDF->m_Streams.m_Count;
        for (uint32_t i = 0; i < stream_count; ++i)
        {
        	dmhash_t stream_id = dmHashString64(buffer_resource->m_BufferDDF->m_Streams[i].m_Name);
        	if (stream_id == resource->m_PositionStreamId) {
        		resource->m_PositionStreamType = buffer_resource->m_BufferDDF->m_Streams[i].m_ValueType;
        		break;
        	}
        }

        return result;
    }

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params)
    {
        MeshResource* mesh_resource = (MeshResource*) params.m_UserData;

        if (mesh_resource->m_BufferVersion != mesh_resource->m_BufferResource->m_Version) {
            if (!BuildVertices(g_GraphicsContext, mesh_resource)) {
                dmLogWarning("Reloading the mesh failed, there might be rendering errors.");
            }
            mesh_resource->m_BufferVersion = mesh_resource->m_BufferResource->m_Version;
        }
    }

    static void ReleaseResources(dmResource::HFactory factory, MeshResource* resource)
    {
        if (resource->m_MeshDDF != 0x0)
            dmDDF::FreeMessage(resource->m_MeshDDF);
        resource->m_MeshDDF = 0x0;
        if (resource->m_Material != 0x0)
            dmResource::Release(factory, resource->m_Material);
        resource->m_Material = 0x0;
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (resource->m_Textures[i])
                dmResource::Release(factory, (void*)resource->m_Textures[i]);
            resource->m_Textures[i] = 0x0;
        }
    }

    dmResource::Result ResMeshPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmMeshDDF::MeshDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmMeshDDF_MeshDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);
        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Vertices);
        for (uint32_t i = 0; i < ddf->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, ddf->m_Textures[i]);
        }

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMeshCreate(const dmResource::ResourceCreateParams& params)
    {
        // FIXME: Not very nice to keep a global reference to the graphics context...
        // Needed by the reload callback since we need to rebuild the vertex declaration and vertbuffer.
        g_GraphicsContext = (dmGraphics::HContext) params.m_Context;

        MeshResource* mesh_resource = new MeshResource();
        memset(mesh_resource, 0, sizeof(MeshResource));
        mesh_resource->m_MeshDDF = (dmMeshDDF::MeshDesc*) params.m_PreloadData;
        dmResource::Result r = AcquireResources((dmGraphics::HContext) params.m_Context, params.m_Factory, mesh_resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) mesh_resource;
        }
        else
        {
            ReleaseResources(params.m_Factory, mesh_resource);
            delete mesh_resource;
        }

        mesh_resource->m_BufferVersion = mesh_resource->m_BufferResource->m_Version;

        dmResource::RegisterResourceReloadedCallback(params.m_Factory, ResourceReloadedCallback, mesh_resource);
        return r;
    }

    dmResource::Result ResMeshDestroy(const dmResource::ResourceDestroyParams& params)
    {
        MeshResource* mesh_resource = (MeshResource*)params.m_Resource->m_Resource;
        dmResource::UnregisterResourceReloadedCallback(params.m_Factory, ResourceReloadedCallback, mesh_resource);
        ReleaseResources(params.m_Factory, mesh_resource);
        delete mesh_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMeshRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmMeshDDF::MeshDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmMeshDDF_MeshDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        MeshResource* mesh_resource = (MeshResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, mesh_resource);
        mesh_resource->m_MeshDDF = ddf;
        return AcquireResources((dmGraphics::HContext) params.m_Context, params.m_Factory, mesh_resource, params.m_Filename);
    }
}
