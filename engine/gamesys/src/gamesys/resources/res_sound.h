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

#ifndef DM_GAMESYS_RES_SOUND_H
#define DM_GAMESYS_RES_SOUND_H

#include <stdint.h>

#include <sound/sound.h>
#include <resource/resource.h>

namespace dmGameSystem
{
    struct Sound
    {
        Sound();

        dmhash_t            m_GroupHash;
        dmSound::HSoundData m_SoundData;
        float               m_Gain;
        float               m_Pan;
        float               m_Speed;
        uint8_t             m_Looping:1;
    };

    dmResource::Result ResSoundPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResSoundCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResSoundDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResSoundRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif
