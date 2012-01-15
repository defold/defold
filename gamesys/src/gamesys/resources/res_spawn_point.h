#ifndef DM_GAMESYS_RES_SPAWN_POINT_H
#define DM_GAMESYS_RES_SPAWN_POINT_H

#include <stdint.h>

#include <resource/resource.h>

#include "gamesys_ddf.h"

namespace dmGameSystem
{
    struct SpawnPointResource
    {
        dmGameSystemDDF::SpawnPointDesc* m_SpawnPointDesc;
        void*                            m_Prototype;
    };

    dmResource::Result ResSpawnPointCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::Result ResSpawnPointDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResSpawnPointRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_RES_SPAWN_POINT_H
