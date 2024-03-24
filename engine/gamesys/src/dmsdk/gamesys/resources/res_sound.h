// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_GAMESYS_RES_SOUND_H
#define DMSDK_GAMESYS_RES_SOUND_H

#include <stdint.h>
#include <dmsdk/dlib/hash.h>

namespace dmGameSystem
{
    struct SoundDataResource;

    struct Sound
    {
        Sound();

        dmhash_t                    m_GroupHash;
        SoundDataResource*          m_SoundDataRes;
        float                       m_Gain;
        float                       m_Pan;
        float                       m_Speed;
        uint8_t                     m_Loopcount;
        uint8_t                     m_Looping:1;
    };
}

#endif
