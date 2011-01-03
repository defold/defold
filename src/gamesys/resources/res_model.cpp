#include "res_model.h"

#include "mesh_ddf.h"
#include "model_ddf.h"

namespace dmGameSystem
{
    dmResource::CreateResult ResCreateModel(dmResource::HFactory factory,
                                      void* context,
                                      const void* buffer, uint32_t buffer_size,
                                      dmResource::SResourceDescriptor* resource,
                                      const char* filename)
    {
        dmModelDDF::ModelDesc* model_desc;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmModelDDF_ModelDesc_DESCRIPTOR, (void**) &model_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        Mesh* mesh = 0x0;
        dmRender::HMaterial material = 0;
        dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(dmGraphics::HTexture));

        dmResource::Get(factory, model_desc->m_Mesh, (void**) &mesh);
        dmResource::Get(factory, model_desc->m_Material, (void**) &material);
        for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            dmResource::Get(factory, model_desc->m_Textures[i], (void**) &textures[i]);

        dmDDF::FreeMessage((void*) model_desc);
        if (mesh == 0 || material == 0 || textures[0] == 0)
        {
            if (mesh) dmResource::Release(factory, (void*) mesh);
            if (material) dmResource::Release(factory, (void*) material);
            for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                if (textures[i]) dmResource::Release(factory, (void*) textures[i]);

            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        Model* model = new Model();
        model->m_Mesh = mesh;
        model->m_Material = material;
        for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            model->m_Textures[i] = textures[i];

        resource->m_Resource = (void*) model;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResDestroyModel(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        Model* model = (Model*)resource->m_Resource;

        dmResource::Release(factory, (void*)model->m_Mesh);
        dmResource::Release(factory, (void*)model->m_Material);
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            if (model->m_Textures[i]) dmResource::Release(factory, (void*)model->m_Textures[i]);

        delete model;

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResRecreateModel(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmModelDDF::ModelDesc* model_desc;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmModelDDF_ModelDesc_DESCRIPTOR, (void**) &model_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        Mesh* mesh = 0x0;
        dmRender::HMaterial material = 0;
        dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(dmGraphics::HTexture));

        dmResource::Get(factory, model_desc->m_Mesh, (void**) &mesh);
        dmResource::Get(factory, model_desc->m_Material, (void**) &material);
        for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            dmResource::Get(factory, model_desc->m_Textures[i], (void**) &textures[i]);

        dmDDF::FreeMessage((void*) model_desc);
        if (mesh == 0 || material == 0 || textures[0] == 0)
        {
            if (mesh) dmResource::Release(factory, (void*) mesh);
            if (material) dmResource::Release(factory, (void*) material);
            for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                if (textures[i]) dmResource::Release(factory, (void*) textures[i]);

            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        Model* model = (Model*)resource->m_Resource;
        dmResource::Release(factory, (void*)model->m_Mesh);
        dmResource::Release(factory, (void*)model->m_Material);
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            if (model->m_Textures[i]) dmResource::Release(factory, (void*)model->m_Textures[i]);

        model->m_Mesh = mesh;
        model->m_Material = material;
        for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            model->m_Textures[i] = textures[i];

        return dmResource::CREATE_RESULT_OK;
    }
}
