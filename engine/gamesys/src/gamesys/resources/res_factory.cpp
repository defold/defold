#include "res_factory.h"
#include <dlib/dstrings.h>

#include <gameobject/gameobject.h>

namespace dmGameSystem
{

    static dmResource::Result AcquireResources(dmResource::HFactory factory, dmGameSystemDDF::FactoryDesc* factory_desc, FactoryResource* factory_res)
    {
        factory_res->m_FactoryDesc = factory_desc;
        if(factory_desc->m_LoadDynamically)
        {
            return dmResource::RESULT_OK;
        }
        return dmResource::Get(factory, factory_desc->m_Prototype, (void **)&factory_res->m_Prototype);
    }

    static void ReleaseResources(dmResource::HFactory factory, FactoryResource* factory_res)
    {
        if(factory_res->m_Prototype)
            dmResource::Release(factory, factory_res->m_Prototype);
        if(factory_res->m_FactoryDesc)
            dmDDF::FreeMessage(factory_res->m_FactoryDesc);
    }

    dmResource::Result ResFactoryPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::FactoryDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;
        if((!ddf->m_LoadDynamically) && (params.m_HintInfo))
        {
            dmResource::PreloadHint(params.m_HintInfo, ddf->m_Prototype);
        }
        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFactoryCreate(const dmResource::ResourceCreateParams& params)
    {
        dmGameSystemDDF::FactoryDesc* ddf = (dmGameSystemDDF::FactoryDesc*) params.m_PreloadData;
        FactoryResource* factory_res = new FactoryResource;
        memset(factory_res, 0, sizeof(FactoryResource));
        dmResource::Result res = AcquireResources(params.m_Factory, ddf, factory_res);

        if(res == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) factory_res;
        }
        else
        {
            ReleaseResources(params.m_Factory, factory_res);
            delete factory_res;
        }
        return res;
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
        if (e != dmDDF::RESULT_OK)
            return dmResource::RESULT_DDF_ERROR;

        FactoryResource tmp_factory_res;
        memset(&tmp_factory_res, 0, sizeof(FactoryResource));
        dmResource::Result r = AcquireResources(params.m_Factory, ddf, &tmp_factory_res);

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
