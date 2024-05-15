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

#include <math.h>
#include <cfloat>

/**
 * Defold simple sound system
 * NOTE: Must units is in frames, i.e a sample in time with N channels
 *
 */
namespace dmSound
{
    using namespace dmVMath;

    #define SOUND_MAX_MIX_CHANNELS (2)
    #define SOUND_OUTBUFFER_COUNT (6)
    #define SOUND_MAX_SPEED (5)

    // TODO: How many bits?
    const uint32_t RESAMPLE_FRACTION_BITS = 31;

    const dmhash_t MASTER_GROUP_HASH = dmHashString64("master");
    const uint32_t GROUP_MEMORY_BUFFER_COUNT = 64;

    static void SoundThread(void* ctx);

    /**
     * Value with memory for "ramping" of values. See also struct Ramp below.
     */
    struct Value
    {
        inline void Reset(float value)
        {
            // NOTE: We always ramp from zero to x. Otherwise we can
            // get initial clicks when playing sounds
            m_Prev = 0.0f;
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
     * Helper for calculating ramps
     */
    struct Ramp
    {
        float m_From, m_To, m_TotalSamplesRecip;

        Ramp(const Value* value, uint32_t buffer, uint32_t total_buffers, uint32_t total_samples)
        {
            float ramp_length = (value->m_Current - value->m_Prev) / total_buffers;
            m_From = value->m_Prev + ramp_length * buffer;
            m_To = m_From + ramp_length;
            m_TotalSamplesRecip = 1.0f / total_samples;
        }

        inline float GetValue(int i) const
        {
            float mix = i * m_TotalSamplesRecip;
            return m_From + mix * (m_To - m_From);
        }
    };

    /**
     * Context with data for mixing N buffers, i.e. during update
     */
    struct MixContext
    {
        MixContext(uint32_t current_buffer, uint32_t total_buffers)
        {
            m_CurrentBuffer = current_buffer;
            m_TotalBuffers = total_buffers;
        }
        // Current buffer index
        uint32_t m_CurrentBuffer;
        // How many buffers to mix
        uint32_t m_TotalBuffers;
    };

    Ramp GetRamp(const MixContext* mix_context, const Value* value, uint32_t total_samples)
    {
        Ramp ramp(value, mix_context->m_CurrentBuffer, mix_context->m_TotalBuffers, total_samples);
        return ramp;
    }

    struct SoundData
    {
        dmhash_t      m_NameHash;
        void*         m_Data;
        int           m_Size;
        // Index in m_SoundData
        uint16_t      m_Index;
        SoundDataType m_Type;
        uint16_t      m_RefCount;
    };

    struct SoundInstance
    {
        dmSoundCodec::HDecoder m_Decoder;
        void*       m_Frames;
        dmhash_t    m_Group;

        Value       m_Gain;     // default: 1.0f
        Value       m_Pan;      // 0 = -45deg left, 1 = 45 deg right
        float       m_Speed;    // 1.0 = normal speed, 0.5 = half speed, 2.0 = double speed
        uint32_t    m_FrameCount;
        uint64_t    m_FrameFraction;

        uint16_t    m_Index;
        uint16_t    m_SoundDataIndex;
        uint8_t     m_Looping : 1;
        uint8_t     m_EndOfStream : 1;
        uint8_t     m_Playing : 1;
        uint8_t     : 5;
        int8_t      m_Loopcounter; // if set to 3, there will be 3 loops effectively playing the sound 4 times.
    };

    struct SoundGroup
    {
        dmhash_t m_NameHash;
        Value    m_Gain;
        float*   m_MixBuffer;
        float    m_SumSquaredMemory[SOUND_MAX_MIX_CHANNELS * GROUP_MEMORY_BUFFER_COUNT];
        float    m_PeakMemorySq[SOUND_MAX_MIX_CHANNELS * GROUP_MEMORY_BUFFER_COUNT];
        int      m_NextMemorySlot;
    };

    struct SoundSystem
    {
        dmSoundCodec::HCodecContext   m_CodecContext;
        DeviceType*                   m_DeviceType;
        HDevice                       m_Device;
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
        uint32_t                m_FrameCount;
        uint32_t                m_PlayCounter;

        int16_t*                m_OutBuffers[SOUND_OUTBUFFER_COUNT];
        uint16_t                m_NextOutBuffer;

        bool                    m_IsDeviceStarted;
        bool                    m_IsAudioInterrupted;
        bool                    m_HasWindowFocus;
    };

    // Since using threads is optional, we want to make it easy to switch on/off the mutex behavior
    struct OptionalScopedMutexLock
    {
        OptionalScopedMutexLock(dmMutex::HMutex mutex) : m_Mutex(mutex) {
            if (m_Mutex)
                dmMutex::Lock(m_Mutex);
        }
        ~OptionalScopedMutexLock() {
            if (m_Mutex)
                dmMutex::Unlock(m_Mutex);
        }

        dmMutex::HMutex m_Mutex;
    };
    #define DM_MUTEX_OPTIONAL_SCOPED_LOCK(mutex) OptionalScopedMutexLock SCOPED_LOCK_PASTE2(lock, __LINE__)(mutex);

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
        params->m_BufferSize = 12 * 4096;
        params->m_FrameCount = 768;
        params->m_MaxInstances = 256;
        params->m_UseThread = true;
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
        group->m_Gain.Reset(1.0f);
        size_t mix_buffer_size = sound->m_FrameCount * sizeof(float) * SOUND_MAX_MIX_CHANNELS;
        group->m_MixBuffer = (float*) malloc(mix_buffer_size);
        memset(group->m_MixBuffer, 0, mix_buffer_size);
        sound->m_GroupMap.Put(group_hash, index);
        return index;
    }

