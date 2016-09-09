#include "res_model.h"

#include <dlib/log.h>

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory, ModelResource* resource, const char* filename)
    {
        dmResource::Result result = dmResource::Get(factory, resource->m_Model->m_Mesh, (void**) &resource->m_RigScene);
        if (result != dmResource::RESULT_OK)
            return result;
        result = dmResource::Get(factory, resource->m_Model->m_Material, (void**) &resource->m_Material);
        return result;
    }

    static void ReleaseResources(dmResource::HFactory factory, ModelResource* resource)
    {
        if (resource->m_Model != 0x0)
            dmDDF::FreeMessage(resource->m_Model);
        if (resource->m_RigScene != 0x0)
            dmResource::Release(factory, resource->m_RigScene);
        if (resource->m_Material != 0x0)
            dmResource::Release(factory, resource->m_Material);
    }

    dmResource::Result ResModelPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmModelDDF::ModelDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmModelDDF_ModelDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);
        for (uint32_t i = 0; i < ddf->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, ddf->m_Textures[i]);
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Mesh);
        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResModelCreate(const dmResource::ResourceCreateParams& params)
    {
        ModelResource* model_resource = new ModelResource();
        model_resource->m_Model = (dmModelDDF::ModelDesc*) params.m_PreloadData;
        dmResource::Result r = AcquireResources(params.m_Factory, model_resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) model_resource;
        }
        else
        {
            ReleaseResources(params.m_Factory, model_resource);
            delete model_resource;
        }

        dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(dmGraphics::HTexture));

        dmResource::Result tmp_r = dmResource::RESULT_OK;
        dmResource::Result first_error = dmResource::RESULT_OK;
        for (uint32_t i = 0; i < model_resource->m_Model->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            r = dmResource::Get(params.m_Factory, model_resource->m_Model->m_Textures[i], (void**) &textures[i]);
            if (r != dmResource::RESULT_OK)
            {
                if (first_error == dmResource::RESULT_OK)
                    first_error = r;
            }
        }

        if (first_error != dmResource::RESULT_OK)
        {
            for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                if (textures[i]) dmResource::Release(params.m_Factory, (void*) textures[i]);

            ReleaseResources(params.m_Factory, model_resource);
            delete model_resource;
            return first_error;
        }

        memcpy(model_resource->m_Textures, textures, sizeof(dmGraphics::HTexture) * dmRender::RenderObject::MAX_TEXTURE_COUNT);

        return r;
    }

    dmResource::Result ResModelDestroy(const dmResource::ResourceDestroyParams& params)
    {
        ModelResource* model_resource = (ModelResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, model_resource);

        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            if (model_resource->m_Textures[i]) dmResource::Release(params.m_Factory, (void*)model_resource->m_Textures[i]);

        delete model_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResModelRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmModelDDF::ModelDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmModelDDF_ModelDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        ModelResource* model_resource = (ModelResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, model_resource);
        model_resource->m_Model = ddf;
        return AcquireResources(params.m_Factory, model_resource, params.m_Filename);
    }
}
