#include "res_spawn_point.h"

namespace dmGameSystem
{
    dmResource::CreateResult ResSpawnPointCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmGameSystemDDF::SpawnPointDesc* spawn_point_desc;
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &spawn_point_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        void* prototype;
        dmResource::FactoryResult fact_r = dmResource::Get(factory, spawn_point_desc->m_Prototype, &prototype);
        if (fact_r != dmResource::FACTORY_RESULT_OK)
        {
            dmDDF::FreeMessage((void*) spawn_point_desc);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        SpawnPoint* spawn_point = new SpawnPoint;
        spawn_point->m_SpawnPointDesc = spawn_point_desc;
        spawn_point->m_Prototype = prototype;
        resource->m_Resource = (void*) spawn_point;

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResSpawnPointDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        SpawnPoint* spawn_point = (SpawnPoint*) resource->m_Resource;
        dmDDF::FreeMessage(spawn_point->m_SpawnPointDesc);
        dmResource::Release(factory, spawn_point->m_Prototype);
        delete spawn_point;
        return dmResource::CREATE_RESULT_OK;
    }
}

