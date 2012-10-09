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
    };

    dmResource::Result ResSoundCreate(dmResource::HFactory factory,
                                      void* context,
                                      const void* buffer, uint32_t buffer_size,
                                      dmResource::SResourceDescriptor* resource,
                                      const char* filename);

    dmResource::Result ResSoundDestroy(dmResource::HFactory factory,
                                       void* context,
                                       dmResource::SResourceDescriptor* resource);

    dmResource::Result ResSoundRecreate(dmResource::HFactory factory,
                                        void* context,
                                        const void* buffer, uint32_t buffer_size,
                                        dmResource::SResourceDescriptor* resource,
                                        const char* filename);
}

#endif
