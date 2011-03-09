#include "res_collection_spawn_point.h"

namespace dmGameSystem
{
    bool AcquireResource(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, CollectionSpawnPointResource* resource)
    {
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &resource->m_DDF);
        if ( e != dmDDF::RESULT_OK )
            return false;

        return true;
    }

    void ReleaseResources(dmResource::HFactory factory, CollectionSpawnPointResource* resource)
    {
        if (resource->m_DDF != 0x0)
            dmDDF::FreeMessage(resource->m_DDF);
    }

    dmResource::CreateResult ResCollectionSpawnPointCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {

        CollectionSpawnPointResource* cspr = new CollectionSpawnPointResource();
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

    dmResource::CreateResult ResCollectionSpawnPointDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        CollectionSpawnPointResource* cspr = (CollectionSpawnPointResource*) resource->m_Resource;
        ReleaseResources(factory, cspr);
        delete cspr;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResCollectionSpawnPointRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        CollectionSpawnPointResource tmp_cspr;
        if (AcquireResource(factory, buffer, buffer_size, &tmp_cspr))
        {
            CollectionSpawnPointResource* cspr = (CollectionSpawnPointResource*) resource->m_Resource;
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
