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

#include <stdint.h>
#include <dlib/array.h>
#include <dlib/atomic.h>
#include <dlib/hashtable.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/profile.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/profile.h>

#include "sound.h"
#include "sound_codec.h"
#include "sound_private.h"
#include "sound_dsp.h"

#include <math.h>

/**
 * Defold simple sound system
 * NOTE: Must units is in frames, i.e a sample in time with N channels
 *
 */
namespace dmSound
{
    using namespace dmVMath;

    static void SoundThread(void* ctx);

    /**
     * Value with memory for "ramping" of values. See also struct Ramp below.
     */
    struct Value
    {
        inline void Reset(float value)
        {
            // note: we do NOT always start at zero for values as...
            // - orderly authored samples should start at zero (or very close to it)
            // - if coming back from a "skip" we could start at non-zero, but almost all cases for this involve a volume ramp up anyways as they are triggered by zero gain values (only skip by speed=0 could click)
            // - for panning values this would make for at times very unwanted behavior
            // - for samples it basically wold make it impossible to have a "punshy" start as we'd always need quite a few samples to reach full (intended) amplitude
            m_Prev = value;
            m_Current = value;
            m_Next = value;
        }

        inline void Set(float value, bool reset)
        {
            if (reset) {
                Reset(value);
            } else {
               m_Next = value;
            }
        }

        inline bool IsZero()
        {
            // Let's do the check in integer space instead (16 instructions -> 6)
            const uint32_t* pa = (const uint32_t*)&m_Prev;
            const uint32_t* pb = (const uint32_t*)&m_Current;
            const uint32_t* pc = (const uint32_t*)&m_Next;
            return (*pa | *pb | *pc ) == 0;
        }

        /**
         * Set previous value to current and current to next, i.e. "sample"
         */
        inline void Step()
        {
            m_Prev = m_Current;
            m_Current = m_Next;
        }

        Value()
        {
            Reset(1.0f);
        }

        float m_Prev;
        float m_Current;
        float m_Next;
    };

    /**
     * Context with data for mixing N buffers, i.e. during update
     */
    struct MixContext
    {
        MixContext(uint32_t current_buffer, uint32_t total_buffers, uint32_t frame_count)
        : m_CurrentBuffer(current_buffer),
          m_TotalBuffers(total_buffers),
          m_FrameCount(frame_count) {}

        // Current buffer index
        uint32_t m_CurrentBuffer;
        // How many buffers to mix
        uint32_t m_TotalBuffers;
        // How many frames to get from the buffer
        uint32_t m_FrameCount;
    };

    float GetRampDelta(const MixContext* mix_context, const Value* value, uint32_t total_samples, float& from)
    {
        float ramp_length = (value->m_Current - value->m_Prev) / mix_context->m_TotalBuffers;
        from = value->m_Prev + ramp_length * mix_context->m_CurrentBuffer;
        return ramp_length / total_samples;
    }

    void GetRampDelta(uint32_t num_channels, const MixContext* mix_context, const Value value[], uint32_t total_samples, float from[], float delta[])
    {
        for(uint32_t c=0; c<num_channels; ++c) {
            float ramp_length = (value[c].m_Current - value[c].m_Prev) / mix_context->m_TotalBuffers;
            from[c] = value[c].m_Prev + ramp_length * mix_context->m_CurrentBuffer;
            delta[c] = ramp_length / total_samples;
        }
    }

    struct SoundDataCallbacks
    {
        void*               m_Context;
        FSoundDataGetData   m_GetData;
    };

    struct SoundData
    {
        dmhash_t            m_NameHash;
        void*               m_Data;
        int                 m_Size;
        SoundDataCallbacks  m_DataCallbacks;
        // Index in m_SoundData
        uint16_t            m_Index;
        SoundDataType       m_Type;
        uint16_t            m_RefCount;
    };

    struct SoundInstance
    {
        dmSoundCodec::HDecoder m_Decoder;
        float*      m_Frames[SOUND_MAX_DECODE_CHANNELS];
        uint32_t    m_FrameCount;

        dmhash_t    m_Group;

        Value       m_Gain;     // default: 1.0f
        Value       m_Pan;      // 0 = -45deg left, 1 = 45 deg right
        Value       m_ScaleL[SOUND_MAX_DECODE_CHANNELS];
        Value       m_ScaleR[SOUND_MAX_DECODE_CHANNELS];
        float       m_Speed;    // 1.0 = normal speed, 0.5 = half speed, 2.0 = double speed
        uint64_t    m_FrameFraction;

        uint16_t    m_Index;
        uint16_t    m_SoundDataIndex;
        uint8_t     m_Looping : 1;
        uint8_t     m_EndOfStream : 1;
        uint8_t     m_Playing : 1;
        uint8_t     m_ScaleDirty : 1;
        uint8_t     m_ScaleInit : 1;
        uint8_t     : 3;
        int8_t      m_Loopcounter; // if set to 3, there will be 3 loops effectively playing the sound 4 times.
    };

    struct SoundGroup
    {
        dmhash_t m_NameHash;
        Value    m_Gain;
        float    m_GainParameter;
        bool     m_IsMuted;
        float*   m_MixBuffer[SOUND_MAX_MIX_CHANNELS];
        float    m_SumSquaredMemory[SOUND_MAX_MIX_CHANNELS * GROUP_MEMORY_BUFFER_COUNT];
        float    m_PeakMemorySq[SOUND_MAX_MIX_CHANNELS * GROUP_MEMORY_BUFFER_COUNT];
        uint16_t m_FrameCounts[GROUP_MEMORY_BUFFER_COUNT];
        int      m_NextMemorySlot;
    };

    struct SoundSystem
    {
        dmSoundCodec::HCodecContext   m_CodecContext;
        DeviceType*                   m_DeviceType;
        HDevice                       m_Device;
        OpenDeviceParams              m_DeviceParams;
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
        uint32_t                m_DeviceFrameCount;
        uint32_t                m_FrameCount; // Updated for each available buffer
        uint32_t                m_PlayCounter;

        void*                   m_DecoderTempOutput;
        float*                  m_DecoderOutput[SOUND_MAX_DECODE_CHANNELS];

        void*                   m_OutBuffers[SOUND_OUTBUFFER_MAX_COUNT];
        uint16_t                m_OutBufferCount;
        uint16_t                m_NextOutBuffer;
        uint8_t                 m_UseFloatOutput : 1;
        uint8_t                 m_NormalizeFloatOutput : 1;
        uint8_t                 m_NonInterleavedOutput : 1;
        uint8_t                 : 5;

        bool                    m_IsDeviceStarted;
        bool                    m_IsAudioInterrupted;
        bool                    m_HasWindowFocus;
        bool                    m_UseLinearGain;
        bool                    m_DeviceResetPending;

        float* GetDecoderBufferBase(uint8_t channel) const { assert(channel < SOUND_MAX_DECODE_CHANNELS); return (float*)((uintptr_t)m_DecoderOutput[channel] + SOUND_MAX_HISTORY * sizeof(float)); }
    };

    SoundSystem* g_SoundSystem = 0;

    DeviceType* g_FirstDevice = 0;

    void SetDefaultInitializeParams(InitializeParams* params)
    {
        memset(params, 0, sizeof(InitializeParams));
        params->m_OutputDevice = "default";
        params->m_MasterGain = 1.0f;
        params->m_MaxSoundData = 128;
        params->m_MaxSources = 16;
        params->m_MaxBuffers = 32;
        params->m_FrameCount = 0; // Let the sound system choose by default
        params->m_MaxInstances = 256;
        params->m_UseThread = true;
        params->m_DSPImplementation = DSPIMPL_TYPE_DEFAULT;
        params->m_UseLinearGain = true;
    }

    Result RegisterDevice(struct DeviceType* device)
    {
        device->m_Next = g_FirstDevice;
        g_FirstDevice = device;
        return RESULT_OK;
    }

    static Result OpenDevice(const char* name, const OpenDeviceParams* params, DeviceType** device_type, HDevice* device)
    {
        DeviceType* d = g_FirstDevice;
        while (d) {
            if (strcmp(d->m_Name, name) == 0) {
                *device_type = d;
                return d->m_Open(params, device);
            }
            d = d->m_Next;
        }

        return RESULT_DEVICE_NOT_FOUND;
    }

