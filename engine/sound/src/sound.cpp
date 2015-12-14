#include <string.h>
#include <stdint.h>
#include <dlib/log.h>
#include <dlib/index_pool.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include "sound.h"
#include "sound_private.h"
#include "sound_codec.h"

#if defined(__MACH__)
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include "stb_vorbis/stb_vorbis.h"

namespace dmSound
{
    struct SoundData
    {
        SoundDataType m_Type;
        const void*   m_Data;

        // Index in m_SoundData
        uint16_t      m_Index;

        // AL_FORMAT_MONO8, AL_FORMAT_MONO16, AL_FORMAT_STEREO8, AL_FORMAT_STEREO16
        ALenum        m_Format;
        // Size in bytes
        ALsizei       m_Size;
        ALfloat       m_Frequency;
    };

    struct SoundInstance
    {
        uint32_t        m_CurrentBufferOffset;
        uint16_t        m_Index;
        uint16_t        m_SoundDataIndex;
        uint16_t        m_SourceIndex;
        uint16_t        m_BufferIndices[2];
        float           m_Gain;
        stb_vorbis*     m_StbVorbis;
        char*           m_VorbisBuffer;
        uint32_t        m_VorbisBufferSize;
        uint32_t        m_VorbisBufferOffset;
        uint32_t        m_Looping : 1;
        uint32_t        m_EndOfStream : 1;
    };

    struct SoundSystem
    {
        ALCdevice*             m_Device;
        ALCcontext*            m_Context;

        dmArray<SoundInstance> m_Instances;
        dmIndexPool16          m_InstancesPool;

        dmArray<SoundData>     m_SoundData;
        dmIndexPool16          m_SoundDataPool;

        dmArray<ALuint>        m_Buffers;
        dmIndexPool32          m_BuffersPool;

        dmArray<ALuint>        m_Sources;
        dmIndexPool16          m_SourcesPool;

        float                  m_MasterGain;
        uint32_t               m_BufferSize;
        void*                  m_TempBuffer;

        dmSoundCodec::HCodecContext m_CodecContext;

        Stats                  m_Stats;
    };

    SoundSystem* g_SoundSystem = 0;

    void CheckAndPrintError()
    {
        ALenum error = alGetError();
        if (error != AL_NO_ERROR)
        {
            dmLogError("%s", alGetString(error));
        }
    }

