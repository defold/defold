#ifndef DM_GAMESYS_COLLECTION_FACTORY_H
#define DM_GAMESYS_COLLECTION_FACTORY_H

#include <resource/resource.h>
#include <gameobject/gameobject.h>

#include "../resources/res_collection_factory.h"

namespace dmGameSystem
{
    struct CollectionFactoryComponent
    {
        CollectionFactoryResource*    m_Resource;
        int m_Callback;
        dmResource::HPreloader          m_Preloader;
        uint32_t m_AddedToUpdate : 1;
    };

    dmGameObject::CreateResult CompCollectionFactoryNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompCollectionFactoryDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompCollectionFactoryCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompCollectionFactoryDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompCollectionFactoryAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompCollectionFactoryUpdate(const dmGameObject::ComponentsUpdateParams& params);

    bool Create(CollectionFactoryComponent* component);
}

#endif
