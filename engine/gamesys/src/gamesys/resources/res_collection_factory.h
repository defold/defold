#ifndef DM_GAMESYS_RES_COLLECTION_FACTORY_H
#define DM_GAMESYS_RES_COLLECTION_FACTORY_H

#include <stdint.h>

#include <resource/resource.h>

#include "gamesys_ddf.h"

namespace dmGameSystem
{
    struct CollectionFactoryResource
    {
        dmGameSystemDDF::CollectionFactoryDesc*   m_CollectionFactoryDesc;
    };

    dmResource::Result ResCollectionFactoryCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void *preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::Result ResCollectionFactoryDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResCollectionFactoryRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_RES_FACTORY_H
