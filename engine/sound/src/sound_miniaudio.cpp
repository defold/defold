// Copyright 2020-2024 The Defold Foundation
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

#include "sound.h"

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/atomic.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/dlib/time.h>
#include <dmsdk/dlib/thread.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/profile.h>

#include <dlib/index_pool.h>

#include "miniaudio/miniaudio.h"
#include "sound_private.h"

namespace dmSound
{
    const dmhash_t MASTER_GROUP_HASH = dmHashString64("master");
    const uint16_t INVALID_INDEX = 0xFFFF;

    static Result UpdateInternal(struct SoundSystem* sound);
    static void SoundThread(void* ctx);

    static ma_result CheckAndPrintResult(ma_result result)
    {
        if (result != MA_SUCCESS)
        {
            dmLogError("Miniaudio %d: %s", result, ma_result_description(result));
        }
        return result;
    }

    struct SoundData
    {
        dmhash_t      m_NameHash;   // the path hash
        void*         m_Data;
        int           m_Size;
        // Index in m_SoundData
        uint16_t      m_Index;      // Index into sound->m_SoundData
        SoundDataType m_Type;
        uint16_t      m_RefCount;
    };

    struct SoundInstance
    {
        ma_data_converter   m_Converter;
        //ma_sound    m_Sound;
        //ma_decoder  m_Decoder;
        // dmSoundCodec::HDecoder m_Decoder;
        // void*       m_Frames;
        dmhash_t            m_Group;
        int64_t             m_FramePos;

        // Value       m_Gain;     // default: 1.0f
        // Value       m_Pan;      // 0 = -45deg left, 1 = 45 deg right
        // float       m_Speed;    // 1.0 = normal speed, 0.5 = half speed, 2.0 = double speed
        // uint32_t    m_FrameCount;
        // uint64_t    m_FrameFraction;

        float       m_Gain;        // default: 1.0f
        float       m_Pan;         // 0 = -45deg left, 1 = 45 deg right
        float       m_Speed;       // 1.0 = normal speed, 0.5 = half speed, 2.0 = double speed

        uint16_t    m_Index;            // Index into sound->m_Instances
        uint16_t    m_SoundDataIndex;   // Index into sound->m_SoundData
        uint8_t     m_Looping : 1;
        //uint8_t     m_EndOfStream : 1;
        uint8_t     m_Playing : 1;
        uint8_t     : 5;
        int8_t      m_Loopcounter; // if set to 3, there will be 3 loops effectively playing the sound 4 times.
    };

    struct SoundGroup
    {
        dmhash_t m_NameHash;
        //Value    m_Gain;
        // float*   m_MixBuffer;
        // float    m_SumSquaredMemory[SOUND_MAX_MIX_CHANNELS * GROUP_MEMORY_BUFFER_COUNT];
        // float    m_PeakMemorySq[SOUND_MAX_MIX_CHANNELS * GROUP_MEMORY_BUFFER_COUNT];
        // int      m_NextMemorySlot;
    };


    struct SoundSystem
    {
        // ma_engine               m_Engine;
        ma_context                  m_Context;
        ma_device_config            m_DeviceConfig;
        ma_device                   m_Device;
        //ma_resource_manager         m_ResourceManager;

        dmThread::Thread              m_Thread;
        dmMutex::HMutex               m_Mutex;

        dmArray<SoundInstance>  m_Instances;
        dmIndexPool16           m_InstancesPool;

        dmArray<SoundData>      m_SoundData;
        dmIndexPool16           m_SoundDataPool;

        dmHashTable<dmhash_t, int> m_GroupMap;
        SoundGroup              m_Groups[MAX_GROUPS];

        int32_atomic_t          m_IsRunning;
        int32_atomic_t          m_IsPaused;
        int32_atomic_t          m_Status; // type Result

        uint32_t                m_MixRate;
        // uint32_t                m_FrameCount;
        uint32_t                m_PlayCounter; // For generating play id's from scripting

        // int16_t*                m_OutBuffers[SOUND_OUTBUFFER_COUNT];
        // uint16_t                m_NextOutBuffer;

        // config settings
        float                   m_MasterGain;
        uint32_t                m_SleepInterval;

        // internal state
        bool                    m_IsDeviceStarted;
        bool                    m_IsAudioInterrupted;
        bool                    m_HasWindowFocus;
    };

    SoundSystem* g_SoundSystem = 0;


    static int GetOrCreateSoundGroup(SoundSystem* sound, const char* group_name)
    {
        dmhash_t group_hash = dmHashString64(group_name);

        if (sound->m_GroupMap.Full()) {
            return -1;
        }

        if (sound->m_GroupMap.Get(group_hash)) {
            return *sound->m_GroupMap.Get(group_hash);
        }

        uint32_t index = sound->m_GroupMap.Size();
        SoundGroup* group = &sound->m_Groups[index];
        group->m_NameHash = group_hash;
        group->m_Gain = 1.0f;
        //group->m_Gain.Reset(1.0f);
        // size_t mix_buffer_size = sound->m_FrameCount * sizeof(float) * SOUND_MAX_MIX_CHANNELS;
        // group->m_MixBuffer = (float*) malloc(mix_buffer_size);
        // memset(group->m_MixBuffer, 0, mix_buffer_size);
        sound->m_GroupMap.Put(group_hash, index);
        return index;
    }

    static SoundGroup* GetSoundGroup(SoundSystem* sound, dmhash_t group)
    {
        int* index = sound->m_GroupMap.Get(group);
        if (!index)
            return 0;
        return &sound->m_Groups[*index];
    }

