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

    dmResource::Result ResFactoryPreload(dmResource::HFactory factory,
            dmResource::HPreloadHintInfo hint_info,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void** preload_data,
            const char* filename)
    {
        dmGameSystemDDF::FactoryDesc* ddf;

        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &ddf);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        dmResource::PreloadHint(hint_info, ddf->m_Prototype);

        *preload_data = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFactoryCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void* preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        FactoryResource* factory_res = new FactoryResource;
        factory_res->m_FactoryDesc = (dmGameSystemDDF::FactoryDesc*) preload_data;
        dmResource::Result r = AcquireResource(factory, factory_res);
        if (r == dmResource::RESULT_OK)
        {
            resource->m_Resource = (void*) factory_res;
        }
        else
        {
            ReleaseResources(factory, factory_res);
        }
        return r;
    }

    dmResource::Result ResFactoryDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        FactoryResource* factory_res = (FactoryResource*) resource->m_Resource;
        ReleaseResources(factory, factory_res);
        delete factory_res;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFactoryRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        FactoryResource tmp_factory_res;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &tmp_factory_res.m_FactoryDesc);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        dmResource::Result r = AcquireResource(factory, &tmp_factory_res);
        if (r == dmResource::RESULT_OK)
        {
            FactoryResource* factory_res = (FactoryResource*) resource->m_Resource;
            ReleaseResources(factory, factory_res);
            *factory_res = tmp_factory_res;
        }
        else
        {
            ReleaseResources(factory, &tmp_factory_res);
        }
        return r;
    }
}
