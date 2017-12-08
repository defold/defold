#ifndef DM_GAMESYS_COMP_MODEL_H
#define DM_GAMESYS_COMP_MODEL_H

#include <stdint.h>
#include <dlib/object_pool.h>
#include <gameobject/gameobject.h>
#include <rig/rig.h>

#include "../resources/res_model.h"

namespace dmGameSystem
{
    typedef struct ModelWorld* HModelWorld;
    typedef struct ModelComponent* HModelComponent;

    dmGameObject::CreateResult CompModelNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompModelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompModelCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompModelDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompModelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompModelUpdate(const dmGameObject::ComponentsUpdateParams& params);

    dmGameObject::UpdateResult CompModelRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompModelOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompModelOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompModelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompModelSetProperty(const dmGameObject::ComponentSetPropertyParams& params);

    bool CompModelSetIKTargetInstance(HModelComponent component, dmhash_t constraint_id, float mix, dmhash_t instance_id);
    bool CompModelSetIKTargetPosition(HModelComponent component, dmhash_t constraint_id, float mix, Point3 position);

    HModelComponent CompModelGetComponent(const HModelWorld model_world, uint32_t component_index);

    RigSceneResource* CompModelGetRigScene(const HModelComponent component);

    dmArray<dmGameObject::HInstance>& CompModelGetBoneInstances(const HModelComponent component);
}

#endif // DM_GAMESYS_COMP_MODEL_H
