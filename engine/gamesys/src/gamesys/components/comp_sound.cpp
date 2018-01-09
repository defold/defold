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
        dmGameObject::HInstance m_Instance;
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
            }
        }

        // An update is required after dmSound::Stop and the follwing dmSound::DeleteSoundInstance
        // Immediate stop isn't supported and will rending in AL_INVALID_OPERATION. Probably due to state-mismatch in source/buffer
        dmSound::Update();

        for (uint32_t i = 0; i < size; ++i)
        {
            PlayEntry& entry = world->m_Entries[i];
            if (entry.m_SoundInstance != 0)
            {
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

    dmGameObject::CreateResult CompSoundAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        // Intentional pass-through
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSoundUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult&)
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

    /*# Sound API documentation
     *
     * @document
     * @name Sound
     * @namespace sound
     */

    /*# plays a sound
     * Post this message to a sound-component to make it play its sound. Multiple voices is support. The limit is set to 32 voices per sound component.
     *
     * [icon:attention] Note that gain is in linear scale, between 0 and 1.
     * To get the dB value from the gain, use the formula `20 * log(gain)`.
     * Inversely, to find the linear value from a dB value, use the formula
     * <code>10<sup>db/20</sup></code>.
     *
     * @message
     * @name play_sound
     * @param [delay] [type:number] delay in seconds before the sound starts playing, default is 0.
     * @param [gain] [type:number] sound gain between 0 and 1, default is 1.
     * @examples
     *
     * Assuming the script belongs to an instance with a sound-component with id "sound", this will make the component play its sound after 1 second:
     *
     * ```lua
     * msg.post("#sound", "play_sound", {delay = 1, gain = 0.5})
     * ```
     */

    /*# stop a playing a sound(s)
     * Post this message to a sound-component to make it stop playing all active voices
     *
     * @message
     * @name stop_sound
     * @examples
     *
     * Assuming the script belongs to an instance with a sound-component with id "sound", this will make the component stop all playing voices:
     *
     * ```lua
     * msg.post("#sound", "stop_sound")
     * ```
     */

    /*# set sound gain
     * Post this message to a sound-component to set gain on all active playing voices.
     *
     * [icon:attention] Note that gain is in linear scale, between 0 and 1.
     * To get the dB value from the gain, use the formula `20 * log(gain)`.
     * Inversely, to find the linear value from a dB value, use the formula
     * <code>10<sup>db/20</sup></code>.
     *
     * @message
     * @name set_gain
     * @param [gain] [type:number] sound gain between 0 and 1, default is 1.
     * @examples
     *
     * Assuming the script belongs to an instance with a sound-component with id "sound", this will set the gain to 0.5
     *
     * ```lua
     * msg.post("#sound", "set_gain", {gain = 0.5})
     * ```
     */

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
                entry.m_Instance = params.m_Instance;
                entry.m_Delay = play_sound->m_Delay;
                dmSound::Result result = dmSound::NewSoundInstance(sound_data, &entry.m_SoundInstance);
                if (result == dmSound::RESULT_OK)
                {
                    result = dmSound::SetInstanceGroup(entry.m_SoundInstance, entry.m_Sound->m_GroupHash);
                    if (result != dmSound::RESULT_OK) {
                        dmLogError("Failed to set sound group (%d)", result);
                    }

                    float gain = play_sound->m_Gain * entry.m_Sound->m_Gain;
                    dmSound::SetParameter(entry.m_SoundInstance, dmSound::PARAMETER_GAIN, Vectormath::Aos::Vector4(gain, 0, 0, 0));
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
                if (entry.m_SoundInstance != 0 && entry.m_Sound == (Sound*) *params.m_UserData && entry.m_Instance == params.m_Instance)
                {
                    entry.m_StopRequested = 1;
                }
            }
        }
        else if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::SetGain::m_DDFDescriptor)
        {
            World* world = (World*)params.m_World;
            dmGameSystemDDF::SetGain* set_gain = (dmGameSystemDDF::SetGain*)params.m_Message->m_Data;

            for (uint32_t i = 0; i < world->m_Entries.Size(); ++i)
            {
                PlayEntry& entry = world->m_Entries[i];
                if (entry.m_SoundInstance != 0 && entry.m_Sound == (Sound*) *params.m_UserData && entry.m_Instance == params.m_Instance)
                {
                    float gain = set_gain->m_Gain * entry.m_Sound->m_Gain;
                    dmSound::Result r = dmSound::SetParameter(entry.m_SoundInstance, dmSound::PARAMETER_GAIN, Vector4(gain, 0, 0, 0));
                    if (r != dmSound::RESULT_OK)
                    {
                        dmLogError("Fail to set gain on sound");
                    }
                }
            }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }
}