    Result Initialize(dmConfigFile::HConfig config, const InitializeParams* params)
    {
        Result r = PlatformInitialize(config, params);
        if (r != RESULT_OK) {
            return r;
        }


        ALCdevice* device;
        ALCcontext* context;

        device = alcOpenDevice(0);
        if (device == 0) {
            dmLogError("Failed to create OpenAL device");
            return RESULT_UNKNOWN_ERROR;
        }

        context = alcCreateContext(device, 0);
        if (context == 0) {
            dmLogError("Failed to create OpenAL context");
            alcCloseDevice (device);
            return RESULT_UNKNOWN_ERROR;
        }

        if (!alcMakeContextCurrent (context)) {
            dmLogError("Failed to make OpenAL context current");
            alcDestroyContext (context);
            alcCloseDevice (device);
            return RESULT_UNKNOWN_ERROR;
        }

        float master_gain = params->m_MasterGain;

        g_SoundSystem = new SoundSystem();
        SoundSystem* sound = g_SoundSystem;
        sound->m_Device = device;
        sound->m_Context = context;

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

        dmSoundCodec::NewCodecContextParams codec_params;
        codec_params.m_MaxDecoders = max_instances;
        sound->m_CodecContext = dmSoundCodec::New(&codec_params);

        sound->m_Instances.SetCapacity(max_instances);
        sound->m_Instances.SetSize(max_instances);
        sound->m_InstancesPool.SetCapacity(max_instances);
        for (uint32_t i = 0; i < max_instances; ++i)
        {
            sound->m_Instances[i].m_Index = 0xffff;
            sound->m_Instances[i].m_SoundDataIndex = 0xffff;
            sound->m_Instances[i].m_SourceIndex = 0xffff;
            sound->m_Instances[i].m_StbVorbis = 0;
            sound->m_Instances[i].m_VorbisBuffer = 0;
            sound->m_Instances[i].m_VorbisBufferSize = 0;
            sound->m_Instances[i].m_VorbisBufferOffset = 0;
        }

        sound->m_SoundData.SetCapacity(max_sound_data);
        sound->m_SoundData.SetSize(max_sound_data);
        sound->m_SoundDataPool.SetCapacity(max_sound_data);
        for (uint32_t i = 0; i < max_sound_data; ++i)
        {
            sound->m_SoundData[i].m_Index = 0xffff;
        }

        sound->m_Buffers.SetCapacity(max_buffers);
        sound->m_Buffers.SetSize(max_buffers);
        sound->m_BuffersPool.SetCapacity(max_buffers);

        sound->m_Sources.SetCapacity(max_sources);
        sound->m_Sources.SetSize(max_sources);
        sound->m_SourcesPool.SetCapacity(max_sources);

        sound->m_MasterGain = master_gain;
        sound->m_BufferSize = params->m_BufferSize;
        sound->m_TempBuffer = malloc(params->m_BufferSize);

        for (uint32_t i = 0; i < max_sources; ++i)
        {
            alGenSources (1, &sound->m_Sources[i]);
            CheckAndPrintError();
        }

        for (uint32_t i = 0; i < max_buffers; ++i)
        {
            alGenBuffers(1, &sound->m_Buffers[i]);
            CheckAndPrintError();
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
            if (sound->m_SoundDataPool.Size() > 0)
            {
                dmLogError("%d sound-data not deleted", sound->m_SoundDataPool.Size());
                result = RESULT_RESOURCE_LEAK;
            }

            if (sound->m_InstancesPool.Size() > 0)
            {
                dmLogError("%d sound-instances not deleted", sound->m_InstancesPool.Size());
                result = RESULT_RESOURCE_LEAK;
            }

            alSourceStopv(sound->m_Sources.Size(), &sound->m_Sources[0]);
            for (uint32_t i = 0; i < sound->m_Sources.Size(); ++i)
            {
                alSourcei(sound->m_Sources[i], AL_BUFFER, AL_NONE);
            }
            alDeleteSources(sound->m_Sources.Size(), &sound->m_Sources[0]);

            if (alcMakeContextCurrent(0)) {
                alcDestroyContext(sound->m_Context);
                if (!alcCloseDevice(sound->m_Device)) {
                    dmLogError("Failed to close OpenAL device");
                }
            } else {
                dmLogError("Failed to make OpenAL context current");
            }

            free(sound->m_TempBuffer);
            dmSoundCodec::Delete(sound->m_CodecContext);
            delete sound;
            g_SoundSystem = 0;
        }
        return result;
    }

    void GetStats(Stats* stats)
    {
        *stats = g_SoundSystem->m_Stats;
    }

    Result SetSoundDataWav(HSoundData sound_data, const void* sound_buffer, uint32_t sound_buffer_size)
    {
        SoundSystem* sound = g_SoundSystem;

        dmSoundCodec::HDecoder decoder;
        dmSoundCodec::Result r = dmSoundCodec::NewDecoder(sound->m_CodecContext, dmSoundCodec::FORMAT_WAV, sound_buffer, sound_buffer_size, &decoder);
        if (r != dmSoundCodec::RESULT_OK) {
            return RESULT_UNKNOWN_ERROR;
        }

        dmSoundCodec::Info info;
        dmSoundCodec::GetInfo(sound->m_CodecContext, decoder, &info);

        uint32_t decoded;
        char* buffer = (char*) malloc(info.m_Size);
        r = dmSoundCodec::Decode(sound->m_CodecContext, decoder, buffer, info.m_Size, &decoded);
        if (r != dmSoundCodec::RESULT_OK) {
            free(buffer);
            dmSoundCodec::DeleteDecoder(sound->m_CodecContext, decoder);
            return RESULT_UNKNOWN_ERROR;
        }

        if (sound_data->m_Data != 0x0)
            free((void*)sound_data->m_Data);

        ALenum format;
        if (info.m_BitsPerSample == 8) {
            if (info.m_Channels == 1) {
                format = AL_FORMAT_MONO8;
            } else {
                format = AL_FORMAT_STEREO8;
            }
        } else {
            if (info.m_Channels == 1) {
                format = AL_FORMAT_MONO16;
            } else {
                format = AL_FORMAT_STEREO16;
            }
        }

        sound_data->m_Data = buffer;
        sound_data->m_Format = format;
        sound_data->m_Size = decoded;
        sound_data->m_Frequency = info.m_Rate;
        dmSoundCodec::DeleteDecoder(sound->m_CodecContext, decoder);

        return RESULT_OK;
    }

