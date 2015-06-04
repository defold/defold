#include "res_model.h"

#include "mesh_ddf.h"
#include "model_ddf.h"

namespace dmGameSystem
{
    dmResource::Result ResPreloadModel(dmResource::HFactory factory, dmResource::HPreloadHintInfo hint_info,
                                      void* context,
                                      const void* buffer, uint32_t buffer_size,
                                      void** preload_data,
                                      const char* filename)
    {
        dmModelDDF::ModelDesc* model_desc;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmModelDDF_ModelDesc_DESCRIPTOR, (void**) &model_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::PreloadHint(hint_info, model_desc->m_Material);
        for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            dmResource::PreloadHint(hint_info, model_desc->m_Textures[i]);
        }

        *preload_data = model_desc;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCreateModel(dmResource::HFactory factory,
                                      void* context,
                                      const void* buffer, uint32_t buffer_size,
                                      void* preload_data,
                                      dmResource::SResourceDescriptor* resource,
                                      const char* filename)
    {
        dmModelDDF::ModelDesc* model_desc = (dmModelDDF::ModelDesc*) preload_data;

        Mesh* mesh = 0x0;
        dmRender::HMaterial material = 0;
        dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(dmGraphics::HTexture));

        dmResource::Result tmp_r = dmResource::RESULT_OK;
        dmResource::Result first_error = dmResource::RESULT_OK;
        tmp_r = dmResource::Get(factory, model_desc->m_Mesh, (void**) &mesh);
        if (tmp_r != dmResource::RESULT_OK)
        {
            if (first_error == dmResource::RESULT_OK)
                first_error = tmp_r;
        }

        tmp_r = dmResource::Get(factory, model_desc->m_Material, (void**) &material);
        if (tmp_r != dmResource::RESULT_OK)
        {
            if (first_error == dmResource::RESULT_OK)
                first_error = tmp_r;
        }

        for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            tmp_r = dmResource::Get(factory, model_desc->m_Textures[i], (void**) &textures[i]);
            if (tmp_r != dmResource::RESULT_OK)
            {
                if (first_error == dmResource::RESULT_OK)
                    first_error = tmp_r;
            }
        }

        dmDDF::FreeMessage((void*) model_desc);
        if (first_error != dmResource::RESULT_OK)
        {
            if (mesh) dmResource::Release(factory, (void*) mesh);
            if (material) dmResource::Release(factory, (void*) material);
            for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                if (textures[i]) dmResource::Release(factory, (void*) textures[i]);

            return first_error;
        }

        Model* model = new Model();
        model->m_Mesh = mesh;
        model->m_Material = material;
        memcpy(model->m_Textures, textures, sizeof(dmGraphics::HTexture) * dmRender::RenderObject::MAX_TEXTURE_COUNT);

        resource->m_Resource = (void*) model;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResDestroyModel(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        Model* model = (Model*)resource->m_Resource;

        dmResource::Release(factory, (void*)model->m_Mesh);
        dmResource::Release(factory, (void*)model->m_Material);
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            if (model->m_Textures[i]) dmResource::Release(factory, (void*)model->m_Textures[i]);

        delete model;

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRecreateModel(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmModelDDF::ModelDesc* model_desc;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmModelDDF_ModelDesc_DESCRIPTOR, (void**) &model_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        Mesh* mesh = 0x0;
        dmRender::HMaterial material = 0;
        dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(dmGraphics::HTexture));

        dmResource::Get(factory, model_desc->m_Mesh, (void**) &mesh);
        dmResource::Get(factory, model_desc->m_Material, (void**) &material);
        for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            dmResource::Get(factory, model_desc->m_Textures[i], (void**) &textures[i]);

        if (mesh == 0 || material == 0 || textures[0] == 0)
        {
            if (mesh) dmResource::Release(factory, (void*) mesh);
            if (material) dmResource::Release(factory, (void*) material);
            for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                if (textures[i]) dmResource::Release(factory, (void*) textures[i]);

            dmDDF::FreeMessage((void*) model_desc);
            return dmResource::RESULT_FORMAT_ERROR;
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

        dmDDF::FreeMessage((void*) model_desc);
        return dmResource::RESULT_OK;
    }
}
