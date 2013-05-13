#ifndef DM_GAMESYS_COMP_MODEL_H
#define DM_GAMESYS_COMP_MODEL_H

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompModelNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompModelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompModelCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompModelDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::UpdateResult CompModelUpdate(const dmGameObject::ComponentsUpdateParams& params);

    dmGameObject::UpdateResult CompModelOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    dmGameObject::PropertyResult CompModelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompModelSetProperty(const dmGameObject::ComponentSetPropertyParams& params);
}

#endif
