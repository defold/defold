#include "res_collection_proxy.h"

namespace dmGameSystem
{
    bool AcquireResource(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, CollectionProxyResource* resource)
    {
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &resource->m_DDF);
        if ( e != dmDDF::RESULT_OK )
            return false;

        return true;
    }

    void ReleaseResources(dmResource::HFactory factory, CollectionProxyResource* resource)
    {
        if (resource->m_DDF != 0x0)
            dmDDF::FreeMessage(resource->m_DDF);
    }

    dmResource::CreateResult ResCollectionProxyCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {

        CollectionProxyResource* cspr = new CollectionProxyResource();
        if (AcquireResource(factory, buffer, buffer_size, cspr))
        {
            resource->m_Resource = (void*) cspr;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, cspr);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResCollectionProxyDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        CollectionProxyResource* cspr = (CollectionProxyResource*) resource->m_Resource;
        ReleaseResources(factory, cspr);
        delete cspr;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResCollectionProxyRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        CollectionProxyResource tmp_cspr;
        if (AcquireResource(factory, buffer, buffer_size, &tmp_cspr))
        {
            CollectionProxyResource* cspr = (CollectionProxyResource*) resource->m_Resource;
            ReleaseResources(factory, cspr);
            *cspr = tmp_cspr;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, &tmp_cspr);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }
}
