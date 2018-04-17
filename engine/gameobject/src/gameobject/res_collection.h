#ifndef DM_GAMEOBJECT_RES_COLLECTION_H
#define DM_GAMEOBJECT_RES_COLLECTION_H

#include <stdint.h>

#include <resource/resource.h>

#include "../proto/gameobject/gameobject_ddf.h"

namespace dmGameObject
{
    struct CollectionResource {
        dmGameObjectDDF::CollectionDesc* m_DDF;
    };

    dmResource::Result ResCollectionPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResCollectionCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResCollectionDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResCollectionRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMEOBJECT_RES_COLLECTION_H
