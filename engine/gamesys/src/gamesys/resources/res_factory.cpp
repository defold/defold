#include "res_factory.h"
#include <dlib/dstrings.h>

namespace dmGameSystem
{

    void ReleaseResources(dmResource::HFactory factory, FactoryResource* factory_res)
    {
        if(factory_res->m_FactoryDesc)
            dmDDF::FreeMessage(factory_res->m_FactoryDesc);
    }

    dmResource::Result ResFactoryPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::FactoryDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        FactoryResource* factory_res = new FactoryResource;
        memset(factory_res, 0, sizeof(FactoryResource));
        factory_res->m_FactoryDesc = ddf;

        if(!factory_res->m_FactoryDesc->m_LoadDynamically)
        {
            if(params.m_HintInfo)
            {
                dmResource::PreloadHint(params.m_HintInfo, ddf->m_Prototype);
            }
        }

        *params.m_PreloadData = factory_res;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFactoryCreate(const dmResource::ResourceCreateParams& params)
    {
        params.m_Resource->m_Resource = (void*) params.m_PreloadData;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFactoryDestroy(const dmResource::ResourceDestroyParams& params)
    {
        FactoryResource* factory_res = (FactoryResource*) params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, factory_res);
        delete factory_res;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFactoryRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmGameSystemDDF::FactoryDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        FactoryResource* factory_res = (FactoryResource*) params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, factory_res);
        memset(factory_res, 0, sizeof(FactoryResource));
        factory_res->m_FactoryDesc = ddf;
        return dmResource::RESULT_OK;
    }
}
