#ifndef DM_GAMESYS_COLLECTION_PROXY_H
#define DM_GAMESYS_COLLECTION_PROXY_H

#include <gameobject/gameobject.h>
#include <dlib/index_pool.h>

namespace dmGameSystem
{
    typedef struct CollectionProxyWorld* HCollectionProxyWorld;

    dmhash_t GetUrlHashFromComponent(const HCollectionProxyWorld world, dmhash_t instanceId, uint32_t index);

    dmGameObject::CreateResult CompCollectionProxyNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompCollectionProxyDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompCollectionProxyCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompCollectionProxyDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompCollectionProxyFinal(const dmGameObject::ComponentFinalParams& params);

    dmGameObject::CreateResult CompCollectionProxyAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompCollectionProxyUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompCollectionProxyRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompCollectionProxyPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params);

    dmGameObject::UpdateResult CompCollectionProxyOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    dmGameObject::InputResult CompCollectionProxyOnInput(const dmGameObject::ComponentOnInputParams& params);
}

#endif // DM_GAMESYS_COLLECTION_PROXY_H
