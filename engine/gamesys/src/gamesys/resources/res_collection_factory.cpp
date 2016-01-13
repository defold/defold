#include "res_collection_factory.h"

namespace dmGameSystem
{
    dmResource::Result AcquireResource(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, CollectionFactoryResource* factory_res)
    {
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &factory_res->m_CollectionFactoryDesc);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;
        return dmResource::RESULT_OK;
    }

    void ReleaseResources(dmResource::HFactory factory, CollectionFactoryResource* factory_res)
    {
        if (factory_res->m_CollectionFactoryDesc != 0x0)
            dmDDF::FreeMessage(factory_res->m_CollectionFactoryDesc);
    }

    dmResource::Result ResCollectionFactoryCreate(const dmResource::ResourceCreateParams& params)
    {
        CollectionFactoryResource* factory_res = new CollectionFactoryResource;
        dmResource::Result r = AcquireResource(params.m_Factory, params.m_Buffer, params.m_BufferSize, factory_res);
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

    dmResource::Result ResCollectionFactoryDestroy(const dmResource::ResourceDestroyParams& params)
    {
        CollectionFactoryResource* factory_res = (CollectionFactoryResource*) params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, factory_res);
        delete factory_res;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollectionFactoryRecreate(const dmResource::ResourceRecreateParams& params)
    {
        CollectionFactoryResource tmp_factory_res;
        dmResource::Result r = AcquireResource(params.m_Factory, params.m_Buffer, params.m_BufferSize, &tmp_factory_res);
        if (r == dmResource::RESULT_OK)
        {
            CollectionFactoryResource* factory_res = (CollectionFactoryResource*) params.m_Resource->m_Resource;
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
