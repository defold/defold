#ifndef DM_GAMESYS_FACTORY_H
#define DM_GAMESYS_FACTORY_H

#include <gameobject/gameobject.h>

#include "../resources/res_factory.h"

namespace dmGameSystem
{
    struct FactoryComponent
    {
        FactoryResource*    m_Resource;
    };

    dmGameObject::CreateResult CompFactoryNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompFactoryDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompFactoryCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompFactoryDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::UpdateResult CompFactoryOnMessage(const dmGameObject::ComponentOnMessageParams& params);
}

#endif
