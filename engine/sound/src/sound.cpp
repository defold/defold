#include <stdint.h>
#include <dlib/hashtable.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include <dlib/time.h>

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
    #define SOUND_MAX_MIX_CHANNELS (2)
    #define SOUND_OUTBUFFER_COUNT (6)

    // TODO: How many bits?
    const uint32_t RESAMPLE_FRACTION_BITS = 31;

    const dmhash_t MASTER_GROUP_HASH = dmHashString64("master");
    const uint32_t MAX_GROUPS = 32;
    const uint32_t GROUP_MEMORY_BUFFER_COUNT = 64;

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
    };

    struct SoundInstance
    {
        uint16_t        m_Index;
        uint16_t        m_SoundDataIndex;
        Value           m_Gain;

        dmSoundCodec::HDecoder m_Decoder;
        void*           m_Frames;
        uint32_t        m_FrameCount;
        uint32_t        m_FrameFraction;

        dmhash_t        m_Group;

        uint32_t        m_Looping : 1;
        uint32_t        m_EndOfStream : 1;
        uint32_t        m_Playing : 1;
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

        dmArray<SoundInstance>  m_Instances;
        dmIndexPool16           m_InstancesPool;

        dmArray<SoundData>      m_SoundData;
        dmIndexPool16           m_SoundDataPool;

        dmHashTable<dmhash_t, int> m_GroupMap;
        SoundGroup              m_Groups[MAX_GROUPS];

        Stats                   m_Stats;

        uint32_t                m_MixRate;
        uint32_t                m_FrameCount;

        int16_t*                m_OutBuffers[SOUND_OUTBUFFER_COUNT];
        uint16_t                m_NextOutBuffer;

        bool                    m_IsSoundActive;
        bool                    m_IsPhoneCallActive;
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
        params->m_BufferSize = 12 * 4096;
        params->m_FrameCount = 768;
        params->m_MaxInstances = 256;
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

        HDevice device;
        OpenDeviceParams device_params;
        // TODO: m_BufferCount configurable?
        device_params.m_BufferCount = SOUND_OUTBUFFER_COUNT;
        device_params.m_FrameCount = params->m_FrameCount;
        DeviceType* device_type;
        r = OpenDevice(params->m_OutputDevice, &device_params, &device_type, &device);
        if (r != RESULT_OK) {
            dmLogError("Failed to Open device '%s'", params->m_OutputDevice);
            return r;
        }

        DeviceInfo device_info;
        device_type->m_DeviceInfo(device, &device_info);

        float master_gain = params->m_MasterGain;

        g_SoundSystem = new SoundSystem();
        SoundSystem* sound = g_SoundSystem;
        sound->m_IsSoundActive = false;
        sound->m_IsPhoneCallActive = false;
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
            instance->m_Frames = malloc((params->m_FrameCount + 1) * sizeof(int16_t) * SOUND_MAX_MIX_CHANNELS);
            instance->m_FrameCount = 0;
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

        memset(&g_SoundSystem->m_Stats, 0, sizeof(g_SoundSystem->m_Stats));
        sound->m_GroupMap.SetCapacity(MAX_GROUPS * 2 + 1, MAX_GROUPS);
        for (uint32_t i = 0; i < MAX_GROUPS; ++i) {
            memset(&sound->m_Groups[i], 0, sizeof(SoundGroup));
        }

        int master_index = GetOrCreateGroup("master");
        SoundGroup* master = &sound->m_Groups[master_index];
        master->m_Gain.Reset(master_gain);

        return RESULT_OK;
    }

    Result Finalize()
    {
        PlatformFinalize();

        Result result = RESULT_OK;

        if (g_SoundSystem)
        {
            SoundSystem* sound = g_SoundSystem;
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

            sound->m_DeviceType->m_Close(sound->m_Device);

            delete sound;
            g_SoundSystem = 0;
        }

        return result;

        return RESULT_OK;
    }

    void GetStats(Stats* stats)
    {
        *stats = g_SoundSystem->m_Stats;
    }

    Result NewSoundData(const void* sound_buffer, uint32_t sound_buffer_size, SoundDataType type, HSoundData* sound_data, dmhash_t name)
    {
        SoundSystem* sound = g_SoundSystem;

        if (sound->m_SoundDataPool.Remaining() == 0)
        {
            *sound_data = 0;
            dmLogError("Out of sound data slots (%u). Increase the project setting 'sound.max_sound_data'", sound->m_SoundDataPool.Capacity());
            return RESULT_OUT_OF_INSTANCES;
        }
        uint16_t index = sound->m_SoundDataPool.Pop();

        SoundData* sd = &sound->m_SoundData[index];
        sd->m_NameHash = name;
        sd->m_Type = type;
        sd->m_Index = index;
        sd->m_Data = 0;
        sd->m_Size = 0;

        Result result = SetSoundData(sd, sound_buffer, sound_buffer_size);
        if (result == RESULT_OK)
            *sound_data = sd;
        else
            DeleteSoundData(sd);

        return result;
    }

    Result SetSoundData(HSoundData sound_data, const void* sound_buffer, uint32_t sound_buffer_size)
    {
        free(sound_data->m_Data);
        sound_data->m_Data = malloc(sound_buffer_size);
        sound_data->m_Size = sound_buffer_size;
        memcpy(sound_data->m_Data, sound_buffer, sound_buffer_size);
        return RESULT_OK;
    }

    uint32_t GetSoundResourceSize(HSoundData sound_data)
    {
        return sound_data->m_Size + sizeof(SoundData);
    }

    Result DeleteSoundData(HSoundData sound_data)
    {
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

        if (ss->m_InstancesPool.Remaining() == 0)
        {
            *sound_instance = 0;
            dmLogError("Out of sound data instance slots (%u). Increase the project setting 'sound.max_sound_instances'", ss->m_InstancesPool.Capacity());
            return RESULT_OUT_OF_INSTANCES;
        }

        dmSoundCodec::HDecoder decoder;

        dmSoundCodec::Format codec_format = dmSoundCodec::FORMAT_WAV;
        if (sound_data->m_Type == SOUND_DATA_TYPE_WAV) {
            codec_format = dmSoundCodec::FORMAT_WAV;
        } else if (sound_data->m_Type == SOUND_DATA_TYPE_OGG_VORBIS) {
            codec_format = dmSoundCodec::FORMAT_VORBIS;
        } else {
            assert(0);
        }

        dmSoundCodec::Result r = dmSoundCodec::NewDecoder(ss->m_CodecContext, codec_format, sound_data->m_Data, sound_data->m_Size, &decoder);
        if (r != dmSoundCodec::RESULT_OK) {
            dmLogError("Failed to decode sound (%d)", r);
            return RESULT_INVALID_STREAM_DATA;
        }

        dmSoundCodec::Info info;
        dmSoundCodec::GetInfo(ss->m_CodecContext, decoder, &info);

        uint16_t index = ss->m_InstancesPool.Pop();
        SoundInstance* si = &ss->m_Instances[index];
        assert(si->m_Index == 0xffff);

        si->m_SoundDataIndex = sound_data->m_Index;
        si->m_Index = index;
        si->m_Gain.Reset(1.0f);
        si->m_Looping = 0;
        si->m_EndOfStream = 0;
        si->m_Playing = 0;
        si->m_Decoder = decoder;
        si->m_Group = MASTER_GROUP_HASH;

        *sound_instance = si;

        return RESULT_OK;
    }

    Result DeleteSoundInstance(HSoundInstance sound_instance)
    {
        SoundSystem* sound = g_SoundSystem;
        if (IsPlaying(sound_instance))
        {
            dmLogError("Deleting playing sound instance");
            Stop(sound_instance);
        }
        uint16_t index = sound_instance->m_Index;
        sound->m_InstancesPool.Push(index);
        sound_instance->m_Index = 0xffff;
        sound_instance->m_SoundDataIndex = 0xffff;
        dmSoundCodec::DeleteDecoder(sound->m_CodecContext, sound_instance->m_Decoder);
        sound_instance->m_Decoder = 0;
        sound_instance->m_FrameCount = 0;

        return RESULT_OK;
    }

    Result SetInstanceGroup(HSoundInstance instance, const char* group_name)
    {
        dmhash_t group_hash = dmHashString64(group_name);
        return SetInstanceGroup(instance, group_hash);
    }

    Result SetInstanceGroup(HSoundInstance instance, dmhash_t group_hash)
    {
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
        int index = GetOrCreateGroup(group);
        if (index == -1) {
            return RESULT_OUT_OF_GROUPS;
        }
        return RESULT_OK;
    }

    Result SetGroupGain(dmhash_t group_hash, float gain)
    {
        SoundSystem* sound = g_SoundSystem;
        int* index = sound->m_GroupMap.Get(group_hash);
        if (!index) {
            return RESULT_NO_SUCH_GROUP;
        }

        SoundGroup* group = &sound->m_Groups[*index];
        group->m_Gain.m_Next = gain;
        return RESULT_OK;
    }

    Result GetGroupGain(dmhash_t group_hash, float* gain)
    {
        SoundSystem* sound = g_SoundSystem;
        int* index = sound->m_GroupMap.Get(group_hash);
        if (!index) {
            return RESULT_NO_SUCH_GROUP;
        }

        SoundGroup* group = &sound->m_Groups[*index];
        *gain = group->m_Gain.m_Next;
        return RESULT_OK;
    }

    uint32_t GetGroupCount()
    {
        SoundSystem* sound = g_SoundSystem;
        return sound->m_GroupMap.Size();
    }

    Result GetGroupHash(uint32_t index, dmhash_t* hash)
    {
        SoundSystem* sound = g_SoundSystem;
        if (index >= sound->m_GroupMap.Size()) {
            return RESULT_NO_SUCH_GROUP;
        }

        SoundGroup* group = &sound->m_Groups[index];
        *hash = group->m_NameHash;
        return RESULT_OK;
    }

    Result GetGroupRMS(dmhash_t group_hash, float window, float* rms_left, float* rms_right)
    {
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
        int count = 0;
        while (left > 0) {
            max_peak_left_sq = dmMath::Max(max_peak_left_sq, g->m_PeakMemorySq[2 * ss_index + 0]);
            max_peak_right_sq = dmMath::Max(max_peak_right_sq, g->m_PeakMemorySq[2 * ss_index + 1]);

            left -= sound->m_FrameCount;
            ss_index = (ss_index - 1) % GROUP_MEMORY_BUFFER_COUNT;
            count++;
        }

        *peak_left = sqrtf(max_peak_left_sq) / 32767.0f;
        *peak_right = sqrtf(max_peak_right_sq) / 32767.0f;

        return RESULT_OK;
    }

    Result Play(HSoundInstance sound_instance)
    {
        sound_instance->m_Playing = 1;
        return RESULT_OK;
    }

    Result Stop(HSoundInstance sound_instance)
    {
        SoundSystem* sound = g_SoundSystem;
        sound_instance->m_Playing = 0;
        dmSoundCodec::Reset(sound->m_CodecContext, sound_instance->m_Decoder);
        return RESULT_OK;
    }

    bool IsPlaying(HSoundInstance sound_instance)
    {
        return sound_instance->m_Playing; // && !sound_instance->m_EndOfStream;
    }

    Result SetLooping(HSoundInstance sound_instance, bool looping)
    {
        sound_instance->m_Looping = (uint32_t) looping;
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
            default:
                dmLogError("Invalid parameter: %d\n", parameter);
                return RESULT_INVALID_PROPERTY;
        }
        return RESULT_OK;
    }

    template <typename T, int offset, int scale>
    static void MixResampleUpMono(const MixContext* mix_context, SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count)
    {
        const uint32_t mask = (1U << RESAMPLE_FRACTION_BITS) - 1U;
        const float range_recip = 1.0f / mask; // TODO: Divide by (1 << RESAMPLE_FRACTION_BITS) OR (1 << RESAMPLE_FRACTION_BITS) - 1?

        uint32_t frac = instance->m_FrameFraction;
        uint32_t prev_index = 0;
        uint32_t index = 0;
        uint64_t tmp = (((uint64_t) rate) << RESAMPLE_FRACTION_BITS) / mix_rate;
        uint32_t delta = (uint32_t) tmp;

        T* frames = (T*) instance->m_Frames;

        // Typically when the buffer is less than a mix-buffer we might overfetch
        // We never overfetch for identity mixing as identity mixing is a special case
        frames[instance->m_FrameCount] = frames[instance->m_FrameCount-1];

        Ramp ramp = GetRamp(mix_context, &instance->m_Gain, mix_buffer_count);
        for (int i = 0; i < mix_buffer_count; i++)
        {
            float gain = ramp.GetValue(i);
            float mix = frac * range_recip;
            T s1 = frames[index];
            T s2 = frames[index + 1];
            s1 = (s1 - offset) * scale;
            s2 = (s2 - offset) * scale;

            float s = (1.0f - mix) * s1 + mix * s2;
            mix_buffer[2 * i] += s * gain;
            mix_buffer[2 * i + 1] += s * gain;

            prev_index = index;
            frac += delta;
            index += frac >> RESAMPLE_FRACTION_BITS;

            frac &= ((1U << RESAMPLE_FRACTION_BITS) - 1U);
        }
        instance->m_FrameFraction = frac;

        assert(prev_index <= instance->m_FrameCount);

        memmove(instance->m_Frames, (char*) instance->m_Frames + index * sizeof(T), (instance->m_FrameCount - index) * sizeof(T));
        instance->m_FrameCount -= index;
    }

    template <typename T, int offset, int scale>
    static void MixResampleUpStereo(const MixContext* mix_context, SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count)
    {
        const uint32_t mask = (1U << RESAMPLE_FRACTION_BITS) - 1U;
        const float range_recip = 1.0f / mask; // TODO: Divide by (1 << RESAMPLE_FRACTION_BITS) OR (1 << RESAMPLE_FRACTION_BITS) - 1?

        uint32_t frac = instance->m_FrameFraction;
        uint32_t prev_index = 0;
        uint32_t index = 0;
        uint64_t tmp = (((uint64_t) rate) << RESAMPLE_FRACTION_BITS) / mix_rate;
        uint32_t delta = (uint32_t) tmp;

        T* frames = (T*) instance->m_Frames;

        // Typically when the buffer is less than a mix-buffer we might overfetch
        // We never overfetch for identity mixing as identity mixing is a special case
        frames[2 * instance->m_FrameCount] = frames[2 * instance->m_FrameCount - 2];
        frames[2 * instance->m_FrameCount + 1] = frames[2 * instance->m_FrameCount - 1];

        Ramp ramp = GetRamp(mix_context, &instance->m_Gain, mix_buffer_count);
        for (int i = 0; i < mix_buffer_count; i++)
        {
            float gain = ramp.GetValue(i);
            float mix = frac * range_recip;
            T sl1 = frames[2 * index];
            T sl2 = frames[2 * index + 2];
            sl1 = (sl1 - offset) * scale;
            sl2 = (sl2 - offset) * scale;

            T sr1 = frames[2 * index + 1];
            T sr2 = frames[2 * index + 3];
            sr1 = (sr1 - offset) * scale;
            sr2 = (sr2 - offset) * scale;

            float sl = (1.0f - mix) * sl1 + mix * sl2;
            float sr = (1.0f - mix) * sr1 + mix * sr2;
            mix_buffer[2 * i] += sl * gain;
            mix_buffer[2 * i + 1] += sr * gain;

            prev_index = index;
            frac += delta;
            index += frac >> RESAMPLE_FRACTION_BITS;

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
        assert(instance->m_FrameCount == mix_buffer_count);
        T* frames = (T*) instance->m_Frames;
        Ramp ramp = GetRamp(mix_context, &instance->m_Gain, mix_buffer_count);
        for (int i = 0; i < mix_buffer_count; i++)
        {
            float gain = ramp.GetValue(i);
            float s = frames[i];
            s = (s - offset) * scale * gain;
            mix_buffer[2 * i] += s;
            mix_buffer[2 * i + 1] += s;
        }
        instance->m_FrameCount -= mix_buffer_count;
    }

    template <typename T, int offset, int scale>
    static void MixResampleIdentityStereo(const MixContext* mix_context, SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count)
    {
        assert(instance->m_FrameCount == mix_buffer_count);
        T* frames = (T*) instance->m_Frames;
        Ramp ramp = GetRamp(mix_context, &instance->m_Gain, mix_buffer_count);
        for (int i = 0; i < mix_buffer_count; i++)
        {
            float gain = ramp.GetValue(i);
            float s1 = frames[2 * i];
            float s2 = frames[2 * i + 1];
            s1 = (s1 - offset) * scale * gain;
            s2 = (s2 - offset) * scale * gain;
            mix_buffer[2 * i] += s1;
            mix_buffer[2 * i + 1] += s2;
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

        if (rate == mix_rate) {
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
        DM_PROFILE(Sound, "Mix")

        SoundSystem* sound = g_SoundSystem;
        uint64_t delta = (uint32_t) ((((uint64_t) info->m_Rate) << RESAMPLE_FRACTION_BITS) / sound->m_MixRate);
        uint32_t mix_count = ((uint64_t) (instance->m_FrameCount) << RESAMPLE_FRACTION_BITS) / delta;
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
        if (info.m_BitsPerSample == 16 && info.m_Channels > SOUND_MAX_MIX_CHANNELS ) {
            dmLogError("Only mono/stereo with 16 bits per sample is supported");
            return;
        }

        if (info.m_Rate > sound->m_MixRate) {
            dmLogError("Sounds with rate higher than sample-rate not supported (%d > %d)", info.m_Rate, sound->m_MixRate);
            return;
        }

        bool is_muted = dmSound::IsMuted(instance);

        dmSoundCodec::Result r = dmSoundCodec::RESULT_OK;

        if (instance->m_FrameCount < sound->m_FrameCount && instance->m_Playing) {

            const uint32_t stride = info.m_Channels * (info.m_BitsPerSample / 8);
            uint32_t n = sound->m_FrameCount - instance->m_FrameCount;

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

            if (instance->m_FrameCount < sound->m_FrameCount) {

                if (instance->m_Looping) {
                    dmSoundCodec::Reset(sound->m_CodecContext, instance->m_Decoder);

                    uint32_t n = sound->m_FrameCount - instance->m_FrameCount;
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
                    instance->m_EndOfStream = 1;
                }
            }
        }

        if (r != dmSoundCodec::RESULT_OK) {
            dmhash_t hash = sound->m_SoundData[instance->m_SoundDataIndex].m_NameHash;
            dmLogWarning("Unable to decode file '%s'. Result %d", dmHashReverseSafe64(hash), r);

            instance->m_Playing = 0;
            return;
        }

        Mix(mix_context, instance, &info);

        if (instance->m_FrameCount <= 1 && instance->m_EndOfStream) {
            // NOTE: Due to round-off errors, e.g 32000 -> 44100,
            // the last frame might be partially sampled and
            // used in the *next* buffer. We truncate such scenarios to 0
            instance->m_FrameCount = 0;
        }
    }

    static Result MixInstances(const MixContext* mix_context) {
        DM_PROFILE(Sound, "MixInstances")
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

        uint32_t num_playing = 0;
        uint32_t instances = sound->m_Instances.Size();
        for (uint32_t i = 0; i < instances; ++i) {
            SoundInstance* instance = &sound->m_Instances[i];
            if (instance->m_Playing || instance->m_FrameCount > 0)
            {
                MixInstance(mix_context, instance);
                num_playing += dmSound::IsMuted(instance) ? 0 : 1;
            }

            if (instance->m_EndOfStream && instance->m_FrameCount == 0) {
                instance->m_Playing = 0;
            }

        }

        return num_playing > 0 ? RESULT_OK : RESULT_NOTHING_TO_PLAY;
    }

    static void Master(const MixContext* mix_context) {
        DM_PROFILE(Sound, "Master")

        SoundSystem* sound = g_SoundSystem;
        uint32_t n = sound->m_FrameCount;
        int16_t* out = sound->m_OutBuffers[sound->m_NextOutBuffer];
        int* master_index = sound->m_GroupMap.Get(MASTER_GROUP_HASH);
        SoundGroup* master = &sound->m_Groups[*master_index];
        float* mix_buffer = master->m_MixBuffer;

        for (uint32_t i = 0; i < MAX_GROUPS; i++) {
            SoundGroup* g = &sound->m_Groups[i];
            Ramp ramp = GetRamp(mix_context, &g->m_Gain, n);
            if (g->m_MixBuffer && g->m_NameHash != MASTER_GROUP_HASH) {
                for (uint32_t i = 0; i < n; i++) {
                    float gain = ramp.GetValue(i);
                    gain = dmMath::Clamp(gain, 0.0f, 1.0f);

                    float s1 = g->m_MixBuffer[2 * i];
                    float s2 = g->m_MixBuffer[2 * i + 1];
                    mix_buffer[2 * i] += s1 * gain;
                    mix_buffer[2 * i + 1] += s2 * gain;
                }
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
            }
        }
    }

    Result Update()
    {
        DM_PROFILE(Sound, "Update")
        SoundSystem* sound = g_SoundSystem;

        bool currentIsPhoneCallActive = IsPhoneCallActive();
        if (!sound->m_IsPhoneCallActive && currentIsPhoneCallActive)
        {
            sound->m_IsPhoneCallActive = true;
            sound->m_DeviceType->m_DeviceStop(sound->m_Device);
        }
        else if (sound->m_IsPhoneCallActive && !currentIsPhoneCallActive)
        {
            sound->m_IsPhoneCallActive = false;
            sound->m_DeviceType->m_DeviceRestart(sound->m_Device);
            (void) PlatformAcquireAudioFocus();
        }

        if (sound->m_IsPhoneCallActive)
        {
            // We can't play sounds when the phone is active.
            return RESULT_OK;
        }

        if (!sound->m_IsSoundActive && sound->m_InstancesPool.Size() == 0)
        {
            return RESULT_NOTHING_TO_PLAY;
        }

        uint32_t free_slots = sound->m_DeviceType->m_FreeBufferSlots(sound->m_Device);
        if (free_slots > 0) {
            StepGroupValues();
            StepInstanceValues();
        }

        uint32_t current_buffer = 0;
        uint32_t total_buffers = free_slots;
        while (free_slots > 0) {
            MixContext mix_context(current_buffer, total_buffers);
            Result result = MixInstances(&mix_context);

            // DEF-3130 Don't request the audio focus when we know nothing it being played
            // This allows the client to check for sound.is_music_playing() and mute sounds accordingly
            if (result == RESULT_OK && sound->m_IsSoundActive == false)
            {
                sound->m_IsSoundActive = true;
                (void) PlatformAcquireAudioFocus();
            }

            Master(&mix_context);

            // DEF-2540: By not feeding the sound device, you'll get more slots free,
            // thus updating sound (redundantly) every call, resulting in a huge performance hit.
            // Also, you'll fast forward the sounds.
            sound->m_DeviceType->m_Queue(sound->m_Device, (const int16_t*) sound->m_OutBuffers[sound->m_NextOutBuffer], sound->m_FrameCount);

            sound->m_NextOutBuffer = (sound->m_NextOutBuffer + 1) % SOUND_OUTBUFFER_COUNT;
            current_buffer++;
            free_slots--;
        }

        if (sound->m_IsSoundActive == true && sound->m_InstancesPool.Size() == 0)
        {
            sound->m_IsSoundActive = false;
            (void) PlatformReleaseAudioFocus();
        }

        return RESULT_OK;
    }

    bool IsMusicPlaying()
    {
        return PlatformIsMusicPlaying();
    }

    bool IsPhoneCallActive()
    {
        return PlatformIsPhoneCallActive();
    }

}

