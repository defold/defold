#ifndef DM_GAMESYS_RES_COLLECTION_SPAWN_POINT_H
#define DM_GAMESYS_RES_COLLECTION_SPAWN_POINT_H

#include <stdint.h>

#include <resource/resource.h>

#include "gamesys_ddf.h"

namespace dmGameSystem
{
    struct CollectionSpawnPointResource
    {
        dmGameSystemDDF::CollectionSpawnPointDesc*  m_DDF;
    };

    dmResource::CreateResult ResCollectionSpawnPointCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::CreateResult ResCollectionSpawnPointDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResCollectionSpawnPointRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_RES_COLLECTION_SPAWN_POINT_H
