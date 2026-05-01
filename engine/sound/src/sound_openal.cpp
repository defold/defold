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
#include <dlib/hashtable.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/atomic.h>
#include <dlib/profile.h>
#include <dlib/time.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/profile.h>

#include "sound.h"
#include "sound_codec.h"
#include "sound_private.h"
#include "sound_dsp.h"

#if DM_SOUND_WASM_SUPPORT_THREADS
#include <dmsdk/dlib/thread.h>
#include <dlib/condition_variable.h>
#endif

#include <math.h>

#if defined(__MACH__)
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)
#define checkForAlErrors(...)\
    {\
        ALenum err = alGetError();\
        if (err != AL_NO_ERROR) {\
            dmLogError("OpenAL error %i at " __FILE__ ":" STRINGIZE(__LINE__) ": " __VA_ARGS__, (int)err);\
        }\
    }
#define checkForAlcErrors(device, ...)\
    {\
        ALCenum err = alcGetError(device);\
        if (err != AL_NO_ERROR) {\
            dmLogError("OpenAL context error %i at " __FILE__ ":" STRINGIZE(__LINE__) ": " __VA_ARGS__, (int)err);\
        }\
    }

/**
 * Used to determine amount of buffering needed.
 *
 * Pessimistically assume 48khz, 32-bit samples, stereo; 1024 sample frames ~= 1 60 FPS frame.
 * Queue 2 buffers per tick as with smaller buffers we can keep the total buffered amount closer to
 * full.
 */
#define SAMPLES_PER_FRAME 1024
#define BUFFERS_PER_FRAME 2

/**
 * Defold simple sound system
 * NOTE: Must units is in frames, i.e a sample in time with N channels
 *
 */
namespace dmSound
{
    using namespace dmVMath;

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

        dmhash_t    m_Group;

        float       m_Parameters[PARAMETER_MAX];

        uint16_t    m_Index;
        uint16_t    m_SoundDataIndex;
        uint8_t     m_Looping : 1;
        uint8_t     m_Playing : 1;
        uint8_t     m_ScaleDirty : 1;
        uint8_t     m_ScaleInit : 1;
        uint8_t     : 3;
        int8_t      m_Loopcounter; // if set to 3, there will be 3 loops effectively playing the sound 4 times.