    static Result ResetDevice(SoundSystem* sound)
    {
        if (!sound->m_DeviceType)
        {
            return RESULT_DEVICE_NOT_FOUND;
        }

        if (sound->m_Device)
        {
            if (sound->m_IsDeviceStarted)
            {
                sound->m_DeviceType->m_DeviceStop(sound->m_Device);
                sound->m_IsDeviceStarted = false;
            }
            sound->m_DeviceType->m_Close(sound->m_Device);
            sound->m_Device = 0;
        }

        HDevice new_device = 0;
        Result result = sound->m_DeviceType->m_Open(&sound->m_DeviceParams, &new_device);
        if (result != RESULT_OK)
        {
            dmLogError("Failed to reopen audio device");
            return result;
        }

        sound->m_Device = new_device;

        DeviceInfo device_info = {0};
        sound->m_DeviceType->m_DeviceInfo(sound->m_Device, &device_info);

        if (device_info.m_FrameCount && device_info.m_FrameCount != sound->m_DeviceFrameCount)
        {
            dmLogWarning("Audio device reported frame count %u after reset (previous %u).", device_info.m_FrameCount, sound->m_DeviceFrameCount);
        }

        if (device_info.m_MixRate && device_info.m_MixRate != sound->m_MixRate)
        {
            dmLogWarning("Audio device reported mix rate %u after reset (previous %u).", device_info.m_MixRate, sound->m_MixRate);
            sound->m_MixRate = device_info.m_MixRate;
        }

        sound->m_FrameCount = sound->m_DeviceFrameCount;
        sound->m_NextOutBuffer = 0;

        return RESULT_OK;
    }

    static float GainToScale(float gain)
    {
        if (g_SoundSystem->m_UseLinearGain) {
            return gain;
        }

        gain = dmMath::Clamp(gain, 0.0f, 1.0f);
        // Convert "gain" to scale so progression over the range 'feels' linear
        // (roughly 60dB(A) range assumed; rough approximation would be simply X^4 -- if this ever is too costly)
        const float l = 0.1f;   // linear taper-off range
        const float a = 1e-3f;
        const float b = 6.908f;
        float scale = a * expf(gain * b);
        if (gain < l)
            scale *= gain * (1.0f / l);
        return dmMath::Min(scale, 1.0f);
    }

    Result GetScaleFromGain(float gain, float* scale)
    {
        *scale = GainToScale(gain);
        return RESULT_OK;
    }

    static int GetOrCreateGroup(const char* group_name)
    {
        dmhash_t group_hash = dmHashString64(group_name);

        SoundSystem* sound = g_SoundSystem;
        if (sound->m_GroupMap.Full()) {
            return -1;
        }

        if (sound->m_GroupMap.Get(group_hash)) {
            return *sound->m_GroupMap.Get(group_hash);
        }

        uint32_t index = sound->m_GroupMap.Size();
        SoundGroup* group = &sound->m_Groups[index];
        group->m_NameHash = group_hash;
        group->m_GainParameter = 1.0f;
        group->m_Gain.Reset(GainToScale(group->m_GainParameter));
        group->m_IsMuted = false;

        for(uint32_t c=0; c<SOUND_MAX_MIX_CHANNELS; ++c)
        {
            size_t mix_buffer_size = sound->m_DeviceFrameCount * sizeof(float);
            group->m_MixBuffer[c] = (float*) malloc(mix_buffer_size);
            memset(group->m_MixBuffer[c], 0, mix_buffer_size);
        }
        sound->m_GroupMap.Put(group_hash, index);
        return index;
    }

    Result Initialize(dmConfigFile::HConfig config, const InitializeParams* params)
    {
        Result r = PlatformInitialize(config, params);
        if (r != RESULT_OK) {
            return r;
        }

        float    master_gain = params->m_MasterGain;
        uint32_t max_sound_data = params->m_MaxSoundData;
        uint32_t max_sources = params->m_MaxSources;
        uint32_t max_instances = params->m_MaxInstances;
        uint32_t sample_frame_count = params->m_FrameCount; // 0 means, use the defaults
        bool use_linear_gain = params->m_UseLinearGain;

        if (config)
        {
            master_gain = dmConfigFile::GetFloat(config, "sound.gain", 1.0f);
            max_sound_data = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_data", (int32_t) max_sound_data);
            max_sources = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_sources", (int32_t) max_sources);
            max_instances = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_instances", (int32_t) max_instances);
            sample_frame_count = (uint32_t) dmConfigFile::GetInt(config, "sound.sample_frame_count", (int32_t) sample_frame_count);
            use_linear_gain = dmConfigFile::GetInt(config, "sound.use_linear_gain", (int32_t) use_linear_gain) != 0;
        }

        HDevice device = 0;
        OpenDeviceParams device_params;

        bool use_thread = params->m_UseThread;

        // Configure number of buffers lower if we are running on a thread (we can guarantee more frequent and more reliable updates)
        // TODO: m_BufferCount configurable?
        const uint16_t num_outbuffers = use_thread ? SOUND_OUTBUFFER_COUNT : SOUND_OUTBUFFER_COUNT_NO_THREADS;
        assert(num_outbuffers <= SOUND_OUTBUFFER_MAX_COUNT);

        device_params.m_BufferCount = num_outbuffers;
        device_params.m_FrameCount = sample_frame_count; // May be 0
        DeviceType* device_type;
        DeviceInfo device_info = {0};
        r = OpenDevice(params->m_OutputDevice, &device_params, &device_type, &device);
        if (r != RESULT_OK) {
            dmLogError("Failed to open device '%s'", params->m_OutputDevice);
            device_info.m_MixRate = 44100;
            device_type = 0;
        }
        else
        {
            device_type->m_DeviceInfo(device, &device_info);
        }

        g_SoundSystem = new SoundSystem();
        SoundSystem* sound = g_SoundSystem;
        sound->m_IsDeviceStarted = false;
        sound->m_IsAudioInterrupted = false;
        sound->m_HasWindowFocus = true; // Assume we startup with the window focused
        sound->m_DeviceType = device_type;
        sound->m_Device = device;
        sound->m_DeviceParams = device_params; // Stash frame and buffer count for potential device reset
        sound->m_DeviceResetPending = false;
        dmSoundCodec::NewCodecContextParams codec_params;
        codec_params.m_MaxDecoders = params->m_MaxInstances;
        sound->m_CodecContext = dmSoundCodec::New(&codec_params);
        sound->m_UseLinearGain = use_linear_gain;

        // The device wanted to provide the count (e.g. Wasapi)
        if (device_info.m_FrameCount)
        {
            sound->m_DeviceFrameCount = device_info.m_FrameCount;
        }
        else
        {
            if (sample_frame_count != 0)
            {
                sound->m_DeviceFrameCount = sample_frame_count;
            }
            else
            {
                sound->m_DeviceFrameCount = GetDefaultFrameCount(device_info.m_MixRate);
            }
        }

        // note: this will be an NoOp if the build target has DM_SOUND_EXPECTED_SIMD defined (compile-time implementation selection)
        DSPImplType dsp_impl = params->m_DSPImplementation;
        if (dsp_impl == DSPIMPL_TYPE_DEFAULT) {
            dsp_impl = device_info.m_DSPImplementation;
        }
        SelectDSPImpl(dsp_impl);

        sound->m_FrameCount = sound->m_DeviceFrameCount;
        sound->m_MixRate = device_info.m_MixRate;
        sound->m_Instances.SetCapacity(max_instances);
        sound->m_Instances.SetSize(max_instances);
        sound->m_InstancesPool.SetCapacity(max_instances);
        for (uint32_t i = 0; i < max_instances; ++i)
        {
            SoundInstance* instance = &sound->m_Instances[i];
            memset(instance, 0, sizeof(*instance));
            instance->m_Index = 0xffff;
            instance->m_SoundDataIndex = 0xffff;
            // memory to keep around history / future sample state
            for (uint32_t c = 0; c < SOUND_MAX_DECODE_CHANNELS; ++c)
            {
                instance->m_Frames[c] = (float*)malloc(SOUND_INSTANCE_STATEFRAMECOUNT * sizeof(float));
            }
            instance->m_FrameCount = 0;
            instance->m_Speed = 1.0f;
        }

        sound->m_SoundData.SetCapacity(max_sound_data);
        sound->m_SoundData.SetSize(max_sound_data);
        sound->m_SoundDataPool.SetCapacity(max_sound_data);
        for (uint32_t i = 0; i < max_sound_data; ++i)
        {
            sound->m_SoundData[i].m_Index = 0xffff;
        }

        for (uint32_t i = 0; i < SOUND_MAX_DECODE_CHANNELS; ++i)
        {
            sound->m_DecoderOutput[i] = (float*)malloc((sound->m_DeviceFrameCount * SOUND_MAX_SPEED + SOUND_MAX_HISTORY + SOUND_MAX_FUTURE) * sizeof(float));;
        }
        sound->m_DecoderTempOutput = malloc((sound->m_DeviceFrameCount * SOUND_MAX_SPEED + SOUND_MAX_HISTORY + SOUND_MAX_FUTURE) * sizeof(int16_t) * SOUND_MAX_DECODE_CHANNELS);

        sound->m_UseFloatOutput = device_info.m_UseFloats;
        sound->m_NormalizeFloatOutput = device_info.m_UseNormalized;
        sound->m_NonInterleavedOutput = device_info.m_UseNonInterleaved;
        sound->m_OutBufferCount = num_outbuffers;
        for (int i = 0; i < num_outbuffers; ++i) {
            sound->m_OutBuffers[i] = malloc(sound->m_DeviceFrameCount * (sound->m_UseFloatOutput ? sizeof(float) : sizeof(int16_t)) * SOUND_MAX_MIX_CHANNELS);
        }
        sound->m_NextOutBuffer = 0;

        sound->m_GroupMap.SetCapacity(MAX_GROUPS * 2 + 1, MAX_GROUPS);
        for (uint32_t i = 0; i < MAX_GROUPS; ++i) {
            memset(&sound->m_Groups[i], 0, sizeof(SoundGroup));
        }

        int master_index = GetOrCreateGroup("master");
        SoundGroup* master = &sound->m_Groups[master_index];
        master->m_GainParameter = master_gain;
        master->m_Gain.Reset(GainToScale(master->m_GainParameter));
        master->m_IsMuted = false;

        dmAtomicStore32(&sound->m_IsRunning, 1);
        dmAtomicStore32(&sound->m_IsPaused, 0);
        dmAtomicStore32(&sound->m_Status, (int)RESULT_NOTHING_TO_PLAY);

        sound->m_Thread = 0;
        sound->m_Mutex = 0;
        if (use_thread)
        {
            sound->m_Mutex = dmMutex::New();
            sound->m_Thread = dmThread::New((dmThread::ThreadStart)SoundThread, 0x80000, sound, "sound");
        }

        dmLogInfo("Sound");
        dmLogInfo("  nSamplesPerSec:   %d", device_info.m_MixRate);
        dmLogInfo("       useThread:   %d", use_thread);

        return r;
    }

