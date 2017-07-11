#include "res_sound.h"

#include <dlib/log.h>
#include <sound/sound.h>
#include "sound_ddf.h"

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory,
                                        dmSoundDDF::SoundDesc* sound_desc,
                                        Sound** sound)
    {
        dmSound::HSoundData sound_data = 0;
        dmResource::Result fr = dmResource::Get(factory, sound_desc->m_Sound, (void**) &sound_data);
        if (fr == dmResource::RESULT_OK)
        {
            Sound*s = new Sound();
            s->m_SoundData = sound_data;
            s->m_Looping = sound_desc->m_Looping;
            s->m_GroupHash = dmHashString64(sound_desc->m_Group);
            s->m_Gain = sound_desc->m_Gain;

            dmSound::Result result = dmSound::AddGroup(sound_desc->m_Group);
            if (result != dmSound::RESULT_OK) {
                dmLogError("Failed to create group '%s' (%d)", sound_desc->m_Group, result);
            }

            *sound = s;
        }

        dmDDF::FreeMessage(sound_desc);
        return fr;
    }


    dmResource::Result ResSoundPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmSoundDDF::SoundDesc* sound_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmSoundDDF::SoundDesc>(params.m_Buffer, params.m_BufferSize, &sound_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmResource::PreloadHint(params.m_HintInfo, sound_desc->m_Sound);
        *params.m_PreloadData = sound_desc;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSoundCreate(const dmResource::ResourceCreateParams& params)
    {
        dmSoundDDF::SoundDesc* sound_desc = (dmSoundDDF::SoundDesc*) params.m_PreloadData;
        Sound* sound;
        dmResource::Result r = AcquireResources(params.m_Factory, sound_desc, &sound);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) sound;
        }
        return r;
    }

    dmResource::Result ResSoundDestroy(const dmResource::ResourceDestroyParams& params)
    {
        Sound* sound = (Sound*) params.m_Resource->m_Resource;

        dmResource::Release(params.m_Factory, (void*) sound->m_SoundData);
        delete sound;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSoundRecreate(const dmResource::ResourceRecreateParams& params)
    {
        // Not supported yet. This might be difficult to set a new HSoundInstance
        // while a sound is playing?
        return dmResource::RESULT_OK;
    }
}
