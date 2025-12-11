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

#ifndef DM_GAMESYS_COMP_LIGHT_H
#define DM_GAMESYS_COMP_LIGHT_H

#include <gameobject/component.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompLightNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompLightDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompLightCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompLightDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompLightAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompLightLateUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompLightOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void*                      CompLightGetComponent(const dmGameObject::ComponentGetParams& params);
}

#endif