    static void ResetSoundInstance(HSoundInstance sound_instance)
    {
        memset(sound_instance, 0, sizeof(SoundInstance));
        sound_instance->m_Index = INVALID_INDEX;
        sound_instance->m_SoundDataIndex = INVALID_INDEX;
        sound_instance->m_Gain  = 1.0f;
        sound_instance->m_Speed = 1.0f;
    }

    static bool IsMuted(SoundInstance* instance) {
        SoundSystem* sound = g_SoundSystem;

        if (instance->m_Gain == 0.0f || instance->m_Speed == 0.0f)
        {
            return true;
        }

        int* group_index = sound->m_GroupMap.Get(instance->m_Group);
        if (group_index != NULL) {
            SoundGroup* group = &sound->m_Groups[*group_index];
            if (group->m_Gain == 0.0f ) {
                return true;
            }
        }

        int* master_index = sound->m_GroupMap.Get(MASTER_GROUP_HASH);
        if (master_index != NULL) {
            SoundGroup* master = &sound->m_Groups[*master_index];
            if (master->m_Gain == 0.0f ) {
                return true;
            }
        }

        return false;
    }

    static uint32_t ReadAudioInternal(SoundInstance* instance, uint8_t* frames_out, uint32_t num_frames)
    {
    }

    static uint32_t ReadAudio(SoundInstance* instance, float* frames_out, uint32_t num_frames)
    {
        uint8_t  input_buffer[4096];
        uint32_t input_buffer_frames = sizeof(input_buffer) / ma_get_bytes_per_frame(instance->m_Converter.formatIn, instance->m_Converter.channelsIn);
        uint32_t num_channels_out = instance->m_Converter.channelsOut;

        uint32_t num_frames_processed = 0;
        while (num_frames_processed < num_frames)
        {
            uint64_t num_output_frames = num_frames - num_frames_processed;
            uint64_t num_input_frames = 0;

            ma_data_converter_get_required_input_frame_count(&instance->m_Converter, num_output_frames, &num_input_frames);
            if (num_input_frames > input_buffer_frames)
                num_input_frames = input_buffer_frames;

            float* frame_write_ptr = frames_out + num_frames_processed * num_channels_out;

            uint64_t num_input_frames_processed = ReadAudioInternal(instance, input_buffer, num_input_frames);
            uint64_t num_output_frames_processed = num_input_frames_processed;
            ma_data_converter_process_pcm_frames(&instance->m_Converter, input_buffer, &inputFramesProcessedThisIteration, frame_write_ptr, &num_output_frames_processed);

            num_frames_processed += num_output_frames_processed;

            if (num_input_frames_processed < num_input_frames)
            {
                break;
            }

            if (num_input_frames_processed == 0 && num_output_frames_processed == 0)
                break;
        }

        return num_frames_processed;
    }

    static void MixInstance(SoundSystem* sound, ma_device* device, SoundInstance* instance, void* frames_out, uint32_t num_frames)
    {
        uint32_t decoded = 0;

        // dmSoundCodec::Info info;
        // dmSoundCodec::GetInfo(sound->m_CodecContext, instance->m_Decoder, &info);
        // bool correct_bit_depth = info.m_BitsPerSample == 16 || info.m_BitsPerSample == 8;
        // bool correct_num_channels = info.m_Channels == 1 || info.m_Channels == 2;
        // if (!correct_bit_depth || !correct_num_channels) {
        //     dmLogError("Only mono/stereo with 8/16 bits per sample is supported (%s): %u bpp %u ch", GetSoundName(sound, instance), (uint32_t)info.m_BitsPerSample, (uint32_t)info.m_Channels);
        //     instance->m_Playing = 0;
        //     return;
        // }

        // if (info.m_Rate > sound->m_MixRate) {
        //     dmLogError("Sounds with rate higher than sample-rate not supported (%d hz > %d hz) (%s)", info.m_Rate, sound->m_MixRate, GetSoundName(sound, instance));
        //     instance->m_Playing = 0;
        //     return;
        // }

        bool is_muted = dmSound::IsMuted(instance);
        if (is_muted)
        {
            return; // frames are already cleared
        }

        const uint32_t num_channels = 2; // number of channels of the device
        uint32_t frames_read = 0;
        while (frames_read < num_frames)
        {
            uint32_t frames_to_read = num_frames - frames_read;

            // Read them a little by little
            while (frames_to_read > 0)
            {
                float temp_buffer[1024] = { 0 };
                uint32_t temp_num_frames = (sizeof(temp_buffer)/sizeof(temp_buffer[0])) / num_channels;

                uint32_t num_read = ReadAudio(instance, temp_buffer, temp_num_frames);
                if (num_read)
                {
                    // Mix audio

                    frames_read += num_read;
                    frames_to_read -= num_read;
                }

                if (num_read < temp_num_frames)
                {
                    // We came to end of sound
                    if (instance->m_Looping && instance->m_Loopcounter != 0)
                    {
                    }
                    else
                    {
                        StopNoLock(sound, instance);
                        break;
                    }
                }
            }
        }

        // dmSoundCodec::Result r = dmSoundCodec::RESULT_OK;
        // uint32_t mixed_instance_FrameCount = ceilf(sound->m_FrameCount * dmMath::Max(1.0f, instance->m_Speed));

        // if (instance->m_FrameCount < mixed_instance_FrameCount && instance->m_Playing) {

        //     const uint32_t stride = info.m_Channels * (info.m_BitsPerSample / 8);
        //     uint32_t n = mixed_instance_FrameCount - instance->m_FrameCount; // if the result contains a fractional part and we don't ceil(), we'll end up with a smaller number. Later, when deciding the mix_count in Mix(), a smaller value (integer) will be produced. This will result in leaving a small gap in the mix buffer resulting in sound crackling when the chunk changes.

        //     if (!is_muted)
        //     {
        //         r = dmSoundCodec::Decode(sound->m_CodecContext,
        //                                  instance->m_Decoder,
        //                                  ((char*) instance->m_Frames) + instance->m_FrameCount * stride,
        //                                  n * stride,
        //                                  &decoded);
        //     }
        //     else
        //     {
        //         r = dmSoundCodec::Skip(sound->m_CodecContext, instance->m_Decoder, n * stride, &decoded);
        //         memset(((char*) instance->m_Frames) + instance->m_FrameCount * stride, 0x00, n * stride);
        //     }

        //     assert(decoded % stride == 0);
        //     instance->m_FrameCount += decoded / stride;

        //     if (instance->m_FrameCount < mixed_instance_FrameCount) {

        //         if (instance->m_Looping && instance->m_Loopcounter != 0) {
        //             dmSoundCodec::Reset(sound->m_CodecContext, instance->m_Decoder);
        //             if ( instance->m_Loopcounter > 0 ) {
        //                 instance->m_Loopcounter --;
        //             }

        //             uint32_t n = mixed_instance_FrameCount - instance->m_FrameCount;
        //             if (!is_muted)
        //             {
        //                 r = dmSoundCodec::Decode(sound->m_CodecContext,
        //                                          instance->m_Decoder,
        //                                          ((char*) instance->m_Frames) + instance->m_FrameCount * stride,
        //                                          n * stride,
        //                                          &decoded);
        //             }
        //             else
        //             {
        //                 r = dmSoundCodec::Skip(sound->m_CodecContext, instance->m_Decoder, n * stride, &decoded);
        //                 memset(((char*) instance->m_Frames) + instance->m_FrameCount * stride, 0x00, n * stride);
        //             }

        //             assert(decoded % stride == 0);
        //             instance->m_FrameCount += decoded / stride;

        //         } else {

        //             if  (instance->m_FrameCount < instance->m_Speed) {
        //                 // since this is the last mix and no more frames will be added, trailing frames will linger on forever
        //                 // if they are less than m_Speed. We will truncate them to avoid this.
        //                 instance->m_FrameCount = 0;
        //             }
        //             instance->m_EndOfStream = 1;
        //         }
        //     }
        // }

        // if (r != dmSoundCodec::RESULT_OK) {
        //     dmLogWarning("Unable to decode file '%s'. Result %d", GetSoundName(sound, instance), r);
        //     instance->m_Playing = 0;
        //     return;
        // }

        if (instance->m_FrameCount > 0)
            Mix(mix_context, instance, &info);

        if (instance->m_FrameCount <= 1 && instance->m_EndOfStream) {
            // NOTE: Due to round-off errors, e.g 32000 -> 44100,
            // the last frame might be partially sampled and
            // used in the *next* buffer. We truncate such scenarios to 0
            instance->m_FrameCount = 0;
        }
    }