    Result Initialize(dmConfigFile::HConfig config, const InitializeParams* params)
    {
        Result r = PlatformInitialize(config, params);
        if (r != RESULT_OK) {
            return r;
        }

        HDevice device = 0;
        OpenDeviceParams device_params;
        // TODO: m_BufferCount configurable?
        device_params.m_BufferCount = SOUND_OUTBUFFER_COUNT;
        device_params.m_FrameCount = params->m_FrameCount;
        DeviceType* device_type;
        DeviceInfo device_info;
        r = OpenDevice(params->m_OutputDevice, &device_params, &device_type, &device);
        if (r != RESULT_OK) {
            dmLogError("Failed to Open device '%s'", params->m_OutputDevice);
            device_info.m_MixRate = 44100;
            device_type = 0;
        }
        else
        {
            device_type->m_DeviceInfo(device, &device_info);
        }

        float master_gain = params->m_MasterGain;

        g_SoundSystem = new SoundSystem();
        SoundSystem* sound = g_SoundSystem;
        sound->m_IsDeviceStarted = false;
        sound->m_IsAudioInterrupted = false;
        sound->m_HasWindowFocus = true; // Assume we startup with the window focused
        sound->m_DeviceType = device_type;
        sound->m_Device = device;
        dmSoundCodec::NewCodecContextParams codec_params;
        codec_params.m_MaxDecoders = params->m_MaxInstances;
        sound->m_CodecContext = dmSoundCodec::New(&codec_params);

        uint32_t max_sound_data = params->m_MaxSoundData;
        uint32_t max_buffers = params->m_MaxBuffers;
        uint32_t max_sources = params->m_MaxSources;
        uint32_t max_instances = params->m_MaxInstances;

        if (config)
        {
            master_gain = dmConfigFile::GetFloat(config, "sound.gain", 1.0f);
            max_sound_data = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_data", (int32_t) max_sound_data);
            max_buffers = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_buffers", (int32_t) max_buffers);
            max_sources = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_sources", (int32_t) max_sources);
            max_instances = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_instances", (int32_t) max_instances);
        }

        sound->m_Instances.SetCapacity(max_instances);
        sound->m_Instances.SetSize(max_instances);
        sound->m_InstancesPool.SetCapacity(max_instances);
        for (uint32_t i = 0; i < max_instances; ++i)
        {
            SoundInstance* instance = &sound->m_Instances[i];
            memset(instance, 0, sizeof(*instance));
            instance->m_Index = 0xffff;
            instance->m_SoundDataIndex = 0xffff;
            // NOTE: +1 for "over-fetch" when up-sampling
            // NOTE: and x SOUND_MAX_SPEED for potential pitch range
            instance->m_Frames = malloc((params->m_FrameCount * SOUND_MAX_SPEED + 1) * sizeof(int16_t) * SOUND_MAX_MIX_CHANNELS);
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

        sound->m_MixRate = device_info.m_MixRate;
        sound->m_FrameCount = params->m_FrameCount;
        for (int i = 0; i < SOUND_OUTBUFFER_COUNT; ++i) {
            sound->m_OutBuffers[i] = (int16_t*) malloc(params->m_FrameCount * sizeof(int16_t) * SOUND_MAX_MIX_CHANNELS);
        }
        sound->m_NextOutBuffer = 0;

        sound->m_GroupMap.SetCapacity(MAX_GROUPS * 2 + 1, MAX_GROUPS);
        for (uint32_t i = 0; i < MAX_GROUPS; ++i) {
            memset(&sound->m_Groups[i], 0, sizeof(SoundGroup));
        }

        int master_index = GetOrCreateGroup("master");
        SoundGroup* master = &sound->m_Groups[master_index];
        master->m_Gain.Reset(master_gain);

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
                free(instance->m_Frames);
                memset(instance, 0, sizeof(*instance));
            }

            for (int i = 0; i < SOUND_OUTBUFFER_COUNT; ++i) {
                free((void*) sound->m_OutBuffers[i]);
            }

            for (uint32_t i = 0; i < MAX_GROUPS; i++) {
                SoundGroup* g = &sound->m_Groups[i];
                if (g->m_MixBuffer) {
                    free((void*) g->m_MixBuffer);
                }
            }

            if (sound->m_Device)
            {
                sound->m_DeviceType->m_Close(sound->m_Device);
            }

            delete sound;
            g_SoundSystem = 0;
        }

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

