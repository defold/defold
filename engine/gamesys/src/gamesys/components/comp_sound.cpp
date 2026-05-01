// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "comp_sound.h"

#include <string.h>

#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/object_pool.h>
#include <dlib/profile.h>
#include <sound/sound.h>

#include <gamesys/gamesys_ddf.h>
#include "../gamesys.h"
#include "../gamesys_private.h"
#include "../resources/res_sound.h"
#include "../resources/res_sound_data.h"
#include "comp_private.h"

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_Sound, 0, PROFILE_PROPERTY_FRAME_RESET, "# components", &rmtp_Components);
DM_PROPERTY_U32(rmtp_SoundPlaying, 0, PROFILE_PROPERTY_FRAME_RESET, "# sounds playing", &rmtp_Sound);

namespace dmGameSystem
{
    static const dmhash_t SOUND_EXT_HASHES[] = { dmHashString64("wavc"), dmHashString64("oggc"), dmHashString64("opusc") };

    struct PlayEntry
    {
        Sound*                  m_Sound;
        dmSound::HSoundInstance m_SoundInstance;
        dmMessage::URL          m_Listener;
        dmMessage::URL          m_Receiver;
        dmGameObject::HInstance m_Instance;
        uintptr_t               m_LuaCallback;
        float                   m_Delay;
        uint32_t                m_PlayId;

        uint8_t                 m_StopRequested         : 1;
        uint8_t                 m_PauseRequested        : 1;
        uint8_t                 m_Paused                : 1;
        uint8_t                 m_ShouldDispatchEvents  : 1;
        uint8_t                                         : 4;
    };

    struct SoundComponent
    {
        Sound*              m_Resource;
        SoundDataResource*  m_SoundData; // Override
        float   m_Pan;
        float   m_Gain;
        float   m_Speed;
    };

    struct SoundWorld
    {
        dmArray<PlayEntry>              m_Entries;
        dmObjectPool<SoundComponent>    m_Components;
        dmIndexPool32                   m_EntryIndices;
    };

    struct SoundContext
    {
        dmResource::HFactory    m_Factory;
        uint32_t                m_MaxComponentCount;
        uint32_t                m_MaxSoundInstances;
    };


    static const dmhash_t SOUND_PROP_GAIN   = dmHashString64("gain");
    static const dmhash_t SOUND_PROP_PAN    = dmHashString64("pan");
    static const dmhash_t SOUND_PROP_SPEED  = dmHashString64("speed");
    static const dmhash_t SOUND_PROP_SOUND  = dmHashString64("sound");

    static dmGameObject::CreateResult CompSoundNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        SoundContext* sound_context = (SoundContext*)params.m_Context;

