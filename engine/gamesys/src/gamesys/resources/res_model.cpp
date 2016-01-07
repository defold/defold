#include "res_model.h"

#include "mesh_ddf.h"
#include "model_ddf.h"

namespace dmGameSystem
{
    dmResource::Result ResPreloadModel(const dmResource::ResourcePreloadParams& params)
    {
        dmModelDDF::ModelDesc* model_desc;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmModelDDF_ModelDesc_DESCRIPTOR, (void**) &model_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, model_desc->m_Material);
        for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, model_desc->m_Textures[i]);
        }

        *params.m_PreloadData = model_desc;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCreateModel(const dmResource::ResourceCreateParams& params)
    {
        dmModelDDF::ModelDesc* model_desc = (dmModelDDF::ModelDesc*) params.m_PreloadData;

        Mesh* mesh = 0x0;
        dmRender::HMaterial material = 0;
        dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(dmGraphics::HTexture));

        dmResource::Result tmp_r = dmResource::RESULT_OK;
        dmResource::Result first_error = dmResource::RESULT_OK;
        tmp_r = dmResource::Get(params.m_Factory, model_desc->m_Mesh, (void**) &mesh);
        if (tmp_r != dmResource::RESULT_OK)
        {
            if (first_error == dmResource::RESULT_OK)
                first_error = tmp_r;
        }

        tmp_r = dmResource::Get(params.m_Factory, model_desc->m_Material, (void**) &material);
        if (tmp_r != dmResource::RESULT_OK)
        {
            if (first_error == dmResource::RESULT_OK)
                first_error = tmp_r;
        }

        for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            tmp_r = dmResource::Get(params.m_Factory, model_desc->m_Textures[i], (void**) &textures[i]);
            if (tmp_r != dmResource::RESULT_OK)
            {
                if (first_error == dmResource::RESULT_OK)
                    first_error = tmp_r;
            }
        }

        dmDDF::FreeMessage((void*) model_desc);
        if (first_error != dmResource::RESULT_OK)
        {
            if (mesh) dmResource::Release(params.m_Factory, (void*) mesh);
            if (material) dmResource::Release(params.m_Factory, (void*) material);
            for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                if (textures[i]) dmResource::Release(params.m_Factory, (void*) textures[i]);

            return first_error;
        }

        Model* model = new Model();
        model->m_Mesh = mesh;
        model->m_Material = material;
        memcpy(model->m_Textures, textures, sizeof(dmGraphics::HTexture) * dmRender::RenderObject::MAX_TEXTURE_COUNT);

        params.m_Resource->m_Resource = (void*) model;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResDestroyModel(const dmResource::ResourceDestroyParams& params)
    {
        Model* model = (Model*)params.m_Resource->m_Resource;

        dmResource::Release(params.m_Factory, (void*)model->m_Mesh);
        dmResource::Release(params.m_Factory, (void*)model->m_Material);
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            if (model->m_Textures[i]) dmResource::Release(params.m_Factory, (void*)model->m_Textures[i]);

        delete model;

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRecreateModel(const dmResource::ResourceRecreateParams& params)
    {
        dmModelDDF::ModelDesc* model_desc;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmModelDDF_ModelDesc_DESCRIPTOR, (void**) &model_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        Mesh* mesh = 0x0;
        dmRender::HMaterial material = 0;
        dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(dmGraphics::HTexture));

        dmResource::Get(params.m_Factory, model_desc->m_Mesh, (void**) &mesh);
        dmResource::Get(params.m_Factory, model_desc->m_Material, (void**) &material);
        for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            dmResource::Get(params.m_Factory, model_desc->m_Textures[i], (void**) &textures[i]);

        if (mesh == 0 || material == 0 || textures[0] == 0)
        {
            if (mesh) dmResource::Release(params.m_Factory, (void*) mesh);
            if (material) dmResource::Release(params.m_Factory, (void*) material);
            for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                if (textures[i]) dmResource::Release(params.m_Factory, (void*) textures[i]);

            dmDDF::FreeMessage((void*) model_desc);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        Model* model = (Model*)params.m_Resource->m_Resource;
        dmResource::Release(params.m_Factory, (void*)model->m_Mesh);
        dmResource::Release(params.m_Factory, (void*)model->m_Material);
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            if (model->m_Textures[i]) dmResource::Release(params.m_Factory, (void*)model->m_Textures[i]);

        model->m_Mesh = mesh;
        model->m_Material = material;
        for (uint32_t i = 0; i < model_desc->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            model->m_Textures[i] = textures[i];

        dmDDF::FreeMessage((void*) model_desc);
        return dmResource::RESULT_OK;
    }
}