    static void MixInstances(SoundSystem* sound, ma_device* device, void* frames_out, uint32_t num_frames)
    {
        DM_PROFILE(__FUNCTION__);

        // for (uint32_t i = 0; i < MAX_GROUPS; i++) {
        //     SoundGroup* g = &sound->m_Groups[i];

        //     if (g->m_MixBuffer) {
        //         uint32_t frame_count = sound->m_FrameCount;
        //         float sum_sq_left = 0;
        //         float sum_sq_right = 0;
        //         float max_sq_left = 0;
        //         float max_sq_right = 0;
        //         for (uint32_t j = 0; j < frame_count; j++) {

        //             float gain = g->m_Gain.m_Current;

        //             float left = g->m_MixBuffer[2 * j + 0] * gain;
        //             float right = g->m_MixBuffer[2 * j + 1] * gain;
        //             float left_sq = left * left;
        //             float right_sq = right * right;
        //             sum_sq_left += left_sq;
        //             sum_sq_right += right_sq;
        //             max_sq_left = dmMath::Max(max_sq_left, left_sq);
        //             max_sq_right = dmMath::Max(max_sq_right, right_sq);
        //         }
        //         g->m_SumSquaredMemory[2 * g->m_NextMemorySlot + 0] = sum_sq_left;
        //         g->m_SumSquaredMemory[2 * g->m_NextMemorySlot + 1] = sum_sq_right;
        //         g->m_PeakMemorySq[2 * g->m_NextMemorySlot + 0] = max_sq_left;
        //         g->m_PeakMemorySq[2 * g->m_NextMemorySlot + 1] = max_sq_right;
        //         g->m_NextMemorySlot = (g->m_NextMemorySlot + 1) % GROUP_MEMORY_BUFFER_COUNT;

        //         memset(g->m_MixBuffer, 0, sound->m_FrameCount * sizeof(float) * 2);
        //     }
        // }

        uint32_t instances = sound->m_Instances.Size();
        for (uint32_t i = 0; i < instances; ++i) {
            SoundInstance* instance = &sound->m_Instances[i];
            if (instance->m_Playing || instance->m_FrameCount > 0)
            {
                MixInstance(sound, device, instance, frames_out, num_frames);
            }

            if (instance->m_EndOfStream && instance->m_FrameCount == 0) {
                instance->m_Playing = 0;
            }
        }
    }

    static void MiniaudioCallback(ma_device* device, void* frames_out, const void* frames_in, ma_uint32 num_frames)
    {
        // In playback mode copy data to pOutput. In capture mode read data from pInput. In full-duplex mode, both
        // pOutput and pInput will be valid and you can move data from pInput into pOutput. Never process more than
        // frameCount frames.

        // This is called once per frame

        memset(frames_out, 0, num_frames * device->playback.channels * ma_get_bytes_per_sample(device->playback.format));

        SoundSystem* sound = (SoundSystem*)device->pUserData;
        SoundGroup* master = GetSoundGroup(sound, MASTER_GROUP_HASH);
        if (master->m_Gain == 0.0f)
            return;

        MixInstances(sound, device, frames_out, num_frames);
    }

