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

#ifndef DM_GAMESYS_SOUND_H
#define DM_GAMESYS_SOUND_H

#include <gameobject/component.h>

namespace dmGameSystemDDF
{
    struct PlaySound;
}

namespace dmGameSystem
{
    struct SoundWorld;
    struct SoundComponent;

    // UINTPTR_MAX suppresses completion events for direct calls without a callback.
    bool CompSoundPlay(SoundWorld* world, dmGameObject::HInstance instance, SoundComponent* component, const dmGameSystemDDF::PlaySound& play_sound, const dmMessage::URL& receiver, const dmMessage::URL& listener, uintptr_t callback);
    void CompSoundStop(SoundWorld* world, dmGameObject::HInstance instance, SoundComponent* component, uint32_t play_id);
    void CompSoundPause(SoundWorld* world, dmGameObject::HInstance instance, SoundComponent* component, bool pause);
    dmGameObject::PropertyResult CompSoundSetGain(SoundWorld* world, dmGameObject::HInstance instance, SoundComponent* component, float gain, uint32_t play_id, dmhash_t component_id);
    dmGameObject::PropertyResult CompSoundSetPan(SoundWorld* world, dmGameObject::HInstance instance, SoundComponent* component, float pan, uint32_t play_id, dmhash_t component_id);
}

#endif
