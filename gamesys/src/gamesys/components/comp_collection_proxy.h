#ifndef DM_GAMESYS_COLLECTION_PROXY_H
#define DM_GAMESYS_COLLECTION_PROXY_H

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompCollectionProxyNewWorld(void* context, void** world);

    dmGameObject::CreateResult CompCollectionProxyDeleteWorld(void* context, void* world);

    dmGameObject::CreateResult CompCollectionProxyCreate(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data);

    dmGameObject::CreateResult CompCollectionProxyDestroy(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* world,
                                                void* context,
                                                uintptr_t* user_data);

    dmGameObject::UpdateResult CompCollectionProxyUpdate(dmGameObject::HCollection collection,
                                               const dmGameObject::UpdateContext* update_context,
                                               void* world,
                                               void* context);

    dmGameObject::UpdateResult CompCollectionProxyPostUpdate(dmGameObject::HCollection collection,
                                               void* world,
                                               void* context);

    dmGameObject::UpdateResult CompCollectionProxyOnMessage(dmGameObject::HInstance instance,
                                                const dmGameObject::InstanceMessageData* message_data,
                                                void* context,
                                                uintptr_t* user_data);

    dmGameObject::InputResult CompCollectionProxyOnInput(dmGameObject::HInstance instance,
                                                const dmGameObject::InputAction* input_action,
                                                void* context,
                                                uintptr_t* user_data);
}

#endif // DM_GAMESYS_COLLECTION_PROXY_H