    static void MiniaudioLog(void *pUserData, ma_uint32 level, const char* msg)
    {
        switch(level) {
        case MA_LOG_LEVEL_DEBUG:    dmLogDebug("Miniaudio: %s", msg); break;
        case MA_LOG_LEVEL_INFO:     dmLogInfo("Miniaudio: %s", msg); break;
        case MA_LOG_LEVEL_WARNING:  dmLogWarning("Miniaudio: %s", msg); break;
        case MA_LOG_LEVEL_ERROR:
        default:                    dmLogError("Miniaudio: %s", msg); break;
        }
    }

    void SetDefaultInitializeParams(InitializeParams* params)
    {
        memset(params, 0, sizeof(InitializeParams));
        params->m_OutputDevice = "default";
        params->m_MasterGain = 1.0f;
        params->m_MaxSoundData = 128;
        params->m_MaxSources = 16;
        params->m_MaxBuffers = 32;
        params->m_BufferSize = 12 * 4096;
        params->m_FrameCount = 768;
        params->m_MaxInstances = 256;
        params->m_UseThread = true;
    }

    Result Initialize(dmConfigFile::HConfig config, const InitializeParams* params)
    {
        g_SoundSystem = new SoundSystem();
        SoundSystem* sound = g_SoundSystem;
        sound->m_IsDeviceStarted = false;
        sound->m_IsAudioInterrupted = false;
        sound->m_HasWindowFocus = true; // Assume we startup with the window focused
        //sound->m_DeviceType = device_type;
        //sound->m_Device = device;


        //float master_gain = params->m_MasterGain;
        uint32_t max_sound_data = params->m_MaxSoundData;
        // uint32_t max_buffers = params->m_MaxBuffers;
        // uint32_t max_sources = params->m_MaxSources;
        uint32_t max_instances = params->m_MaxInstances;
        uint32_t sleep_interval = 8000;// params->m_MaxInstances;

        if (config)
        {
            //master_gain = dmConfigFile::GetFloat(config, "sound.gain", 1.0f);
            max_sound_data = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_data", (int32_t) max_sound_data);
            // max_buffers = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_buffers", (int32_t) max_buffers);
            // max_sources = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_sources", (int32_t) max_sources);
            max_instances = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_instances", (int32_t) max_instances);
            sleep_interval = (uint32_t) dmConfigFile::GetInt(config, "sound.thread_sleep_interval", (int32_t) sleep_interval);
        }

        sound->m_SleepInterval = sleep_interval;

        // miniaudio settings
        // sound->m_UsePitch = (uint32_t) dmConfigFile::GetInt(config, "miniaudio.use_pitch", 1); // MA_SOUND_FLAG_NO_PITCH
        // sound->m_UseSpatialization = (uint32_t) dmConfigFile::GetInt(config, "miniaudio.use_spatialization", 1); // MA_SOUND_FLAG_NO_SPATIALIZATION


        sound->m_Instances.SetCapacity(max_instances);
        sound->m_Instances.SetSize(max_instances);
        sound->m_InstancesPool.SetCapacity(max_instances);
        for (uint32_t i = 0; i < max_instances; ++i)
        {
            SoundInstance* instance = &sound->m_Instances[i];
            ResetSoundInstance(instance);
            // NOTE: +1 for "over-fetch" when up-sampling
            // NOTE: and x SOUND_MAX_SPEED for potential pitch range
            // instance->m_Frames = malloc((params->m_FrameCount * SOUND_MAX_SPEED + 1) * sizeof(int16_t) * SOUND_MAX_MIX_CHANNELS);
            // instance->m_FrameCount = 0;
            //instance->m_Speed = 1.0f;
        }

        sound->m_SoundData.SetCapacity(max_sound_data);
        sound->m_SoundData.SetSize(max_sound_data);
        sound->m_SoundDataPool.SetCapacity(max_sound_data);
        for (uint32_t i = 0; i < max_sound_data; ++i)
        {
            sound->m_SoundData[i].m_Index = INVALID_INDEX;
        }

        // sound->m_FrameCount = params->m_FrameCount;
        // for (int i = 0; i < SOUND_OUTBUFFER_COUNT; ++i) {
        //     sound->m_OutBuffers[i] = (int16_t*) malloc(params->m_FrameCount * sizeof(int16_t) * SOUND_MAX_MIX_CHANNELS);
        // }
        // sound->m_NextOutBuffer = 0;

        sound->m_GroupMap.SetCapacity(MAX_GROUPS * 2 + 1, MAX_GROUPS);
        for (uint32_t i = 0; i < MAX_GROUPS; ++i) {
            memset(&sound->m_Groups[i], 0, sizeof(SoundGroup));
        }

        GetOrCreateSoundGroup("master");

        dmAtomicStore32(&sound->m_IsRunning, 1);
        dmAtomicStore32(&sound->m_IsPaused, 0);
        dmAtomicStore32(&sound->m_Status, (int)RESULT_NOTHING_TO_PLAY);

        sound->m_Thread = 0;
        sound->m_Mutex = 0;
        if (params->m_UseThread)
        {
            sound->m_Mutex = dmMutex::New();
            sound->m_Thread = dmThread::New((dmThread::ThreadStart)SoundThread, 0x80000, sound, "sound");
        }

        //////////////////////////////////////////////////////////////////////////////////////////
        // Miniaudio initialization

        ma_log_callback_init(MiniaudioLog, NULL);

        ma_context_config context_config = ma_context_config_init();
        ma_result result = ma_context_init(NULL, 0, &context_config, &sound->m_Context);
        if (result != MA_SUCCESS)
        {
            CheckAndPrintResult(result);
            return dmSound::RESULT_UNKNOWN_ERROR;
        }

        sound->m_DeviceConfig = ma_device_config_init(ma_device_type_playback);
        sound->m_DeviceConfig.playback.format     = ma_format_f32;
        sound->m_DeviceConfig.playback.channels   = 2;
        sound->m_DeviceConfig.sampleRate          = 44100;
        sound->m_DeviceConfig.dataCallback        = MiniaudioCallback;
        sound->m_DeviceConfig.pUserData           = sound;

        result = ma_device_init(NULL, &sound->m_DeviceConfig, &sound->m_Device);
        if (MA_SUCCESS != result)
        {
            ma_context_uninit(&sound->m_Context);
            CheckAndPrintResult(result);
            return dmSound::RESULT_UNKNOWN_ERROR;
        }

        // ma_resource_manager_config resource_manager_config = ma_resource_manager_config_init();
        // resource_manager_config.decodedFormat     = sound->m_DeviceConfig.playback.format;
        // resource_manager_config.decodedChannels   = sound->m_DeviceConfig.playback.channels;
        // resource_manager_config.decodedSampleRate = sound->m_DeviceConfig.sampleRate;

        // resource_manager_config.jobThreadCount = 0;                            // We'll manage the threads ourselves
        // resource_manager_config.flags = MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING; // Optional. Makes ma_resource_manager_next_job() non-blocking.

        // result = ma_resource_manager_init(&resource_manager_config, &sound->m_ResourceManager);
        // if (result != MA_SUCCESS) {
        //     ma_device_uninit(&sound->m_Device);

        //     CheckAndPrintResult(result);
        //     return dmSound::RESULT_UNKNOWN_ERROR;
        // }

        result = ma_device_start(&sound->m_Device);
        if (result != MA_SUCCESS) {
            ma_device_uninit(&sound->m_Device);
            ma_context_uninit(&sound->m_Context);
            //ma_resource_manager_uninit(&sound->m_ResourceManager);

            CheckAndPrintResult(result);
            return dmSound::RESULT_UNKNOWN_ERROR;
        }

        sound->m_MixRate = sound->m_Device.playback.internalSampleRate;

        // ma_engine_config engine_config;
        // engine_config = ma_engine_config_init();
        // ma_result result = ma_engine_init(&engine_config, &sound->m_Engine);
        // if (result != MA_SUCCESS)
        // {
        //     CheckAndPrintResult(result);
        //     return dmSound::RESULT_UNKNOWN_ERROR;
        // }

        dmLogInfo("SOUND: Device initialized successfully");
        dmLogInfo("    > Backend:       miniaudio | %s", ma_get_backend_name(sound->m_Context.backend));
        dmLogInfo("    > Format:        %s -> %s", ma_get_format_name(sound->m_Device.playback.format), ma_get_format_name(sound->m_Device.playback.internalFormat));
        dmLogInfo("    > Channels:      %d -> %d", sound->m_Device.playback.channels, sound->m_Device.playback.internalChannels);
        dmLogInfo("    > Sample rate:   %d -> %d", sound->m_Device.sampleRate, sound->m_MixRate);
        //dmLogInfo("    > Periods size:  %d", AUDIO.System.device.playback.internalPeriodSizeInFrames*AUDIO.System.device.playback.internalPeriods);

        return RESULT_OK;
    }

