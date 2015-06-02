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

    dmResource::Result ResCollectionFactoryCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void* preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        CollectionFactoryResource* factory_res = new CollectionFactoryResource;
        dmResource::Result r = AcquireResource(factory, buffer, buffer_size, factory_res);
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

    dmResource::Result ResCollectionFactoryDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        CollectionFactoryResource* factory_res = (CollectionFactoryResource*) resource->m_Resource;
        ReleaseResources(factory, factory_res);
        delete factory_res;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollectionFactoryRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        CollectionFactoryResource tmp_factory_res;
        dmResource::Result r = AcquireResource(factory, buffer, buffer_size, &tmp_factory_res);
        if (r == dmResource::RESULT_OK)
        {
            CollectionFactoryResource* factory_res = (CollectionFactoryResource*) resource->m_Resource;
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