    Result SetSoundData(HSoundData sound_data, const void* sound_buffer, uint32_t sound_buffer_size)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        return SetSoundDataNoLock(sound_data, sound_buffer, sound_buffer_size);
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

    Result NewSoundInstance(HSoundData sound_data, HSoundInstance* sound_instance)
    {
        SoundSystem* ss = g_SoundSystem;

        dmSoundCodec::HDecoder decoder;

        dmSoundCodec::Format codec_format = dmSoundCodec::FORMAT_WAV;
        if (sound_data->m_Type == SOUND_DATA_TYPE_WAV) {
            codec_format = dmSoundCodec::FORMAT_WAV;
        } else if (sound_data->m_Type == SOUND_DATA_TYPE_OGG_VORBIS) {
            codec_format = dmSoundCodec::FORMAT_VORBIS;
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

            dmSoundCodec::Result r = dmSoundCodec::NewDecoder(ss->m_CodecContext, codec_format, sound_data->m_Data, sound_data->m_Size, &decoder);
            if (r != dmSoundCodec::RESULT_OK) {
                dmLogError("Failed to decode sound (%d)", r);
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
        si->m_Looping = 0;
        si->m_EndOfStream = 0;
        si->m_Playing = 0;
        si->m_Decoder = decoder;
        si->m_Group = MASTER_GROUP_HASH;

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
        group->m_Gain.Set(gain, reset);
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
        *gain = group->m_Gain.m_Next;
        return RESULT_OK;
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

        SoundGroup* g = &sound->m_Groups[*index];
        uint32_t rms_frames = (uint32_t) (sound->m_MixRate * window);
        int left = rms_frames;
        int ss_index = (g->m_NextMemorySlot - 1) % GROUP_MEMORY_BUFFER_COUNT;
        float sum_sq_left = 0;
        float sum_sq_right = 0;
        int count = 0;
        while (left > 0) {
            sum_sq_left += g->m_SumSquaredMemory[2 * ss_index + 0];
            sum_sq_right += g->m_SumSquaredMemory[2 * ss_index + 1];

            left -= sound->m_FrameCount;
            ss_index = (ss_index - 1) % GROUP_MEMORY_BUFFER_COUNT;
            count++;
        }

        *rms_left = sqrtf(sum_sq_left / (float) (count * sound->m_FrameCount)) / 32767.0f;
        *rms_right = sqrtf(sum_sq_right / (float) (count * sound->m_FrameCount)) / 32767.0f;

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

        SoundGroup* g = &sound->m_Groups[*index];
        uint32_t rms_frames = (uint32_t) (sound->m_MixRate * window);
        int left = rms_frames;
        int ss_index = (g->m_NextMemorySlot - 1) % GROUP_MEMORY_BUFFER_COUNT;
        float max_peak_left_sq = 0;
        float max_peak_right_sq = 0;
        while (left > 0) {
            max_peak_left_sq = dmMath::Max(max_peak_left_sq, g->m_PeakMemorySq[2 * ss_index + 0]);
            max_peak_right_sq = dmMath::Max(max_peak_right_sq, g->m_PeakMemorySq[2 * ss_index + 1]);

            left -= sound->m_FrameCount;
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
        return sound_instance->m_Playing; // && !sound_instance->m_EndOfStream;
    }

    Result SetLooping(HSoundInstance sound_instance, bool looping, int8_t loopcounter)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        sound_instance->m_Looping = (uint32_t) looping;
        sound_instance->m_Loopcounter = loopcounter;
        return RESULT_OK;
    }

    Result SetParameter(HSoundInstance sound_instance, Parameter parameter, const Vector4& value)
    {
        bool reset = !sound_instance->m_Playing;
        switch(parameter)
        {
            case PARAMETER_GAIN:
                sound_instance->m_Gain.Set(dmMath::Max(0.0f, value.getX()), reset);
                break;
            case PARAMETER_PAN:
                {
                    float pan = dmMath::Max(-1.0f, dmMath::Min(1.0f, value.getX()));
                    pan = (pan + 1.0f) * 0.5f; // map [-1,1] to [0,1] for easier calculations later
                    sound_instance->m_Pan.Set(pan, reset);
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

    static inline void GetPanScale(float pan, float* left_scale, float* right_scale)
    {
        // Constant power panning: https://www.cs.cmu.edu/~music/icm-online/readings/panlaws/index.html
        const float theta = pan * M_PI_2;
        *left_scale = cosf(theta);
        *right_scale = sinf(theta);
    }

    /*
     *
     * Template parameters
     *
     * offset:  determines the value around which the audio samples are oscillating in the source audio data. if 0, samples are
     *          both positive and negative.
     * scale: changes the scale of the samples when mixed by multiplying their values with the 'scale' template param.
     */
    template <typename T, int offset, int scale>
    static void MixResampleUpMono(const MixContext* mix_context, SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count)
    {
        const uint32_t mask = (1U << RESAMPLE_FRACTION_BITS) - 1U;
        const float range_recip = 1.0f / mask; // TODO: Divide by (1 << RESAMPLE_FRACTION_BITS) OR (1 << RESAMPLE_FRACTION_BITS) - 1?

        uint64_t frac = instance->m_FrameFraction;
        uint32_t prev_index = 0;
        uint32_t index = 0;
        uint64_t delta = (((uint64_t) rate) << RESAMPLE_FRACTION_BITS) / mix_rate;
        delta *= instance->m_Speed;

        T* frames = (T*) instance->m_Frames;

        // Typically when the buffer is less than a mix-buffer we might overfetch
        // We never overfetch for identity mixing as identity mixing is a special case
        frames[instance->m_FrameCount] = frames[instance->m_FrameCount-1];

        Ramp gain_ramp = GetRamp(mix_context, &instance->m_Gain, mix_buffer_count);
        Ramp pan_ramp = GetRamp(mix_context, &instance->m_Pan, mix_buffer_count);
        for (uint32_t i = 0; i < mix_buffer_count; i++)
        {
            float gain = gain_ramp.GetValue(i);
            float pan = pan_ramp.GetValue(i);
            float mix = frac * range_recip; // determines the bias between two consecutive samples in the sound instance. It ranges from 0-1. A mix of 0, makes only the first sample count while a mix of 0.5 will count equally both samples.
            T s1 = frames[index];
            T s2 = frames[index + 1];
            s1 = (s1 - offset) * scale;
            s2 = (s2 - offset) * scale;

            float left_scale, right_scale;
            GetPanScale(pan, &left_scale, &right_scale);

            float s = (1.0f - mix) * s1 + mix * s2; // resulting destination sample value is a mix of two source samples since a kind of fractional indexing is used
            mix_buffer[2 * i] += s * gain * left_scale;
            mix_buffer[2 * i + 1] += s * gain * right_scale;

            prev_index = index; // keep old index for assertion
            frac += delta;

            index += (uint32_t)(frac >> RESAMPLE_FRACTION_BITS);

            frac &= ((1U << RESAMPLE_FRACTION_BITS) - 1U); // Keep lower RESAMPLE_FRACTION_BITS bits. Clear higher.
        }
        instance->m_FrameFraction = frac;

        assert(prev_index <= instance->m_FrameCount);

        // copy any remaining frames not mixed to the start of m_Frames
        assert( instance->m_FrameCount >= index);
        memmove(instance->m_Frames, (char*) instance->m_Frames + index * sizeof(T), (instance->m_FrameCount - index) * sizeof(T));
        instance->m_FrameCount -= index;
    }

    template <typename T, int offset, int scale>
    static void MixResampleUpStereo(const MixContext* mix_context, SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count)
    {
        const uint32_t mask = (1U << RESAMPLE_FRACTION_BITS) - 1U;
        const float range_recip = 1.0f / mask; // TODO: Divide by (1 << RESAMPLE_FRACTION_BITS) OR (1 << RESAMPLE_FRACTION_BITS) - 1?

        uint64_t frac = instance->m_FrameFraction;
        uint32_t prev_index = 0;
        uint32_t index = 0;
        uint64_t delta = (((uint64_t) rate) << RESAMPLE_FRACTION_BITS) / mix_rate;
        delta *= instance->m_Speed;

        T* frames = (T*) instance->m_Frames;

        // Typically when the buffer is less than a mix-buffer we might overfetch
        // We never overfetch for identity mixing as identity mixing is a special case
        frames[2 * instance->m_FrameCount] = frames[2 * instance->m_FrameCount - 2];
        frames[2 * instance->m_FrameCount + 1] = frames[2 * instance->m_FrameCount - 1];

        Ramp gain_ramp = GetRamp(mix_context, &instance->m_Gain, mix_buffer_count);
        Ramp pan_ramp = GetRamp(mix_context, &instance->m_Pan, mix_buffer_count);
        for (uint32_t i = 0; i < mix_buffer_count; i++)
        {
            float gain = gain_ramp.GetValue(i);
            float pan = pan_ramp.GetValue(i);
            float mix = frac * range_recip;
            T sl1 = frames[2 * index];
            T sl2 = frames[2 * index + 2];
            sl1 = (sl1 - offset) * scale;
            sl2 = (sl2 - offset) * scale;

            T sr1 = frames[2 * index + 1];
            T sr2 = frames[2 * index + 3];
            sr1 = (sr1 - offset) * scale;
            sr2 = (sr2 - offset) * scale;

            float left_scale, right_scale;
            GetPanScale(pan, &left_scale, &right_scale);

            float sl = (1.0f - mix) * sl1 + mix * sl2;
            float sr = (1.0f - mix) * sr1 + mix * sr2;
            mix_buffer[2 * i]       += sl * gain * left_scale;
            mix_buffer[2 * i + 1]   += sr * gain * right_scale;

            prev_index = index;
            frac += delta;
            index += (uint32_t)(frac >> RESAMPLE_FRACTION_BITS);

            frac &= ((1U << RESAMPLE_FRACTION_BITS) - 1U);
        }
        instance->m_FrameFraction = frac;

        assert(prev_index <= instance->m_FrameCount);

        memmove(instance->m_Frames, (char*) instance->m_Frames + index * sizeof(T) * 2, (instance->m_FrameCount - index) * sizeof(T) * 2);
        instance->m_FrameCount -= index;
    }

    template <typename T, int offset, int scale>
    static void MixResampleIdentityMono(const MixContext* mix_context, SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count)
    {
        (void)rate;
        (void)mix_rate;
        assert(instance->m_FrameCount == mix_buffer_count);
        T* frames = (T*) instance->m_Frames;
        Ramp gain_ramp = GetRamp(mix_context, &instance->m_Gain, mix_buffer_count);
        Ramp pan_ramp = GetRamp(mix_context, &instance->m_Pan, mix_buffer_count);

        for (uint32_t i = 0; i < mix_buffer_count; i++)
        {
            float gain = gain_ramp.GetValue(i);
            float pan = pan_ramp.GetValue(i);
            float s = frames[i];
            s = (s - offset) * scale * gain;

            float left_scale, right_scale;
            GetPanScale(pan, &left_scale, &right_scale);
            mix_buffer[2 * i]       += s * left_scale;
            mix_buffer[2 * i + 1]   += s * right_scale;
        }
        instance->m_FrameCount -= mix_buffer_count;
    }

    template <typename T, int offset, int scale>
    static void MixResampleIdentityStereo(const MixContext* mix_context, SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count)
    {
        (void)rate;
        (void)mix_rate;
        assert(instance->m_FrameCount == mix_buffer_count);
        T* frames = (T*) instance->m_Frames;
        Ramp gain_ramp = GetRamp(mix_context, &instance->m_Gain, mix_buffer_count);
        Ramp pan_ramp = GetRamp(mix_context, &instance->m_Pan, mix_buffer_count);

        for (uint32_t i = 0; i < mix_buffer_count; i++)
        {
            float gain = gain_ramp.GetValue(i);
            float pan = pan_ramp.GetValue(i);
            float s1 = frames[2 * i];
            float s2 = frames[2 * i + 1];
            s1 = (s1 - offset) * scale * gain;
            s2 = (s2 - offset) * scale * gain;

            float left_scale, right_scale;
            GetPanScale(pan, &left_scale, &right_scale);
            mix_buffer[2 * i]       += s1 * left_scale;
            mix_buffer[2 * i + 1]   += s2 * right_scale;
        }
        instance->m_FrameCount -= mix_buffer_count;
    }

    typedef void (*MixerFunction)(const MixContext* mix_context, SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count);

    struct Mixer
    {
        uint32_t        m_Channels;
        uint32_t        m_BitsPerSample;
        MixerFunction   m_Mixer;
        Mixer(uint32_t channels, uint32_t bits_per_sample, MixerFunction mixer)
        {
            m_Channels = channels;
            m_BitsPerSample = bits_per_sample;
            m_Mixer = mixer;
        }
    };

    Mixer g_Mixers[] = {
            Mixer(1, 8, MixResampleUpMono<uint8_t, 128, 255>),
            Mixer(1, 16, MixResampleUpMono<int16_t, 0, 1>),
            Mixer(2, 8, MixResampleUpStereo<uint8_t, 128, 255>),
            Mixer(2, 16, MixResampleUpStereo<int16_t, 0, 1>),
    };

    Mixer g_IdentityMixers[] = {
            Mixer(1, 8, MixResampleIdentityMono<uint8_t, 128, 255>),
            Mixer(1, 16, MixResampleIdentityMono<int16_t, 0, 1>),
            Mixer(2, 8, MixResampleIdentityStereo<uint8_t, 128, 255>),
            Mixer(2, 16, MixResampleIdentityStereo<int16_t, 0, 1>),
    };

    static void MixResample(const MixContext* mix_context, SoundInstance* instance, const dmSoundCodec::Info* info, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count)
    {
        const uint32_t rate = info->m_Rate;
        assert(rate <= mix_rate);

        void (*mixer)(const MixContext* mix_context, SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count) = 0;

        bool identity_mixer = rate == mix_rate && instance->m_Speed == 1.0f;

        if (identity_mixer) {
            uint32_t n = sizeof(g_IdentityMixers) / sizeof(g_IdentityMixers[0]);
            for (uint32_t i = 0; i < n; i++) {
                const Mixer& m = g_IdentityMixers[i];
                if (m.m_BitsPerSample == info->m_BitsPerSample &&
                    m.m_Channels == info->m_Channels) {
                    mixer = m.m_Mixer;
                    break;
                }
            }
        } else {
            uint32_t n = sizeof(g_Mixers) / sizeof(g_Mixers[0]);
            for (uint32_t i = 0; i < n; i++) {
                const Mixer& m = g_Mixers[i];
                if (m.m_BitsPerSample == info->m_BitsPerSample &&
                    m.m_Channels == info->m_Channels) {
                    mixer = m.m_Mixer;
                    break;
                }
            }
        }
        mixer(mix_context, instance, rate, mix_rate, mix_buffer, mix_buffer_count);
    }

    static void Mix(const MixContext* mix_context, SoundInstance* instance, const dmSoundCodec::Info* info)
    {
        DM_PROFILE(__FUNCTION__);

        SoundSystem* sound = g_SoundSystem;
        uint64_t delta = (uint32_t) ((((uint64_t) info->m_Rate) << RESAMPLE_FRACTION_BITS) / sound->m_MixRate);
        uint32_t mix_count = ((uint64_t) (instance->m_FrameCount) << RESAMPLE_FRACTION_BITS) / (delta * instance->m_Speed);
        mix_count = dmMath::Min(mix_count, sound->m_FrameCount);
        assert(mix_count <= sound->m_FrameCount);

        int* index = sound->m_GroupMap.Get(instance->m_Group);
        if (index) {
            SoundGroup* group = &sound->m_Groups[*index];
            MixResample(mix_context, instance, info, sound->m_MixRate, group->m_MixBuffer, mix_count);
        } else {
            dmLogError("Sound group not found");
        }
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
            if (group->m_Gain.IsZero()) {
                return true;
            }
        }

        int* master_index = sound->m_GroupMap.Get(MASTER_GROUP_HASH);
        if (master_index != NULL) {
            SoundGroup* master = &sound->m_Groups[*master_index];
            if (master->m_Gain.IsZero()) {
                return true;
            }
        }

        return false;
    }

    static void MixInstance(const MixContext* mix_context, SoundInstance* instance) {
        SoundSystem* sound = g_SoundSystem;
        uint32_t decoded = 0;

        dmSoundCodec::Info info;
        dmSoundCodec::GetInfo(sound->m_CodecContext, instance->m_Decoder, &info);
        bool correct_bit_depth = info.m_BitsPerSample == 16 || info.m_BitsPerSample == 8;
        bool correct_num_channels = info.m_Channels == 1 || info.m_Channels == 2;
        if (!correct_bit_depth || !correct_num_channels) {
            dmLogError("Only mono/stereo with 8/16 bits per sample is supported (%s): %u bpp %u ch", GetSoundName(sound, instance), (uint32_t)info.m_BitsPerSample, (uint32_t)info.m_Channels);
            instance->m_Playing = 0;
            return;
        }

        if (info.m_Rate > sound->m_MixRate) {
            dmLogError("Sounds with rate higher than sample-rate not supported (%d hz > %d hz) (%s)", info.m_Rate, sound->m_MixRate, GetSoundName(sound, instance));
            instance->m_Playing = 0;
            return;
        }

        bool is_muted = dmSound::IsMuted(instance);

        dmSoundCodec::Result r = dmSoundCodec::RESULT_OK;
        uint32_t mixed_instance_FrameCount = ceilf(sound->m_FrameCount * dmMath::Max(1.0f, instance->m_Speed));

        if (instance->m_FrameCount < mixed_instance_FrameCount && instance->m_Playing) {

            const uint32_t stride = info.m_Channels * (info.m_BitsPerSample / 8);
            uint32_t n = mixed_instance_FrameCount - instance->m_FrameCount; // if the result contains a fractional part and we don't ceil(), we'll end up with a smaller number. Later, when deciding the mix_count in Mix(), a smaller value (integer) will be produced. This will result in leaving a small gap in the mix buffer resulting in sound crackling when the chunk changes.

            if (!is_muted)
            {
                r = dmSoundCodec::Decode(sound->m_CodecContext,
                                         instance->m_Decoder,
                                         ((char*) instance->m_Frames) + instance->m_FrameCount * stride,
                                         n * stride,
                                         &decoded);
            }
            else
            {
                r = dmSoundCodec::Skip(sound->m_CodecContext, instance->m_Decoder, n * stride, &decoded);
                memset(((char*) instance->m_Frames) + instance->m_FrameCount * stride, 0x00, n * stride);
            }

            assert(decoded % stride == 0);
            instance->m_FrameCount += decoded / stride;

            if (instance->m_FrameCount < mixed_instance_FrameCount) {

                if (instance->m_Looping && instance->m_Loopcounter != 0) {
                    dmSoundCodec::Reset(sound->m_CodecContext, instance->m_Decoder);
                    if ( instance->m_Loopcounter > 0 ) {
                        instance->m_Loopcounter --;
                    }

                    uint32_t n = mixed_instance_FrameCount - instance->m_FrameCount;
                    if (!is_muted)
                    {
                        r = dmSoundCodec::Decode(sound->m_CodecContext,
                                                 instance->m_Decoder,
                                                 ((char*) instance->m_Frames) + instance->m_FrameCount * stride,
                                                 n * stride,
                                                 &decoded);
                    }
                    else
                    {
                        r = dmSoundCodec::Skip(sound->m_CodecContext, instance->m_Decoder, n * stride, &decoded);
                        memset(((char*) instance->m_Frames) + instance->m_FrameCount * stride, 0x00, n * stride);
                    }

                    assert(decoded % stride == 0);
                    instance->m_FrameCount += decoded / stride;

                } else {

                    if  (instance->m_FrameCount < instance->m_Speed) {
                        // since this is the last mix and no more frames will be added, trailing frames will linger on forever
                        // if they are less than m_Speed. We will truncate them to avoid this.
                        instance->m_FrameCount = 0;
                    }
                    instance->m_EndOfStream = 1;
                }
            }
        }

        if (r != dmSoundCodec::RESULT_OK) {
            dmLogWarning("Unable to decode file '%s'. Result %d", GetSoundName(sound, instance), r);
            instance->m_Playing = 0;
            return;
        }

        if (instance->m_FrameCount > 0)
            Mix(mix_context, instance, &info);

        if (instance->m_FrameCount <= 1 && instance->m_EndOfStream) {
            // NOTE: Due to round-off errors, e.g 32000 -> 44100,
            // the last frame might be partially sampled and
            // used in the *next* buffer. We truncate such scenarios to 0
            instance->m_FrameCount = 0;
        }
    }

    static void MixInstances(const MixContext* mix_context)
    {
        DM_PROFILE(__FUNCTION__);
        SoundSystem* sound = g_SoundSystem;

        for (uint32_t i = 0; i < MAX_GROUPS; i++) {
            SoundGroup* g = &sound->m_Groups[i];

            if (g->m_MixBuffer) {
                uint32_t frame_count = sound->m_FrameCount;
                float sum_sq_left = 0;
                float sum_sq_right = 0;
                float max_sq_left = 0;
                float max_sq_right = 0;
                for (uint32_t j = 0; j < frame_count; j++) {

                    float gain = g->m_Gain.m_Current;

                    float left = g->m_MixBuffer[2 * j + 0] * gain;
                    float right = g->m_MixBuffer[2 * j + 1] * gain;
                    float left_sq = left * left;
                    float right_sq = right * right;
                    sum_sq_left += left_sq;
                    sum_sq_right += right_sq;
                    max_sq_left = dmMath::Max(max_sq_left, left_sq);
                    max_sq_right = dmMath::Max(max_sq_right, right_sq);
                }
                g->m_SumSquaredMemory[2 * g->m_NextMemorySlot + 0] = sum_sq_left;
                g->m_SumSquaredMemory[2 * g->m_NextMemorySlot + 1] = sum_sq_right;
                g->m_PeakMemorySq[2 * g->m_NextMemorySlot + 0] = max_sq_left;
                g->m_PeakMemorySq[2 * g->m_NextMemorySlot + 1] = max_sq_right;
                g->m_NextMemorySlot = (g->m_NextMemorySlot + 1) % GROUP_MEMORY_BUFFER_COUNT;

                memset(g->m_MixBuffer, 0, sound->m_FrameCount * sizeof(float) * 2);
            }
        }

        uint32_t instances = sound->m_Instances.Size();
        for (uint32_t i = 0; i < instances; ++i) {
            SoundInstance* instance = &sound->m_Instances[i];
            if (instance->m_Playing || instance->m_FrameCount > 0)
            {
                MixInstance(mix_context, instance);
            }

            if (instance->m_EndOfStream && instance->m_FrameCount == 0) {
                instance->m_Playing = 0;
            }
        }
    }

    static void Master(const MixContext* mix_context)
    {
        DM_PROFILE(__FUNCTION__);

        SoundSystem* sound = g_SoundSystem;
        uint32_t n = sound->m_FrameCount;
        int16_t* out = sound->m_OutBuffers[sound->m_NextOutBuffer];
        int* master_index = sound->m_GroupMap.Get(MASTER_GROUP_HASH);
        SoundGroup* master = &sound->m_Groups[*master_index];
        float* mix_buffer = master->m_MixBuffer;

        if (master->m_Gain.IsZero())
        {
            memset(out, 0, n * sizeof(uint16_t) * 2);
            return;
        }

        for (uint32_t i = 0; i < MAX_GROUPS; i++) {
            SoundGroup* g = &sound->m_Groups[i];
            if (g->m_MixBuffer == 0x0)
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
            Ramp ramp = GetRamp(mix_context, &g->m_Gain, n);
            for (uint32_t i = 0; i < n; i++) {
                float gain = ramp.GetValue(i);
                gain = dmMath::Clamp(gain, 0.0f, 1.0f);

                float s1 = g->m_MixBuffer[2 * i];
                float s2 = g->m_MixBuffer[2 * i + 1];
                mix_buffer[2 * i] += s1 * gain;
                mix_buffer[2 * i + 1] += s2 * gain;
            }
        }

        Ramp ramp = GetRamp(mix_context, &master->m_Gain, n);
        for (uint32_t i = 0; i < n; i++) {
            float gain = ramp.GetValue(i);
            float s1 = mix_buffer[2 * i] * gain;
            float s2 = mix_buffer[2 * i + 1] * gain;
            s1 = dmMath::Min(32767.0f, s1);
            s1 = dmMath::Max(-32768.0f, s1);
            s2 = dmMath::Min(32767.0f, s2);
            s2 = dmMath::Max(-32768.0f, s2);
            out[2 * i] = (int16_t) s1;
            out[2 * i + 1] = (int16_t) s2;
        }
    }

    static void StepGroupValues()
    {
        SoundSystem* sound = g_SoundSystem;

        for (uint32_t i = 0; i < MAX_GROUPS; i++) {
            SoundGroup* g = &sound->m_Groups[i];
            if (g->m_MixBuffer) {
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
            }
        }
    }

    static Result UpdateInternal(SoundSystem* sound)
    {
        DM_PROFILE(__FUNCTION__);
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
                if (free_slots == SOUND_OUTBUFFER_COUNT)
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

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);

        uint32_t free_slots = sound->m_DeviceType->m_FreeBufferSlots(sound->m_Device);
        if (free_slots > 0) {
            StepGroupValues();
            StepInstanceValues();
        }

        uint32_t current_buffer = 0;
        uint32_t total_buffers = free_slots;
        while (free_slots > 0) {
            MixContext mix_context(current_buffer, total_buffers);
            MixInstances(&mix_context);

            Master(&mix_context);

            // DEF-2540: Make sure to keep feeding the sound device if audio is being generated,
            // if you don't you'll get more slots free, thus updating sound (redundantly) every call,
            // resulting in a huge performance hit. Also, you'll fast forward the sounds.
            sound->m_DeviceType->m_Queue(sound->m_Device, (const int16_t*) sound->m_OutBuffers[sound->m_NextOutBuffer], sound->m_FrameCount);

            sound->m_NextOutBuffer = (sound->m_NextOutBuffer + 1) % SOUND_OUTBUFFER_COUNT;
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
