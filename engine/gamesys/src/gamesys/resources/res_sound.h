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
