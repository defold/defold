#ifndef DM_GAMESYS_COLLECTION_FACTORY_H
#define DM_GAMESYS_COLLECTION_FACTORY_H

#include <gameobject/gameobject.h>

#include "../resources/res_collection_factory.h"

namespace dmGameSystem
{
    struct CollectionFactoryComponent
    {
        CollectionFactoryResource*    m_Resource;
    };

    dmGameObject::CreateResult CompCollectionFactoryNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompCollectionFactoryDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompCollectionFactoryCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompCollectionFactoryDestroy(const dmGameObject::ComponentDestroyParams& params);
}

#endif