    Result SetSoundDataOggVorbis(HSoundData sound_data, const void* sound_buffer, uint32_t sound_buffer_size)
    {
        void* sound_buffer_copy = malloc(sound_buffer_size);
        if (!sound_buffer_copy)
        {
            return RESULT_OUT_OF_MEMORY;
        }
        memcpy(sound_buffer_copy, sound_buffer, sound_buffer_size);

        if (sound_data->m_Data != 0x0)
            free((void*)sound_data->m_Data);

        sound_data->m_Data = sound_buffer_copy;
        sound_data->m_Size = sound_buffer_size;

        int error;
        stb_vorbis* vorbis = stb_vorbis_open_memory((unsigned char*) sound_buffer_copy, sound_buffer_size, &error, NULL);
        if (vorbis)
        {
            stb_vorbis_info info = stb_vorbis_get_info(vorbis);
            stb_vorbis_close(vorbis);
            if (info.channels == 1)
            {
                sound_data->m_Format = AL_FORMAT_MONO16;
            }
            else if (info.channels == 2)
            {
                sound_data->m_Format = AL_FORMAT_STEREO16;
            }
            else
            {
                dmLogError("Unsupported channel count in ogg-vorbis stream: %d", info.channels);
                return RESULT_UNKNOWN_ERROR;
            }
            sound_data->m_Frequency = info.sample_rate;
        }
        else
        {
            free(sound_buffer_copy);
            return RESULT_INVALID_STREAM_DATA;
        }

        return RESULT_OK;
    }

    Result NewSoundData(const void* sound_buffer, uint32_t sound_buffer_size, SoundDataType type, HSoundData* sound_data)
    {
        SoundSystem* sound = g_SoundSystem;

        if (sound->m_SoundDataPool.Remaining() == 0)
        {
            dmLogWarning("Could not create new sound data, out of sound data pool (sound.max_sound_data: %d).", sound->m_SoundDataPool.Capacity());
            *sound_data = 0;
            return RESULT_OUT_OF_INSTANCES;
        }
        uint16_t index = sound->m_SoundDataPool.Pop();

        SoundData* sd = &sound->m_SoundData[index];
        sd->m_Type = type;
        sd->m_Index = index;
        sd->m_Data = 0x0;
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
        switch (sound_data->m_Type)
        {
            case SOUND_DATA_TYPE_WAV:
                return SetSoundDataWav(sound_data, sound_buffer, sound_buffer_size);
            case SOUND_DATA_TYPE_OGG_VORBIS:
                return SetSoundDataOggVorbis(sound_data, sound_buffer, sound_buffer_size);
            default:
                return RESULT_UNKNOWN_SOUND_TYPE;
        }
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

        uint16_t index = ss->m_InstancesPool.Pop();
        SoundInstance* si = &ss->m_Instances[index];
        assert(si->m_Index == 0xffff);

        si->m_CurrentBufferOffset = 0;
        si->m_SoundDataIndex = sound_data->m_Index;
        si->m_Index = index;
        si->m_SourceIndex = 0xffff;
        si->m_BufferIndices[0] = 0xffff;
        si->m_BufferIndices[1] = 0xffff;
        si->m_Gain = 1.0f;
        si->m_Looping = 0;
        si->m_EndOfStream = 0;

        if (sound_data->m_Type == SOUND_DATA_TYPE_OGG_VORBIS)
        {
            int error;
            si->m_StbVorbis = stb_vorbis_open_memory((unsigned char*) sound_data->m_Data, sound_data->m_Size, &error, NULL);
            assert(si->m_StbVorbis);
            si->m_VorbisBufferSize = ss->m_BufferSize;
            si->m_VorbisBufferOffset = 0;
            si->m_VorbisBuffer = (char*) malloc(si->m_VorbisBufferSize);
        }
        *sound_instance = si;

        return RESULT_OK;
    }

