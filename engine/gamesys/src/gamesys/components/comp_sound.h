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

#ifndef DM_GAMESYS_SOUND_H
#define DM_GAMESYS_SOUND_H

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompSoundNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompSoundDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompSoundCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompSoundDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompSoundAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompSoundUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompSoundOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    dmGameObject::PropertyResult CompSoundGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompSoundSetProperty(const dmGameObject::ComponentSetPropertyParams& params);
}

#endif
