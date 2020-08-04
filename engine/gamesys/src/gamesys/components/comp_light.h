// Copyright 2020 The Defold Foundation
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

#include <dlib/array.h>
#include <gameobject/gameobject.h>
#include "gamesys_ddf.h"

namespace dmGameSystem
{
    struct DM_ALIGNED(16) Light
    {
        dmGameObject::HInstance      m_Instance;
        dmGameSystemDDF::LightDesc** m_LightResource;
        uint16_t                     m_AddedToUpdate : 1;
        uint16_t                     m_Padding : 15;
        Light(dmGameObject::HInstance instance, dmGameSystemDDF::LightDesc** light_resource)
        {
            m_Instance = instance;
            m_LightResource = light_resource;
            m_AddedToUpdate = 1;
        }
    };

    struct LightWorld
    {
        dmArray<Light*> DM_ALIGNED(16) m_Lights;
    };

    dmGameObject::CreateResult CompLightNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompLightDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompLightCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompLightDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompLightAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompLightUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompLightOnMessage(const dmGameObject::ComponentOnMessageParams& params);
}

#endif
