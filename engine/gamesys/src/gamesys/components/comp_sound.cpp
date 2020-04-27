#include "comp_sound.h"

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
        dmMessage::URL          m_Listener;
        dmMessage::URL          m_Receiver;
        dmGameObject::HInstance m_Instance;
        float                   m_Delay;
        uint32_t                m_PlayId;
        uint32_t                m_StopRequested  : 1;
        uint32_t                m_PauseRequested : 1;
        uint32_t                m_Paused         : 1;
    };

    struct World
    {
        dmArray<PlayEntry>  m_Entries;
        dmIndexPool32       m_EntryIndices;
    };

    static const dmhash_t SOUND_PROP_GAIN   = dmHashString64("gain");
    static const dmhash_t SOUND_PROP_PAN    = dmHashString64("pan");
    static const dmhash_t SOUND_PROP_SPEED  = dmHashString64("speed");

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
                    else if (!dmSound::IsPlaying(entry.m_SoundInstance) && !(entry.m_PauseRequested || entry.m_Paused))
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
                        else if (entry.m_PlayId != dmSound::INVALID_PLAY_ID && entry.m_Listener.m_Fragment != 0x0)
                        {
                            dmhash_t message_id = dmGameSystemDDF::SoundDone::m_DDFDescriptor->m_NameHash;
                            dmGameSystemDDF::SoundDone message;

                            dmMessage::URL receiver = entry.m_Listener;
                            dmMessage::URL sender = entry.m_Receiver;

                            if (dmMessage::IsSocketValid(sender.m_Socket) && dmMessage::IsSocketValid(receiver.m_Socket))
                            {
                                uintptr_t descriptor = (uintptr_t)dmGameSystemDDF::SoundDone::m_DDFDescriptor;
                                uint32_t data_size = sizeof(dmGameSystemDDF::SoundDone);

                                message.m_PlayId = entry.m_PlayId;

                                if (dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &message, data_size, 0) != dmMessage::RESULT_OK)
                                {
                                    dmLogError("Could not send sound_done to listener.");
                                }
                            }

                            dmMessage::ResetURL(entry.m_Receiver);
                            dmMessage::ResetURL(entry.m_Listener);
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
                    else if (entry.m_PauseRequested)
                    {
                        entry.m_PauseRequested = 0;
                        dmSound::Result r = dmSound::Pause(entry.m_SoundInstance, (bool)entry.m_Paused);
                        if (r != dmSound::RESULT_OK)
                        {
                            dmLogError("Error pausing sound: (%d)", r);
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
     * Post this message to a sound-component to make it play its sound. Multiple voices is supported. The limit is set to 32 voices per sound component.
     *
     * [icon:attention] Note that gain is in linear scale, between 0 and 1.
     * To get the dB value from the gain, use the formula `20 * log(gain)`.
     * Inversely, to find the linear value from a dB value, use the formula
     * <code>10<sup>db/20</sup></code>.
     *
     * [icon:attention] A sound will continue to play even if the game object the sound component belonged to is deleted. You can send a `stop_sound` to stop the sound.
     *
     * @message
     * @name play_sound
     * @param [delay] [type:number] delay in seconds before the sound starts playing, default is 0.
     * @param [gain] [type:number] sound gain between 0 and 1, default is 1.
     * @param [play_id] [type:number] the identifier of the sound, can be used to distinguish between consecutive plays from the same component.
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

    /*# reports when a sound has finished playing
     * This message is sent back to the sender of a `play_sound` message, if the sound
     * could be played to completion.
     *
     * @message
     * @name sound_done
     * @param [play_id] [type:number] id number supplied when the message was posted.
     */

    static dmGameObject::PropertyResult SoundSetParameter(World* world, dmGameObject::HInstance instance, Sound* sound, dmSound::Parameter type, float value)
    {
        uint32_t size = world->m_Entries.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            PlayEntry& entry = world->m_Entries[i];
            if (entry.m_SoundInstance != 0 && entry.m_Sound == sound && entry.m_Instance == instance)
            {
                float v = value;
                switch(type) {
                case dmSound::PARAMETER_GAIN:   v *= entry.m_Sound->m_Gain; break;
                case dmSound::PARAMETER_PAN:    v += entry.m_Sound->m_Pan; break;
                case dmSound::PARAMETER_SPEED:  v *= entry.m_Sound->m_Speed; break;
                default:
                    return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
                }

                dmSound::Result r = dmSound::SetParameter(entry.m_SoundInstance, type, Vectormath::Aos::Vector4(v, 0, 0, 0));
                if (r != dmSound::RESULT_OK)
                {
                    return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
                }
            }
        }
        return dmGameObject::PROPERTY_RESULT_OK;
    }

    static dmGameObject::PropertyResult SoundGetParameter(World* world, dmGameObject::HInstance instance, Sound* sound, dmSound::Parameter type, dmGameObject::PropertyDesc& out_value)
    {
        uint32_t size = world->m_Entries.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            PlayEntry& entry = world->m_Entries[i];
            if (entry.m_SoundInstance != 0 && entry.m_Sound == sound && entry.m_Instance == instance)
            {
                float value;
                switch(type) {
                case dmSound::PARAMETER_GAIN:   value = entry.m_Sound->m_Gain; break;
                case dmSound::PARAMETER_PAN:    value = entry.m_Sound->m_Pan; break;
                case dmSound::PARAMETER_SPEED:  value = entry.m_Sound->m_Speed; break;
                default:
                    return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
                }

                out_value.m_Variant = dmGameObject::PropertyVar(value);
                return dmGameObject::PROPERTY_RESULT_OK;
            }
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
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
                entry.m_PauseRequested = 0;
                entry.m_Paused = 0;
                entry.m_Instance = params.m_Instance;
                entry.m_Receiver = params.m_Message->m_Receiver;
                entry.m_Delay = play_sound->m_Delay;
                entry.m_PlayId = play_sound->m_PlayId;
                dmMessage::ResetURL(entry.m_Listener);
                dmSound::Result result = dmSound::NewSoundInstance(sound_data, &entry.m_SoundInstance);
                if (result == dmSound::RESULT_OK)
                {
                    result = dmSound::SetInstanceGroup(entry.m_SoundInstance, entry.m_Sound->m_GroupHash);
                    if (result != dmSound::RESULT_OK) {
                        dmLogError("Failed to set sound group (%d)", result);
                    }

                    float gain = play_sound->m_Gain * entry.m_Sound->m_Gain;
                    float pan = play_sound->m_Pan + entry.m_Sound->m_Pan;
                    float speed = play_sound->m_Speed * entry.m_Sound->m_Speed;
                    dmSound::SetParameter(entry.m_SoundInstance, dmSound::PARAMETER_GAIN, Vectormath::Aos::Vector4(gain, 0, 0, 0));
                    dmSound::SetParameter(entry.m_SoundInstance, dmSound::PARAMETER_PAN, Vectormath::Aos::Vector4(pan, 0, 0, 0));
                    dmSound::SetParameter(entry.m_SoundInstance, dmSound::PARAMETER_SPEED, Vectormath::Aos::Vector4(speed, 0, 0, 0));
                    dmSound::SetLooping(entry.m_SoundInstance, sound->m_Looping);

                    entry.m_Listener = params.m_Message->m_Sender;
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
        else if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::PauseSound::m_DDFDescriptor)
        {
            World* world = (World*)params.m_World;
            dmGameSystemDDF::PauseSound* pause_sound = (dmGameSystemDDF::PauseSound*)params.m_Message->m_Data;
            uint32_t pause = (uint32_t)pause_sound->m_Pause;
            for (uint32_t i = 0; i < world->m_Entries.Size(); ++i)
            {
                PlayEntry& entry = world->m_Entries[i];
                if (entry.m_SoundInstance != 0 && entry.m_Sound == (Sound*) *params.m_UserData && entry.m_Instance == params.m_Instance)
                {
                    entry.m_PauseRequested = 1;
                    entry.m_Paused = pause;
                }
            }
        }
        else if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::SetGain::m_DDFDescriptor)
        {
            World* world = (World*) params.m_World;
            Sound* sound = (Sound*) *params.m_UserData;
            dmGameSystemDDF::SetGain* set_gain = (dmGameSystemDDF::SetGain*)params.m_Message->m_Data;

            if (dmGameObject::PROPERTY_RESULT_OK != SoundSetParameter(world, params.m_Instance, sound, dmSound::PARAMETER_GAIN, set_gain->m_Gain))
            {
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
            return dmGameObject::UPDATE_RESULT_OK;
        }
        else if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::SetPan::m_DDFDescriptor)
        {
            World* world = (World*)params.m_World;
            dmGameSystemDDF::SetPan* ddf = (dmGameSystemDDF::SetPan*)params.m_Message->m_Data;

            for (uint32_t i = 0; i < world->m_Entries.Size(); ++i)
            {
                PlayEntry& entry = world->m_Entries[i];
                if (entry.m_SoundInstance != 0 && entry.m_Sound == (Sound*) *params.m_UserData && entry.m_Instance == params.m_Instance)
                {
                    float pan = ddf->m_Pan + entry.m_Sound->m_Pan;
                    dmSound::Result r = dmSound::SetParameter(entry.m_SoundInstance, dmSound::PARAMETER_PAN, Vectormath::Aos::Vector4(pan, 0, 0, 0));
                    if (r != dmSound::RESULT_OK)
                    {
                        dmLogError("Fail to set pan on sound");
                    }
                }
            }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static dmSound::Parameter GetSoundParameterType(dmhash_t propertyId)
    {
        if (propertyId == SOUND_PROP_GAIN) return dmSound::PARAMETER_GAIN;
        if (propertyId == SOUND_PROP_PAN) return dmSound::PARAMETER_PAN;
        if (propertyId == SOUND_PROP_SPEED) return dmSound::PARAMETER_SPEED;
        return dmSound::PARAMETER_MAX;
    }

    dmGameObject::PropertyResult CompSoundGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        World* world = (World*) params.m_World;
        Sound* sound = (Sound*) *params.m_UserData;

        dmSound::Parameter parameter = GetSoundParameterType(params.m_PropertyId);
        if (parameter == dmSound::PARAMETER_MAX) {
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }

        return SoundGetParameter(world, params.m_Instance, sound, parameter, out_value);
    }

    dmGameObject::PropertyResult CompSoundSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        World* world = (World*) params.m_World;
        Sound* sound = (Sound*) *params.m_UserData;

        if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
            return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

        dmSound::Parameter parameter = GetSoundParameterType(params.m_PropertyId);
        if (parameter == dmSound::PARAMETER_MAX) {
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }

        return SoundSetParameter(world, params.m_Instance, sound, parameter, params.m_Value.m_Number);
    }
}
