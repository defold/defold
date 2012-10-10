#include "comp_model.h"

#include <string.h>

#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/hash.h>
#include <sound/sound.h>

#include "gamesys_ddf.h"
#include "../gamesys_private.h"
#include "../resources/res_sound.h"

namespace dmGameSystem
{
    struct PlayEntry
    {
        dmResource::HFactory    m_Factory;
        Sound*                  m_Sound;
        dmSound::HSoundInstance m_SoundInstance;
        float                   m_Delay;
        uint32_t                m_StopRequested : 1;
    };

    struct World
    {
        dmArray<PlayEntry>  m_Entries;
        dmIndexPool32       m_EntryIndices;
    };

    dmGameObject::CreateResult CompSoundNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        World* world = new World();
        const uint32_t MAX_INSTANCE_COUNT = 32;
        world->m_Entries.SetCapacity(MAX_INSTANCE_COUNT);
        world->m_Entries.SetSize(MAX_INSTANCE_COUNT);
        world->m_EntryIndices.SetCapacity(MAX_INSTANCE_COUNT);
        memset(&world->m_Entries.Front(), 0, MAX_INSTANCE_COUNT * sizeof(PlayEntry));
        *params.m_World = (void*)world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSoundDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        World* world = (World*)params.m_World;
        uint32_t size = world->m_Entries.Size();

        for (uint32_t i = 0; i < size; ++i)
        {
            PlayEntry& entry = world->m_Entries[i];
            if (entry.m_SoundInstance != 0)
            {
                dmSound::Stop(entry.m_SoundInstance);
                dmSound::DeleteSoundInstance(entry.m_SoundInstance);
                dmResource::Release(entry.m_Factory, entry.m_Sound);
            }
        }
        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSoundCreate(const dmGameObject::ComponentCreateParams& params)
    {
        *params.m_UserData = (uintptr_t) params.m_Resource;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSoundDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSoundUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        dmGameObject::UpdateResult update_result = dmGameObject::UPDATE_RESULT_OK;
        World* world = (World*)params.m_World;
        for (uint32_t i = 0; i < world->m_Entries.Size(); ++i)
        {
            PlayEntry& entry = world->m_Entries[i];
            if (entry.m_SoundInstance != 0)
            {
                float prev_delay = entry.m_Delay;
                entry.m_Delay -= params.m_UpdateContext->m_DT;
                if (entry.m_Delay < 0.0f)
                {
                    if (prev_delay >= 0.0f)
                    {
                        dmSound::Result r = dmSound::Play(entry.m_SoundInstance);
                        if (r != dmSound::RESULT_OK)
                        {
                            dmLogError("Error playing sound: (%d)", r);
                            update_result = dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
                            // IsPlaying will hopefully and eventually be true
                            // so that the instance can be removed
                        }
                    }
                    else if (!dmSound::IsPlaying(entry.m_SoundInstance))
                    {
                        dmResource::Release(entry.m_Factory, entry.m_Sound);
                        dmSound::Result r = dmSound::DeleteSoundInstance(entry.m_SoundInstance);
                        entry.m_SoundInstance = 0;
                        world->m_EntryIndices.Push(i);
                        if (r != dmSound::RESULT_OK)
                        {
                            dmLogError("Error deleting sound: (%d)", r);
                            update_result = dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
                        }
                    }
                    else if (entry.m_StopRequested)
                    {
                        dmSound::Result r = dmSound::Stop(entry.m_SoundInstance);
                        if (r != dmSound::RESULT_OK)
                        {
                            dmLogError("Error deleting sound: (%d)", r);
                            update_result = dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
                        }
                    }
                }
            }
        }
        dmSound::Update();
        return update_result;
    }

    dmGameObject::UpdateResult CompSoundOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::PlaySound::m_DDFDescriptor)
        {
            World* world = (World*)params.m_World;
            if (world->m_EntryIndices.Remaining() > 0)
            {
                dmGameSystemDDF::PlaySound* play_sound = (dmGameSystemDDF::PlaySound*)params.m_Message->m_Data;
                Sound* sound = (Sound*) *params.m_UserData;
                dmSound::HSoundData sound_data = sound->m_SoundData;
                uint32_t index = world->m_EntryIndices.Pop();
                PlayEntry& entry = world->m_Entries[index];
                dmResource::HFactory factory = dmGameObject::GetFactory(dmGameObject::GetCollection(params.m_Instance));
                // NOTE: We must increase ref-count as a sound might be active after the component is destroyed
                dmResource::IncRef(factory, sound);
                entry.m_Factory = factory;
                entry.m_Sound = sound;
                entry.m_StopRequested = 0;
                entry.m_Delay = play_sound->m_Delay;
                dmSound::Result result = dmSound::NewSoundInstance(sound_data, &entry.m_SoundInstance);
                if (result == dmSound::RESULT_OK)
                {
                    dmSound::SetParameter(entry.m_SoundInstance, dmSound::PARAMETER_GAIN, Vectormath::Aos::Vector4(play_sound->m_Gain, 0, 0, 0));
                    dmSound::SetLooping(entry.m_SoundInstance, sound->m_Looping);
                }
                else
                {
                    // Free the sound index slot if we can't create a sound-instance.
                    world->m_EntryIndices.Push(index);
                    LogMessageError(params.m_Message, "A sound could not be played, error: %d.", result);
                }
            }
            else
            {
                LogMessageError(params.m_Message, "A sound could not be played since the sound buffer is full (%d).", world->m_EntryIndices.Capacity());
            }
        }
        else if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::StopSound::m_DDFDescriptor)
        {
            World* world = (World*)params.m_World;
            for (uint32_t i = 0; i < world->m_Entries.Size(); ++i)
            {
                PlayEntry& entry = world->m_Entries[i];
                if (entry.m_SoundInstance != 0 && entry.m_Sound == (Sound*) *params.m_UserData)
                {
                    entry.m_StopRequested = 1;
                }
            }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }
}
