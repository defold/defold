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


    dmResource::Result AcquireResources(dmGraphics::HContext context, dmResource::HFactory factory, MeshResource* resource, const char* filename)
    {
        dmResource::Result result = dmResource::Get(factory, resource->m_Mesh->m_Material, (void**) &resource->m_Material);
        if (result != dmResource::RESULT_OK)
            return result;

        dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(dmGraphics::HTexture));
        for (uint32_t i = 0; i < resource->m_Mesh->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            const char* texture_path = resource->m_Mesh->m_Textures[i];
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
        if (resource->m_Mesh != 0x0)
            dmDDF::FreeMessage(resource->m_Mesh);
        resource->m_Mesh = 0x0;
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
        dmMeshDDF::Mesh* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmMeshDDF_Mesh_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);
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
        mesh_resource->m_Mesh = (dmMeshDDF::Mesh*) params.m_PreloadData;
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
        dmMeshDDF::Mesh* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmMeshDDF_Mesh_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        MeshResource* mesh_resource = (MeshResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, mesh_resource);
        mesh_resource->m_Mesh = ddf;
        return AcquireResources((dmGraphics::HContext) params.m_Context, params.m_Factory, mesh_resource, params.m_Filename);
    }
}
