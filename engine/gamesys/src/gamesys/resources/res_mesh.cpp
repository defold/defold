#include "res_mesh.h"

#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/dstrings.h>
#include <dlib/memory.h>

#include "gamesys.h"

namespace dmGameSystem
{
    // static void ReleaseVertexBuffer(MeshContext* context, MeshResource* resource)
    // {
    //     dmGraphics::SetVertexBufferData(resource->m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
    //     dmGraphics::DeleteVertexBuffer(resource->m_VertexBuffer);
    //     dmGraphics::DeleteVertexDeclaration(resource->m_VertexDeclaration);
    // }

    // static void CreateVertexBuffer(MeshContext* context, MeshResource* resource)
    // {
    //     if (resource->m_VertexBuffer != 0x0)
    //     {
    //         ReleaseVertexBuffer(context, resource);
    //     }

    //     void* data = (void*)resource->m_Mesh->m_DataPtr;
    //     uint64_t data_size = resource->m_Mesh->m_DataSize;
    //     dmGraphics::VertexElement* vert_decl = (dmGraphics::VertexElement*)resource->m_Mesh->m_VertDeclPtr;
    //     uint32_t vert_decl_count = resource->m_Mesh->m_VertDeclCount;
    //     uint64_t elem_count = resource->m_Mesh->m_ElemCount;

    //     resource->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(context->m_RenderContext), vert_decl, vert_decl_count);
    //     resource->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(context->m_RenderContext), 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    //     dmGraphics::SetVertexBufferData(resource->m_VertexBuffer, data_size, data, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
    // }

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

        // Setup vertex declaration
        // dmGraphics::VertexElement ve[] =
        // {
        //         {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
        //         {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT, false},
        // };
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

        mesh_resource->m_VertSize = vert_size;

        // Init vertex declaration
        // sprite_world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(render_context), ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));
        mesh_resource->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(context, vert_decls, stream_count);

        // Create vertex buffer handles
        // sprite_world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), 0, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        // sprite_world->m_VertexBufferData = (SpriteVertex*) malloc(sizeof(SpriteVertex) * 4 * sprite_world->m_Components.Capacity());
        // mesh_resource->m_VertexBufferData = malloc(vert_size * buffer_resource->m_ElementCount);

        // Fill vertex data

        // {
        //     uint32_t vertex_count = 4*sprite_context->m_MaxSpriteCount;
        //     uint32_t size_type = vertex_count <= 65536 ? sizeof(uint16_t) : sizeof(uint32_t);
        //     sprite_world->m_Is16BitIndex = size_type == sizeof(uint16_t) ? 1 : 0;

        //     uint32_t indices_count = 6*sprite_context->m_MaxSpriteCount;
        //     size_t indices_size = indices_count * size_type;
        //     void* indices = (void*)malloc(indices_count * size_type);
        //     if (sprite_world->m_Is16BitIndex) {
        //         uint16_t* index = (uint16_t*)indices;
        //         for(uint32_t i = 0, v = 0; i < indices_count; i += 6, v += 4)
        //         {
        //             *index++ = v;
        //             *index++ = v+1;
        //             *index++ = v+2;
        //             *index++ = v+2;
        //             *index++ = v+3;
        //             *index++ = v;
        //         }

        uint8_t* bytes = 0x0;
        uint32_t size = 0;
        dmBuffer::Result r = dmBuffer::GetBytes(buffer_resource->m_Buffer, (void**)&bytes, &size);
        if (r != dmBuffer::RESULT_OK) {
            free(vert_decls);
            dmLogError("Could not get bytes from buffer!");
            return false;
        }

        assert(size == mesh_resource->m_VertSize * buffer_resource->m_ElementCount);
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

        // if(dmRender::GetMaterialVertexSpace(resource->m_Material) ==  dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL)
        // {
        //     if(resource->m_RigScene->m_AnimationSetRes || resource->m_RigScene->m_SkeletonRes)
        //     {
        //         dmLogError("Failed to create Model component. Material vertex space option VERTEX_SPACE_LOCAL does not support skinning.");
        //         return dmResource::RESULT_NOT_SUPPORTED;
        //     }
        //     dmRigDDF::MeshSet* mesh_set = resource->m_RigScene->m_MeshSetRes->m_MeshSet;
        //     if(mesh_set)
        //     {
        //         if(mesh_set->m_MeshEntries.m_Count && mesh_set->m_MeshAttachments.m_Count)
        //         {
        //             CreateGPUBuffers(context, resource, mesh_set->m_MeshAttachments[0]);
        //         }
        //     }
        // }

        BuildVertices(context, resource);

        return result;
    }

    static void ReleaseResources(dmResource::HFactory factory, MeshResource* resource)
    {
        // if (resource->m_VertexBuffer != 0x0)
        // {
        //     dmGraphics::DeleteVertexBuffer(resource->m_VertexBuffer);
        //     resource->m_VertexBuffer = 0x0;
        // }
        // if (resource->m_IndexBuffer != 0x0)
        // {
        //     dmGraphics::DeleteVertexBuffer(resource->m_IndexBuffer);
        //     resource->m_IndexBuffer = 0x0;
        //     resource->m_ElementCount = 0;
        // }
        if (resource->m_MeshDDF != 0x0)
            dmDDF::FreeMessage(resource->m_MeshDDF);
        resource->m_MeshDDF = 0x0;
        // if (resource->m_RigScene != 0x0)
        //     dmResource::Release(factory, resource->m_RigScene);
        // resource->m_RigScene = 0x0;
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
        return r;
    }

    dmResource::Result ResMeshDestroy(const dmResource::ResourceDestroyParams& params)
    {
        MeshResource* mesh_resource = (MeshResource*)params.m_Resource->m_Resource;
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
