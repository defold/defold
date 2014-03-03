#include <stdint.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include "sound.h"
#include "sound2.h"
#include "sound_codec.h"
#include "sound_private.h"


/**
 * Defold simple sound system
 * NOTE: Must units is in frames, i.e a sample in time with N channels
 *
 */
namespace dmSound
{
    #define SOUND_MAX_MIX_CHANNELS (2)
    #define SOUND_OUTBUFFER_COUNT (2)

    // TODO: How many bits?
    const uint32_t RESAMPLE_FRACTION_BITS = 31;

    struct SoundData
    {
        SoundDataType m_Type;
        void*         m_Data;
        int           m_Size;
        // Index in m_SoundData
        uint16_t      m_Index;
    };

    struct SoundInstance
    {
        uint16_t        m_Index;
        uint16_t        m_SoundDataIndex;
        float           m_Gain;

        dmSoundCodec::HDecoder m_Decoder;
        void*           m_Frames;
        uint32_t        m_FrameCount;
        uint32_t        m_FrameFraction;


        uint32_t        m_Looping : 1;
        uint32_t        m_EndOfStream : 1;
        uint32_t        m_Playing : 1;
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

        float                   m_MasterGain;
        Stats                   m_Stats;

        uint32_t                m_MixRate;
        uint32_t                m_FrameCount;
        float*                  m_MixBuffer;

        int16_t*                m_OutBuffers[SOUND_OUTBUFFER_COUNT];
        uint16_t                m_NextOutBuffer;
    };

    SoundSystem* g_SoundSystem = 0;

