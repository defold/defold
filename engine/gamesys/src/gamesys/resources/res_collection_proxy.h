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
        uint32_t m_DDFSize;
        dmhash_t m_UrlHash;
    };

    dmResource::Result ResCollectionProxyCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResCollectionProxyDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResCollectionProxyRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResCollectionProxyGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif // DM_GAMESYS_RES_COLLECTION_PROXY_H
