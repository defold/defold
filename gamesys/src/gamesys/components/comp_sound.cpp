#include "comp_model.h"

#include <dlib/log.h>
#include <dlib/hash.h>
#include <sound/sound.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompSoundNewWorld(void* context, void** world)
    {
        *world = 0;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSoundDeleteWorld(void* context, void* world)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSoundCreate(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data)
    {
        dmSound::HSoundData sound_data = (dmSound::HSoundData) resource;
        dmSound::HSoundInstance sound_instance;
        dmSound::Result r = dmSound::NewSoundInstance(sound_data, &sound_instance);
        if (r != dmSound::RESULT_OK)
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        *user_data = (uintptr_t) sound_instance;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSoundDestroy(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* world,
                                                void* context,
                                                uintptr_t* user_data)
    {
        dmSound::HSoundInstance sound_instance = (dmSound::HSoundInstance)*user_data;
        dmSound::DeleteSoundInstance(sound_instance);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSoundUpdate(dmGameObject::HCollection collection,
                                               const dmGameObject::UpdateContext* update_context,
                                               void* world,
                                               void* context)
    {
        dmSound::Update();
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSoundOnMessage(dmGameObject::HInstance instance,
                                                const dmGameObject::InstanceMessageData* message_data,
                                                void* context,
                                                uintptr_t* user_data)
    {
        if (message_data->m_MessageId == dmHashString64("play_sound"))
        {
            dmSound::HSoundInstance sound_instance = (dmSound::HSoundInstance)*user_data;
            dmSound::Result r = dmSound::Play(sound_instance);
            if (r != dmSound::RESULT_OK)
            {
                dmLogWarning("Error playing sound (%d)", r);
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }
}
