#ifndef DM_GAMESYS_COMP_MODEL_H
#define DM_GAMESYS_COMP_MODEL_H

#include <stdint.h>
#include <dlib/object_pool.h>
#include <gameobject/gameobject.h>
#include <rig/rig.h>

#include "../resources/res_model.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;
    using namespace dmGameSystemDDF;

    struct ModelComponent;
    struct ModelWorld;

    dmGameObject::CreateResult CompModelNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompModelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompModelCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompModelDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompModelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompModelUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompModelRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompModelOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompModelOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompModelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompModelSetProperty(const dmGameObject::ComponentSetPropertyParams& params);

    bool CompModelSetIKTargetInstance(ModelComponent* component, dmhash_t constraint_id, float mix, dmhash_t instance_id);
    bool CompModelSetIKTargetPosition(ModelComponent* component, dmhash_t constraint_id, float mix, Point3 position);

    // Used in the script_model.cpp
    ModelComponent* CompModelGetComponent(ModelWorld* world, uintptr_t user_data);
    ModelResource* CompModelGetModelResource(ModelComponent* component);
    dmGameObject::HInstance CompModelGetNodeInstance(ModelComponent* component, uint32_t bone_index);
}

#endif // DM_GAMESYS_COMP_MODEL_H
