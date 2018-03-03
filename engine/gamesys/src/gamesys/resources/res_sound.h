#ifndef DM_GAMESYS_RES_SOUND_H
#define DM_GAMESYS_RES_SOUND_H

#include <stdint.h>
#include <string.h>

#include <sound/sound.h>
#include <resource/resource.h>

namespace dmGameSystem
{
    struct Sound
    {
        Sound()
        {
            memset(this, 0, sizeof(*this));
        }

        dmSound::HSoundData m_SoundData;
        bool m_Looping;
        dmhash_t m_GroupHash;
        float    m_Gain;
    };

    dmResource::Result ResSoundPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResSoundCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResSoundDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResSoundRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResSoundGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif
