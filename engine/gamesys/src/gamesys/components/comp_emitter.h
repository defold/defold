#ifndef DM_GAMESYS_COMP_EMITTER_H
#define DM_GAMESYS_COMP_EMITTER_H

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompEmitterNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompEmitterDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompEmitterCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompEmitterDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::UpdateResult CompEmitterOnMessage(const dmGameObject::ComponentOnMessageParams& params);

}

#endif // DM_GAMESYS_COMP_EMITTER_H
