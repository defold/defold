#ifndef DM_GAMESYS_RES_COLLECTION_FACTORY_H
#define DM_GAMESYS_RES_COLLECTION_FACTORY_H

#include <stdint.h>

#include <resource/resource.h>
#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    struct CollectionFactoryResource
    {
        dmGameObject::HCollectionDesc m_CollectionDesc;
        bool m_LoadDynamically;
    };

    dmResource::Result ResCollectionFactoryPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResCollectionFactoryCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResCollectionFactoryDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResCollectionFactoryRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_FACTORY_H
