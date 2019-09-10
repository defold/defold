#include "res_buffer.h"

#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/dstrings.h>
#include <dlib/memory.h>

#include <render/render.h>

namespace dmGameSystem
{
    // dmResource::Result AcquireResources(dmGraphics::HContext context, dmResource::HFactory factory, BufferResource* resource, const char* filename)
    // {
    //     dmResource::Result result = dmResource::Get(factory, resource->m_BufferDDF->m_Vertices, (void**) &resource->m_Vertices);
    //     if (result != dmResource::RESULT_OK)
    //         return result;

    //     result = dmResource::Get(factory, resource->m_BufferDDF->m_Material, (void**) &resource->m_Material);
    //     if (result != dmResource::RESULT_OK)
    //         return result;

    //     dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
    //     memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(dmGraphics::HTexture));
    //     for (uint32_t i = 0; i < resource->m_BufferDDF->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
    //     {
    //         const char* texture_path = resource->m_BufferDDF->m_Textures[i];
    //         if (*texture_path != 0)
    //         {
    //             dmResource::Result r = dmResource::Get(factory, texture_path, (void**) &textures[i]);
    //             if (r != dmResource::RESULT_OK)
    //             {
    //                 if (result == dmResource::RESULT_OK) {
    //                     result = r;
    //                 }
    //             } else {
    //                 r = dmResource::GetPath(factory, textures[i], &resource->m_TexturePaths[i]);
    //                 if (r != dmResource::RESULT_OK) {
    //                    result = r;
    //                 }
    //             }
    //         }
    //     }
    //     if (result != dmResource::RESULT_OK)
    //     {
    //         for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
    //             if (textures[i]) dmResource::Release(factory, (void*) textures[i]);
    //         return result;
    //     }
    //     memcpy(resource->m_Textures, textures, sizeof(dmGraphics::HTexture) * dmRender::RenderObject::MAX_TEXTURE_COUNT);

    //     if(dmRender::GetMaterialVertexSpace(resource->m_Material) ==  dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL)
    //     {
    //         if(resource->m_RigScene->m_AnimationSetRes || resource->m_RigScene->m_SkeletonRes)
    //         {
    //             dmLogError("Failed to create Model component. Material vertex space option VERTEX_SPACE_LOCAL does not support skinning.");
    //             return dmResource::RESULT_NOT_SUPPORTED;
    //         }
    //         dmRigDDF::MeshSet* mesh_set = resource->m_RigScene->m_MeshSetRes->m_MeshSet;
    //         if(mesh_set)
    //         {
    //             if(mesh_set->m_MeshEntries.m_Count && mesh_set->m_MeshAttachments.m_Count)
    //             {
    //                 CreateGPUBuffers(context, resource, mesh_set->m_MeshAttachments[0]);
    //             }
    //         }
    //     }

    //     return result;
    // }

    static void ReleaseResources(dmResource::HFactory factory, BufferResource* resource)
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
        if (resource->m_BufferDDF != 0x0)
            dmDDF::FreeMessage(resource->m_BufferDDF);
        resource->m_BufferDDF = 0x0;
        // if (resource->m_RigScene != 0x0)
        //     dmResource::Release(factory, resource->m_RigScene);
        // resource->m_RigScene = 0x0;
        // if (resource->m_Material != 0x0)
        //     dmResource::Release(factory, resource->m_Material);
        // resource->m_Material = 0x0;
        // for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        // {
        //     if (resource->m_Textures[i])
        //         dmResource::Release(factory, (void*)resource->m_Textures[i]);
        //     resource->m_Textures[i] = 0x0;
        // }
    }

    dmResource::Result ResBufferPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmBufferDDF::BufferDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmBufferDDF_BufferDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        // dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);
        // dmResource::PreloadHint(params.m_HintInfo, ddf->m_Vertices);
        // for (uint32_t i = 0; i < ddf->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        // {
        //     dmResource::PreloadHint(params.m_HintInfo, ddf->m_Textures[i]);
        // }

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResBufferCreate(const dmResource::ResourceCreateParams& params)
    {
        BufferResource* buffer_resource = new BufferResource();
        memset(buffer_resource, 0, sizeof(BufferResource));
        buffer_resource->m_BufferDDF = (dmBufferDDF::BufferDesc*) params.m_PreloadData;
        params.m_Resource->m_Resource = (void*) buffer_resource;
        // dmResource::Result r = AcquireResources((dmGraphics::HContext) params.m_Context, params.m_Factory, buffer_resource, params.m_Filename);
        // if (r == dmResource::RESULT_OK)
        // {
        //     params.m_Resource->m_Resource = (void*) buffer_resource;
        // }
        // else
        // {
        //     ReleaseResources(params.m_Factory, buffer_resource);
        //     delete buffer_resource;
        // }
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResBufferDestroy(const dmResource::ResourceDestroyParams& params)
    {
        BufferResource* buffer_resource = (BufferResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, buffer_resource);
        delete buffer_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResBufferRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmBufferDDF::BufferDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmBufferDDF_BufferDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        BufferResource* buffer_resource = (BufferResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, buffer_resource);
        buffer_resource->m_BufferDDF = ddf;
        // return AcquireResources((dmGraphics::HContext) params.m_Context, params.m_Factory, buffer_resource, params.m_Filename);
        return dmResource::RESULT_OK;
    }
}
