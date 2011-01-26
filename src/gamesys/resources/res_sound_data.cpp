#include <sound/sound.h>
#include "res_sound_data.h"

namespace dmGameSystem
{
    dmResource::CreateResult ResSoundDataCreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename)
    {
        dmSound::HSoundData sound_data;
        dmSound::Result r = dmSound::NewSoundData(buffer, buffer_size, dmSound::SOUND_DATA_TYPE_WAV, &sound_data);
        if (r != dmSound::RESULT_OK)
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        resource->m_Resource = (void*) sound_data;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResSoundDataDestroy(dmResource::HFactory factory,
                                                 void* context,
                                                 dmResource::SResourceDescriptor* resource)
    {
        dmSound::HSoundData sound_data = (dmSound::HSoundData) resource->m_Resource;
        dmSound::Result r = dmSound::DeleteSoundData(sound_data);
        if (r != dmSound::RESULT_OK)
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResSoundDataRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmSound::HSoundData sound_data = (dmSound::HSoundData) resource->m_Resource;
        dmSound::Result r = dmSound::SetSoundData(sound_data, buffer, buffer_size);
        if (r != dmSound::RESULT_OK)
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        return dmResource::CREATE_RESULT_OK;
    }
}
