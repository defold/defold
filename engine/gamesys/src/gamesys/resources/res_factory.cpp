#include "res_factory.h"

namespace dmGameSystem
{
    dmResource::Result AcquireResource(dmResource::HFactory factory, FactoryResource* factory_res)
    {
        dmResource::Result fact_r = dmResource::Get(factory, factory_res->m_FactoryDesc->m_Prototype, &factory_res->m_Prototype);
        return fact_r;
    }

    void ReleaseResources(dmResource::HFactory factory, FactoryResource* factory_res)
    {
        if (factory_res->m_FactoryDesc != 0x0)
            dmDDF::FreeMessage(factory_res->m_FactoryDesc);
        if (factory_res->m_Prototype != 0x0)
            dmResource::Release(factory, factory_res->m_Prototype);
    }

    dmResource::Result ResFactoryPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::FactoryDesc* ddf;

        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Prototype);

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFactoryCreate(const dmResource::ResourceCreateParams& params)
    {
        FactoryResource* factory_res = new FactoryResource;
        factory_res->m_FactoryDesc = (dmGameSystemDDF::FactoryDesc*) params.m_PreloadData;
        dmResource::Result r = AcquireResource(params.m_Factory, factory_res);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) factory_res;
        }
        else
        {
            ReleaseResources(params.m_Factory, factory_res);
        }
        return r;
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
        FactoryResource tmp_factory_res;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &tmp_factory_res.m_FactoryDesc);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        dmResource::Result r = AcquireResource(params.m_Factory, &tmp_factory_res);
        if (r == dmResource::RESULT_OK)
        {
            FactoryResource* factory_res = (FactoryResource*) params.m_Resource->m_Resource;
            ReleaseResources(params.m_Factory, factory_res);
            *factory_res = tmp_factory_res;
        }
        else
        {
            ReleaseResources(params.m_Factory, &tmp_factory_res);
        }
        return r;
    }
}
