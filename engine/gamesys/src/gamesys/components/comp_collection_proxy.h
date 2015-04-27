#ifndef DM_GAMESYS_COLLECTION_PROXY_H
#define DM_GAMESYS_COLLECTION_PROXY_H

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompCollectionProxyNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompCollectionProxyDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompCollectionProxyCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompCollectionProxyDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompCollectionProxyAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompCollectionProxyUpdate(const dmGameObject::ComponentsUpdateParams& params);

    dmGameObject::UpdateResult CompCollectionProxyRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompCollectionProxyPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params);

    dmGameObject::UpdateResult CompCollectionProxyOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    dmGameObject::InputResult CompCollectionProxyOnInput(const dmGameObject::ComponentOnInputParams& params);
}

#endif // DM_GAMESYS_COLLECTION_PROXY_H
