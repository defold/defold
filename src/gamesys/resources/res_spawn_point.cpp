#include "res_spawn_point.h"

namespace dmGameSystem
{
    bool AcquireResource(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, SpawnPoint* spawn_point)
    {
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &spawn_point->m_SpawnPointDesc);
        if ( e != dmDDF::RESULT_OK )
            return false;

        dmResource::FactoryResult fact_r = dmResource::Get(factory, spawn_point->m_SpawnPointDesc->m_Prototype, &spawn_point->m_Prototype);
        if (fact_r != dmResource::FACTORY_RESULT_OK)
            return false;

        return true;
    }

    void ReleaseResources(dmResource::HFactory factory, SpawnPoint* spawn_point)
    {
        if (spawn_point->m_SpawnPointDesc != 0x0)
            dmDDF::FreeMessage(spawn_point->m_SpawnPointDesc);
        if (spawn_point->m_Prototype != 0x0)
            dmResource::Release(factory, spawn_point->m_Prototype);
    }

    dmResource::CreateResult ResSpawnPointCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {

        SpawnPoint* spawn_point = new SpawnPoint;
        if (AcquireResource(factory, buffer, buffer_size, spawn_point))
        {
            resource->m_Resource = (void*) spawn_point;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, spawn_point);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResSpawnPointDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        SpawnPoint* spawn_point = (SpawnPoint*) resource->m_Resource;
        ReleaseResources(factory, spawn_point);
        delete spawn_point;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResSpawnPointRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        SpawnPoint tmp_spawn_point;
        if (AcquireResource(factory, buffer, buffer_size, &tmp_spawn_point))
        {
            SpawnPoint* spawn_point = (SpawnPoint*) resource->m_Resource;
            ReleaseResources(factory, spawn_point);
            *spawn_point = tmp_spawn_point;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, &tmp_spawn_point);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }
}
