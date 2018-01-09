#ifndef DM_GAMESYS_COMP_CAMERA_H
#define DM_GAMESYS_COMP_CAMERA_H

#include <stdint.h>
#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompCameraNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompCameraDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompCameraCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompCameraDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompCameraAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompCameraUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompCameraOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompCameraOnReload(const dmGameObject::ComponentOnReloadParams& params);
}

#endif // DM_GAMESYS_COMP_CAMERA_H
