#include "res_collection_proxy.h"

namespace dmGameSystem
{
    dmResource::Result AcquireResource(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, CollectionProxyResource* resource)
    {
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &resource->m_DDF);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        return dmResource::RESULT_OK;
    }

    void ReleaseResources(dmResource::HFactory factory, CollectionProxyResource* resource)
    {
        if (resource->m_DDF != 0x0)
            dmDDF::FreeMessage(resource->m_DDF);
    }

    dmResource::Result ResCollectionProxyCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void* preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {

        CollectionProxyResource* cspr = new CollectionProxyResource();
        dmResource::Result r = AcquireResource(factory, buffer, buffer_size, cspr);
        if (r == dmResource::RESULT_OK)
        {
            resource->m_Resource = (void*) cspr;
        }
        else
        {
            ReleaseResources(factory, cspr);
        }
        return r;
    }

    dmResource::Result ResCollectionProxyDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        CollectionProxyResource* cspr = (CollectionProxyResource*) resource->m_Resource;
        ReleaseResources(factory, cspr);
        delete cspr;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollectionProxyRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        CollectionProxyResource tmp_cspr;
        dmResource::Result r = AcquireResource(factory, buffer, buffer_size, &tmp_cspr);
        if (r == dmResource::RESULT_OK)
        {
            CollectionProxyResource* cspr = (CollectionProxyResource*) resource->m_Resource;
            ReleaseResources(factory, cspr);
            *cspr = tmp_cspr;
        }
        else
        {
            ReleaseResources(factory, &tmp_cspr);
        }
        return r;
    }
}
