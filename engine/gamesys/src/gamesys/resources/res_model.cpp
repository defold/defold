#include "res_model.h"

#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/dstrings.h>
#include <dlib/memory.h>
#include <rig/rig.h>


namespace dmGameSystem
{
    static inline void GetModelVertex(const dmRigDDF::Mesh& mesh, const dmRigDDF::MeshVertexIndices *in, dmRig::RigModelVertex* out)
    {
        const float* v = &mesh.m_Positions[in->m_Position*3];
        out->x = v[0];
        out->y = v[1];
        out->z = v[2];
        v = &mesh.m_Texcoord0[in->m_Texcoord0*2];
        out->u = v[0];
        out->v = v[1];
        v = &mesh.m_Normals[in->m_Normal*3];
        out->nx = v[0];
        out->ny = v[1];
        out->nz = v[2];
    }

    static void CreateGPUBuffers(dmGraphics::HContext context, ModelResource* resource, dmRigDDF::Mesh& mesh)
    {
        if(mesh.m_IndicesFormat == dmRig::INDEXBUFFER_FORMAT_32)
        {
            const uint32_t index_count = mesh.m_Indices.m_Count>>2;
            if(dmGraphics::IsIndexBufferFormatSupported(context, dmGraphics::INDEXBUFFER_FORMAT_32))
            {
                resource->m_IndexBuffer = dmGraphics::NewIndexBuffer(context, mesh.m_Indices.m_Count, mesh.m_Indices.m_Data, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                resource->m_IndexBufferElementType = dmGraphics::TYPE_UNSIGNED_INT;
                resource->m_ElementCount = index_count;
            }
            else
            {
                // If not supporting 32-bit indices, create triangle list as a fallback
                dmRig::RigModelVertex* rmv_buffer = new dmRig::RigModelVertex[index_count];
                dmRig::RigModelVertex* rmv = rmv_buffer;
                dmRigDDF::MeshVertexIndices* mvi =  mesh.m_Vertices.m_Data;
                uint32_t *mi = (uint32_t*) mesh.m_Indices.m_Data;
                for(uint32_t i = 0; i < index_count; ++i, ++rmv, ++mi)
                {
                    GetModelVertex(mesh, &mvi[*mi], rmv);
                }
                resource->m_VertexBuffer = dmGraphics::NewVertexBuffer(context, index_count*sizeof(dmRig::RigModelVertex), rmv_buffer, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                delete []rmv_buffer;
                resource->m_ElementCount = index_count;
                return;
            }
        }
        else
        {
            resource->m_IndexBuffer = dmGraphics::NewIndexBuffer(context, mesh.m_Indices.m_Count, mesh.m_Indices.m_Data, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
            resource->m_IndexBufferElementType = dmGraphics::TYPE_UNSIGNED_SHORT;
            resource->m_ElementCount = mesh.m_Indices.m_Count>>1;;
        }
        dmRig::RigModelVertex* rmv_buffer = new dmRig::RigModelVertex[mesh.m_Vertices.m_Count];
        dmRig::RigModelVertex* rmv = rmv_buffer;
        dmRigDDF::MeshVertexIndices* mvi =  mesh.m_Vertices.m_Data;
        for(uint32_t i = 0; i < mesh.m_Vertices.m_Count; ++i, ++mvi, ++rmv)
        {
            GetModelVertex(mesh, mvi, rmv);
        }
        resource->m_VertexBuffer = dmGraphics::NewVertexBuffer(context, mesh.m_Vertices.m_Count*sizeof(dmRig::RigModelVertex), rmv_buffer, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        delete []rmv_buffer;
    }

    dmResource::Result AcquireResources(dmGraphics::HContext context, dmResource::HFactory factory, ModelResource* resource, const char* filename)
    {
        dmResource::Result result = dmResource::Get(factory, resource->m_Model->m_RigScene, (void**) &resource->m_RigScene);
        if (result != dmResource::RESULT_OK)
            return result;

        result = dmResource::Get(factory, resource->m_Model->m_Material, (void**) &resource->m_Material);
        if (result != dmResource::RESULT_OK)
            return result;

        dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(dmGraphics::HTexture));
        for (uint32_t i = 0; i < resource->m_Model->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            const char* texture_path = resource->m_Model->m_Textures[i];
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

        if(dmRender::GetMaterialVertexSpace(resource->m_Material) ==  dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL)
        {
            if(resource->m_RigScene->m_AnimationSetRes || resource->m_RigScene->m_SkeletonRes)
            {
                dmLogError("Failed to create Model component. Material vertex space option VERTEX_SPACE_LOCAL does not support skinning.");
                return dmResource::RESULT_NOT_SUPPORTED;
            }
            dmRigDDF::MeshSet* mesh_set = resource->m_RigScene->m_MeshSetRes->m_MeshSet;
            if(mesh_set)
            {
                if(mesh_set->m_MeshEntries.m_Count && mesh_set->m_MeshEntries[0].m_Meshes.m_Count)
                {
                    CreateGPUBuffers(context, resource, mesh_set->m_MeshEntries[0].m_Meshes[0]);
                }
            }
        }

        return result;
    }

    static void ReleaseResources(dmResource::HFactory factory, ModelResource* resource)
    {
        if (resource->m_VertexBuffer != 0x0)
        {
            dmGraphics::DeleteVertexBuffer(resource->m_VertexBuffer);
            resource->m_VertexBuffer = 0x0;
        }
        if (resource->m_IndexBuffer != 0x0)
        {
            dmGraphics::DeleteVertexBuffer(resource->m_IndexBuffer);
            resource->m_IndexBuffer = 0x0;
            resource->m_ElementCount = 0;
        }
        if (resource->m_Model != 0x0)
            dmDDF::FreeMessage(resource->m_Model);
        resource->m_Model = 0x0;
        if (resource->m_RigScene != 0x0)
            dmResource::Release(factory, resource->m_RigScene);
        resource->m_RigScene = 0x0;
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

    dmResource::Result ResModelPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmModelDDF::Model* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmModelDDF_Model_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);
        for (uint32_t i = 0; i < ddf->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, ddf->m_Textures[i]);
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_RigScene);
        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResModelCreate(const dmResource::ResourceCreateParams& params)
    {
        ModelResource* model_resource = new ModelResource();
        memset(model_resource, 0, sizeof(ModelResource));
        model_resource->m_Model = (dmModelDDF::Model*) params.m_PreloadData;
        dmResource::Result r = AcquireResources((dmGraphics::HContext) params.m_Context, params.m_Factory, model_resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) model_resource;
        }
        else
        {
            ReleaseResources(params.m_Factory, model_resource);
            delete model_resource;
        }
        return r;
    }

    dmResource::Result ResModelDestroy(const dmResource::ResourceDestroyParams& params)
    {
        ModelResource* model_resource = (ModelResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, model_resource);
        delete model_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResModelRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmModelDDF::Model* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmModelDDF_Model_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        ModelResource* model_resource = (ModelResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, model_resource);
        model_resource->m_Model = ddf;
        return AcquireResources((dmGraphics::HContext) params.m_Context, params.m_Factory, model_resource, params.m_Filename);
    }
}
