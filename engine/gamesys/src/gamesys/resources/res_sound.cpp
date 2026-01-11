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

#include "res_sound.h"
#include <string.h>

#include <dlib/log.h>
#include <sound/sound.h>
#include <gamesys/sound_ddf.h>

namespace dmGameSystem
{
    Sound::Sound()
    {
        memset(this, 0, sizeof(*this));
    }

    dmResource::Result AcquireResources(dmResource::HFactory factory,
                                        dmSoundDDF::SoundDesc* sound_desc,
                                        Sound** sound)
    {
        SoundDataResource* sound_data_res = 0;
        dmResource::Result fr = dmResource::Get(factory, sound_desc->m_Sound, (void**) &sound_data_res);
        if (fr == dmResource::RESULT_OK)
        {
            Sound* s = new Sound();
            s->m_SoundDataRes = sound_data_res;
            s->m_Looping = sound_desc->m_Looping;
            s->m_Loopcount = sound_desc->m_Loopcount;
            s->m_GroupHash = dmHashString64(sound_desc->m_Group);
            s->m_Gain = sound_desc->m_Gain;
            s->m_Pan = sound_desc->m_Pan;
            s->m_Speed = sound_desc->m_Speed;

            dmSound::Result result = dmSound::AddGroup(sound_desc->m_Group);
            if (result != dmSound::RESULT_OK) {
                dmLogError("Failed to create group '%s' (%d)", sound_desc->m_Group, result);
            }

            *sound = s;
        }

        dmDDF::FreeMessage(sound_desc);
        return fr;
    }


    dmResource::Result ResSoundPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmSoundDDF::SoundDesc* sound_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmSoundDDF::SoundDesc>(params->m_Buffer, params->m_BufferSize, &sound_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmResource::PreloadHint(params->m_HintInfo, sound_desc->m_Sound);
        *params->m_PreloadData = sound_desc;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSoundCreate(const dmResource::ResourceCreateParams* params)
    {
        dmSoundDDF::SoundDesc* sound_desc = (dmSoundDDF::SoundDesc*) params->m_PreloadData;
        Sound* sound;
        dmResource::Result r = AcquireResources(params->m_Factory, sound_desc, &sound);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, sound);
        }
        return r;
    }

    dmResource::Result ResSoundDestroy(const dmResource::ResourceDestroyParams* params)
    {
        Sound* sound = (Sound*) dmResource::GetResource(params->m_Resource);

        dmResource::Release(params->m_Factory, (void*) sound->m_SoundDataRes);
        delete sound;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSoundRecreate(const dmResource::ResourceRecreateParams* params)
    {
        // Not supported yet. This might be difficult to set a new HSoundInstance
        // while a sound is playing?
        return dmResource::RESULT_OK;
    }
}
