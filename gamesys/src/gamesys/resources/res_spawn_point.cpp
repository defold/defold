#include "res_spawn_point.h"

namespace dmGameSystem
{
    dmResource::Result AcquireResource(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, SpawnPointResource* spawn_point)
    {
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &spawn_point->m_SpawnPointDesc);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        dmResource::Result fact_r = dmResource::Get(factory, spawn_point->m_SpawnPointDesc->m_Prototype, &spawn_point->m_Prototype);
        return fact_r;
    }

    void ReleaseResources(dmResource::HFactory factory, SpawnPointResource* spawn_point)
    {
        if (spawn_point->m_SpawnPointDesc != 0x0)
            dmDDF::FreeMessage(spawn_point->m_SpawnPointDesc);
        if (spawn_point->m_Prototype != 0x0)
            dmResource::Release(factory, spawn_point->m_Prototype);
    }

    dmResource::Result ResSpawnPointCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {

        SpawnPointResource* spawn_point = new SpawnPointResource;
        dmResource::Result r = AcquireResource(factory, buffer, buffer_size, spawn_point);
        if (r == dmResource::RESULT_OK)
        {
            resource->m_Resource = (void*) spawn_point;
        }
        else
        {
            ReleaseResources(factory, spawn_point);
        }
        return r;
    }

    dmResource::Result ResSpawnPointDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        SpawnPointResource* spawn_point = (SpawnPointResource*) resource->m_Resource;
        ReleaseResources(factory, spawn_point);
        delete spawn_point;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSpawnPointRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        SpawnPointResource tmp_spawn_point;
        dmResource::Result r = AcquireResource(factory, buffer, buffer_size, &tmp_spawn_point);
        if (r == dmResource::RESULT_OK)
        {
            SpawnPointResource* spawn_point = (SpawnPointResource*) resource->m_Resource;
            ReleaseResources(factory, spawn_point);
            *spawn_point = tmp_spawn_point;
        }
        else
        {
            ReleaseResources(factory, &tmp_spawn_point);
        }
        return r;
    }
}