        uint16_t    m_SourceIndex;
        uint8_t     m_BufferCount;
        uint8_t     m_BufferPtr;
        ALenum      m_Format;
        uint32_t    m_Rate;
        uint8_t     m_Channels;
        uint8_t     m_BitsPerSample;
        uint8_t     m_Interleaved;
        uint32_t    m_StreamOffset;
    };

    struct SoundGroup
    {
        dmhash_t m_NameHash;
        float    m_Gain;
        bool     m_IsMuted;
    };

    struct SoundSystem
    {
        dmSoundCodec::HCodecContext   m_CodecContext;

        dmMutex::HMutex          m_Mutex;
#if DM_SOUND_WASM_SUPPORT_THREADS
        dmConditionVariable::HConditionVariable m_CondVar;
        dmThread::Thread        m_Thread;
#endif
        int32_atomic_t          m_IsRunning;
        int32_atomic_t          m_Status;
        ALCcontext*             m_AlContext;
        ALCdevice*              m_AlDevice;

        dmArray<SoundInstance>  m_Instances;
        dmIndexPool16           m_InstancesPool;
        dmArray<int16_t>        m_InstancesActive;

        dmArray<SoundData>      m_SoundData;
        dmIndexPool16           m_SoundDataPool;

        dmArray<ALCuint>        m_Sources;
        dmIndexPool16           m_SourcesPool;

        dmArray<ALCuint>        m_Buffers;

        dmHashTable<dmhash_t, int> m_GroupMap;
        SoundGroup              m_Groups[MAX_GROUPS];

        uint32_t                m_MixRate;
        uint32_t                m_PlayCounter;

        bool                    m_HasWindowFocus;

        uint32_t                m_MaxBufferSize;
        uint8_t                 m_BuffersPerSource;
        char*                   m_BufferData;

        ALenum                  m_FormatMonoFloat32;
        ALenum                  m_FormatStereoFloat32;
        bool                    m_SourceSpatialize;

        ALenum                  m_Eos;
        ALenum                  m_DirectStreamingFormat[SOUND_DATA_TYPE_MAX];

        Result InitOpenal() {
            m_AlDevice = alcOpenDevice(nullptr);
            if (!m_AlDevice) {
                dmLogError("couldn't open OpenAL default device");
                return RESULT_INIT_ERROR;
            }
            checkForAlcErrors(m_AlDevice, "alcOpenDevice");
            m_AlContext = alcCreateContext(m_AlDevice, nullptr);
            if (!m_AlContext) {
                dmLogError("couldn't create OpenAL context");
                return RESULT_INIT_ERROR;
            }
            checkForAlcErrors(m_AlDevice, "alcCreateContext");
            if (!alcMakeContextCurrent(m_AlContext)) {
                dmLogError("couldn't set OpenAL context");
                return RESULT_INIT_ERROR;
            }
            checkForAlcErrors(m_AlDevice, "alcMakeContextCurrent");

            // no distance-based attenuation; currently we use positioning for panning only
            alDistanceModel(AL_NONE);

            return RESULT_OK;
        }

        Result FinalizeOpenal() {
            alDeleteSources(m_Sources.Capacity(), m_Sources.Begin());
            checkForAlErrors("alDeleteSources");
            alDeleteBuffers(m_Buffers.Size(), m_Buffers.Begin());
            checkForAlErrors("alDeleteBuffers");
            alcMakeContextCurrent(nullptr);
            if (m_AlContext) {
                alcDestroyContext(m_AlContext);
                checkForAlcErrors(m_AlDevice, "alDestroyContext");
            }
            if (m_AlDevice) {
                alcCloseDevice(m_AlDevice);
                checkForAlErrors("alcCloseDevice");
            }
            return RESULT_OK;
        }
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
        params->m_MaxInstances = 256;
    }

    Result RegisterDevice(struct DeviceType* device)
    {
        device->m_Next = g_FirstDevice;
        g_FirstDevice = device;
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
        group->m_Gain = 1.0f;
        group->m_IsMuted = false;
        sound->m_GroupMap.Put(group_hash, index);
        return index;
    }

    struct ThreadArgs {
        SoundSystem* m_Sound;
        float    m_MasterGain;
        uint32_t m_MaxSoundData;
        uint32_t m_MaxSources;
        uint32_t m_MaxInstances;
        uint32_t m_MaxBuffers;
    };

    static Result InitializeInternal(SoundSystem* sound, ThreadArgs* args)
    {
        float    master_gain = args->m_MasterGain;
        uint32_t max_sound_data = args->m_MaxSoundData;
        uint32_t max_sources = args->m_MaxSources;
        uint32_t max_instances = args->m_MaxInstances;
        uint32_t max_buffers = args->m_MaxBuffers;

        Result r = sound->InitOpenal();
        if (r != RESULT_OK) {
            return r;
        }

        ALCint sample_rate;
        alcGetIntegerv(sound->m_AlDevice, ALC_FREQUENCY, 1, &sample_rate);
        sound->m_MixRate = (uint32_t)sample_rate;
        sound->m_Instances.SetCapacity(max_instances);
        sound->m_Instances.SetSize(max_instances);
        sound->m_InstancesPool.SetCapacity(max_instances);
        sound->m_InstancesActive.SetCapacity(max_instances);
        for (uint32_t i = 0; i < max_instances; ++i)
        {
            SoundInstance* instance = &sound->m_Instances[i];
            memset(instance, 0, sizeof(*instance));
            instance->m_Index = 0xffff;
            instance->m_SourceIndex = 0xffff;
            instance->m_SoundDataIndex = 0xffff;
            instance->m_Parameters[PARAMETER_SPEED] = 1.0f;
        }

        sound->m_SoundData.SetCapacity(max_sound_data);
        sound->m_SoundData.SetSize(max_sound_data);
        sound->m_SoundDataPool.SetCapacity(max_sound_data);
        for (uint32_t i = 0; i < max_sound_data; ++i)
        {
            sound->m_SoundData[i].m_Index = 0xffff;
        }

        sound->m_Sources.SetCapacity(max_sources);
        sound->m_Sources.SetSize(max_sources);
        sound->m_SourcesPool.SetCapacity(max_sources);
        alGenSources(max_sources, sound->m_Sources.Begin());
        checkForAlErrors("alGenSources");

        /**
         * Currently we repurpose the unused max_sound_buffers option to determine the number of
         * buffers per source; we interpret this as total number of buffered 60FPS frames worth of
         * audio across all sources, plus a minimum of two per source, because it works out to a
         * reasonable amount of buffering. Ideally we should create a dedicated setting for this.
         */
        uint32_t buffers_per_source = (2 + (max_buffers / max_sources)) * BUFFERS_PER_FRAME;
        uint32_t total_buffers = buffers_per_source * max_sources;
        uint32_t buffer_size = SAMPLES_PER_FRAME / BUFFERS_PER_FRAME * sizeof(float) * SOUND_MAX_DECODE_CHANNELS;
        uint32_t total_buffer_size = total_buffers * buffer_size;
        /**
         * This is a single contiguous block to simplify buffer allocation, but as a result it
         * must assume that every source could require the maximum amount of buffer (stereo,
         * 32-bit samples).
         */
        sound->m_BufferData = (char*)malloc(total_buffer_size);
        sound->m_BuffersPerSource = buffers_per_source;
        sound->m_MaxBufferSize = buffer_size;
        sound->m_Buffers.SetCapacity(total_buffers);
        sound->m_Buffers.SetSize(total_buffers);
        alGenBuffers(total_buffers, sound->m_Buffers.Begin());
        checkForAlErrors("alGenBuffers");

        sound->m_GroupMap.SetCapacity(MAX_GROUPS * 2 + 1, MAX_GROUPS);
        for (uint32_t i = 0; i < MAX_GROUPS; ++i) {
            memset(&sound->m_Groups[i], 0, sizeof(SoundGroup));
            sound->m_Groups[i].m_Gain = 1.0f;
            sound->m_Groups[i].m_IsMuted = false;
        }

        int master_index = GetOrCreateGroup("master");
        SoundGroup* master = &sound->m_Groups[master_index];
        master->m_Gain = master_gain;
        master->m_IsMuted = false;

        const char *al_vendor = alGetString(AL_VENDOR);
        const char *al_version = alGetString(AL_VERSION);
        const char *al_renderer = alGetString(AL_RENDERER);
        const char *al_extensions = alGetString(AL_EXTENSIONS);
        dmLogInfo("OpenAL sound backend");
        dmLogInfo("  Vendor:       %s", al_vendor);
        dmLogInfo("  Version:      %s", al_version);
        dmLogInfo("  Renderer:     %s", al_renderer);
        dmLogInfo("  Extensions:   %s", al_extensions);
        dmLogInfo("  Sample Rate:  %d", sound->m_MixRate);
        dmLogInfo("  Sources:      %u", max_sources);
        dmLogInfo("  Buffers/Src:  %ux%u", buffers_per_source, buffer_size);
        dmLogInfo("  Total Buffer: %u", total_buffer_size);

        if (alIsExtensionPresent("AL_EXT_float32")) {
            sound->m_FormatMonoFloat32 = alGetEnumValue("AL_FORMAT_MONO_FLOAT32");
            sound->m_FormatStereoFloat32 = alGetEnumValue("AL_FORMAT_STEREO_FLOAT32");
            checkForAlErrors("alGetEnumValue");
        } else {
            sound->m_FormatMonoFloat32 = 0;
            sound->m_FormatStereoFloat32 = 0;
        }

        // check for ability to stream encoded audio
        sound->m_Eos = AL_NONE;
        memset(sound->m_DirectStreamingFormat, 0, sizeof(sound->m_DirectStreamingFormat));
        if (alIsExtensionPresent("AL_NF_end_of_stream")) {
            sound->m_Eos = alGetEnumValue("AL_NF_END_OF_STREAM");
            checkForAlErrors("alGetEnumValue");
            if (sound->m_Eos) {
                dmLogInfo("OpenAL streaming supported");

                // check for specific codec support
                if (alIsExtensionPresent("AL_EXT_vorbis")) {
                    dmLogInfo("ogg/vorbis streaming supported");
                    sound->m_DirectStreamingFormat[SOUND_DATA_TYPE_OGG_VORBIS] = alGetEnumValue("AL_FORMAT_VORBIS_EXT");
                }
                if (alIsExtensionPresent("AL_NF_opus")) {
                    dmLogInfo("opus streaming supported");
                    sound->m_DirectStreamingFormat[SOUND_DATA_TYPE_OPUS] = alGetEnumValue("AL_FORMAT_OPUS_EXT");
                }
                if (alIsExtensionPresent("AL_NF_wav")) {
                    dmLogInfo("wav streaming supported");
                    sound->m_DirectStreamingFormat[SOUND_DATA_TYPE_WAV] = alGetEnumValue("AL_FORMAT_WAV_EXT");
                }
            }
        }

        sound->m_SourceSpatialize = alIsExtensionPresent("AL_SOFT_source_spatialize");

        checkForAlErrors("Initialize");

        return r;
    }

    #if DM_SOUND_WASM_SUPPORT_THREADS
    static Result UpdateInternal(SoundSystem* sound);

    static void SoundThreadEmscriptenCallback(void *ctx)
    {
        SoundSystem* sound = (SoundSystem*)ctx;

        if (!dmAtomicGet32(&sound->m_IsRunning)) {
            emscripten_cancel_main_loop();
            return;
        }

        Result result = UpdateInternal(sound);
        dmAtomicStore32(&sound->m_Status, (int)result);
    }

    static void SoundThread(void* _args)
    {
        ThreadArgs* args = (ThreadArgs*)_args;
        SoundSystem* sound = args->m_Sound;

        dmMutex::Lock(sound->m_Mutex);
        Result result = InitializeInternal(sound, args);
        if (result == RESULT_OK)
        {
            dmAtomicStore32(&sound->m_Status, (int)result);

            dmConditionVariable::Signal(sound->m_CondVar);
            dmMutex::Unlock(sound->m_Mutex);

            emscripten_set_main_loop_arg(SoundThreadEmscriptenCallback, sound, 1000 / 16, 1);   // roughly '60fps'

            dmMutex::Lock(sound->m_Mutex);
            result = sound->FinalizeOpenal();
        }

        dmAtomicStore32(&sound->m_Status, (int)result);
        dmConditionVariable::Signal(sound->m_CondVar);
        dmMutex::Unlock(sound->m_Mutex);
    }
    #endif

    Result Initialize(dmConfigFile::HConfig config, const InitializeParams* params)
    {
        Result r = PlatformInitialize(config, params);
        if (r != RESULT_OK) {
            return r;
        }

        g_SoundSystem = new SoundSystem();
        SoundSystem* sound = g_SoundSystem;
        sound->m_HasWindowFocus = true; // Assume we startup with the window focused
        dmSoundCodec::NewCodecContextParams codec_params;
        codec_params.m_MaxDecoders = params->m_MaxInstances;
        sound->m_CodecContext = dmSoundCodec::New(&codec_params);

        float    master_gain = params->m_MasterGain;
        uint32_t max_sound_data = params->m_MaxSoundData;
        uint32_t max_sources = params->m_MaxSources;
        uint32_t max_instances = params->m_MaxInstances;
        uint32_t max_buffers = params->m_MaxBuffers;

        if (config)
        {
            master_gain = dmConfigFile::GetFloat(config, "sound.gain", 1.0f);
            max_sound_data = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_data", (int32_t) max_sound_data);
            max_sources = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_sources", (int32_t) max_sources);
            max_instances = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_instances", (int32_t) max_instances);
            max_buffers = (uint32_t) dmConfigFile::GetInt(config, "sound.max_sound_buffers", (int32_t) max_buffers);
        }

        uint32_t min_buffers = max_sources * 2;
        if (max_buffers < min_buffers) {
            max_buffers = min_buffers;
            dmLogWarning("invalid setting for sound.max_sound_buffers; using a minimum value of %u", min_buffers);
        }

        ThreadArgs args = {
            sound,
            master_gain,
            max_sound_data,
            max_sources,
            max_instances,
            max_buffers
        };

        dmAtomicStore32(&sound->m_IsRunning, 1);
        dmAtomicStore32(&sound->m_Status, (int)RESULT_NOTHING_TO_PLAY);

        sound->m_Mutex = 0;
#if DM_SOUND_WASM_SUPPORT_THREADS
        sound->m_Thread = 0;
        sound->m_CondVar = 0;

        if (params->m_UseThread) {
            dmLogInfo("sound updates on worker thread");

            sound->m_Mutex = dmMutex::New();
            sound->m_CondVar = dmConditionVariable::New();

            dmMutex::Lock(sound->m_Mutex);

            sound->m_Thread = dmThread::New(SoundThread, 0x20000, &args, "sound");
            dmConditionVariable::Wait(sound->m_CondVar, sound->m_Mutex);

            dmMutex::Unlock(sound->m_Mutex);

            r = (Result)dmAtomicGet32(&sound->m_Status);
        }
        else
#endif
        {
            dmLogInfo("sound updates on main thread");
            r = InitializeInternal(sound, &args);
        }

        return r;
    }

    Result Finalize()
    {
        SoundSystem* sound = g_SoundSystem;
        if (!sound)
            return RESULT_OK;

        dmAtomicStore32(&sound->m_IsRunning, 0);

        Result result;
#if DM_SOUND_WASM_SUPPORT_THREADS
        if (sound->m_Thread == 0) {
            result = sound->FinalizeOpenal();
            if (result != RESULT_OK) {
                return result;
            }
        } else {
            dmThread::Join(sound->m_Thread);
            dmMutex::Delete(sound->m_Mutex);
            dmConditionVariable::Delete(sound->m_CondVar);
            result = (Result)dmAtomicGet32(&sound->m_Status);
        }
#else
        result = sound->FinalizeOpenal();
        if (result != RESULT_OK) {
            return result;
        }
#endif

        PlatformFinalize();

        if (sound)
        {
            dmSoundCodec::Delete(sound->m_CodecContext);

            for (uint32_t i = 0; i < sound->m_Instances.Size(); ++i)
            {
                SoundInstance* instance = &sound->m_Instances[i];
                instance->m_Index = 0xffff;
                instance->m_SoundDataIndex = 0xffff;
                memset(instance, 0, sizeof(*instance));
            }

            free(sound->m_BufferData);

            delete sound;
            g_SoundSystem = 0;
        }

        checkForAlErrors("Finalize");

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


    Result SetSoundData(HSoundData sound_data, const void* sound_buffer, uint32_t sound_buffer_size)
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
            result = SetSoundData(sd, sound_buffer, sound_buffer_size);
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

    Result SetSoundDataCallback(HSoundData sound_data, FSoundDataGetData cbk, void* cbk_ctx)
    {
        sound_data->m_DataCallbacks.m_Context = cbk_ctx;
        sound_data->m_DataCallbacks.m_GetData = cbk;
        return RESULT_OK;
    }

    bool IsSoundDataValid(HSoundData sound_data)
    {
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

    // Called by the decoders in the context of a dmSound update loop
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
            if (ss->m_InstancesPool.Remaining() == 0)
            {
                *sound_instance = 0;
                dmLogError("Out of sound data instance slots (%u). Increase the project setting 'sound.max_sound_instances'", ss->m_InstancesPool.Capacity());
                return RESULT_OUT_OF_INSTANCES;
            }

            dmSoundCodec::Result r = dmSoundCodec::NewDecoder(ss->m_CodecContext, codec_format, sound_data, &decoder);
            if (r != dmSoundCodec::RESULT_OK) {
                dmLogError("Failed to decode sound %s: (%d)", dmHashReverseSafe64(sound_data->m_NameHash), r);
                return RESULT_INVALID_STREAM_DATA;
            }

            index = ss->m_InstancesPool.Pop();
        }

        sound_data->m_RefCount ++;

        SoundInstance* si = &ss->m_Instances[index];
        assert(si->m_Index == 0xffff);
        assert(si->m_SourceIndex == 0xffff);

        si->m_SoundDataIndex = sound_data->m_Index;
        si->m_Index = index;
        si->m_Parameters[PARAMETER_GAIN] = 1.0f;
        si->m_Parameters[PARAMETER_PAN] = 0.0f;
        si->m_ScaleDirty = 1;
        si->m_ScaleInit = 1;
        si->m_ScaleDirty = 1;
        si->m_ScaleInit = 1;
        si->m_Looping = 0;
        si->m_Playing = 0;
        si->m_Decoder = decoder;
        si->m_Group = MASTER_GROUP_HASH;
        si->m_Format = 0;
        si->m_Rate = 0;
        si->m_StreamOffset = 0;

        *sound_instance = si;

        return RESULT_OK;
    }

    Result DeleteSoundInstance(HSoundInstance sound_instance)
    {
        SoundSystem* sound = g_SoundSystem;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(sound->m_Mutex);

        if (IsPlaying(sound_instance))
        {
            dmLogError("Deleting playing sound instance (%s)", GetSoundName(sound, sound_instance));
            Stop(sound_instance);
            uint32_t last = sound->m_InstancesActive.Size() - 1;
            for (uint32_t i = 0; i < last; ++i) {
                if (sound->m_InstancesActive[i] == sound_instance->m_Index) {
                    if (i < last) {
                        // swap to end of array so we can pop
                        sound->m_InstancesActive.EraseSwap(i);
                    }
                    sound->m_InstancesActive.Pop();
                    sound_instance->m_SoundDataIndex = 0xffff;
                    break;
                }
            }
        }

        uint16_t index = sound_instance->m_Index;
        sound_instance->m_Index = 0xffff;
        DeleteSoundData(&sound->m_SoundData[sound_instance->m_SoundDataIndex]);
        sound_instance->m_SoundDataIndex = 0xffff;
        dmSoundCodec::DeleteDecoder(sound->m_CodecContext, sound_instance->m_Decoder);
        sound_instance->m_Decoder = 0;
        sound_instance->m_Parameters[PARAMETER_SPEED] = 1.0f;
        sound->m_InstancesPool.Push(index);

        return RESULT_OK;
    }

    Result SetInstanceGroup(HSoundInstance instance, const char* group_name)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        dmhash_t group_hash = dmHashString64(group_name);
        return SetInstanceGroup(instance, group_hash);
    }

    Result SetInstanceGroup(HSoundInstance instance, dmhash_t group_hash)
    {
        SoundSystem* sound = g_SoundSystem;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(sound->m_Mutex);
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
        SoundSystem* sound = g_SoundSystem;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(sound->m_Mutex);
        int* index = sound->m_GroupMap.Get(group_hash);
        if (!index) {
            return RESULT_NO_SUCH_GROUP;
        }

        SoundGroup* group = &sound->m_Groups[*index];
        group->m_Gain = gain;
        return RESULT_OK;
    }

    Result GetGroupGain(dmhash_t group_hash, float* gain)
    {
        SoundSystem* sound = g_SoundSystem;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(sound->m_Mutex);
        int* index = sound->m_GroupMap.Get(group_hash);
        if (!index) {
            return RESULT_NO_SUCH_GROUP;
        }

        SoundGroup* group = &sound->m_Groups[*index];
        *gain = group->m_Gain;
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
        SoundSystem* sound = g_SoundSystem;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(sound->m_Mutex);
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
        // unsupported
        *rms_left = 0;
        *rms_right = 0;
        return RESULT_OK;
    }

    Result GetGroupPeak(dmhash_t group_hash, float window, float* peak_left, float* peak_right)
    {
        // unsupported
        *peak_left = 0;
        *peak_right = 0;
        return RESULT_OK;
    }

    static dmSoundCodec::Result GetInstanceProperties(HSoundInstance sound_instance) {
        SoundSystem* sound = g_SoundSystem;
        SoundData &sound_data = sound->m_SoundData[sound_instance->m_SoundDataIndex];
        dmSoundCodec::Info info;
        dmSoundCodec::GetInfo(sound->m_CodecContext, sound_instance->m_Decoder, &info);
        sound_instance->m_Rate = info.m_Rate;
        sound_instance->m_Interleaved = info.m_IsInterleaved;
        uint8_t channels = sound_instance->m_Channels = info.m_Channels;
        uint8_t bits_per_sample = sound_instance->m_BitsPerSample = info.m_BitsPerSample;

        if (sound->m_DirectStreamingFormat[sound_data.m_Type]) {
            sound_instance->m_Format = sound->m_DirectStreamingFormat[sound_data.m_Type];
        } else switch (channels) {
            case 1: {
                if (bits_per_sample == 8) {
                    sound_instance->m_Format = AL_FORMAT_MONO8;
                } else if (bits_per_sample == 16) {
                    sound_instance->m_Format = AL_FORMAT_MONO16;
                } else if (bits_per_sample == 32 && sound->m_FormatMonoFloat32) {
                    sound_instance->m_Format = sound->m_FormatMonoFloat32;
                } else {
                    dmLogError("unsupported mono bits per sample: %i", (int)bits_per_sample);
                    return dmSoundCodec::RESULT_INVALID_FORMAT;
                }
                break;
            }
            case 2: {
                if (bits_per_sample == 8) {
                    sound_instance->m_Format = AL_FORMAT_STEREO8;
                } else if (bits_per_sample == 16) {
                    sound_instance->m_Format = AL_FORMAT_STEREO16;
                } else if (bits_per_sample == 32 && sound->m_FormatStereoFloat32) {
                    sound_instance->m_Format = sound->m_FormatStereoFloat32;
                } else {
                    dmLogError("unsupported stereo bits per sample: %i", (int)bits_per_sample);
                    return dmSoundCodec::RESULT_INVALID_FORMAT;
                }
                break;
            }
            default: {
                dmLogError("unsupported channel count: %i", (int)channels);
                return dmSoundCodec::RESULT_INVALID_FORMAT;
            }
        }
        return dmSoundCodec::RESULT_OK;
    }

    static dmSoundCodec::Result Feed(HSoundInstance sound_instance) {
        DM_PROFILE(__FUNCTION__);
        SoundSystem* sound = g_SoundSystem;
        SoundData &sound_data = sound->m_SoundData[sound_instance->m_SoundDataIndex];
        dmSoundCodec::Result r;

        if (!sound_instance->m_Rate) {
            r = GetInstanceProperties(sound_instance);
            if (r != dmSoundCodec::RESULT_OK) {
                sound_instance->m_Playing = false;
                return r;
            }
        }

        uint16_t buffer_index = sound_instance->m_SourceIndex * sound->m_BuffersPerSource + sound_instance->m_BufferPtr;
        ALuint source = sound->m_Sources[sound_instance->m_SourceIndex];
        ALuint *buffer = &sound->m_Buffers[buffer_index];
        char *original_ptr = &sound->m_BufferData[buffer_index * sound->m_MaxBufferSize];
        uint32_t buffer_size = SAMPLES_PER_FRAME / BUFFERS_PER_FRAME * sound_instance->m_Channels * sound_instance->m_BitsPerSample / 8;
        char *buffer_ptr = original_ptr;
        uint32_t total_decoded = 0;
        uint32_t decoded = 0;
        do {
            if (sound->m_DirectStreamingFormat[sound_data.m_Type]) {
                // this is a supported streaming format; we can directly read from the stream and pipe that data to OpenAL
                Result read_result = SoundDataRead(&sound_data, sound_instance->m_StreamOffset, buffer_size - total_decoded, buffer_ptr, &decoded);
                sound_instance->m_StreamOffset += decoded;
                if (read_result == Result::RESULT_OK) {
                    r = dmSoundCodec::Result::RESULT_OK;
                } else if (read_result == Result::RESULT_PARTIAL_DATA) {
                    r = dmSoundCodec::Result::RESULT_OK;
                } else if (read_result == Result::RESULT_END_OF_STREAM) {
                    r = dmSoundCodec::RESULT_END_OF_STREAM;
                } else {
                    dmLogError("stream read error: %i\n", (int)read_result);
                    r = dmSoundCodec::Result::RESULT_UNKNOWN_ERROR;
                }
            } else {
                // decode PCM
                r = dmSoundCodec::Decode(sound->m_CodecContext, sound_instance->m_Decoder, &buffer_ptr, buffer_size, &decoded);
            }
            if (r == dmSoundCodec::RESULT_END_OF_STREAM) {
                if (sound_instance->m_Looping && sound_instance->m_Loopcounter != 0) {
                    // loop
                    dmSoundCodec::Reset(sound->m_CodecContext, sound_instance->m_Decoder);
                    if (sound_instance->m_Loopcounter > 0) {
                        --sound_instance->m_Loopcounter;
                    }
                } else {
                    sound_instance->m_Playing = false;
                    break;
                }
            } else if (r != dmSoundCodec::RESULT_OK || !decoded) {
                break;
            }
            total_decoded += decoded;
            buffer_ptr += decoded;
        } while (total_decoded < buffer_size);
        if ((r == dmSoundCodec::RESULT_OK || r == dmSoundCodec::RESULT_END_OF_STREAM) && total_decoded) {
            alBufferData(*buffer, sound_instance->m_Format, original_ptr, (ALsizei)total_decoded, sound_instance->m_Rate);
            checkForAlErrors("alBufferData");
            alSourceQueueBuffers(source, 1, buffer);
            checkForAlErrors("alSourceQueueBuffers");
            float gain = sound_instance->m_Parameters[PARAMETER_GAIN];
            bool muted = false;

            int *group_index = sound->m_GroupMap.Get(sound_instance->m_Group);
            if (group_index) {
                SoundGroup* group = &sound->m_Groups[*group_index];
                gain *= group->m_Gain;
                muted = group->m_IsMuted;
            }

            int* master_index = sound->m_GroupMap.Get(MASTER_GROUP_HASH);
            if (master_index) {
                SoundGroup* master = &sound->m_Groups[*master_index];
                gain *= master->m_Gain;
                muted |= master->m_IsMuted;
            }

            if (muted) {
                gain = 0.0f;
            }
            alSourcef(source, AL_PITCH, sound_instance->m_Parameters[PARAMETER_SPEED]);
            alSourcef(source, AL_GAIN, gain);
            checkForAlErrors("alSourcef");
            float pan = sound_instance->m_Parameters[PARAMETER_PAN];
            // openal position: x is left-right, y is down-up, and z is away-towards
            if (pan == 0.0f) {
                alSource3f(source, AL_POSITION, 0.0, 0.0, -1.0);
            } else {
                if (sound_instance->m_Channels > 1 && sound->m_SourceSpatialize) {
                    // if AL_SOFT_source_spatialize is present, we can explicitly enable spatialization;
                    // otherwise multichannel sources will disable spatialization by default
                    alSourcei(source, alGetEnumValue("AL_SOURCE_SPATIALIZE_SOFT"), AL_TRUE);
                    checkForAlErrors("alSourcei");
                }
                // 90 degree FOV; pan is between -1 and 1
                // defold panning is linear, so it directly sets the x value; from there we find the
                // z value that results in unit distance from the origin
                ALfloat x = pan * 0.5;
                ALfloat z = -sqrtf(1 - x * x);
                alSource3f(source, AL_POSITION, x, 0, z);
                checkForAlErrors("alSource3f");
            }
            sound_instance->m_BufferPtr = (sound_instance->m_BufferPtr + 1) % sound->m_BuffersPerSource;
            ++sound_instance->m_BufferCount;
        }
        if (r == dmSoundCodec::RESULT_END_OF_STREAM) {
            sound_instance->m_StreamOffset = 0;
            alSourcei(source, sound->m_Eos, AL_TRUE);
            checkForAlErrors("alSourcei");
        }
        return r;
    }

    Result Play(HSoundInstance sound_instance)
    {
        SoundSystem* sound = g_SoundSystem;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(sound->m_Mutex);
        if (sound_instance->m_Playing) {
            return RESULT_OK;
        }
        if (!sound->m_SourcesPool.Remaining()) {
            return RESULT_OUT_OF_SOURCES;
        }
        sound_instance->m_Playing = 1;
        if (sound_instance->m_SourceIndex != 0xffff) {
            return RESULT_OK;
        }
        uint16_t source_index = sound->m_SourcesPool.Pop();
        sound_instance->m_SourceIndex = source_index;
        sound->m_InstancesActive.Push(sound_instance->m_Index);
        return RESULT_OK;
    }

    void ReclaimSource(HSoundInstance sound_instance) {
        assert(sound_instance->m_SourceIndex != 0xffff);
        SoundSystem* sound = g_SoundSystem;
        ALuint source_index = sound_instance->m_SourceIndex;
        sound_instance->m_SourceIndex = 0xffff;
        sound->m_SourcesPool.Push(source_index);
    }

    void SwapAndPopActiveInstance(int index) {
        SoundSystem* sound = g_SoundSystem;
        int16_t instance_index = sound->m_InstancesActive[index];
        uint16_t last = sound->m_InstancesActive.Size() - 1;
        if (index < last) {
            // swap to end of array so we can pop
            uint16_t tmp = sound->m_InstancesActive[last];
            sound->m_InstancesActive[last] = instance_index;
            sound->m_InstancesActive[index] = tmp;
        }
        sound->m_InstancesActive.Pop();
    }

    Result Stop(HSoundInstance sound_instance)
    {
        SoundSystem* sound = g_SoundSystem;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(sound->m_Mutex);
        sound_instance->m_Playing = 0;
        dmSoundCodec::Reset(g_SoundSystem->m_CodecContext, sound_instance->m_Decoder);
        if (sound_instance->m_SourceIndex != 0xffff) {
            ALuint source = sound->m_Sources[sound_instance->m_SourceIndex];
            alSourceStop(source);
            checkForAlErrors("alSourceStop");
            // setting AL_BUFFER to AL_NONE will unqueue any buffers
            alSourcei(source, AL_BUFFER, AL_NONE);
            checkForAlErrors("alSourcei AL_BUFFER=0");
            sound_instance->m_BufferCount = 0;
            ReclaimSource(sound_instance);
            for (uint32_t i = 0; i < sound->m_InstancesActive.Size(); ++i) {
                if (sound->m_InstancesActive[i] == sound_instance->m_Index) {
                    SwapAndPopActiveInstance(i);
                    break;
                }
            }
        }
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

    void GetDecoderOutputSettings(DecoderOutputSettings* settings)
    {
        memset(settings, 0, sizeof(DecoderOutputSettings));
        settings->m_UseNormalizedFloatRange = true;
        settings->m_UseInterleaved = true;
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
        // until we reclaim the source, we aren't finished playing
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        return sound_instance->m_SourceIndex != 0xffff;
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
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_SoundSystem->m_Mutex);
        sound_instance->m_Parameters[parameter] = value.getX();
        return RESULT_OK;
    }

    // private function. Currently part of the test_sound.cpp
    void GetPanScale(float pan, float* left_scale, float* right_scale)
    {
    }

    // private function. Currently part of the test_sound.cpp
    Result GetScaleFromGain(float gain, float* scale)
    {
        //*scale = GainToScale(gain);
        return RESULT_UNSUPPORTED;
    }

    Result SetStartFrame(HSoundInstance sound_instance, uint32_t start_frame)
    {
        return RESULT_UNSUPPORTED;
    }

    Result SetStartTime(HSoundInstance sound_instance, float start_time_seconds)
    {
        return RESULT_UNSUPPORTED;
    }

    static Result UpdateInternal(SoundSystem* sound)
    {
        DM_PROFILE(__FUNCTION__);

        if (IsAudioInterrupted())
        {
            // We can't play sounds when Audio was interrupted by OS event (Phone call, Alarm etc)
            return RESULT_OK;
        }

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(sound->m_Mutex);

        // feed buffers for active streams
        uint16_t i = 0;
        while (i < sound->m_InstancesActive.Size()) {
            uint16_t instance_index = sound->m_InstancesActive[i];
            SoundInstance &instance = sound->m_Instances[instance_index];
            ALuint source = sound->m_Sources[instance.m_SourceIndex];
            if (instance.m_BufferCount) {
                // drain any processed buffers
                ALuint source = sound->m_Sources[instance.m_SourceIndex];
                ALint buffers_processed = 0;
                alGetSourcei(source, AL_BUFFERS_PROCESSED,
                            &buffers_processed);
                assert(buffers_processed <= instance.m_BufferCount);
                while (buffers_processed) {
                    ALuint buffer;
                    alSourceUnqueueBuffers(source, 1, &buffer);
                    checkForAlErrors("alSourceUnqueueBuffers");
                    --buffers_processed;
                    --instance.m_BufferCount;
                }
            }
            ALenum state;
            alGetSourcei(source, AL_SOURCE_STATE, &state);
            if (instance.m_Playing) {
                // feed new buffers and handle looping
                while (instance.m_BufferCount < sound->m_BuffersPerSource) {
                    dmSoundCodec::Result r = Feed(&instance);
                    if (r == dmSoundCodec::RESULT_OK) {
                        // noop
                    } else if (r == dmSoundCodec::RESULT_END_OF_STREAM) {
                        break;
                    } else {
                        // error
                        dmLogError("AL source feed error: %i\n", (int)r);
                        instance.m_Playing = false;
                        break;
                    }
                }
                if (instance.m_BufferCount && state != AL_PLAYING) {
                    alSourcePlay(source);
                    checkForAlErrors("alSourcePlay");
                }
            } else {
                // if we're no longer playing this instance, we still need to wait for its buffers to finish
                if (state != AL_PLAYING) {
                    // setting AL_BUFFER to AL_NONE will unqueue any buffers
                    alSourcei(source, AL_BUFFER, AL_NONE);
                    checkForAlErrors("alSourcei AL_BUFFER=0");
                    instance.m_BufferCount = 0;
                    if (state != AL_STOPPED) {
                        alSourceStop(source);
                        checkForAlErrors("alSourceStop");
                    }
                    instance.m_BufferCount = 0;
                }
            }

            if (instance.m_Playing || instance.m_BufferCount) {
                // wait until we're finished playing and get through all queued buffers
                ++i;
            } else {
                // finished playing; reclaim source
                Stop(&instance);
            }
        }

        return RESULT_OK;
    }

    Result Update()
    {
        DM_PROFILE("Sound");
        SoundSystem* sound = g_SoundSystem;
#if DM_SOUND_WASM_SUPPORT_THREADS
        if (!sound || sound->m_Thread != 0)
#else
        if (!sound)
#endif
            return RESULT_OK;

        return UpdateInternal(sound);
    }

    Result Pause(bool pause)
    {
        return RESULT_OK;
    }

    bool IsMusicPlaying()
    {
        return PlatformIsMusicPlaying(true, g_SoundSystem->m_HasWindowFocus);
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