    Result Finalize()
    {
        SoundSystem* sound = g_SoundSystem;
        if (!sound)
            return RESULT_OK;

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
            dmSoundCodec::Delete(sound->m_CodecContext);

            for (uint32_t i = 0; i < sound->m_Instances.Size(); ++i)
            {
                SoundInstance* instance = &sound->m_Instances[i];
                instance->m_Index = 0xffff;
                instance->m_SoundDataIndex = 0xffff;
                for (uint32_t c = 0; c < SOUND_MAX_DECODE_CHANNELS; ++c)
                {
                    free(instance->m_Frames[c]);
                }
                memset(instance, 0, sizeof(*instance));
            }

            free(sound->m_DecoderTempOutput);
            for (uint32_t i = 0; i < SOUND_MAX_DECODE_CHANNELS; ++i)
            {
                free(sound->m_DecoderOutput[i]);
            }

            for (int i = 0; i < sound->m_OutBufferCount; ++i) {
                free(sound->m_OutBuffers[i]);
            }

            for (uint32_t i = 0; i < MAX_GROUPS; i++) {
                SoundGroup* g = &sound->m_Groups[i];
                for(uint32_t c=0; c<SOUND_MAX_MIX_CHANNELS; ++c)
                {
                    free((void*) g->m_MixBuffer[c]);
                }
            }

            if (sound->m_Device)
            {
                if (sound->m_IsDeviceStarted)
                {
                    sound->m_DeviceType->m_DeviceStop(sound->m_Device);
                }
                sound->m_DeviceType->m_Close(sound->m_Device);
            }

            delete sound;
            g_SoundSystem = 0;
        }

