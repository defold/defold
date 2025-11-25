// Copyright 2020-2025 The Defold Foundation
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

#include "comp_model.h"

namespace dmGameSystem
{

    dmGameObject::CreateResult CompModelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelCreate(const dmGameObject::ComponentCreateParams& params)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    void* CompModelGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        return 0x0;
    }

    dmGameObject::CreateResult CompModelDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelLateUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelRender(const dmGameObject::ComponentsRenderParams& params)
    {
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompModelOnReload(const dmGameObject::ComponentOnReloadParams& params)
    { }

    dmGameObject::PropertyResult CompModelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        return dmGameObject::PROPERTY_RESULT_OK;
    }

    dmGameObject::PropertyResult CompModelSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        return dmGameObject::PROPERTY_RESULT_OK;
    }

    bool CompModelSetIKTargetInstance(ModelComponent* component, dmhash_t constraint_id, float mix, dmhash_t instance_id)
    {
        return true;
    }

    bool CompModelSetIKTargetPosition(ModelComponent* component, dmhash_t constraint_id, float mix, dmVMath::Point3 position)
    {
        return true;
    }

    ModelResource* CompModelGetModelResource(ModelComponent* component)
    {
        return 0x0;
    }

    dmGameObject::HInstance CompModelGetNodeInstance(ModelComponent* component, uint32_t bone_index)
    {
        return 0x0;
    }

    bool CompModelSetMeshEnabled(ModelComponent* component, dmhash_t mesh_id, bool enabled)
    {
        return false;
    }

    bool CompModelGetMeshEnabled(ModelComponent* component, dmhash_t mesh_id, bool* out)
    {
        return false;
    }

    uint32_t CompModelGetMeshCount(ModelComponent* component)
    {
        return 0;
    }

    void CompModelGetAABB(ModelComponent* component, dmVMath::Vector3* out_min, dmVMath::Vector3* out_max)
    { }

    void CompModelGetMeshAABB(ModelComponent* component, uint32_t mesh_idx, dmhash_t* out_mesh_id, dmVMath::Vector3* out_min, dmVMath::Vector3* out_max)
    { }

    void CompModelIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node)
    { }
}