    Result Finalize()
    {
        SoundSystem* sound = g_SoundSystem;
        if (!sound)
            return RESULT_OK;

        // stop all callbacks
        ma_device_uninit(&sound->m_Device);
        ma_context_uninit(&sound->m_Context);

        // wait for the thread to quit
        dmAtomicStore32(&sound->m_IsRunning, 0);
        if (sound->m_Thread)
        {
            dmThread::Join(sound->m_Thread);
            dmMutex::Delete(sound->m_Mutex);
        }

        PlatformFinalize();

        Result result = RESULT_OK;

        if (sound)
        {
            //dmSoundCodec::Delete(sound->m_CodecContext);

            for (uint32_t i = 0; i < sound->m_Instances.Size(); ++i)
            {
                SoundInstance* instance = &sound->m_Instances[i];
                ResetSoundInstance(instance);
            }

            // for (int i = 0; i < SOUND_OUTBUFFER_COUNT; ++i) {
            //     free((void*) sound->m_OutBuffers[i]);
            // }

            // for (uint32_t i = 0; i < MAX_GROUPS; i++) {
            //     SoundGroup* g = &sound->m_Groups[i];
            //     if (g->m_MixBuffer) {
            //         free((void*) g->m_MixBuffer);
            //     }
            // }

            // if (sound->m_Device)
            // {
            //     sound->m_DeviceType->m_Close(sound->m_Device);
            // }

            delete sound;
            g_SoundSystem = 0;
        }

        //ma_engine_uninit(&sound->m_Engine);

        //ma_resource_manager_uninit(&sound->m_ResourceManager);

        return result;
    }

    static inline const char* GetSoundName(SoundSystem* sound, SoundInstance* instance)
    {
        dmhash_t hash = sound->m_SoundData[instance->m_SoundDataIndex].m_NameHash;
        return dmHashReverseSafe64(hash);
    }

    static Result SetSoundDataNoLock(HSoundData sound_data, const void* sound_buffer, uint32_t sound_buffer_size)
    {
        free(sound_data->m_Data);
        sound_data->m_Data = malloc(sound_buffer_size);
        sound_data->m_Size = sound_buffer_size;
        memcpy(sound_data->m_Data, sound_buffer, sound_buffer_size);
        return RESULT_OK;
    }