        return result;
    }

    uint32_t GetMixRate()
    {
        return g_SoundSystem->m_MixRate;
    }

    uint32_t GetDefaultFrameCount(uint32_t rate)
    {
        if (rate == 48000)
            return 1024;

        if (rate == 44100)
            return 768;

        // for generic devices, we try to calculate a conservative yet small number of frames
        uint32_t frame_count = rate / 60; // use default count
        float f_frame_count = frame_count / 32; // try to round it to some nice sample alignment (e.g. 16bits *2 channels)
        float extra_percent = 1.05f;
        return ceilf(f_frame_count * extra_percent) * 32;
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

    static Result NewSoundDataInternal(const void* sound_buffer, uint32_t sound_buffer_size,
                                        FSoundDataGetData cbk, void* cbk_ctx,
                                        SoundDataType type, HSoundData* sound_data, dmhash_t name)
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
        sd->m_DataCallbacks.m_Context = cbk_ctx;
        sd->m_DataCallbacks.m_GetData = cbk;
        sd->m_RefCount = 1;

        Result result = RESULT_OK;
        if (sound_buffer != 0)
        {
            result = SetSoundDataNoLock(sd, sound_buffer, sound_buffer_size);
        }
        if (RESULT_OK == result)
            *sound_data = sd;
        else
            DeleteSoundData(sd);
        return result;
    }

    Result NewSoundData(const void* sound_buffer, uint32_t sound_buffer_size, SoundDataType type, HSoundData* sound_data, dmhash_t name)
    {
        assert(sound_buffer);
        return NewSoundDataInternal(sound_buffer, sound_buffer_size, 0, 0, type, sound_data, name);
    }

    Result NewSoundDataStreaming(FSoundDataGetData cbk, void* cbk_ctx, SoundDataType type, HSoundData* sound_data, dmhash_t name)
    {
        assert(cbk);
        assert(cbk_ctx);
        return NewSoundDataInternal(0, 0, cbk, cbk_ctx, type, sound_data, name);
    }

    Result SetSoundData(HSoundData sound_data, const void* sound_buffer, uint32_t sound_buffer_size)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        return SetSoundDataNoLock(sound_data, sound_buffer, sound_buffer_size);
    }

    Result SetSoundDataCallback(HSoundData sound_data, FSoundDataGetData cbk, void* cbk_ctx)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        sound_data->m_DataCallbacks.m_Context = cbk_ctx;
        sound_data->m_DataCallbacks.m_GetData = cbk;
        return RESULT_OK;
    }

    bool IsSoundDataValid(HSoundData sound_data)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        if (sound_data->m_DataCallbacks.m_GetData)
            return true;
        if (sound_data->m_Data)
            return true;
        return false;
    }

    uint32_t GetSoundResourceSize(HSoundData sound_data)
    {
        return sound_data->m_Size + sizeof(SoundData);
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
        sound_data->m_Index = 0xffff;

        return RESULT_OK;
    }

    // Called by the decoders in the context of a dmSound update loop (MixInstances), the the mutex acquired
    Result SoundDataRead(HSoundData sound_data, uint32_t offset, uint32_t size, void* out, uint32_t* out_size)
    {
        assert(sound_data);

        if (sound_data->m_DataCallbacks.m_GetData)
        {
            return sound_data->m_DataCallbacks.m_GetData(sound_data->m_DataCallbacks.m_Context, offset, size, out, out_size);
        }

        if (sound_data->m_Data && offset < sound_data->m_Size)
        {
            if (size == 0) {
                if (out_size)
                    *out_size = 0;
                return RESULT_OK;
            }

            uint32_t read_size = dmMath::Min(sound_data->m_Size - offset, size);
            if (read_size != 0)
            {
                memcpy(out, (void*)((uintptr_t)sound_data->m_Data + offset), read_size);
                if (out_size)
                    *out_size = read_size;
                return (read_size < size) ? RESULT_PARTIAL_DATA : RESULT_OK;
            }
        }

        return RESULT_END_OF_STREAM;
    }

    Result NewSoundInstance(HSoundData sound_data, HSoundInstance* sound_instance)
    {
        SoundSystem* ss = g_SoundSystem;

        dmSoundCodec::HDecoder decoder;

        dmSoundCodec::Format codec_format = dmSoundCodec::FORMAT_WAV;
        if (sound_data->m_Type == SOUND_DATA_TYPE_WAV) {
            codec_format = dmSoundCodec::FORMAT_WAV;
        } else if (sound_data->m_Type == SOUND_DATA_TYPE_OGG_VORBIS) {
            codec_format = dmSoundCodec::FORMAT_VORBIS;
        } else if (sound_data->m_Type == SOUND_DATA_TYPE_OPUS) {
            codec_format = dmSoundCodec::FORMAT_OPUS;
        } else {
            assert(0);
        }

        uint16_t index;
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(ss->m_Mutex);

            if (ss->m_InstancesPool.Remaining() == 0)
            {
                *sound_instance = 0;
                dmLogError("Out of sound data instance slots (%u). Increase the project setting 'sound.max_sound_instances'", ss->m_InstancesPool.Capacity());
                return RESULT_OUT_OF_INSTANCES;
            }

            dmSoundCodec::Result r = dmSoundCodec::NewDecoder(ss->m_CodecContext, codec_format, sound_data, &decoder);
            if (r != dmSoundCodec::RESULT_OK) {
                if (r == dmSoundCodec::RESULT_UNSUPPORTED) {
                    const char* name = dmHashReverseSafe64(sound_data->m_NameHash);
                    const char* format_str = dmSoundCodec::FormatToString(codec_format);
                    dmLogError("Sound '%s' uses %s, but no decoder was found. Ensure the codec is included in your App Manifest.", name, format_str);
                } else {
                    dmLogError("Failed to decode sound %s: (%d)", dmHashReverseSafe64(sound_data->m_NameHash), r);
                }
                return RESULT_INVALID_STREAM_DATA;
            }

            index = ss->m_InstancesPool.Pop();
        }

        sound_data->m_RefCount ++;

        SoundInstance* si = &ss->m_Instances[index];
        assert(si->m_Index == 0xffff);

        si->m_SoundDataIndex = sound_data->m_Index;
        si->m_Index = index;
        si->m_Gain.Reset(1.0f);
        si->m_Pan.Reset(0.5f);
        for(uint32_t c=0; c<SOUND_MAX_DECODE_CHANNELS; ++c)
        {
            si->m_ScaleL[c].Reset(0.70711f);
            si->m_ScaleR[c].Reset(0.70711f);
        }
        si->m_ScaleDirty = 1;
        si->m_ScaleInit = 1;
        for(uint32_t c=0; c<SOUND_MAX_DECODE_CHANNELS; ++c)
        {
            si->m_ScaleL[c].Reset(0.70711f);
            si->m_ScaleR[c].Reset(0.70711f);
        }
        si->m_ScaleDirty = 1;
        si->m_ScaleInit = 1;
        si->m_Looping = 0;
        si->m_EndOfStream = 0;
        si->m_Playing = 0;
        si->m_Decoder = decoder;
        si->m_Group = MASTER_GROUP_HASH;

        // prep sample history with silence
        for(uint32_t c=0; c<SOUND_MAX_DECODE_CHANNELS; ++c)
        {
            memset(si->m_Frames[c], 0, SOUND_MAX_HISTORY * sizeof(float));
        }
        si->m_FrameCount = SOUND_MAX_HISTORY;

        *sound_instance = si;

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

        uint16_t index = sound_instance->m_Index;
        sound->m_InstancesPool.Push(index);
        sound_instance->m_Index = 0xffff;
        DeleteSoundData(&sound->m_SoundData[sound_instance->m_SoundDataIndex]);
        sound_instance->m_SoundDataIndex = 0xffff;
        dmSoundCodec::DeleteDecoder(sound->m_CodecContext, sound_instance->m_Decoder);
        sound_instance->m_Decoder = 0;
        sound_instance->m_FrameCount = 0;
        sound_instance->m_Speed = 1.0f;

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
        int index = GetOrCreateGroup(group);
        if (index == -1) {
            return RESULT_OUT_OF_GROUPS;
        }
        return RESULT_OK;
    }

    Result SetGroupGain(dmhash_t group_hash, float gain)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        SoundSystem* sound = g_SoundSystem;
        int* index = sound->m_GroupMap.Get(group_hash);
        if (!index) {
            return RESULT_NO_SUCH_GROUP;
        }

        // If all playing sounds is currently at gain zero
        // we can safely do a hard reset of the group gain
        bool reset = true;
        uint32_t instances = sound->m_Instances.Size();
        for (uint32_t i = 0; i < instances; ++i)
        {
            SoundInstance* instance = &sound->m_Instances[i];
            if (instance->m_Group != group_hash)
            {
                continue;
            }
            if (instance->m_Playing || instance->m_FrameCount > 0)
            {
                if (instance->m_Gain.m_Prev == 0.0)
                {
                    continue;
                }
                reset = false;
                break;
            }
        }
        SoundGroup* group = &sound->m_Groups[*index];
        group->m_Gain.Set(GainToScale(gain), reset);
        group->m_GainParameter = gain;

        return RESULT_OK;
    }

    Result GetGroupGain(dmhash_t group_hash, float* gain)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        SoundSystem* sound = g_SoundSystem;
        int* index = sound->m_GroupMap.Get(group_hash);
        if (!index) {
            return RESULT_NO_SUCH_GROUP;
        }

        SoundGroup* group = &sound->m_Groups[*index];
        *gain = group->m_GainParameter;
        return RESULT_OK;
    }

    Result SetGroupMute(dmhash_t group_hash, bool mute)
    {
        if (!g_SoundSystem)
            return RESULT_OK;

        SoundSystem* sound = g_SoundSystem;

        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(sound->m_Mutex);
            int* index = sound->m_GroupMap.Get(group_hash);
            if (!index)
                return RESULT_NO_SUCH_GROUP;

            SoundGroup* group = &sound->m_Groups[*index];
            group->m_IsMuted = mute;
        }

        return RESULT_OK;
    }

    Result ToggleGroupMute(dmhash_t group_hash)
    {
        return SetGroupMute(group_hash, !IsGroupMuted(group_hash));
    }

    bool IsGroupMuted(dmhash_t group_hash)
    {
        if (!g_SoundSystem)
            return false;

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        SoundSystem* sound = g_SoundSystem;
        int* index = sound->m_GroupMap.Get(group_hash);
        if (!index)
            return false;

        return sound->m_Groups[*index].m_IsMuted;
    }

    Result GetGroupHashes(uint32_t* count, dmhash_t* buffer)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        SoundSystem* sound = g_SoundSystem;
        uint32_t size = sound->m_GroupMap.Size();
        assert(*count >= size);
        for (uint32_t i = 0; i < size; ++i)
        {
            buffer[i] = sound->m_Groups[i].m_NameHash;
        }
        *count = size;
        return RESULT_OK;
    }

    Result GetGroupRMS(dmhash_t group_hash, float window, float* rms_left, float* rms_right)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);

        SoundSystem* sound = g_SoundSystem;
        int* index = sound->m_GroupMap.Get(group_hash);
        if (!index) {
            return RESULT_NO_SUCH_GROUP;
        }

        if (sound->m_FrameCount == 0)
        {
            *rms_left = 0;
            *rms_right = 0;
            return RESULT_OK;
        }

        SoundGroup* g = &sound->m_Groups[*index];
        uint32_t rms_frames = (uint32_t) (sound->m_MixRate * window);
        int left = rms_frames;
        int ss_index = (g->m_NextMemorySlot - 1) % GROUP_MEMORY_BUFFER_COUNT;
        float sum_sq_left = 0;
        float sum_sq_right = 0;
        int total_frame_count = 0;
        while (left > 0) {
            sum_sq_left += g->m_SumSquaredMemory[2 * ss_index + 0];
            sum_sq_right += g->m_SumSquaredMemory[2 * ss_index + 1];
            uint16_t frame_count = g->m_FrameCounts[ss_index];
            if (frame_count == 0)
                break;

            left -= frame_count;
            total_frame_count += frame_count;
            ss_index = (ss_index - 1) % GROUP_MEMORY_BUFFER_COUNT;
        }

        *rms_left = sqrtf(sum_sq_left / (float) (total_frame_count)) / 32767.0f;
        *rms_right = sqrtf(sum_sq_right / (float) (total_frame_count)) / 32767.0f;

        return RESULT_OK;
    }

    Result GetGroupPeak(dmhash_t group_hash, float window, float* peak_left, float* peak_right)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);

        SoundSystem* sound = g_SoundSystem;
        int* index = sound->m_GroupMap.Get(group_hash);
        if (!index) {
            return RESULT_NO_SUCH_GROUP;
        }

        if (sound->m_FrameCount == 0)
        {
            *peak_left = 0;
            *peak_right = 0;
            return RESULT_OK;
        }

        SoundGroup* g = &sound->m_Groups[*index];
        uint32_t rms_frames = (uint32_t) (sound->m_MixRate * window);
        int left = rms_frames;
        int ss_index = (g->m_NextMemorySlot - 1) % GROUP_MEMORY_BUFFER_COUNT;
        float max_peak_left_sq = 0;
        float max_peak_right_sq = 0;
        while (left > 0) {
            max_peak_left_sq = dmMath::Max(max_peak_left_sq, g->m_PeakMemorySq[2 * ss_index + 0]);
            max_peak_right_sq = dmMath::Max(max_peak_right_sq, g->m_PeakMemorySq[2 * ss_index + 1]);
            uint16_t frame_count = g->m_FrameCounts[ss_index];
            if (frame_count == 0)
                break;

            left -= frame_count;
            ss_index = (ss_index - 1) % GROUP_MEMORY_BUFFER_COUNT;
        }

        *peak_left = sqrtf(max_peak_left_sq) / 32767.0f;
        *peak_right = sqrtf(max_peak_right_sq) / 32767.0f;

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
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        sound_instance->m_Playing = 0;
        dmSoundCodec::Reset(sound->m_CodecContext, sound_instance->m_Decoder);
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
       if (g_SoundSystem->m_PlayCounter == dmSound::INVALID_PLAY_ID)
       {
           g_SoundSystem->m_PlayCounter = 0;
       }
       return g_SoundSystem->m_PlayCounter++;
    }

    bool IsPlaying(HSoundInstance sound_instance)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        return sound_instance->m_Playing;
    }

    Result SetLooping(HSoundInstance sound_instance, bool looping, int8_t loopcounter)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        sound_instance->m_Looping = (uint32_t) looping;
        sound_instance->m_Loopcounter = loopcounter;
        return RESULT_OK;
    }

    void GetPanScale(float pan, float* left_scale, float* right_scale)
    {
        // Constant power panning: https://www.cs.cmu.edu/~music/icm-online/readings/panlaws/index.html
        const float theta = pan * M_PI_2;
        *left_scale = cosf(theta);
        *right_scale = sinf(theta);
    }

    Result SetParameter(HSoundInstance sound_instance, Parameter parameter, const Vector4& value)
    {
        bool reset = !sound_instance->m_Playing;
        switch(parameter)
        {
            case PARAMETER_GAIN:
                {
                    sound_instance->m_Gain.Set(GainToScale(value.getX()), reset);
                    // Trigger volume scale updates as soon as we know how many channels the instance has
                    // (we might not have that info initially)
                    sound_instance->m_ScaleDirty = 1;
                }
                break;
            case PARAMETER_PAN:
                {
                    float pan = dmMath::Max(-1.0f, dmMath::Min(1.0f, value.getX()));
                    pan = (pan + 1.0f) * 0.5f; // map [-1,1] to [0,1] for easier calculations later
                    sound_instance->m_Pan.Set(pan, reset);
                    // Trigger volume scale updates as soon as we know how many channels the instance has
                    // (we might not have that info initially)
                    sound_instance->m_ScaleDirty = 1;
                }
                break;
            case PARAMETER_SPEED:
                sound_instance->m_Speed = dmMath::Max(0.0f, dmMath::Min((float)SOUND_MAX_SPEED, value.getX()));
                break;
            default:
                dmLogError("Invalid parameter: %d (%s)\n", parameter, GetSoundName(g_SoundSystem, sound_instance));
                return RESULT_INVALID_PROPERTY;
        }
        return RESULT_OK;
    }

    static inline uint32_t GetSkipStrideBytes(const dmSoundCodec::Info& info)
    {
        // Match stride logic used in mixing when calling Decode/Skip
        // If decoder delivers float non-interleaved (or mono), use sizeof(float) per frame
        // Otherwise use interleaved stride: channels * (bits/8)
        if (info.m_BitsPerSample == 32 && (!info.m_IsInterleaved || info.m_Channels == 1))
            return (uint32_t)sizeof(float);
        return (uint32_t)(info.m_Channels * (info.m_BitsPerSample / 8));
    }

    static Result SkipToStartFrameNoLock(HSoundInstance sound_instance, uint64_t start_frame)
    {
        DM_PROFILE(__FUNCTION__);
        dmSoundCodec::Info info;
        dmSoundCodec::GetInfo(g_SoundSystem->m_CodecContext, sound_instance->m_Decoder, &info);

        uint64_t total_bytes = start_frame * (uint64_t)GetSkipStrideBytes(info);
        while (total_bytes > 0)
        {
            uint32_t skipped = 0;
            dmSoundCodec::Result r = dmSoundCodec::Skip(g_SoundSystem->m_CodecContext, sound_instance->m_Decoder, total_bytes, &skipped);
            if (r == dmSoundCodec::RESULT_END_OF_STREAM)
            {
                break;
            }
            if (r != dmSoundCodec::RESULT_OK)
            {
                return RESULT_INVALID_STREAM_DATA;
            }
            if (skipped == 0)
            {
                break;
            }
            total_bytes -= skipped;
        }
        return RESULT_OK;
    }

    Result SetStartFrame(HSoundInstance sound_instance, uint32_t start_frame)
    {
        if (!g_SoundSystem)
            return RESULT_OK;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        return SkipToStartFrameNoLock(sound_instance, (uint64_t)start_frame);
    }

    Result SetStartTime(HSoundInstance sound_instance, float start_time_seconds)
    {
        if (!g_SoundSystem)
            return RESULT_OK;
        if (start_time_seconds <= 0.0f)
            return RESULT_OK;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);

        dmSoundCodec::Info info;
        dmSoundCodec::GetInfo(g_SoundSystem->m_CodecContext, sound_instance->m_Decoder, &info);
        uint64_t start_frame = (uint64_t)((double)start_time_seconds * (double)info.m_Rate);
        return SkipToStartFrameNoLock(sound_instance, start_frame);
    }

    static inline void PrepTempBufferState(SoundInstance* instance, uint32_t channels)
    {
        if (instance->m_FrameCount > 0)
        {
            // Bring back history and any still available samples (possible left over sample & 'future' samples as available)
            assert(instance->m_FrameCount >= SOUND_MAX_HISTORY);
            uint32_t state_bytes = instance->m_FrameCount * sizeof(float);

            for(uint32_t c=0; c<channels; ++c)
            {
                float* decoder_output_buffer = g_SoundSystem->GetDecoderBufferBase(c);
                memcpy((void*)(decoder_output_buffer - SOUND_MAX_HISTORY), instance->m_Frames[c], state_bytes);
            }
        }
    }

    static inline void SaveTempBufferState(SoundInstance* instance, uint32_t avail_framecount, uint32_t used_framecount, uint32_t channels)
    {
        // Copy any leftover sample, the last 4 (note: always available in front of the decoder buffer even if nothing new was added) & the next 4 (also always there)...
        instance->m_FrameCount = SOUND_MAX_HISTORY + (avail_framecount - used_framecount) + SOUND_MAX_FUTURE;
        assert(instance->m_FrameCount <= SOUND_INSTANCE_STATEFRAMECOUNT);
        uint32_t state_bytes = instance->m_FrameCount * sizeof(float);
        // note: offset can be negative without exceeding allocate memory - we hence need to ensure proper, signed extension to 64-bit on system using 64-bit pointers
        int32_t state_offset = (int32_t)(used_framecount - SOUND_MAX_HISTORY);

        for(uint32_t c=0; c<channels; ++c)
        {
            float* decoder_output_buffer = g_SoundSystem->GetDecoderBufferBase(c);
            memcpy(instance->m_Frames[c], (void*)(decoder_output_buffer + state_offset), state_bytes);
        }
    }

    static inline void MixResampleIdentity(const MixContext* mix_context, SoundInstance* instance, uint32_t channels, uint64_t delta, float* mix_buffer[], uint32_t mix_buffer_count, uint32_t avail_framecount)
    {
        DM_PROFILE(__FUNCTION__);
        (void)delta;

        PrepTempBufferState(instance, channels);

        float scale_l[SOUND_MAX_DECODE_CHANNELS], scale_r[SOUND_MAX_DECODE_CHANNELS];
        float scale_dl[SOUND_MAX_DECODE_CHANNELS], scale_dr[SOUND_MAX_DECODE_CHANNELS];
        GetRampDelta(channels, mix_context, instance->m_ScaleL, mix_buffer_count, scale_l, scale_dl);
        GetRampDelta(channels, mix_context, instance->m_ScaleR, mix_buffer_count, scale_r, scale_dr);

        if (channels == 1)
        {
            MixScaledMonoToStereo(mix_buffer, g_SoundSystem->GetDecoderBufferBase(0), mix_buffer_count, scale_l[0], scale_r[0], scale_dl[0], scale_dr[0]);
        }
        else
        {
            assert(channels == 2);
            MixScaledStereoToStereo(mix_buffer, g_SoundSystem->GetDecoderBufferBase(0), g_SoundSystem->GetDecoderBufferBase(1), mix_buffer_count, scale_l[0], scale_r[0], scale_dl[0], scale_dr[0],
                                                                                                                                                  scale_l[1], scale_r[1], scale_dl[1], scale_dr[1]);
        }

        SaveTempBufferState(instance, avail_framecount, mix_buffer_count, channels);
    }

    static inline void MixResamplePolyphase(const MixContext* mix_context, SoundInstance* instance, uint32_t channels, uint64_t delta, float* mix_buffer[], uint32_t mix_buffer_count, uint32_t avail_framecount)
    {
        DM_PROFILE(__FUNCTION__);
        static_assert(SOUND_MAX_MIX_CHANNELS == 2, "this code assumes 2 mix channels");

        PrepTempBufferState(instance, channels);

        float scale_l[SOUND_MAX_DECODE_CHANNELS], scale_r[SOUND_MAX_DECODE_CHANNELS];
        float scale_dl[SOUND_MAX_DECODE_CHANNELS], scale_dr[SOUND_MAX_DECODE_CHANNELS];
        GetRampDelta(channels, mix_context, instance->m_ScaleL, mix_buffer_count, scale_l, scale_dl);
        GetRampDelta(channels, mix_context, instance->m_ScaleR, mix_buffer_count, scale_r, scale_dr);

        uint64_t frac = instance->m_FrameFraction;

        if (channels == 1)
        {
            frac = MixAndResampleMonoToStereo_Polyphase(mix_buffer, g_SoundSystem->GetDecoderBufferBase(0), mix_buffer_count, frac, delta, scale_l[0], scale_r[0], scale_dl[0], scale_dr[0]);
        }
        else
        {
            assert(channels == 2);
            frac = MixAndResampleStereoToStereo_Polyphase(mix_buffer, g_SoundSystem->GetDecoderBufferBase(0), g_SoundSystem->GetDecoderBufferBase(1), mix_buffer_count, frac, delta, scale_l[0], scale_r[0], scale_dl[0], scale_dr[0],
                                                                                                                                                                                    scale_l[1], scale_r[1], scale_dl[1], scale_dr[1]);
        }

        uint32_t next_index = (uint32_t)(frac >> RESAMPLE_FRACTION_BITS);
        instance->m_FrameFraction = frac & ((1U << RESAMPLE_FRACTION_BITS) - 1U);

        SaveTempBufferState(instance, avail_framecount, next_index, channels);
    }

    static inline void MixResample(const MixContext* mix_context, SoundInstance* instance, const dmSoundCodec::Info* info, uint64_t delta, float* mix_buffer[], uint32_t mix_buffer_count, uint32_t avail_framecount)
    {
        DM_PROFILE(__FUNCTION__);
        // Make sure to update the mixing scale values...
        if (instance->m_ScaleDirty != 0) {
            instance->m_ScaleDirty = 0;

            bool reset = (instance->m_ScaleInit != 0);
            instance->m_ScaleInit = 0;

            float gain = instance->m_Gain.m_Current;

            if (info->m_Channels == 1) {
                float rs, ls;
                GetPanScale(instance->m_Pan.m_Current, &ls, &rs);
                instance->m_ScaleL[0].Set(ls * gain, reset);
                instance->m_ScaleR[0].Set(rs * gain, reset);
            }
            else {
                assert(info->m_Channels == 2);

                float rs, ls;
                GetPanScale(instance->m_Pan.m_Current, &ls, &rs);
                instance->m_ScaleL[0].Set(ls * gain, reset);
                instance->m_ScaleR[0].Set(0.0f, reset);
                instance->m_ScaleL[1].Set(0.0f, reset);
                instance->m_ScaleR[1].Set(rs * gain, reset);
            }
        }

        // 1:1 output only if:
        // - rate is mixrate (including speed factor!) -> delta = 1.0
        // - sampling is not at a fractional position
        bool identity_mixer = delta == (1UL << RESAMPLE_FRACTION_BITS) && instance->m_FrameFraction == 0;

        if (identity_mixer) {
            MixResampleIdentity(mix_context, instance, info->m_Channels, delta, mix_buffer, mix_buffer_count, avail_framecount);
        }
        else
        {
            MixResamplePolyphase(mix_context, instance, info->m_Channels, delta, mix_buffer, mix_buffer_count, avail_framecount);
        }
    }

    static inline void Mix(const MixContext* mix_context, SoundInstance* instance, uint64_t delta, uint32_t avail_frames, const dmSoundCodec::Info* info, SoundGroup* group)
    {
        DM_PROFILE(__FUNCTION__);

        uint32_t avail_mix_count = ((avail_frames << RESAMPLE_FRACTION_BITS) - instance->m_FrameFraction) / delta;
        uint32_t mix_count = dmMath::Min(mix_context->m_FrameCount, avail_mix_count);

        MixResample(mix_context, instance, info, delta, group->m_MixBuffer, mix_count, avail_frames);
    }

    static bool IsMuted(SoundInstance* instance) {
        SoundSystem* sound = g_SoundSystem;

        if (instance->m_Gain.IsZero()) {
            return true;
        }

        if (instance->m_Speed == 0.0f) {
            return true;
        }

        int* group_index = sound->m_GroupMap.Get(instance->m_Group);
        if (group_index != NULL) {
            SoundGroup* group = &sound->m_Groups[*group_index];
            if (group->m_IsMuted || group->m_Gain.IsZero()) {
                return true;
            }
        }

        int* master_index = sound->m_GroupMap.Get(MASTER_GROUP_HASH);
        if (master_index != NULL) {
            SoundGroup* master = &sound->m_Groups[*master_index];
            if (master->m_IsMuted || master->m_Gain.IsZero()) {
                return true;
            }
        }

        return false;
    }

    static inline void MixInstance(const MixContext* mix_context, SoundInstance* instance)
    {
        DM_PROFILE(__FUNCTION__);
        SoundSystem* sound = g_SoundSystem;

        dmSoundCodec::Info info;
        dmSoundCodec::GetInfo(sound->m_CodecContext, instance->m_Decoder, &info);

        // TODO: Move this check to the NewSoundInstance
        bool correct_bit_depth = info.m_BitsPerSample == 32 || info.m_BitsPerSample == 16 || info.m_BitsPerSample == 8;
        bool correct_num_channels = info.m_Channels == 1 || info.m_Channels == 2;
        if (!correct_bit_depth || !correct_num_channels) {
            dmLogError("Only mono/stereo with 8/16 bits per sample is supported (%s): %u bpp %u ch", GetSoundName(sound, instance), (uint32_t)info.m_BitsPerSample, (uint32_t)info.m_Channels);
            instance->m_Playing = 0;
            return;
        }

        SoundGroup* group = nullptr;
        int* index = sound->m_GroupMap.Get(instance->m_Group);
        if (index) {
            group = &sound->m_Groups[*index];
        } else {
            dmLogError("Sound group not found");
            return;
        }

        bool is_muted = dmSound::IsMuted(instance);

        dmSoundCodec::Result r = dmSoundCodec::RESULT_OK;

        // Compute how many input samples we will need to produce the requested output samples
        uint64_t delta = (uint64_t)(((float)(info.m_Rate << RESAMPLE_FRACTION_BITS) / sound->m_MixRate) * instance->m_Speed);
        if (delta == 0) {
            // very low speed settings can get the delta to be zero due to precision limits - we skip the mixing as resulting rates are also (alsmost) inaudibly low as a result
            return;
        }
        uint32_t mixed_instance_frame_count = (uint32_t)((instance->m_FrameFraction + mix_context->m_FrameCount * delta + ((1UL << RESAMPLE_FRACTION_BITS) - 1)) >> RESAMPLE_FRACTION_BITS) + SOUND_MAX_FUTURE;

        // Compute initial amount of samples in temp buffer once we restore per instance state prior to actual mixing
        uint32_t initial_frame_count = (instance->m_FrameCount > SOUND_MAX_HISTORY) ? (instance->m_FrameCount - SOUND_MAX_HISTORY) : 0;
        uint32_t frame_count = initial_frame_count;
        uint32_t new_frame_count = 0;

        void* decoder_temp = sound->m_DecoderTempOutput;

        //
        // Refill as needed...
        //
        if (frame_count < mixed_instance_frame_count && instance->m_Playing) {

            bool is_direct_delivery = (info.m_BitsPerSample == 32 && (!info.m_IsInterleaved || info.m_Channels == 1));

            const uint32_t stride = !is_direct_delivery ? (info.m_Channels * (info.m_BitsPerSample / 8)) : sizeof(float);

            while(frame_count < mixed_instance_frame_count)
            {
                uint32_t n = mixed_instance_frame_count - frame_count; // if the result contains a fractional part and we don't ceil(), we'll end up with a smaller number. Later, when deciding the mix_count in Mix(), a smaller value (integer) will be produced. This will result in leaving a small gap in the mix buffer resulting in sound crackling when the chunk changes.

                char* buffer[SOUND_MAX_DECODE_CHANNELS];
                uint32_t buffer_size;
                if (!is_direct_delivery)
                {
                    // Output is non-float and/or interleaved and needs post output conversion
                    buffer[0] = ((char*) decoder_temp) + new_frame_count * stride;
                }
                else
                {
                    // Output will be either single channel or non-interleaved in float format -> we can just have it delivered into our work buffers
                    for(uint32_t c=0; c<info.m_Channels; ++c)
                    {
                        buffer[c] = (char*)(sound->GetDecoderBufferBase(c) + frame_count);
                    }
                }
                buffer_size = n * stride;

                uint32_t decoded = 0;
                if (!is_muted)
                {
                    r = dmSoundCodec::Decode(sound->m_CodecContext, instance->m_Decoder, buffer, buffer_size, &decoded);
                }
                else
                {
                    r = dmSoundCodec::Skip(sound->m_CodecContext, instance->m_Decoder, buffer_size, &decoded);
//TODO: COULD WE NOT AVOID MIXING ANYTHING IN THIS CASE?
                    uint32_t nc = info.m_IsInterleaved ? 1 : info.m_Channels;
                    for(uint32_t c=0; c<nc; ++c)
                    {
                        memset(buffer[c], 0x00, decoded);
                    }
                }

                if (r == dmSoundCodec::RESULT_OK)
                {
                    if (decoded == 0) // we might be streaming and didn't get any content
                    {
                        break; // Need to break as we're not progressing this sound instance
                    }

                    frame_count += decoded / stride;
                    new_frame_count += decoded / stride;
                }
                else if (r == dmSoundCodec::RESULT_END_OF_STREAM)
                {
                    assert(decoded == 0);

                    if (instance->m_Looping && instance->m_Loopcounter != 0)
                    {
                        // We need to loop...
                        dmSoundCodec::Reset(sound->m_CodecContext, instance->m_Decoder);
                        if ( instance->m_Loopcounter > 0 )
                        {
                            instance->m_Loopcounter --;
                        }
                    }
                    else
                    {
                        // Sound ends...
                        instance->m_EndOfStream = 1;
                        break;
                    }
                }
                else
                {
                    // error case
                    break;
                }
            }

            if (r != dmSoundCodec::RESULT_OK && r != dmSoundCodec::RESULT_END_OF_STREAM)
            {
                dmLogWarning("Unable to decode file '%s': %s %d", GetSoundName(sound, instance), dmSoundCodec::ResultToString(r), r);
                instance->m_Playing = 0;
                return;
            }
        }

        if (frame_count > 0)
        {
            //
            // Convert decoded data as needed...
            //
            if (new_frame_count > 0)
            {
                // Yes. Interleaved?
                if (!info.m_IsInterleaved)
                {
                    // No...

                    // For now we only support floats in this case
                    // (in which case we have nothing more to process)
                    assert(info.m_BitsPerSample == 32);
                }
                else
                {
                    // Yes, convert from temp decoder output (interleaved) to per channel data (non-interleaved) & convert to float as needed
                    const uint32_t nc = info.m_Channels;
                    if (info.m_BitsPerSample == 8)
                    {
                        if (nc == 1)
                        {
                            ConvertFromS8(sound->GetDecoderBufferBase(0) + initial_frame_count, (int8_t*)decoder_temp, new_frame_count);
                        }
                        else
                        {
                            assert(nc == 2);
                            float* out[] = {sound->GetDecoderBufferBase(0) + initial_frame_count,
                                            sound->GetDecoderBufferBase(1) + initial_frame_count};
                            DeinterleaveFromS8(out, (int8_t*)decoder_temp, new_frame_count);
                        }
                    }
                    else if (info.m_BitsPerSample == 16)
                    {
                        if (nc == 1)
                        {
                            ConvertFromS16(sound->GetDecoderBufferBase(0) + initial_frame_count, (int16_t*)decoder_temp, new_frame_count);
                        }
                        else
                        {
                            assert(nc == 2);
                            float* out[] = {sound->GetDecoderBufferBase(0) + initial_frame_count,
                                            sound->GetDecoderBufferBase(1) + initial_frame_count};
                            DeinterleaveFromS16(out, (int16_t*)decoder_temp, new_frame_count);
                        }
                    }
                    else
                    {
                        assert(info.m_BitsPerSample == 32);
                        // We only need to convert anything if the output has more than one channel...
                        if (nc != 1)
                        {
                            assert(nc == 2);
                            float* out[] = {sound->GetDecoderBufferBase(0) + initial_frame_count,
                                            sound->GetDecoderBufferBase(1) + initial_frame_count};
                            Deinterleave(out, (float*)decoder_temp, new_frame_count);
                        }
                    }
                }
            }

            //
            // Ensure proper "future" data (close to end of stream)
            //
            if (frame_count < mixed_instance_frame_count)
            {
                // We generated fewer samples then we asked for. Make sure we can still mix all "real" samples, by ensuring we have enough "future" sample values in any case
                // (should only trigger at the end of samples)
                uint32_t missing_frames = dmMath::Min(mixed_instance_frame_count - frame_count, (uint32_t)SOUND_MAX_FUTURE);

                for(uint32_t c=0; c<info.m_Channels; ++c)
                {
                    float* decoder_output_buffer = sound->GetDecoderBufferBase(c);
                    float last = decoder_output_buffer[frame_count - 1];
                    uint32_t fc = frame_count;
                    for(uint32_t mi=missing_frames; mi > 0; --mi)
                    {
                        decoder_output_buffer[fc++] = last;
                    }
                }
                frame_count += missing_frames;
            }

            //
            // Mix the data
            //
            assert(frame_count > SOUND_MAX_FUTURE);
            Mix(mix_context, instance, delta, frame_count - SOUND_MAX_FUTURE, &info, group);
        }

        if (instance->m_EndOfStream) {
            instance->m_Playing = 0;
        }
    }

    static void MixInstances(const MixContext* mix_context)
    {
        DM_PROFILE(__FUNCTION__);
        SoundSystem* sound = g_SoundSystem;

        uint32_t frame_count = mix_context->m_FrameCount;
        for (uint32_t i = 0; i < MAX_GROUPS; i++) {
            SoundGroup* g = &sound->m_Groups[i];

            if (g->m_MixBuffer[0]) {
                float sum_sq_left;
                float sum_sq_right;
                float max_sq_left;
                float max_sq_right;
                GatherPowerData(g->m_MixBuffer, frame_count, g->m_Gain.m_Current, sum_sq_left, sum_sq_right, max_sq_left, max_sq_right);
                int memory_slot = g->m_NextMemorySlot;
                g->m_FrameCounts[memory_slot] = frame_count;
                g->m_SumSquaredMemory[2 * memory_slot + 0] = sum_sq_left;
                g->m_SumSquaredMemory[2 * memory_slot + 1] = sum_sq_right;
                g->m_PeakMemorySq[2 * memory_slot + 0] = max_sq_left;
                g->m_PeakMemorySq[2 * memory_slot + 1] = max_sq_right;
                g->m_NextMemorySlot = (memory_slot + 1) % GROUP_MEMORY_BUFFER_COUNT;

                memset(g->m_MixBuffer[0], 0, frame_count * sizeof(float));
                memset(g->m_MixBuffer[1], 0, frame_count * sizeof(float));
            }
        }

        uint32_t instances = sound->m_Instances.Size();
        for (uint32_t i = 0; i < instances; ++i) {
            SoundInstance* instance = &sound->m_Instances[i];
            if (instance->m_Playing)
            {
                MixInstance(mix_context, instance);
            }
        }
    }

    static void Master(const MixContext* mix_context)
    {
        DM_PROFILE(__FUNCTION__);

        SoundSystem* sound = g_SoundSystem;

        uint32_t frame_size = 2 * (sound->m_UseFloatOutput ? sizeof(float) : sizeof(int16_t));

        uint32_t n = mix_context->m_FrameCount;
        void* out = sound->m_OutBuffers[sound->m_NextOutBuffer];
        int* master_index = sound->m_GroupMap.Get(MASTER_GROUP_HASH);
        SoundGroup* master = &sound->m_Groups[*master_index];
        float* mix_buffer[2] = {master->m_MixBuffer[0], master->m_MixBuffer[1]};

        if (master->m_Gain.IsZero())
        {
            memset(out, 0, n * frame_size);
            return;
        }

        for (uint32_t i = 0; i < MAX_GROUPS; i++) {
            SoundGroup* g = &sound->m_Groups[i];
            if (g->m_MixBuffer[0] == nullptr)
            {
                continue;
            }
            if (g->m_NameHash == MASTER_GROUP_HASH)
            {
                continue;
            }
            if (g->m_Gain.IsZero())
            {
                continue;
            }
            float gain;
            float gaind = GetRampDelta(mix_context, &g->m_Gain, n, gain);
            ApplyClampedGain(mix_buffer, g->m_MixBuffer, n, gain, gaind);
        }

        float gain;
        float gaind = GetRampDelta(mix_context, &master->m_Gain, n, gain);
        if (sound->m_UseFloatOutput == 0 && sound->m_NonInterleavedOutput == 0) {
            ApplyGainAndInterleaveToS16((int16_t*)out, mix_buffer, n, gain, gaind);
        }
        else {
            assert(sound->m_UseFloatOutput == 1 && sound->m_NonInterleavedOutput == 1);
            if (sound->m_NormalizeFloatOutput) {
                gain *= 1.0f / 32768.0f;
                gaind *= 1.0f / 32768.0f;
            }
            float* ob[] = {(float*)out, (float*)out + n};
            ApplyGain(ob, mix_buffer, n, gain, gaind);
        }
    }

    static void StepGroupValues()
    {
        SoundSystem* sound = g_SoundSystem;

        for (uint32_t i = 0; i < MAX_GROUPS; i++) {
            SoundGroup* g = &sound->m_Groups[i];
            if (g->m_MixBuffer[0]) {
                g->m_Gain.Step();
            }
        }
    }

    static void StepInstanceValues()
    {
        SoundSystem* sound = g_SoundSystem;

        uint32_t instances = sound->m_Instances.Size();
        for (uint32_t i = 0; i < instances; ++i) {
            SoundInstance* instance = &sound->m_Instances[i];
            if (instance->m_Playing || instance->m_FrameCount > 0) {
                instance->m_Gain.Step();
                instance->m_Pan.Step();
                for(uint32_t c=0; c<SOUND_MAX_DECODE_CHANNELS; ++c) {
                    instance->m_ScaleL[c].Step();
                    instance->m_ScaleR[c].Step();
                }
            }
        }
    }

    static Result UpdateInternal(SoundSystem* sound)
    {
        DM_PROFILE(__FUNCTION__);

        if (sound->m_DeviceResetPending)
        {
            Result reset_result = ResetDevice(sound);
            if (reset_result != RESULT_OK)
            {
                return reset_result;
            }
            sound->m_DeviceResetPending = false;
        }

        if (!sound->m_Device)
        {
            return RESULT_OK;
        }

        uint16_t active_instance_count;
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
            active_instance_count = sound->m_InstancesPool.Size();
        }

        bool currentIsAudioInterrupted = IsAudioInterrupted();
        if (!sound->m_IsAudioInterrupted && currentIsAudioInterrupted)
        {
            sound->m_IsAudioInterrupted = true;
            if (sound->m_IsDeviceStarted)
            {
                sound->m_DeviceType->m_DeviceStop(sound->m_Device);
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
                // DEF-3512 Wait with stopping the device until all our queued buffers have played, if any queued buffers are
                // still playing we will get the wrong result in isMusicPlaying on android if we release audio focus to soon
                // since it detects our buffered sounds as "other application".
                uint32_t free_slots = sound->m_DeviceType->m_FreeBufferSlots(sound->m_Device);
                if (free_slots == sound->m_OutBufferCount)
                {
                    sound->m_DeviceType->m_DeviceStop(sound->m_Device);
                    sound->m_IsDeviceStarted = false;
                }
            }
            #endif
            return RESULT_NOTHING_TO_PLAY;
        }
        // DEF-3130 Don't start the device unless something is being played
        // This allows the client to check for sound.is_music_playing() and mute sounds accordingly

        if (sound->m_IsDeviceStarted == false)
        {
            sound->m_DeviceType->m_DeviceStart(sound->m_Device);
            sound->m_IsDeviceStarted = true;
        }

        uint32_t free_slots = sound->m_DeviceType->m_FreeBufferSlots(sound->m_Device);
        if (free_slots > 0)
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
            StepGroupValues();
            StepInstanceValues();
        }

        uint32_t current_buffer = 0;
        uint32_t total_buffers = free_slots;
        while (free_slots > 0)
        {
            // Get the number of frames available (can block, so avoid holding the sound mutex here)
            uint32_t frame_count = sound->m_DeviceFrameCount;
            if (sound->m_DeviceType->m_GetAvailableFrames)
            {
                frame_count = sound->m_DeviceType->m_GetAvailableFrames(sound->m_Device);
                if (frame_count == 0)
                {
                    break;
                }
                // New device after reset/recovery can have a larger frame count than we allocated buffers for
                frame_count = dmMath::Min(frame_count, sound->m_DeviceFrameCount);
            }

            uint32_t buffer_index = 0;

            {
                DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);

                sound->m_FrameCount = frame_count;

                if (frame_count < SOUND_MAX_HISTORY)
                {
                    continue;
                }

                MixContext mix_context(current_buffer, total_buffers, frame_count);
                MixInstances(&mix_context);

                Master(&mix_context);

                buffer_index = sound->m_NextOutBuffer;
                sound->m_NextOutBuffer = (sound->m_NextOutBuffer + 1) % sound->m_OutBufferCount;
            }

            dmSound::Result queue_result;

            // DEF-2540: Make sure to keep feeding the sound device if audio is being generated,
            // if you don't you'll get more slots free, thus updating sound (redundantly) every call,
            // resulting in a huge performance hit. Also, you'll fast forward the sounds.
            {
                DM_PROFILE("QueueBuffer");
                queue_result = sound->m_DeviceType->m_Queue(sound->m_Device, sound->m_OutBuffers[buffer_index], frame_count);
            }

            if (queue_result == dmSound::RESULT_INIT_ERROR || queue_result == dmSound::RESULT_DEVICE_LOST)
            {
                NotifyDeviceInvalidated();
                return queue_result;
            }

            current_buffer++;
            free_slots--;
        }

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
            dmTime::Sleep(8000);
        }
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

    void NotifyDeviceInvalidated()
    {
        SoundSystem* sound = g_SoundSystem;
        if (!sound)
            return;

        if (!sound->m_DeviceResetPending)
        {
            dmLogInfo("Audio device invalidated, scheduling reset");
        }
        sound->m_DeviceResetPending = true;
        sound->m_IsDeviceStarted = false;
    }

    void GetDecoderOutputSettings(DecoderOutputSettings* settings)
    {
        memset(settings, 0, sizeof(DecoderOutputSettings));
        // We need non-normallized float output
        settings->m_UseNormalizedFloatRange = false;
        // Decoder data can also be non-interleaved (for 'direct delivery')
        settings->m_UseInterleaved = false;
    }

    bool IsMusicPlaying()
    {
        return PlatformIsMusicPlaying(g_SoundSystem->m_IsDeviceStarted, g_SoundSystem->m_HasWindowFocus);
    }

    bool IsAudioInterrupted()
    {
        return PlatformIsAudioInterrupted();
    }

    void OnWindowFocus(bool focus)
    {
        SoundSystem* sound = g_SoundSystem;
        if (sound) {
            sound->m_HasWindowFocus = focus;
        }
    }

    const char* ResultToString(Result result)
    {
        switch(result)
        {
#define RESULT_CASE(_NAME) \
    case _NAME: return #_NAME

            RESULT_CASE(RESULT_OK);
            RESULT_CASE(RESULT_PARTIAL_DATA);
            RESULT_CASE(RESULT_OUT_OF_SOURCES);
            RESULT_CASE(RESULT_EFFECT_NOT_FOUND);
            RESULT_CASE(RESULT_OUT_OF_INSTANCES);
            RESULT_CASE(RESULT_RESOURCE_LEAK);
            RESULT_CASE(RESULT_OUT_OF_BUFFERS);
            RESULT_CASE(RESULT_INVALID_PROPERTY);
            RESULT_CASE(RESULT_UNKNOWN_SOUND_TYPE);
            RESULT_CASE(RESULT_INVALID_STREAM_DATA);
            RESULT_CASE(RESULT_OUT_OF_MEMORY);
            RESULT_CASE(RESULT_UNSUPPORTED);
            RESULT_CASE(RESULT_DEVICE_NOT_FOUND);
            RESULT_CASE(RESULT_OUT_OF_GROUPS);
            RESULT_CASE(RESULT_NO_SUCH_GROUP);
            RESULT_CASE(RESULT_NOTHING_TO_PLAY);
            RESULT_CASE(RESULT_INIT_ERROR);
            RESULT_CASE(RESULT_FINI_ERROR);
            RESULT_CASE(RESULT_NO_DATA);
            RESULT_CASE(RESULT_END_OF_STREAM);
            RESULT_CASE(RESULT_UNKNOWN_ERROR);
            default: return "Unknown";
        }
#undef RESULT_CASE
    }

    // Unit tests
    int64_t GetInternalPos(HSoundInstance instance)
    {
        SoundSystem* sound = g_SoundSystem;
        return dmSoundCodec::GetInternalPos(sound->m_CodecContext, instance->m_Decoder);
    }

    // Unit tests
    int32_t GetRefCount(HSoundData data)
    {
        return data->m_RefCount;
    }
}
