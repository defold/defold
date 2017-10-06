#ifndef DM_GAMESYS_RES_COLLECTION_FACTORY_H
#define DM_GAMESYS_RES_COLLECTION_FACTORY_H

#include <stdint.h>

#include <dlib/array.h>
#include <resource/resource.h>
#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    struct CollectionFactoryResource
    {
        CollectionFactoryResource& operator=(CollectionFactoryResource& other)
        {
            m_CollectionDesc = other.m_CollectionDesc;
            m_CollectionResources.Swap(other.m_CollectionResources);
            m_LoadDynamically = other.m_LoadDynamically;
            return *this;
        }

        dmGameObject::HCollectionDesc m_CollectionDesc;
        dmArray<void*> m_CollectionResources;
        bool m_LoadDynamically;
    };

    dmResource::Result ResCollectionFactoryPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResCollectionFactoryCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResCollectionFactoryDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResCollectionFactoryRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_FACTORY_H