    Result DeleteSoundInstance(HSoundInstance sound_instance)
    {
        SoundSystem* sound = g_SoundSystem;
        if (sound_instance->m_SourceIndex != 0xffff)
        {
            ALuint source = sound->m_Sources[sound_instance->m_SourceIndex];
            alSourceStop(source);
            CheckAndPrintError();
            if (sound_instance->m_BufferIndices[0] != 0xffff)
                sound->m_BuffersPool.Push(sound_instance->m_BufferIndices[0]);
            if (sound_instance->m_BufferIndices[1] != 0xffff)
                sound->m_BuffersPool.Push(sound_instance->m_BufferIndices[1]);
        }
        uint16_t index = sound_instance->m_Index;
        sound->m_InstancesPool.Push(index);
        sound_instance->m_Index = 0xffff;
        sound_instance->m_SourceIndex = 0xffff;
        sound_instance->m_SoundDataIndex = 0xffff;

        if (sound_instance->m_StbVorbis)
        {
            stb_vorbis_close(sound_instance->m_StbVorbis);
            sound_instance->m_StbVorbis = 0;
            free(sound_instance->m_VorbisBuffer);
            sound_instance->m_VorbisBuffer = 0;
            sound_instance->m_VorbisBufferOffset = 0;
            sound_instance->m_VorbisBufferSize = 0;
        }

        return RESULT_OK;
    }

    static uint32_t FillBufferWav(SoundData* sound_data, SoundInstance* instance, ALuint buffer)
    {
        SoundSystem* sound = g_SoundSystem;
        assert(instance->m_CurrentBufferOffset <= (uint32_t) sound_data->m_Size);
        uint32_t to_buffer = dmMath::Min(sound->m_BufferSize, sound_data->m_Size - instance->m_CurrentBufferOffset);

        if (instance->m_Looping && to_buffer == 0)
        {
            instance->m_CurrentBufferOffset = 0;
            to_buffer = dmMath::Min(sound->m_BufferSize, sound_data->m_Size - instance->m_CurrentBufferOffset);
        }

        const char* p = (const char*) sound_data->m_Data;
        p += instance->m_CurrentBufferOffset;
        if (to_buffer > 0)
        {
            alBufferData(buffer, sound_data->m_Format, p, to_buffer, sound_data->m_Frequency);
            CheckAndPrintError();
        }

        instance->m_CurrentBufferOffset += to_buffer;
        return to_buffer;
    }

    static uint32_t PreBufferOggVorbis(SoundData* sound_data, SoundInstance* instance)
    {
        DM_PROFILE(Sound, "Ogg")

        const uint32_t vorbis_frame_buffering = 1024;
        int total_read = 0;
        uint32_t left = instance->m_VorbisBufferSize - instance->m_VorbisBufferOffset;
        if (left > 0)
        {
            uint32_t to_buffer = dmMath::Min(left, vorbis_frame_buffering);
            int ret;
            char* p = instance->m_VorbisBuffer + instance->m_VorbisBufferOffset;
            if (sound_data->m_Format == AL_FORMAT_MONO16)
            {
                ret = stb_vorbis_get_samples_short_interleaved(instance->m_StbVorbis, 1, (short*) p, to_buffer / 2);
            }
            else if (sound_data->m_Format == AL_FORMAT_STEREO16)
            {
                ret = stb_vorbis_get_samples_short_interleaved(instance->m_StbVorbis, 2, (short*) p, to_buffer / 2);
            }
            else
            {
                assert(0);
            }

            if (ret < 0)
            {
                dmLogError("Error reading ogg-vorbis stream (%d)",  (int) ret);
                return 0;
            }
            else if (ret == 0)
            {
                if (instance->m_Looping)
                {
                    stb_vorbis_seek_start(instance->m_StbVorbis);
                }
                return 0;
            }
            else
            {
                if (sound_data->m_Format == AL_FORMAT_MONO16)
                {
                    total_read += ret * 2;
                }
                else if (sound_data->m_Format == AL_FORMAT_STEREO16)
                {
                    total_read += ret * 4;
                }
                else
                {
                    assert(0);
                }
            }
        }

        instance->m_VorbisBufferOffset += total_read;

        return total_read;
    }

