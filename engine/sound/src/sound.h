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
        RESULT_DEVICE_NOT_FOUND   = -11,   //!< RESULT_DEVICE_NOT_FOUND
        RESULT_OUT_OF_GROUPS      = -12,   //!< RESULT_OUT_OF_GROUPS
        RESULT_NO_SUCH_GROUP      = -13,   //!< RESULT_NO_SUCH_GROUP
        RESULT_NOTHING_TO_PLAY    = -14,   //!< RESULT_NOTHING_TO_PLAY
        RESULT_INIT_ERROR         = -15,   //!< RESULT_INIT_ERROR
        RESULT_FINI_ERROR         = -16,   //!< RESULT_FINI_ERROR
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
        const char* m_OutputDevice;
        float    m_MasterGain;
        uint32_t m_MaxSoundData;
        uint32_t m_MaxSources;
        uint32_t m_MaxBuffers;
        uint32_t m_BufferSize;
        uint32_t m_FrameCount;
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

    Result SetInstanceGroup(HSoundInstance instance, const char* group);
    Result SetInstanceGroup(HSoundInstance instance, dmhash_t group_hash);

    Result AddGroup(const char* group);
    Result SetGroupGain(dmhash_t group_hash, float gain);
    Result GetGroupGain(dmhash_t group_hash, float* gain);
    uint32_t GetGroupCount();
    Result GetGroupHash(uint32_t index, dmhash_t* hash);

    Result GetGroupRMS(dmhash_t group_hash, float window, float* rms_left, float* rms_right);
    Result GetGroupPeak(dmhash_t group_hash, float window, float* peak_left, float* peak_right);

    Result Update();

    Result Play(HSoundInstance sound_instance);
    Result Stop(HSoundInstance sound_instance);
    bool IsPlaying(HSoundInstance sound_instance);

    Result SetLooping(HSoundInstance sound_instance, bool looping);

    Result SetParameter(HSoundInstance sound_instance, Parameter parameter, const Vector4& value);
    Result GetParameter(HSoundInstance sound_instance, Parameter parameter, Vector4& value);

    bool IsMusicPlaying();
    bool IsPhonePlaying();

}

#endif // DM_SOUND_H
