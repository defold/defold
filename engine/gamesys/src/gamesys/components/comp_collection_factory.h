#ifndef DM_GAMESYS_COLLECTION_FACTORY_H
#define DM_GAMESYS_COLLECTION_FACTORY_H

#include <dlib/array.h>
#include <dlib/hash.h>
#include <resource/resource.h>
#include <gameobject/gameobject.h>

#include "../resources/res_collection_factory.h"

namespace dmGameSystem
{
    struct CollectionFactoryComponent
    {
        void Init();

        CollectionFactoryResource*  m_Resource;

        dmArray<void*> m_CollectionResources;
        dmResource::HPreloader      m_Preloader;
        int m_PreloaderCallbackRef;
        int m_PreloaderSelfRef;
        int m_PreloaderURLRef;
        uint32_t m_Loading : 1;

        uint32_t m_AddedToUpdate : 1;
    };

    dmGameObject::CreateResult CompCollectionFactoryNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompCollectionFactoryDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompCollectionFactoryCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompCollectionFactoryDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompCollectionFactoryAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompCollectionFactoryUpdate(const dmGameObject::ComponentsUpdateParams& params);

    bool CompCollectionFactoryLoad(dmGameObject::HCollection collection, CollectionFactoryComponent* component);

    bool CompCollectionFactoryUnload(dmGameObject::HCollection collection, CollectionFactoryComponent* component);
}

#endif