    static uint32_t FillBufferOggVorbis(SoundData* sound_data, SoundInstance* instance, ALuint buffer)
    {
        DM_PROFILE(Sound, "Ogg")

        SoundSystem* sound = g_SoundSystem;
        uint32_t n = 0;
        do {
            n = PreBufferOggVorbis(sound_data, instance);
        } while (n > 0 && instance->m_VorbisBufferOffset < sound->m_BufferSize);

        if (instance->m_VorbisBufferOffset > 0)
        {
            alBufferData(buffer, sound_data->m_Format, instance->m_VorbisBuffer, instance->m_VorbisBufferOffset, sound_data->m_Frequency);
            CheckAndPrintError();
        }
        uint32_t total = instance->m_VorbisBufferOffset;
        instance->m_VorbisBufferOffset = 0;
        return total;
    }

    static uint32_t FillBuffer(SoundData* sound_data, SoundInstance* instance, ALuint buffer)
    {
        switch (sound_data->m_Type)
        {
            case SOUND_DATA_TYPE_WAV:
                return FillBufferWav(sound_data, instance, buffer);
            case SOUND_DATA_TYPE_OGG_VORBIS:
                return FillBufferOggVorbis(sound_data, instance, buffer);
            default:
                assert(0);
                return 0;
        }
    }

    static uint32_t InitialBufferFill(HSoundInstance sound_instance)
    {
        SoundSystem* sound = g_SoundSystem;

        ALuint source = sound->m_Sources[sound_instance->m_SourceIndex];
        SoundData* sound_data = &sound->m_SoundData[sound_instance->m_SoundDataIndex];

        ALuint buf1 = sound->m_Buffers[sound_instance->m_BufferIndices[0]];
        ALuint buf2 = sound->m_Buffers[sound_instance->m_BufferIndices[1]];

        uint32_t to_buffer1 = FillBuffer(sound_data, sound_instance, buf1);
        uint32_t to_buffer2 = FillBuffer(sound_data, sound_instance, buf2);

        uint32_t total = to_buffer1 + to_buffer2;

        if (to_buffer1 > 0)
        {
            alSourceQueueBuffers(source, 1, &buf1);
            CheckAndPrintError();
        }

        if (to_buffer2 > 0)
        {
            alSourceQueueBuffers(source, 1, &buf2);
            CheckAndPrintError();
        }

        if (total == 0)
        {
            sound_instance->m_EndOfStream = 1;
        }
        else
        {
            alSourcePlay(source);
            CheckAndPrintError();
        }

        return total;
    }

