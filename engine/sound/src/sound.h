#ifndef DM_SOUND_H
#define DM_SOUND_H

#include <dlib/configfile.h>
#include <ddf/ddf.h>

#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;

namespace dmSound
{
    typedef struct SoundData* HSoundData;
    typedef struct SoundInstance* HSoundInstance;

    enum SoundDataType
    {
        SOUND_DATA_TYPE_WAV        = 0,
        SOUND_DATA_TYPE_OGG_VORBIS = 1,
    };

    enum Parameter
    {
        PARAMETER_GAIN = 0,
    };

    enum Result
    {
        RESULT_OK                 =  0,    //!< RESULT_OK
        RESULT_OUT_OF_SOURCES     = -1,    //!< RESULT_OUT_OF_SOURCES
        RESULT_EFFECT_NOT_FOUND   = -2,    //!< RESULT_EFFECT_NOT_FOUND
        RESULT_OUT_OF_INSTANCES   = -3,    //!< RESULT_OUT_OF_INSTANCES
        RESULT_RESOURCE_LEAK      = -4,    //!< RESULT_RESOURCE_LEAK
        RESULT_OUT_OF_BUFFERS     = -5,    //!< RESULT_OUT_OF_BUFFERS
        RESULT_INVALID_PROPERTY   = -6,    //!< RESULT_INVALID_PROPERTY
        RESULT_UNKNOWN_SOUND_TYPE = -7,    //!< RESULT_UNKNOWN_SOUND_TYPE
        RESULT_INVALID_STREAM_DATA= -8,    //!< RESULT_INVALID_STREAM_DATA
        RESULT_OUT_OF_MEMORY      = -9,    //!< RESULT_OUT_OF_MEMORY
        RESULT_UNSUPPORTED        = -10,   //!< RESULT_UNSUPPORTED
        RESULT_UNKNOWN_ERROR      = -1000, //!< RESULT_UNKNOWN_ERROR
    };

    struct Stats
    {
        uint32_t m_BufferUnderflowCount;
    };

    // TODO:
    // - Music streaming.

    struct InitializeParams;
    void SetDefaultInitializeParams(InitializeParams* params);

    struct InitializeParams
    {
        float    m_MasterGain;
        uint32_t m_MaxSoundData;
        uint32_t m_MaxSources;
        uint32_t m_MaxBuffers;
        uint32_t m_BufferSize;
        uint32_t m_MaxInstances;

        InitializeParams()
        {
            SetDefaultInitializeParams(this);
        }
    };

    Result Initialize(dmConfigFile::HConfig config, const InitializeParams* params);
    Result Finalize();

    void   GetStats(Stats* stats);

    Result NewSoundData(const void* sound_buffer, uint32_t sound_buffer_size, SoundDataType type, HSoundData* sound_data);
    Result SetSoundData(HSoundData sound_data, const void* sound_buffer, uint32_t sound_buffer_size);
    Result DeleteSoundData(HSoundData sound_data);

    Result NewSoundInstance(HSoundData sound_data, HSoundInstance* sound_instance);
    Result DeleteSoundInstance(HSoundInstance sound_instance);

    Result Update();

    Result Play(HSoundInstance sound_instance);
    Result Stop(HSoundInstance sound_instance);
    bool IsPlaying(HSoundInstance sound_instance);

    Result SetLooping(HSoundInstance sound_instance, bool looping);

    Result SetParameter(HSoundInstance sound_instance, Parameter parameter, const Vector4& value);
    Result GetParameter(HSoundInstance sound_instance, Parameter parameter, Vector4& value);
}

#endif // DM_SOUND_H