    Result NewSoundData(const void* sound_buffer, uint32_t sound_buffer_size, SoundDataType type, HSoundData* sound_data, dmhash_t name)
    {
        SoundSystem* sound = g_SoundSystem;

        uint16_t index;
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
            if (sound->m_SoundDataPool.Remaining() == 0)
            {
                *sound_data = 0;
                dmLogError("Out of sound data slots (%u). Increase the project setting 'sound.max_sound_data'", sound->m_SoundDataPool.Capacity());
                return RESULT_OUT_OF_INSTANCES;
            }

            index = sound->m_SoundDataPool.Pop();
        }

        SoundData* sd = &sound->m_SoundData[index];
        sd->m_NameHash = name;
        sd->m_Type = type;
        sd->m_Index = index;
        sd->m_Data = 0;
        sd->m_Size = 0;
        sd->m_RefCount = 1;

        Result result = SetSoundDataNoLock(sd, sound_buffer, sound_buffer_size);
        if (result == RESULT_OK)
            *sound_data = sd;
        else
            DeleteSoundData(sd);

        return result;
    }

    uint32_t GetSoundResourceSize(HSoundData sound_data)
    {
        return sizeof(SoundData) + sound_data->m_Size;
    }

    Result SetSoundData(HSoundData sound_data, const void* sound_buffer, uint32_t sound_buffer_size)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        return SetSoundDataNoLock(sound_data, sound_buffer, sound_buffer_size);
    }

    Result DeleteSoundData(HSoundData sound_data)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);

        sound_data->m_RefCount --;
        if (sound_data->m_RefCount > 0)
        {
            return RESULT_OK;
        }

        if (sound_data->m_Data != 0x0)
            free((void*) sound_data->m_Data);

        SoundSystem* sound = g_SoundSystem;
        sound->m_SoundDataPool.Push(sound_data->m_Index);
        sound_data->m_Index = INVALID_INDEX;

        return RESULT_OK;
    }

    Result NewSoundInstance(HSoundData sound_data, HSoundInstance* sound_instance)
    {
        SoundSystem* ss = g_SoundSystem;

        uint16_t index;
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(ss->m_Mutex);

            if (ss->m_InstancesPool.Remaining() == 0)
            {
                *sound_instance = 0;
                dmLogError("Out of sound data instance slots (%u). Increase the project setting 'sound.max_sound_instances'", ss->m_InstancesPool.Capacity());
                return RESULT_OUT_OF_INSTANCES;
            }

            index = ss->m_InstancesPool.Pop();
        }

        SoundInstance* si = &ss->m_Instances[index];
        assert(si->m_Index == INVALID_INDEX);

        si->m_SoundDataIndex = sound_data->m_Index;
        si->m_Index = index;
        si->m_Group = MASTER_GROUP_HASH;

        ma_decoder decoder;
        ma_result result = ma_decoder_init_memory(sound_data->m_Data, sound_data->m_Size, 0, &decoder);
        if (result != MA_SUCCESS)
        {
            CheckAndPrintResult(result);
            return RESULT_UNKNOWN_SOUND_TYPE;
        }

        ma_format format;
        ma_uint32 channels;
        ma_uint32 sample_rate;
        result = ma_decoder_get_data_format(&decoder, &format, &channels, &sample_rate, 0, 0);
        if (result != MA_SUCCESS)
        {
            CheckAndPrintResult(result);
            return RESULT_UNKNOWN_SOUND_TYPE;
        }

        bool correct_num_channels = channels == 1 || channels == 2;
        if (!correct_num_channels) {
            dmLogError("Only mono/stereo is supported (%s): %u ch", GetSoundName(sound, sound_instance), channels);
            instance->m_Playing = 0;
            return;
        }

        if (sample_rate > sound->m_MixRate) {
            dmLogError("Sounds with rate higher than sample-rate not supported (%d hz > %d hz) (%s)", sample_rate, sound->m_MixRate, GetSoundName(sound, sound_instance));
            instance->m_Playing = 0;
            return;
        }

        ma_data_converter_config converter_config = ma_data_converter_config_init(format,       ma_format_f32,
                                                                                  channels,     2,
                                                                                  sample_rate,  ss->m_Device.sampleRate);
        ma_decoder_uninit(&decoder);

        result = ma_data_converter_init(&converter_config, 0, &si->m_Converter);
        if (result != MA_SUCCESS)
        {
            CheckAndPrintResult(result);
            return RESULT_UNSUPPORTED;
        }

        // int flags = 0;
        // if (!ss->m_UsePitch)
        //     flags = MA_SOUND_FLAG_NO_PITCH;
        // if (!ss->m_UseSpatialization)
        //     flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
        // ma_sound_group* master_group = 0;
        // result = ma_sound_init_from_data_source(&ss->m_Engine, &si->m_Decoder, flags, master_group, &si->m_Sound);
        // if (result != MA_SUCCESS)
        // {
        //     ma_decoder_uninit(&si->m_Decoder);
        //     ss->m_InstancesPool.Push(index);
        //     CheckAndPrintResult(result);
        //     return RESULT_INVALID_STREAM_DATA;
        // }

        sound_data->m_RefCount++;
        *sound_instance = si;

        // The sound instance should already be reset
        // si->m_Gain.Reset(1.0f);
        // si->m_Pan.Reset(0.5f);
        // si->m_Looping = 0;
        // si->m_EndOfStream = 0;
        // si->m_Playing = 0;
        //si->m_Decoder = decoder;


        return RESULT_OK;
    }

    static void StopNoLock(SoundSystem* sound, HSoundInstance sound_instance);

    Result DeleteSoundInstance(HSoundInstance sound_instance)
    {
        SoundSystem* sound = g_SoundSystem;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(sound->m_Mutex);

        if (IsPlaying(sound_instance))
        {
            dmLogError("Deleting playing sound instance (%s)", GetSoundName(sound, sound_instance));
            StopNoLock(sound, sound_instance);
        }

        //ma_decoder_uninit(&sound_instance->m_Decoder);
        ma_data_converter_uninit(&sound_instance->m_Converter, 0);
        DeleteSoundData(&sound->m_SoundData[sound_instance->m_SoundDataIndex]);

        uint16_t index = sound_instance->m_Index;
        sound->m_InstancesPool.Push(index);

        ResetSoundInstance(sound_instance);
        return RESULT_OK;
    }

    Result GetGroupRMS(dmhash_t group_hash, float window, float* rms_left, float* rms_right)
    {
        // NOTE: Not supported.
        // sound_null is deprecated and should be replaced by sound2 with null-device
        *rms_left = 0;
        *rms_right = 0;
        return RESULT_OK;
    }

    Result GetGroupPeak(dmhash_t group_hash, float window, float* peak_left, float* peak_right)
    {
        // NOTE: Not supported.
        // sound_null is deprecated and should be replaced by sound2 with null-device
        *peak_left = 0;
        *peak_right = 0;
        return RESULT_OK;
    }

    Result SetGroupGain(dmhash_t group_hash, float gain)
    {
        // NOTE: Not supported.
        // sound_null is deprecated and should be replaced by sound2 with null-device
        return RESULT_OK;
    }

    Result GetGroupGain(dmhash_t group_hash, float* gain)
    {
        // NOTE: Not supported.
        // sound_null is deprecated and should be replaced by sound2 with null-device
        return RESULT_OK;
    }

    Result GetGroupHashes(uint32_t* count, dmhash_t* buffer)
    {
        return RESULT_OK;
    }

    Result SetInstanceGroup(HSoundInstance instance, const char* group_name)
    {
        dmhash_t group_hash = dmHashString64(group_name);
        return SetInstanceGroup(instance, group_hash);
    }

    Result SetInstanceGroup(HSoundInstance instance, dmhash_t group_hash)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        SoundSystem* sound = g_SoundSystem;
        int* index = sound->m_GroupMap.Get(group_hash);
        if (!index) {
            return RESULT_NO_SUCH_GROUP;
        }
        instance->m_Group = group_hash;
        return RESULT_OK;
    }

    Result AddGroup(const char* group)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        int index = GetOrCreateSoundGroup(g_SoundSystem, group);
        if (index == -1) {
            return RESULT_OUT_OF_GROUPS;
        }
        return RESULT_OK;
    }

    Result Update()
    {
        DM_PROFILE("Sound");
        SoundSystem* sound = g_SoundSystem;
        if (!sound)
            return RESULT_OK;

        if (!sound->m_Thread)
            return UpdateInternal(sound);
        return (Result)dmAtomicGet32(&sound->m_Status);
    }

    Result Pause(bool pause)
    {
        SoundSystem* sound = g_SoundSystem;
        if (sound && sound->m_Thread)
        {
            dmAtomicStore32(&sound->m_IsPaused, (int)pause);
        }
        return RESULT_OK;
    }

    Result Play(HSoundInstance sound_instance)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        sound_instance->m_Playing = 1;
        return RESULT_OK;
    }

    static void StopNoLock(SoundSystem* sound, HSoundInstance sound_instance)
    {
        sound_instance->m_Playing = 0;
        sound_instance->m_FrameCount = 0;
    }

    Result Stop(HSoundInstance sound_instance)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        StopNoLock(g_SoundSystem, sound_instance);
        return RESULT_OK;
    }

    Result Pause(HSoundInstance sound_instance, bool pause)
    {
        if (!g_SoundSystem)
            return RESULT_OK;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        sound_instance->m_Playing = (uint8_t)!pause;
        return RESULT_OK;
    }

    uint32_t GetAndIncreasePlayCounter()
    {
        SoundSystem* sound = g_SoundSystem;
        if (sound->m_PlayCounter == dmSound::INVALID_PLAY_ID)
        {
            sound->m_PlayCounter = 0;
        }
        return sound->m_PlayCounter++;
    }


    bool IsPlaying(HSoundInstance sound_instance)
    {
        return sound_instance->m_Playing == 1;
    }

    Result SetLooping(HSoundInstance sound_instance, bool looping, int8_t loopcount)
    {
        sound_instance->m_Looping = looping ? 1 : 0;
        sound_instance->m_Loopcounter = loopcount;
        return RESULT_OK;
    }

    Result SetParameter(HSoundInstance sound_instance, Parameter parameter, const dmVMath::Vector4& value)
    {
        switch(parameter)
        {
        case PARAMETER_GAIN:    sound_instance->m_Gain = value.getX(); break;
        case PARAMETER_SPEED:   sound_instance->m_Speed = value.getX(); break;
        case PARAMETER_PAN:     sound_instance->m_Pan = value.getX(); break;
        default:                return RESULT_INVALID_PROPERTY;
        }
        return RESULT_OK;
    }

    Result GetParameter(HSoundInstance sound_instance, Parameter parameter, dmVMath::Vector4& value)
    {
        switch(parameter)
        {
        case PARAMETER_GAIN:    value.setX(sound_instance->m_Gain); break;
        case PARAMETER_SPEED:   value.setX(sound_instance->m_Speed); break;
        case PARAMETER_PAN:     value.setX(sound_instance->m_Pan); break;
        default:                return RESULT_INVALID_PROPERTY;
        }
        return RESULT_OK;
    }

    bool IsMusicPlaying()
    {
        return false;
    }

    bool IsAudioInterrupted()
    {
        return false;
    }

    void OnWindowFocus(bool focus)
    {
        (void)focus;
    }

    Result RegisterDevice(struct DeviceType* device)
    {
        return RESULT_OK;
    }


    static Result UpdateInternal(SoundSystem* sound)
    {
        DM_PROFILE(__FUNCTION__);
        // if (!sound->m_Device)
        // {
        //     return RESULT_OK;
        // }

        uint16_t active_instance_count;
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(sound->m_Mutex);
            active_instance_count = sound->m_InstancesPool.Size();
        }

        bool currentIsAudioInterrupted = IsAudioInterrupted();
        if (!sound->m_IsAudioInterrupted && currentIsAudioInterrupted)
        {
            sound->m_IsAudioInterrupted = true;
            if (sound->m_IsDeviceStarted)
            {
                //sound->m_DeviceType->m_DeviceStop(sound->m_Device);
                sound->m_IsDeviceStarted = false;
            }
        }
        else if (sound->m_IsAudioInterrupted && !currentIsAudioInterrupted)
        {
            sound->m_IsAudioInterrupted = false;
            if (active_instance_count == 0 && sound->m_IsDeviceStarted == false)
            {
                return RESULT_NOTHING_TO_PLAY;
            }
        }

        if (sound->m_IsAudioInterrupted)
        {
            // We can't play sounds when Audio was interrupted by OS event (Phone call, Alarm etc)
            return RESULT_OK;
        }

        if (active_instance_count == 0 && sound->m_IsDeviceStarted == false)
        {
            return RESULT_NOTHING_TO_PLAY;
        }

        if (active_instance_count == 0)
        {
            #if defined(ANDROID)
            if (sound->m_IsDeviceStarted)
            {
                // // DEF-3512 Wait with stopping the device until all our queued buffers have played, if any queued buffers are
                // // still playing we will get the wrong result in isMusicPlaying on android if we release audio focus to soon
                // // since it detects our buffered sounds as "other application".
                // uint32_t free_slots = sound->m_DeviceType->m_FreeBufferSlots(sound->m_Device);
                // if (free_slots == SOUND_OUTBUFFER_COUNT)
                // {
                //     sound->m_DeviceType->m_DeviceStop(sound->m_Device);
                //     sound->m_IsDeviceStarted = false;
                // }
                sound->m_IsDeviceStarted = false;
            }
            #endif
            return RESULT_NOTHING_TO_PLAY;
        }
        // DEF-3130 Don't start the device unless something is being played
        // This allows the client to check for sound.is_music_playing() and mute sounds accordingly

        if (sound->m_IsDeviceStarted == false)
        {
            //sound->m_DeviceType->m_DeviceStart(sound->m_Device);
            sound->m_IsDeviceStarted = true;
        }

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(sound->m_Mutex);

        // uint32_t free_slots = sound->m_DeviceType->m_FreeBufferSlots(sound->m_Device);
        // if (free_slots > 0) {
        //     StepGroupValues();
        //     StepInstanceValues();
        // }

        // uint32_t current_buffer = 0;
        // uint32_t total_buffers = free_slots;
        // while (free_slots > 0) {
        //     MixContext mix_context(current_buffer, total_buffers);
        //     MixInstances(&mix_context);

        //     Master(&mix_context);

        //     // DEF-2540: Make sure to keep feeding the sound device if audio is being generated,
        //     // if you don't you'll get more slots free, thus updating sound (redundantly) every call,
        //     // resulting in a huge performance hit. Also, you'll fast forward the sounds.
        //     sound->m_DeviceType->m_Queue(sound->m_Device, (const int16_t*) sound->m_OutBuffers[sound->m_NextOutBuffer], sound->m_FrameCount);

        //     sound->m_NextOutBuffer = (sound->m_NextOutBuffer + 1) % SOUND_OUTBUFFER_COUNT;
        //     current_buffer++;
        //     free_slots--;
        // }

        // // Loop over the current jobs
        // for (;;) {
        //     ma_job job;
        //     ma_result result = ma_resource_manager_next_job(pMyResourceManager, &job);
        //     if (result != MA_SUCCESS) {
        //         if (result == MA_NO_DATA_AVAILABLE) {
        //             // No jobs are available. Keep going. Will only get this if the resource manager was initialized
        //             // with MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING.
        //             if (!sound->m_Thread)
        //                 break; // We're running on the main thread, so let's break here
        //             else
        //                 continue; // We're running on the sound thread, let's get the next job
        //         } else if (result == MA_CANCELLED) {
        //             // MA_JOB_TYPE_QUIT was posted. Exit.
        //             break;
        //         } else {
        //             // Some other error occurred.
        //             break;
        //         }
        //     }

        //     ma_job_process(&job);
        // }

        return RESULT_OK;
    }

    static void SoundThread(void* ctx)
    {
        SoundSystem* sound = (SoundSystem*)ctx;
        while (dmAtomicGet32(&sound->m_IsRunning))
        {
            Result result = RESULT_OK;
            if (!dmAtomicGet32(&sound->m_IsPaused))
                result = UpdateInternal(sound);

            dmAtomicStore32(&sound->m_Status, (int)result);
            dmTime::Sleep(sound->m_SleepInterval);
        }
    }

    // Unit tests
    int64_t GetInternalPos(HSoundInstance instance)
    {
        //SoundSystem* sound = g_SoundSystem;
        return instance->m_FramePos;
    }

    // Unit tests
    int32_t GetRefCount(HSoundData data)
    {
        return data->m_RefCount;
    }
}

// Trick until we've fixed so that the dmExportedSymbols() is always generated as long as the `exported_symbols` is used on the waf task gen
extern "C" void DefaultSoundDevice() {}