    Result Update()
    {
        DM_PROFILE(Sound, "Update")

        SoundSystem* sound = g_SoundSystem;

        for (uint32_t i = 0; i < sound->m_Instances.Size(); ++i)
        {
            SoundInstance* instance = &sound->m_Instances[i];

            if (instance->m_SourceIndex == 0xffff)
                continue;

            SoundData* sound_data = &sound->m_SoundData[instance->m_SoundDataIndex];

            ALuint source = sound->m_Sources[instance->m_SourceIndex];

            ALint state;
            alGetSourcei (source, AL_SOURCE_STATE, &state);
            CheckAndPrintError();

            /* Stop the sound if
             *  -     at end of stream
             *  - and source isn't playing
             *  - and not looping
             *
             * We can't just rely on state as OpenAL will put sources in state != AL_PLAYING when
             * out of buffers, i.e. buffer underflow
             */

            if (instance->m_EndOfStream && state != AL_PLAYING && !instance->m_Looping)
            {
                // Instance done playing
                assert(instance->m_BufferIndices[0] != 0xffff);
                assert(instance->m_BufferIndices[1] != 0xffff);
                sound->m_BuffersPool.Push(instance->m_BufferIndices[0]);
                sound->m_BuffersPool.Push(instance->m_BufferIndices[1]);

                instance->m_BufferIndices[0] = 0xffff;
                instance->m_BufferIndices[1] = 0xffff;

                sound->m_SourcesPool.Push(instance->m_SourceIndex);
                instance->m_SourceIndex = 0xffff;
            }
            else
            {
                if (state != AL_PLAYING)
                {
                    // Buffer underflow. Restart the sound and refill the buffers
                    alSourcei(source, AL_BUFFER, AL_NONE);
                    uint32_t total_buffered = InitialBufferFill(instance);
                    if (total_buffered > 0)
                    {
                        sound->m_Stats.m_BufferUnderflowCount++;
                        dmLogWarning("Sound buffer underflow");
                    }
                }
                else
                {
                    alSourcef(source, AL_GAIN, instance->m_Gain * sound->m_MasterGain);
                    CheckAndPrintError();

                    if (instance->m_VorbisBuffer) {
                        SoundData* sound_data = &sound->m_SoundData[instance->m_SoundDataIndex];
                        PreBufferOggVorbis(sound_data, instance);
                    }
                    // Buffer more data
                    int processed;
                    alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
                    while (processed > 0)
                    {
                        ALuint buffer;
                        alSourceUnqueueBuffers(source, 1, &buffer);
                        CheckAndPrintError();

                        uint32_t to_buffer = FillBuffer(sound_data, instance, buffer);
                        CheckAndPrintError();

                        if (to_buffer > 0)
                        {
                            alSourceQueueBuffers(source, 1, &buffer);
                            CheckAndPrintError();
                        }
                        else
                        {
                            instance->m_EndOfStream = 1;
                        }
                        --processed;
                    }
                }
            }
        }

        return RESULT_OK;
    }

    Result Play(HSoundInstance sound_instance)
    {
        if (sound_instance->m_SourceIndex != 0xffff)
        {
            return RESULT_OK;
        }

        SoundSystem* sound = g_SoundSystem;
        if (sound->m_BuffersPool.Remaining() < 2)
        {
            dmLogWarning("Out of sound buffers.");
            return RESULT_OUT_OF_BUFFERS;
        }

        if (sound->m_SourcesPool.Remaining() == 0)
        {
            dmLogWarning("Out of sound sources");
            return RESULT_OUT_OF_SOURCES;
        }

        sound_instance->m_CurrentBufferOffset = 0;

        uint16_t index = sound->m_SourcesPool.Pop();
        sound_instance->m_SourceIndex = index;
        ALuint source = sound->m_Sources[index];
        alSourcei(source, AL_BUFFER, AL_NONE);
        CheckAndPrintError();

        ALint prev_state;
        alGetSourcei (source, AL_SOURCE_STATE, &prev_state);
        CheckAndPrintError();

        alSourcef(source, AL_GAIN, sound_instance->m_Gain * sound->m_MasterGain);
        CheckAndPrintError();

        uint32_t buf_index1 = sound->m_BuffersPool.Pop();
        uint32_t buf_index2 = sound->m_BuffersPool.Pop();

        assert(sound_instance->m_BufferIndices[0] == 0xffff);
        assert(sound_instance->m_BufferIndices[1] == 0xffff);

        sound_instance->m_BufferIndices[0] = buf_index1;
        sound_instance->m_BufferIndices[1] = buf_index2;

        InitialBufferFill(sound_instance);

        return RESULT_OK;
    }

    Result Stop(HSoundInstance sound_instance)
    {
        SoundSystem* sound = g_SoundSystem;
        sound_instance->m_Looping = 0;
        sound_instance->m_EndOfStream = 1;
        if (sound_instance->m_SourceIndex != 0xffff)
        {
            ALuint source = sound->m_Sources[sound_instance->m_SourceIndex];
            alSourceStop(source);
            CheckAndPrintError();
            // NOTE: sound_instance->m_SourceIndex will be set to 0xffff in Update when state != AL_PLAYING
        }
        return RESULT_OK;
    }

    bool IsPlaying(HSoundInstance sound_instance)
    {
        return sound_instance->m_SourceIndex != 0xffff;
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
        return PlatformIsMusicPlaying();
    }


}
