#ifndef DM_GAMESYS_COMP_SPRITE2_H
#define DM_GAMESYS_COMP_SPRITE2_H

#include <stdint.h>
#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompSprite2NewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompSprite2DeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompSprite2Create(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompSprite2Destroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::UpdateResult CompSprite2Update(const dmGameObject::ComponentsUpdateParams& params);

    dmGameObject::UpdateResult CompSprite2OnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompSprite2OnReload(const dmGameObject::ComponentOnReloadParams& params);
}

#endif // DM_GAMESYS_COMP_SPRITE2_H