        SoundWorld* world = new SoundWorld();
        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, sound_context->m_MaxComponentCount);
        const uint32_t max_instances = sound_context->m_MaxSoundInstances;
        world->m_Entries.SetCapacity(max_instances);
        world->m_Entries.SetSize(max_instances);
        world->m_EntryIndices.SetCapacity(max_instances);
        memset(world->m_Entries.Begin(), 0, max_instances * sizeof(PlayEntry));

        world->m_Components.SetCapacity(comp_count);

        *params.m_World = (void*)world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::CreateResult CompSoundDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        SoundWorld* world = (SoundWorld*)params.m_World;
        uint32_t size = world->m_Entries.Size();

        for (uint32_t i = 0; i < size; ++i)
        {
            PlayEntry& entry = world->m_Entries[i];
            if (entry.m_SoundInstance != 0)
            {
                dmSound::Stop(entry.m_SoundInstance);
                dmSound::DeleteSoundInstance(entry.m_SoundInstance);
            }
        }

        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::CreateResult CompSoundCreate(const dmGameObject::ComponentCreateParams& params)
    {
        SoundWorld* world = (SoundWorld*)params.m_World;
        if (world->m_Components.Full())
        {
            ShowFullBufferError("Sound", "sound.max_component_count", world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        uint32_t index = world->m_Components.Alloc();
        SoundComponent* component = &world->m_Components.Get(index);
        component->m_Resource   = (Sound*)params.m_Resource;
        component->m_SoundData  = 0;
        component->m_Gain       = component->m_Resource->m_Gain;
        component->m_Pan        = component->m_Resource->m_Pan;
        component->m_Speed      = component->m_Resource->m_Speed;

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void* CompSoundGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        SoundWorld* world = (SoundWorld*)params.m_World;
        return &world->m_Components.Get(params.m_UserData);
    }

    static dmGameObject::CreateResult CompSoundDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        SoundContext* context = (SoundContext*)params.m_Context;
        SoundWorld* world = (SoundWorld*)params.m_World;
        uint32_t index = *params.m_UserData;

        SoundComponent* component = &world->m_Components.Get(index);
        if (component->m_SoundData)
            dmResource::Release(context->m_Factory, component->m_SoundData);

        world->m_Components.Free(index, false);

        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::CreateResult CompSoundAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        // Intentional pass-through
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void DispatchSoundEvent(PlayEntry& entry, dmhash_t message_id)
    {
        dmGameSystemDDF::SoundEvent message;
        dmMessage::URL receiver = entry.m_Listener;
        dmMessage::URL sender   = entry.m_Receiver;

        if (dmMessage::IsSocketValid(sender.m_Socket) && dmMessage::IsSocketValid(receiver.m_Socket))
        {
            uintptr_t descriptor = (uintptr_t)dmGameSystemDDF::SoundEvent::m_DDFDescriptor;
            uint32_t data_size = sizeof(dmGameSystemDDF::SoundEvent);

            message.m_PlayId = entry.m_PlayId;

            if (dmMessage::Post(&sender, &receiver, message_id, 0, (uintptr_t)entry.m_LuaCallback, descriptor, &message, data_size, 0) != dmMessage::RESULT_OK)
            {
                dmLogError("Could not send sound event (%s) to listener.", dmHashReverseSafe64(message_id));
            }
        }

        // Currently this is a one-time-use since we reset the urls,
        // if we want to reuse it for pause/resume events we need to move this out.
        dmMessage::ResetURL(&entry.m_Receiver);
        dmMessage::ResetURL(&entry.m_Listener);
    }

    static dmGameObject::UpdateResult HandleEntryFinishedPlaying(SoundWorld* world, PlayEntry& entry, uint32_t entry_index)
    {
        // For reverse hashing to work for easier debugging we hash these ids here
        // The hash container is enabled in engine init so it's too early in compilation unit scope
        static const dmhash_t SOUND_EVENT_DONE    = dmHashString64("sound_done");
        static const dmhash_t SOUND_EVENT_STOPPED = dmHashString64("sound_stopped");

        dmSound::Result r = dmSound::DeleteSoundInstance(entry.m_SoundInstance);
        entry.m_SoundInstance = 0;
        world->m_EntryIndices.Push(entry_index);
        if (r != dmSound::RESULT_OK)
        {
            dmLogError("Error deleting sound: (%d)", r);
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }
        else if (entry.m_Listener.m_Fragment != 0x0 && entry.m_ShouldDispatchEvents)
        {
            DispatchSoundEvent(entry, entry.m_StopRequested ? SOUND_EVENT_STOPPED : SOUND_EVENT_DONE);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static dmGameObject::UpdateResult CompSoundUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult&)
    {
        dmGameObject::UpdateResult update_result = dmGameObject::UPDATE_RESULT_OK;
        SoundWorld* world = (SoundWorld*)params.m_World;
        DM_PROPERTY_ADD_U32(rmtp_Sound, world->m_Components.Size());
        for (uint32_t i = 0; i < world->m_Entries.Size(); ++i)
        {
            PlayEntry& entry = world->m_Entries[i];
            if (!entry.m_SoundInstance)
                continue;

            DM_PROPERTY_ADD_U32(rmtp_SoundPlaying, 1);
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
                    update_result = HandleEntryFinishedPlaying(world, entry, i);
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
            else if (entry.m_StopRequested)
            {
                // If a stop was requested before we started playing, we can remove it immediately and dispatch the callback
                update_result = HandleEntryFinishedPlaying(world, entry, i);
            }
        }
        return update_result;
    }

    static dmGameObject::PropertyResult SoundSetParameter(SoundWorld* world, dmGameObject::HInstance instance, SoundComponent* component, dmSound::Parameter type, float value)
    {
        switch(type) {
        case dmSound::PARAMETER_GAIN:   component->m_Gain   = value; break;
        case dmSound::PARAMETER_PAN:    component->m_Pan    = value; break;
        case dmSound::PARAMETER_SPEED:  component->m_Speed  = value; break;
        default:
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }

        Sound* sound = component->m_Resource;
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

                dmSound::Result r = dmSound::SetParameter(entry.m_SoundInstance, type, dmVMath::Vector4(v, 0, 0, 0));
                if (r != dmSound::RESULT_OK)
                {
                    return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
                }
            }
        }
        return dmGameObject::PROPERTY_RESULT_OK;
    }

    static dmGameObject::PropertyResult SoundGetParameter(SoundWorld* world, dmGameObject::HInstance instance, SoundComponent* component, dmSound::Parameter type, dmGameObject::PropertyDesc& out_value)
    {
        float value;
        switch(type) {
        case dmSound::PARAMETER_GAIN:   value = component->m_Gain; break;
        case dmSound::PARAMETER_PAN:    value = component->m_Pan; break;
        case dmSound::PARAMETER_SPEED:  value = component->m_Speed; break;
        default:
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }

        out_value.m_Variant = dmGameObject::PropertyVar(value);
        return dmGameObject::PROPERTY_RESULT_OK;
    }

    static inline SoundDataResource* GetSoundDataResource(const SoundComponent* component)
    {
        return component->m_SoundData ? component->m_SoundData : component->m_Resource->m_SoundDataRes;
    }

    static dmGameObject::UpdateResult CompSoundOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        SoundWorld* world = (SoundWorld*)params.m_World;
        uint32_t index = *params.m_UserData;
        SoundComponent* component = &world->m_Components.Get(index);

        if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::PlaySound::m_DDFDescriptor)
        {
            if (world->m_EntryIndices.Remaining() > 0)
            {
                dmGameSystemDDF::PlaySound* play_sound = (dmGameSystemDDF::PlaySound*)params.m_Message->m_Data;
                Sound* sound = component->m_Resource;
                SoundDataResource* sound_data_resource = GetSoundDataResource(component);
                dmSound::HSoundData sound_data = ResSoundDataGetSoundData(sound_data_resource);

                uint32_t index = world->m_EntryIndices.Pop();
                PlayEntry& entry = world->m_Entries[index];
                entry.m_Sound = sound;
                entry.m_StopRequested = 0;
                entry.m_PauseRequested = 0;
                entry.m_Paused = 0;
                entry.m_Instance = params.m_Instance;
                entry.m_Receiver = params.m_Message->m_Receiver;
                entry.m_Delay = play_sound->m_Delay;
                entry.m_PlayId = play_sound->m_PlayId;
                entry.m_ShouldDispatchEvents = entry.m_PlayId != dmSound::INVALID_PLAY_ID;
                dmMessage::ResetURL(&entry.m_Listener);
                entry.m_LuaCallback = 0;
                dmSound::Result result = dmSound::NewSoundInstance(sound_data, &entry.m_SoundInstance);
                if (result == dmSound::RESULT_OK)
                {
                    result = dmSound::SetInstanceGroup(entry.m_SoundInstance, entry.m_Sound->m_GroupHash);
                    if (result != dmSound::RESULT_OK) {
                        dmLogError("Failed to set sound group (%d)", result);
                    }

                    float gain = play_sound->m_Gain * component->m_Gain;
                    float pan = play_sound->m_Pan + component->m_Pan;
                    float speed = play_sound->m_Speed * component->m_Speed;
                    dmSound::SetParameter(entry.m_SoundInstance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(gain, 0, 0, 0));
                    dmSound::SetParameter(entry.m_SoundInstance, dmSound::PARAMETER_PAN, dmVMath::Vector4(pan, 0, 0, 0));
                    dmSound::SetParameter(entry.m_SoundInstance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(speed, 0, 0, 0));
                    dmSound::SetLooping(entry.m_SoundInstance, sound->m_Looping, (sound->m_Looping && !sound->m_Loopcount) ? -1 : sound->m_Loopcount ); // loopcounter semantics differ a bit from loopcount. If -1, it means loopforever, otherwise it contains the # of loops remaining.

                    // Apply start offset before playback (initial-only; not re-applied on loops)
                    // If both are provided via message, start_frame wins
                    if (play_sound->m_StartFrame != 0)
                    {
                        dmSound::Result rskip = dmSound::SetStartFrame(entry.m_SoundInstance, play_sound->m_StartFrame);
                        if (rskip != dmSound::RESULT_OK)
                        {
                            dmLogWarning("Failed to set start_frame offset (%d)", rskip);
                        }
                    }
                    else if (play_sound->m_StartTime > 0.0f)
                    {
                        dmSound::Result rskip = dmSound::SetStartTime(entry.m_SoundInstance, play_sound->m_StartTime);
                        if (rskip != dmSound::RESULT_OK)
                        {
                            dmLogWarning("Failed to set start_time offset (%d)", rskip);
                        }
                    }

                    entry.m_Listener = params.m_Message->m_Sender;
                    uintptr_t callback = params.m_Message->m_UserData2;
                    if (callback == UINTPTR_MAX)
                    {
                        // UINTPTR_MAX used as default for `sound.play()` in `script_sound.cpp`
                        // to make distinguish between function call and `play_sound` message
                        // if it's a function call without callback (UINTPTR_MAX) then event
                        // shouldn't be dispatched
                        callback = callback == UINTPTR_MAX ? 0x0 : callback;
                        entry.m_ShouldDispatchEvents = 0;
                    }
                    entry.m_LuaCallback = callback;
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
                LogMessageError(params.m_Message, "A sound could not be played since all sounds instances are used (%d). Increase the project setting 'sound.max_sound_instances'", world->m_EntryIndices.Capacity());
            }
        }
        else if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::StopSound::m_DDFDescriptor)
        {
            dmGameSystemDDF::StopSound* stop_sound = (dmGameSystemDDF::StopSound*)params.m_Message->m_Data;
            uint32_t play_id = stop_sound->m_PlayId;
            for (uint32_t i = 0; i < world->m_Entries.Size(); ++i)
            {
                PlayEntry& entry = world->m_Entries[i];
                if (entry.m_SoundInstance != 0 && entry.m_Sound == component->m_Resource && entry.m_Instance == params.m_Instance)
                {
                    if (play_id == dmSound::INVALID_PLAY_ID)
                    {
                        entry.m_StopRequested = 1;
                    }
                    else if (entry.m_PlayId == play_id)
                    {
                        entry.m_StopRequested = 1;
                        break; 
                    }
                }
            }
        }
        else if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::PauseSound::m_DDFDescriptor)
        {
            SoundWorld* world = (SoundWorld*)params.m_World;
            dmGameSystemDDF::PauseSound* pause_sound = (dmGameSystemDDF::PauseSound*)params.m_Message->m_Data;
            uint32_t pause = (uint32_t)pause_sound->m_Pause;
            for (uint32_t i = 0; i < world->m_Entries.Size(); ++i)
            {
                PlayEntry& entry = world->m_Entries[i];
                if (entry.m_SoundInstance != 0 && entry.m_Sound == component->m_Resource && entry.m_Instance == params.m_Instance)
                {
                    entry.m_PauseRequested = 1;
                    entry.m_Paused = pause;
                }
            }
        }
        else if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::SetGain::m_DDFDescriptor)
        {
            dmGameSystemDDF::SetGain* set_gain = (dmGameSystemDDF::SetGain*)params.m_Message->m_Data;

            if (dmGameObject::PROPERTY_RESULT_OK != SoundSetParameter(world, params.m_Instance, component, dmSound::PARAMETER_GAIN, set_gain->m_Gain))
            {
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
            return dmGameObject::UPDATE_RESULT_OK;
        }
        else if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::SetPan::m_DDFDescriptor)
        {
            dmGameSystemDDF::SetPan* set_pan = (dmGameSystemDDF::SetPan*)params.m_Message->m_Data;

            if (dmGameObject::PROPERTY_RESULT_OK != SoundSetParameter(world, params.m_Instance, component, dmSound::PARAMETER_PAN, set_pan->m_Pan))
            {
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
            return dmGameObject::UPDATE_RESULT_OK;
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static inline dmSound::Parameter GetSoundParameterType(dmhash_t propertyId)
    {
        if (propertyId == SOUND_PROP_GAIN) return dmSound::PARAMETER_GAIN;
        if (propertyId == SOUND_PROP_PAN) return dmSound::PARAMETER_PAN;
        if (propertyId == SOUND_PROP_SPEED) return dmSound::PARAMETER_SPEED;
        return dmSound::PARAMETER_MAX;
    }

    static dmGameObject::PropertyResult CompSoundGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        SoundWorld* world = (SoundWorld*) params.m_World;
        uint32_t index = *params.m_UserData;
        SoundComponent* component = &world->m_Components.Get(index);

        if (params.m_PropertyId == SOUND_PROP_SOUND) {
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetSoundDataResource(component), out_value);
        } else {
            dmSound::Parameter parameter = GetSoundParameterType(params.m_PropertyId);
            if (parameter == dmSound::PARAMETER_MAX) {
                return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
            }
            return SoundGetParameter(world, params.m_Instance, component, parameter, out_value);
        }
    }

    static dmGameObject::PropertyResult CompSoundSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        SoundWorld* world = (SoundWorld*) params.m_World;
        uint32_t index = *params.m_UserData;
        SoundComponent* component = &world->m_Components.Get(index);

        if (params.m_PropertyId == SOUND_PROP_SOUND)
        {
            return SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, (dmhash_t*)SOUND_EXT_HASHES, DM_ARRAY_SIZE(SOUND_EXT_HASHES), (void**)&component->m_SoundData);
        }

        dmSound::Parameter parameter = GetSoundParameterType(params.m_PropertyId);
        if (parameter == dmSound::PARAMETER_MAX) {
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }

        if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
            return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

        return SoundSetParameter(world, params.m_Instance, component, parameter, params.m_Value.m_Number);
    }

    static dmGameObject::Result CompSoundcInit(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        SoundContext* context = new SoundContext;
        context->m_Factory = ctx->m_Factory;
        context->m_MaxComponentCount  = dmConfigFile::GetInt(ctx->m_Config, "sound.max_component_count", 32);
        context->m_MaxSoundInstances  = dmConfigFile::GetInt(ctx->m_Config, "sound.max_sound_instances", 256);

        int32_t stream_chunk_size = dmConfigFile::GetInt(ctx->m_Config, "sound.stream_chunk_size", 16384);
        ResSoundDataSetStreamingChunkSize((uint32_t)stream_chunk_size);

        int32_t sound_streaming_cache_size = dmConfigFile::GetInt(ctx->m_Config, "sound.stream_cache_size", 2 * 1024*1024);
        ResSoundDataSetStreamingCacheSize((uint32_t)sound_streaming_cache_size);

        uint32_t cache_size = dmConfigFile::GetInt(ctx->m_Config, "sound.max_sound_instances", 256);

        ComponentTypeSetPrio(type, 600);
        ComponentTypeSetContext(type, context);
        ComponentTypeSetHasUserData(type, true);
        ComponentTypeSetReadsTransforms(type, true);

        ComponentTypeSetNewWorldFn(type, CompSoundNewWorld);
        ComponentTypeSetDeleteWorldFn(type, CompSoundDeleteWorld);
        ComponentTypeSetCreateFn(type, CompSoundCreate);
        ComponentTypeSetDestroyFn(type, CompSoundDestroy);
        ComponentTypeSetAddToUpdateFn(type, CompSoundAddToUpdate);
        ComponentTypeSetUpdateFn(type, CompSoundUpdate);
        ComponentTypeSetOnMessageFn(type, CompSoundOnMessage);
        ComponentTypeSetGetPropertyFn(type, CompSoundGetProperty);
        ComponentTypeSetSetPropertyFn(type, CompSoundSetProperty);
        ComponentTypeSetGetFn(type, CompSoundGetComponent);

        return dmGameObject::RESULT_OK;
    }

    dmGameObject::Result CompSoundcExit(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::HComponentType type)
    {
        SoundContext* context = (SoundContext*)ComponentTypeGetContext(type);
        delete context;
        return dmGameObject::RESULT_OK;
    }
}

DM_DECLARE_COMPONENT_TYPE(ComponentTypeSound, "soundc", dmGameSystem::CompSoundcInit, dmGameSystem::CompSoundcExit);
