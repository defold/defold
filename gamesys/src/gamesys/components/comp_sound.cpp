#include "comp_model.h"

#include <dlib/log.h>
#include <dlib/hash.h>
#include <sound/sound.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompSoundNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        *params.m_World = 0;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSoundDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSoundCreate(const dmGameObject::ComponentCreateParams& params)
    {
        dmSound::HSoundData sound_data = (dmSound::HSoundData) params.m_Resource;
        dmSound::HSoundInstance sound_instance;
        dmSound::Result r = dmSound::NewSoundInstance(sound_data, &sound_instance);
        if (r != dmSound::RESULT_OK)
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        *params.m_UserData = (uintptr_t) sound_instance;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSoundDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        dmSound::HSoundInstance sound_instance = (dmSound::HSoundInstance)*params.m_UserData;
        dmSound::DeleteSoundInstance(sound_instance);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSoundUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        dmSound::Update();
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSoundOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        if (params.m_Message->m_Id == dmHashString64("play_sound"))
        {
            dmSound::HSoundInstance sound_instance = (dmSound::HSoundInstance)*params.m_UserData;
            dmSound::Result r = dmSound::Play(sound_instance);
            if (r != dmSound::RESULT_OK)
            {
                dmLogWarning("Error playing sound (%d)", r);
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }
}