    DeviceType* g_FirstDevice = 0;

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
            return r;
        }

        DeviceInfo device_info;
        device_type->m_DeviceInfo(device, &device_info);

        float master_gain = params->m_MasterGain;

        g_SoundSystem = new SoundSystem();
        SoundSystem* sound = g_SoundSystem;
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
            max_buffers = (uint32_t) dmConfigFile::GetInt(config, "sound.max_buffers", (int32_t) max_buffers);
            max_sources = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sources", (int32_t) max_sources);
            max_instances = (uint32_t) dmConfigFile::GetInt(config, "sound.max_instances", (int32_t) max_instances);
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

        sound->m_MasterGain = master_gain;
        sound->m_MixRate = device_info.m_MixRate;
        sound->m_FrameCount = params->m_FrameCount;
        // TODO: Should we really use buffer-size in bytes?
        // Change two number of frames instead and allocated
        // space required for stereo buffers?
        sound->m_MixBuffer = (float*) malloc(params->m_FrameCount * sizeof(float) * SOUND_MAX_MIX_CHANNELS);
        for (int i = 0; i < SOUND_OUTBUFFER_COUNT; ++i) {
            sound->m_OutBuffers[i] = (int16_t*) malloc(params->m_FrameCount * sizeof(int16_t) * SOUND_MAX_MIX_CHANNELS);
        }

        memset(&g_SoundSystem->m_Stats, 0, sizeof(g_SoundSystem->m_Stats));

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
                memset(instance, 0, sizeof(*instance));
                instance->m_Index = 0xffff;
                instance->m_SoundDataIndex = 0xffff;
                free(instance->m_Frames);
            }

            free((void*) sound->m_MixBuffer);

            for (int i = 0; i < SOUND_OUTBUFFER_COUNT; ++i) {
                free((void*) sound->m_OutBuffers[i]);
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

    Result NewSoundData(const void* sound_buffer, uint32_t sound_buffer_size, SoundDataType type, HSoundData* sound_data)
    {
        SoundSystem* sound = g_SoundSystem;

        if (sound->m_SoundDataPool.Remaining() == 0)
        {
            *sound_data = 0;
            return RESULT_OUT_OF_INSTANCES;
        }
        uint16_t index = sound->m_SoundDataPool.Pop();

        SoundData* sd = &sound->m_SoundData[index];
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
            return RESULT_OUT_OF_INSTANCES;
        }

        dmSoundCodec::HDecoder decoder;

        dmSoundCodec::Format codec_format;
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
            dmSoundCodec::DeleteDecoder(ss->m_CodecContext, decoder);
            return RESULT_INVALID_STREAM_DATA;
        }

        dmSoundCodec::Info info;
        dmSoundCodec::GetInfo(ss->m_CodecContext, decoder, &info);

        uint16_t index = ss->m_InstancesPool.Pop();
        SoundInstance* si = &ss->m_Instances[index];
        assert(si->m_Index == 0xffff);

        si->m_SoundDataIndex = sound_data->m_Index;
        si->m_Index = index;
        si->m_Gain = 1.0f;
        si->m_Looping = 0;
        si->m_EndOfStream = 0;
        si->m_Playing = 0;
        si->m_Decoder = decoder;

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
        switch(parameter)
        {
            case PARAMETER_GAIN:
                sound_instance->m_Gain = dmMath::Max(0.0f, value.getX());
                break;
            default:
                dmLogError("Invalid parameter: %d\n", parameter);
                return RESULT_INVALID_PROPERTY;
        }
        return RESULT_OK;
    }

    template <typename T, int offset, int scale>
    static void MixResampleUpMono(SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count)
    {
        const uint32_t mask = (1 << RESAMPLE_FRACTION_BITS) - 1;
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

        float gain = instance->m_Gain;
        for (int i = 0; i < mix_buffer_count; i++)
        {
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

            frac &= ((1 << RESAMPLE_FRACTION_BITS) - 1);
        }
        instance->m_FrameFraction = frac;

        assert(prev_index < instance->m_FrameCount);

        memmove(instance->m_Frames, (char*) instance->m_Frames + index * sizeof(T), (instance->m_FrameCount - index) * sizeof(T));
        instance->m_FrameCount -= index;
    }

    template <typename T, int offset, int scale>
    static void MixResampleUpStereo(SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count)
    {
        const uint32_t mask = (1 << RESAMPLE_FRACTION_BITS) - 1;
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

        float gain = instance->m_Gain;
        for (int i = 0; i < mix_buffer_count; i++)
        {
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

            frac &= ((1 << RESAMPLE_FRACTION_BITS) - 1);
        }
        instance->m_FrameFraction = frac;

        assert(prev_index < instance->m_FrameCount);

        memmove(instance->m_Frames, (char*) instance->m_Frames + index * sizeof(T) * 2, (instance->m_FrameCount - index) * sizeof(T) * 2);
        instance->m_FrameCount -= index;
    }

    template <typename T, int offset, int scale>
    static void MixResampleIdentityMono(SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count)
    {
        assert(instance->m_FrameCount == mix_buffer_count);
        T* frames = (T*) instance->m_Frames;
        float gain = instance->m_Gain;
        for (int i = 0; i < mix_buffer_count; i++)
        {
            float s = frames[i];
            s = (s - offset) * scale * gain;
            mix_buffer[2 * i] += s;
            mix_buffer[2 * i + 1] += s;
        }
        instance->m_FrameCount -= mix_buffer_count;
    }

    template <typename T, int offset, int scale>
    static void MixResampleIdentityStereo(SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count)
    {
        assert(instance->m_FrameCount == mix_buffer_count);
        T* frames = (T*) instance->m_Frames;
        float gain = instance->m_Gain;
        for (int i = 0; i < mix_buffer_count; i++)
        {
            float s1 = frames[2 * i];
            float s2 = frames[2 * i + 1];
            s1 = (s1 - offset) * scale * gain;
            s2 = (s2 - offset) * scale * gain;
            mix_buffer[2 * i] += s1;
            mix_buffer[2 * i + 1] += s2;
        }
        instance->m_FrameCount -= mix_buffer_count;
    }

    typedef void (*MixerFunction)(SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count);

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

    static void MixResample(SoundInstance* instance, const dmSoundCodec::Info* info, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count)
    {
        const uint32_t rate = info->m_Rate;
        assert(rate <= mix_rate);

        void (*mixer)(SoundInstance* instance, uint32_t rate, uint32_t mix_rate, float* mix_buffer, uint32_t mix_buffer_count) = 0;

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
        mixer(instance, rate, mix_rate, mix_buffer, mix_buffer_count);
    }

    static void Mix(SoundInstance* instance, const dmSoundCodec::Info* info)
    {
        DM_PROFILE(Sound, "Mix")

        SoundSystem* sound = g_SoundSystem;
        uint64_t delta = (uint32_t) ((((uint64_t) info->m_Rate) << RESAMPLE_FRACTION_BITS) / sound->m_MixRate);
        uint32_t mix_count = ((uint64_t) (instance->m_FrameCount) << RESAMPLE_FRACTION_BITS) / delta;
        mix_count = dmMath::Min(mix_count, sound->m_FrameCount);
        assert(mix_count <= sound->m_FrameCount);
        MixResample(instance, info, sound->m_MixRate, sound->m_MixBuffer, mix_count);
    }

    static void MixInstance(SoundInstance* instance) {
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

        dmSoundCodec::Result r = dmSoundCodec::RESULT_OK;

        if (instance->m_FrameCount < sound->m_FrameCount) {
            assert(instance->m_FrameCount < sound->m_FrameCount);
            const uint32_t stride = info.m_Channels * (info.m_BitsPerSample / 8);

            uint32_t n = sound->m_FrameCount - instance->m_FrameCount;
            r = dmSoundCodec::Decode(sound->m_CodecContext,
                                     instance->m_Decoder,
                                     ((char*) instance->m_Frames) + instance->m_FrameCount * stride,
                                     n * stride,
                                     &decoded);

            assert(decoded % stride == 0);
            instance->m_FrameCount += decoded / stride;

            if (instance->m_FrameCount < sound->m_FrameCount) {

                if (instance->m_Looping) {
                    dmSoundCodec::Reset(sound->m_CodecContext, instance->m_Decoder);

                    uint32_t n = sound->m_FrameCount - instance->m_FrameCount;
                    r = dmSoundCodec::Decode(sound->m_CodecContext,
                                             instance->m_Decoder,
                                             ((char*) instance->m_Frames) + instance->m_FrameCount * stride,
                                             n * stride,
                                             &decoded);

                    assert(decoded % stride == 0);
                    instance->m_FrameCount += decoded / stride;

                } else {
                    instance->m_EndOfStream = 1;
                }
            }
        }

        if (r != dmSoundCodec::RESULT_OK) {
            dmLogWarning("Unable to decode (%d)", r);
            instance->m_Playing = 0;
            return;
        }

        Mix(instance, &info);

        if (instance->m_FrameCount <= 1 && instance->m_EndOfStream) {
            // NOTE: Due to round-off errors, e.g 32000 -> 44100,
            // the last frame might be partially sampled and
            // used in the *next* buffer. We truncate such scenarios to 0
            instance->m_FrameCount = 0;
        }
    }

    static Result MixInstances() {
        SoundSystem* sound = g_SoundSystem;

        memset(sound->m_MixBuffer, 0, sound->m_FrameCount * sizeof(float) * 2);

        uint32_t instances = sound->m_Instances.Size();
        for (uint32_t i = 0; i < instances; ++i) {
            SoundInstance* instance = &sound->m_Instances[i];
            if (instance->m_Playing || instance->m_FrameCount > 0) {
                MixInstance(instance);
            }

            if (instance->m_EndOfStream && instance->m_FrameCount == 0) {
                instance->m_Playing = 0;
            }

        }

        return RESULT_OK;
    }

    void Master() {
        DM_PROFILE(Sound, "Master")

        SoundSystem* sound = g_SoundSystem;

        uint32_t n = sound->m_FrameCount;
        int16_t* out = sound->m_OutBuffers[sound->m_NextOutBuffer];
        const float master_gain = sound->m_MasterGain;
        float* mix_buffer = sound->m_MixBuffer;
        for (uint32_t i = 0; i < n; i++) {
            float s1 = mix_buffer[2 * i] * master_gain;
            float s2 = mix_buffer[2 * i + 1] * master_gain;
            s1 = dmMath::Min(32767.0f, s1);
            s1 = dmMath::Max(-32768.0f, s1);
            s2 = dmMath::Min(32767.0f, s2);
            s2 = dmMath::Max(-32768.0f, s2);
            out[2 * i] = (int16_t) s1;
            out[2 * i + 1] = (int16_t) s2;
        }
    }

    Result Update()
    {
        DM_PROFILE(Sound, "Update")
        SoundSystem* sound = g_SoundSystem;

        uint32_t free_slots = sound->m_DeviceType->m_FreeBufferSlots(sound->m_Device);

        while (free_slots > 0) {
            Result r = MixInstances();
            if (r != RESULT_OK) {
                return r;
            }
            Master();
            sound->m_DeviceType->m_Queue(sound->m_Device, (const int16_t*) sound->m_OutBuffers[sound->m_NextOutBuffer], sound->m_FrameCount);
            sound->m_NextOutBuffer = (sound->m_NextOutBuffer + 1) % SOUND_OUTBUFFER_COUNT;

            free_slots--;
        }

        return RESULT_OK;
    }

    bool IsMusicPlaying()
    {
        return PlatformIsMusicPlaying();
    }

}

