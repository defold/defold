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

#ifndef DM_GAMEOBJECT_COMP_ANIM_H
#define DM_GAMEOBJECT_COMP_ANIM_H

#include "component.h"

namespace dmGameObject
{
    struct Collection;
    struct Instance;

    const uint32_t EULER_PENDING = 1;
    const uint32_t EULER_WRITTEN = 2;

    struct EulerRotation
    {
        dmVMath::Vector3 m_Value;
        uint32_t m_InstanceIndex;
        uint32_t m_State;
        uint32_t m_NextFree;
    };

    // Borrowed until the next Euler pool growth. Records retain axis values until
    // instance deletion, including after animations stop, for partial Euler setters.
    EulerRotation* GetEulerRotation(Collection* collection, Instance* instance, bool create);
    void ReleaseEulerRotation(Collection* collection, HGameObject instance);

    // Synchronize in-progress animation writes before a collection-wide transform refresh.
    void CommitPendingEulerRotations(Collection* collection);

    CreateResult CompAnimNewWorld(const ComponentNewWorldParams& params);

    CreateResult CompAnimDeleteWorld(const ComponentDeleteWorldParams& params);

    CreateResult CompAnimAddToUpdate(const ComponentAddToUpdateParams& params);

    UpdateResult CompAnimUpdate(const ComponentsUpdateParams& params, ComponentsUpdateResult& result);
}

#endif // DM_GAMEOBJECT_COMP_ANIM_H
