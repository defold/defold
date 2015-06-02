#ifndef DM_GAMESYS_RES_COLLECTION_PROXY_H
#define DM_GAMESYS_RES_COLLECTION_PROXY_H

#include <stdint.h>

#include <resource/resource.h>

#include "gamesys_ddf.h"

namespace dmGameSystem
{
    struct CollectionProxyResource
    {
        dmGameSystemDDF::CollectionProxyDesc*  m_DDF;
    };

    dmResource::Result ResCollectionProxyCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void *preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::Result ResCollectionProxyDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResCollectionProxyRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_RES_COLLECTION_PROXY_H
