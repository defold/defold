#include <string.h>

#include <SLES/OpenSLES.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/index_pool.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include <dlib/mutex.h>

#include "sound.h"
#include "sound_codec.h"
#include "sound_private.h"

namespace dmSound
{
    const uint32_t NUM_BUFFERS = 2;

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

        SLObjectItf      m_Player;
        SLPlayItf        m_Play;
        SLBufferQueueItf m_BufferQueue;
        SLVolumeItf      m_Volume;

        dmSoundCodec::HDecoder m_Decoder;

        void*           m_Buffers[NUM_BUFFERS];
        uint32_t        m_NextBuffer;

        uint32_t        m_Looping : 1;
        uint32_t        m_EndOfStream : 1;
        uint32_t        m_Playing : 1;
    };

    struct SoundSystem
    {
        dmSoundCodec::HCodecContext   m_Codec;
        dmArray<SoundInstance> m_Instances;
        dmIndexPool16          m_InstancesPool;

        dmArray<SoundData>     m_SoundData;
        dmIndexPool16          m_SoundDataPool;

        SLObjectItf            m_SL;
        SLEngineItf            m_Engine;
        SLObjectItf            m_OutputMix;

        uint32_t               m_BufferSize;
        //void*                  m_TempBuffer;
        float                  m_MasterGain;

        Stats                  m_Stats;
    };

    SoundSystem* g_SoundSystem = 0;

    static void CheckAndPrintError(SLresult res)
    {
        if (res != SL_RESULT_SUCCESS)
        {
            dmLogError("OpenSL error: %d", res);
            assert(0);
        }
    }

    Result Initialize(dmConfigFile::HConfig config, const InitializeParams* params)
    {
        Result r = PlatformInitialize(config, params);
        if (r != RESULT_OK) {
            return r;
        }

        SLObjectItf sl = 0;
        SLEngineItf engine = 0;
        SLObjectItf output_mix = 0;

        SLEngineOption options[] = {
                { (SLuint32) SL_ENGINEOPTION_THREADSAFE,
                  (SLuint32) SL_BOOLEAN_FALSE }
        };

        SLresult res = slCreateEngine(&sl, 1, options, 0, NULL, NULL);
        CheckAndPrintError(res);

        res = (*sl)->Realize(sl, SL_BOOLEAN_FALSE);
        CheckAndPrintError(res);

        res = (*sl)->GetInterface(sl, SL_IID_ENGINE, &engine);
        CheckAndPrintError(res);

        const SLboolean required[] = { };
        SLInterfaceID ids[] = { };
        res = (*engine)->CreateOutputMix(engine, &output_mix, 0, ids, required);
        CheckAndPrintError(res);

        res = (*output_mix)->Realize(output_mix, SL_BOOLEAN_FALSE);
        CheckAndPrintError(res);

        float master_gain = params->m_MasterGain;

        g_SoundSystem = new SoundSystem();
        SoundSystem* sound = g_SoundSystem;
        dmSoundCodec::NewCodecContextParams codec_params;
        codec_params.m_MaxDecoders = params->m_MaxInstances;
        sound->m_Codec = dmSoundCodec::New(&codec_params);
        sound->m_SL = sl;
        sound->m_Engine = engine;
        sound->m_OutputMix = output_mix;

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
            instance->m_NextBuffer = 0;

            for (uint32_t j = 0; j < NUM_BUFFERS; ++j) {
                instance->m_Buffers[j] = malloc(params->m_BufferSize);
            }
        }

        sound->m_SoundData.SetCapacity(max_sound_data);
        sound->m_SoundData.SetSize(max_sound_data);
        sound->m_SoundDataPool.SetCapacity(max_sound_data);
        for (uint32_t i = 0; i < max_sound_data; ++i)
        {
            sound->m_SoundData[i].m_Index = 0xffff;
        }

        sound->m_MasterGain = master_gain;
        sound->m_BufferSize = params->m_BufferSize;
        //sound->m_TempBuffer = malloc(params->m_BufferSize);

        memset(&g_SoundSystem->m_Stats, 0, sizeof(g_SoundSystem->m_Stats));

        return RESULT_OK;    }

    Result Finalize()
    {
        PlatformFinalize();

        Result result = RESULT_OK;

        if (g_SoundSystem)
        {
            SoundSystem* sound = g_SoundSystem;
            (*sound->m_OutputMix)->Destroy(sound->m_OutputMix);
            (*sound->m_SL)->Destroy(sound->m_SL);
            dmSoundCodec::Delete(sound->m_Codec);
            //free(sound->m_TempBuffer);

            for (uint32_t i = 0; i < sound->m_Instances.Size(); ++i)
            {
                SoundInstance* instance = &sound->m_Instances[i];
                memset(instance, 0, sizeof(*instance));
                instance->m_Index = 0xffff;
                instance->m_SoundDataIndex = 0xffff;
                instance->m_NextBuffer = 0;

                for (uint32_t j = 0; j < NUM_BUFFERS; ++j) {
                    free(instance->m_Buffers[j]);
                }
            }

            delete sound;
            g_SoundSystem = 0;
        }

        return result;
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

    dmMutex::Mutex g_Hack = dmMutex::New();

    static void BufferQueueCallback(SLBufferQueueItf queueItf, void *pContext)
    {
        //DM_PROFILE(Sound, "Buffer")

        //DM_MUTEX_SCOPED_LOCK(g_Hack);

        SLresult res;
        SoundSystem* sound = g_SoundSystem;
        SoundInstance* instance = (SoundInstance *) pContext;
        assert(instance->m_Index != 0xffff);

        uint32_t decoded;
        uint32_t total = 0;
        dmSoundCodec::Result r;
        char* buf = (char*) instance->m_Buffers[instance->m_NextBuffer % NUM_BUFFERS];
        instance->m_NextBuffer++;
        bool decode;

        printf("%x, %x, %x\n", instance, queueItf, instance->m_Decoder);

        do {
            do {
                r = dmSoundCodec::Decode(sound->m_Codec, instance->m_Decoder, buf + total, sound->m_BufferSize - total, &decoded);
                if (r == dmSoundCodec::RESULT_OK) {
                    total += decoded;
                } else {
                    decoded = 0;
                }

            } while (r == dmSoundCodec::RESULT_OK && decoded > 0);

            if (r == dmSoundCodec::RESULT_OK &&
                decoded == 0 &&
                total < sound->m_BufferSize &&
                instance->m_Looping) {
                decode = true;
                dmSoundCodec::Reset(sound->m_Codec, instance->m_Decoder);

            } else {
                decode = false;
            }

        } while (decode);

        if (total == 0 && !instance->m_Looping) {
            instance->m_EndOfStream = 1;
        }

        if (total > 0) {
            res = (*queueItf)->Enqueue(queueItf, buf, total);
            CheckAndPrintError(res);
        }
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

        dmSoundCodec::Result r = dmSoundCodec::NewDecoder(ss->m_Codec, codec_format, sound_data->m_Data, sound_data->m_Size, &decoder);
        if (r != dmSoundCodec::RESULT_OK) {
            dmLogError("Failed to decode sound (%d)", r);
            dmSoundCodec::DeleteDecoder(ss->m_Codec, decoder);
            return RESULT_INVALID_STREAM_DATA;
        }

        dmSoundCodec::Info info;
        dmSoundCodec::GetInfo(ss->m_Codec, decoder, &info);

        SLresult res;
        SLEngineItf engine = ss->m_Engine;

        SLObjectItf player = 0;
        SLPlayItf play = 0;
        SLBufferQueueItf buffer_queue = 0;
        SLVolumeItf volume = 0;

        SLDataLocator_BufferQueue locator;
        locator.locatorType = SL_DATALOCATOR_BUFFERQUEUE;
        locator.numBuffers = NUM_BUFFERS;

        SLDataFormat_PCM format;
        format.formatType = SL_DATAFORMAT_PCM;
        format.numChannels = info.m_Channels;
        format.samplesPerSec = info.m_Rate * 1000;
        format.bitsPerSample = info.m_BitsPerSample;
        // TODO: Correct?
        format.containerSize = info.m_BitsPerSample;
        // NOTE: It seems that 0 is the correct default and not SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT
        // The latter results in error for mono sounds
        format.channelMask = 0;
        format.endianness = SL_BYTEORDER_LITTLEENDIAN;

        SLDataSource data_source;
        data_source.pFormat = &format;
        data_source.pLocator = &locator;

        SLDataLocator_OutputMix locator_out_mix;
        locator_out_mix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
        locator_out_mix.outputMix = ss->m_OutputMix;

        SLDataSink sink;
        sink.pFormat = NULL;
        sink.pLocator = &locator_out_mix;

        const SLboolean required[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
        SLInterfaceID ids[] = {SL_IID_VOLUME, SL_IID_BUFFERQUEUE};

        assert(sizeof(required) == sizeof(ids));

        res = (*engine)->CreateAudioPlayer(engine, &player, &data_source, &sink, sizeof(required) / sizeof(required[0]), ids, required);
        if (res != SL_RESULT_SUCCESS) {
            dmLogError("Failed to create player: %d", res);
            dmSoundCodec::DeleteDecoder(ss->m_Codec, decoder);
            return RESULT_UNKNOWN_ERROR;
        }

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

        res = (*player)->Realize(player, SL_BOOLEAN_FALSE);
        CheckAndPrintError(res);
        res = (*player)->GetInterface(player, SL_IID_PLAY, (void*)&play);
        CheckAndPrintError(res);
        res = (*player)->GetInterface(player, SL_IID_BUFFERQUEUE, (void*)&buffer_queue);
        CheckAndPrintError(res);
        res = (*buffer_queue)->RegisterCallback(buffer_queue, BufferQueueCallback, si);
        CheckAndPrintError(res);

        res = (*player)->GetInterface(player, SL_IID_VOLUME, (void*)&volume);
        CheckAndPrintError(res);

        si->m_Play = play;
        si->m_Player = player;
        si->m_BufferQueue = buffer_queue;
        si->m_Volume = volume;

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
        dmSoundCodec::DeleteDecoder(sound->m_Codec, sound_instance->m_Decoder);
        sound_instance->m_Decoder = 0;

        SLObjectItf player = sound_instance->m_Player;
        (*player)->Destroy(player);

        sound_instance->m_Player = 0;
        sound_instance->m_Play = 0;
        sound_instance->m_BufferQueue = 0;
        sound_instance->m_Volume = 0;

        return RESULT_OK;
    }

    Result Update()
    {
        DM_PROFILE(Sound, "Update")
        return RESULT_OK;
    }

    Result Play(HSoundInstance sound_instance)
    {
        BufferQueueCallback(sound_instance->m_BufferQueue, sound_instance);

        SLPlayItf play = sound_instance->m_Play;
        SLresult res = (*play)->SetPlayState(play, SL_PLAYSTATE_PLAYING);
        CheckAndPrintError(res);

        SLVolumeItf volume = sound_instance->m_Volume;

        SoundSystem* sound = g_SoundSystem;

        // Clamp to something reasonable, [-8000, 0]. SL_MILLIBEL_MIN is defined to -32768
        float gain = dmMath::Max(0.0001f, sound_instance->m_Gain * sound->m_MasterGain);
        // NOTE: According to documentation volume level should be specfied
        // in millibels but it seems to be a bug and centibels is probably the
        // correct unit
        float mdb = 100 * 20 * log(gain) / log(10);
        res = (*volume)->SetVolumeLevel(volume, (int) mdb);
        CheckAndPrintError(res);

        sound_instance->m_Playing = 1;
        return RESULT_OK;
    }

    Result Stop(HSoundInstance sound_instance)
    {
        SLPlayItf play = sound_instance->m_Play;
        SLresult res = (*play)->SetPlayState(play, SL_PLAYSTATE_STOPPED);
        CheckAndPrintError(res);

        SLBufferQueueItf buffer_queue = sound_instance->m_BufferQueue;
        (*buffer_queue)->Clear(buffer_queue);

        sound_instance->m_Playing = 0;
        return RESULT_OK;
    }

    bool IsPlaying(HSoundInstance sound_instance)
    {
        return sound_instance->m_Playing && !sound_instance->m_EndOfStream;
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

    Result GetParameter(HSoundInstance sound_instance, Parameter parameter, Vector4& value)
    {
        switch(parameter)
        {
            case PARAMETER_GAIN:
                value = Vector4(sound_instance->m_Gain, 0, 0, 0);
                break;
            default:
                dmLogError("Invalid parameter: %d\n", parameter);
                return RESULT_INVALID_PROPERTY;
        }
        return RESULT_OK;
    }

    bool IsMusicPlaying()
    {
        return false;
    }
}
