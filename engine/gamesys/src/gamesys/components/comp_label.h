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

#ifndef DM_GAMESYS_COMP_LABEL_H
#define DM_GAMESYS_COMP_LABEL_H

#include <gameobject/component.h>

namespace dmRender
{
    struct TextMetrics;
}

namespace dmGameSystem
{
    dmGameObject::CreateResult CompLabelNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompLabelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompLabelCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompLabelDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompLabelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    void*                       CompLabelGetComponent(const dmGameObject::ComponentGetParams& params);

    dmGameObject::UpdateResult CompLabelUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompLabelRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompLabelOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompLabelOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompLabelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompLabelSetProperty(const dmGameObject::ComponentSetPropertyParams& params);

    // For scripting
    struct LabelComponent;

    void CompLabelGetTextMetrics(const LabelComponent* component, dmRender::TextMetrics& metrics);

    const char* CompLabelGetText(const LabelComponent* component);

    dmVMath::Matrix4 CompLabelLocalTransform(const dmVMath::Point3& position, const dmVMath::Quat& rotation, const dmVMath::Vector3& scale, const dmVMath::Vector3& size, uint32_t pivot);

    void CompLabelIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node);
}

#endif // DM_GAMESYS_COMP_LABEL_H
