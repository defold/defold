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

#ifndef DM_GAMESYS_COMP_EMITTER_H
#define DM_GAMESYS_COMP_EMITTER_H

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompEmitterNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompEmitterDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompEmitterCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompEmitterDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::UpdateResult CompEmitterOnMessage(const dmGameObject::ComponentOnMessageParams& params);

}

#endif // DM_GAMESYS_COMP_EMITTER_H
