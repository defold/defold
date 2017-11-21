#include "res_mesh.h"

#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/dstrings.h>
#include "gamesys.h"

namespace dmGameSystem
{
    static void CreateVertexBuffer(MeshContext* context, MeshResource* resource)
    {
        void* data = (void*)resource->m_Mesh->m_DataPtr;
        uint64_t data_size = resource->m_Mesh->m_DataSize;
        dmGraphics::VertexElement* vert_decl = (dmGraphics::VertexElement*)resource->m_Mesh->m_VertDeclPtr;
        uint32_t vert_decl_count = resource->m_Mesh->m_VertDeclCount;
        uint64_t elem_count = resource->m_Mesh->m_ElemCount;

        resource->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(context->m_RenderContext), vert_decl, vert_decl_count);
        resource->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(context->m_RenderContext), 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        dmGraphics::SetVertexBufferData(resource->m_VertexBuffer, data_size, data, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

    }

    dmResource::Result AcquireResources(dmResource::HFactory factory, MeshContext* context, MeshResource* resource, const char* filename)
    {
        // dmResource::Result result = dmResource::Get(factory, resource->m_Mesh->m_RigScene, (void**) &resource->m_RigScene);
        // if (result != dmResource::RESULT_OK)
        //     return result;

        dmResource::Result result = dmResource::RESULT_OK;

        if (resource->m_Mesh->m_DataPtr != 0x0)
        {
            dmLogError("buffer ptr set, should create vert data");
            CreateVertexBuffer(context, resource);
        }

        if (resource->m_Mesh->m_Material != 0x0)
        {
            result = dmResource::Get(factory, resource->m_Mesh->m_Material, (void**) &resource->m_Material);
            if (result != dmResource::RESULT_OK)
                return result;
        }

        if (resource->m_Mesh->m_Textures.m_Count > 0)
        {
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
        }
        return result;
    }

    static void ReleaseResources(dmResource::HFactory factory, MeshResource* resource)
    {
        if (resource->m_Mesh != 0x0)
            dmDDF::FreeMessage(resource->m_Mesh);
        resource->m_Mesh = 0x0;

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

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMeshCreate(const dmResource::ResourceCreateParams& params)
    {
        MeshResource* mesh_resource = new MeshResource();
        mesh_resource->m_Mesh = (dmMeshDDF::Mesh*) params.m_PreloadData;
        dmResource::Result r = AcquireResources(params.m_Factory, (MeshContext*)params.m_Context, mesh_resource, params.m_Filename);
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
        dmMeshDDF::Mesh* ddf = (dmMeshDDF::Mesh*)params.m_Message;
        if( !ddf )
        {
            dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmMeshDDF_Mesh_DESCRIPTOR, (void**) &ddf);
            if ( e != dmDDF::RESULT_OK )
            {
                return dmResource::RESULT_FORMAT_ERROR;
            }
        }

        MeshResource* mesh_resource = (MeshResource*)params.m_Resource->m_Resource;
        // ReleaseResources(params.m_Factory, mesh_resource);
        mesh_resource->m_Mesh = ddf;

        if (ddf->m_DataPtr != 0x0)
        {
            dmLogError("mesh recreated with buffer ptr!");
        } else {
            dmLogError("mesh recreated with WITHOUT buffer ptr!");
        }

        return AcquireResources(params.m_Factory, (MeshContext*)params.m_Context, mesh_resource, params.m_Filename);
    }
}
