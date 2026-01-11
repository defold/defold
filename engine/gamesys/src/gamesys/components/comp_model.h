// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_GAMESYS_COMP_MODEL_H
#define DM_GAMESYS_COMP_MODEL_H

#include <gameobject/component.h>

// for scripting
#include <stdint.h>
#include <dlib/hash.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompModelNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompModelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompModelCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompModelDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompModelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompModelUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompModelLateUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompModelRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompModelOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompModelOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompModelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompModelSetProperty(const dmGameObject::ComponentSetPropertyParams& params);

    void*                   CompModelGetComponent(const dmGameObject::ComponentGetParams& params);

    void CompModelIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node);

    // Used in the script_model.cpp
    struct ModelComponent;
    struct ModelWorld;
    struct ModelResource;

    ModelResource*          CompModelGetModelResource(ModelComponent* component);
    dmGameObject::HInstance CompModelGetNodeInstance(ModelComponent* component, uint32_t bone_index);
    bool                    CompModelSetMeshEnabled(ModelComponent* component, dmhash_t mesh_id, bool enabled);
    bool                    CompModelGetMeshEnabled(ModelComponent* component, dmhash_t mesh_id, bool* out);

    uint32_t                CompModelGetMeshCount(ModelComponent* component);
    void                    CompModelGetAABB(ModelComponent* component, dmVMath::Vector3* out_min, dmVMath::Vector3* out_max);
    void                    CompModelGetMeshAABB(ModelComponent* component, uint32_t mesh_idx, dmhash_t* out_mesh_id, dmVMath::Vector3* out_min, dmVMath::Vector3* out_max);

    // these aren't used yet??
    bool CompModelSetIKTargetInstance(ModelComponent* component, dmhash_t constraint_id, float mix, dmhash_t instance_id);
    bool CompModelSetIKTargetPosition(ModelComponent* component, dmhash_t constraint_id, float mix, dmVMath::Point3 position);
}

#endif // DM_GAMESYS_COMP_MODEL_H
