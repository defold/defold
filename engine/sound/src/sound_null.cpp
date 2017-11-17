#include "sound.h"

#include <string.h>

#include <dlib/array.h>

namespace dmSound
{
    dmArray<SoundInstance*>* g_Instances = 0x0;

    struct SoundData
    {
        char* m_Buffer;
    };

    struct SoundInstance
    {
        Vector4 m_Parameters[PARAMETER_GAIN + 1];
        uint32_t m_Playing : 1;
        uint32_t m_Looping : 1;
    };

    Result Initialize(dmConfigFile::HConfig config, const InitializeParams* params)
    {
        g_Instances = new dmArray<SoundInstance*>();
        g_Instances->SetCapacity(128);

        return RESULT_OK;
    }

    Result Finalize()
    {
        if (g_Instances != 0x0)
        {
            delete g_Instances;
            g_Instances = 0x0;
        }
        return RESULT_OK;
    }

    void GetStats(Stats* stats)
    {
        memset(stats, 0, sizeof(*stats));
    }

    Result NewSoundData(const void* sound_buffer, uint32_t sound_buffer_size, SoundDataType type, HSoundData* sound_data, dmhash_t name)
    {
        HSoundData sd = new SoundData();
        sd->m_Buffer = 0x0;
        Result result = SetSoundData(sd, sound_buffer, sound_buffer_size);
        if (result == RESULT_OK)
            *sound_data = sd;
        else
            DeleteSoundData(sd);
        return result;
    }

    Result SetSoundData(HSoundData sound_data, const void* sound_buffer, uint32_t sound_buffer_size)
    {
        if (sound_data->m_Buffer != 0x0)
            delete [] sound_data->m_Buffer;
        sound_data->m_Buffer = new char[sound_buffer_size];
        memcpy(sound_data->m_Buffer, sound_buffer, sound_buffer_size);
        return RESULT_OK;
    }

    Result DeleteSoundData(HSoundData sound_data)
    {
        if (sound_data->m_Buffer != 0x0)
            delete [] sound_data->m_Buffer;
        delete sound_data;
        return RESULT_OK;
    }

    Result NewSoundInstance(HSoundData sound_data, HSoundInstance* sound_instance)
    {
        SoundInstance* si = new SoundInstance();
        si->m_Looping = 0;
        si->m_Playing = 0;
        *sound_instance = si;
        g_Instances->Push(*sound_instance);
        return RESULT_OK;
    }

    Result DeleteSoundInstance(HSoundInstance sound_instance)
    {
        for (uint32_t i = 0; i < g_Instances->Size(); ++i)
            if ((*g_Instances)[i] == sound_instance)
                g_Instances->EraseSwap(i);
        delete sound_instance;
        return RESULT_OK;
    }

    Result SetInstanceGroup(HSoundInstance instance, const char* group_name)
    {
        // NOTE: Not supported.
        // sound_null is deprecated and should be replaced by sound2 with null-device
        return RESULT_OK;
    }

    Result SetInstanceGroup(HSoundInstance instance, dmhash_t group_hash)
    {
        // NOTE: Not supported.
        // sound_null is deprecated and should be replaced by sound2 with null-device
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

    uint32_t GetGroupCount()
    {
        // NOTE: Not supported.
        // sound_null is deprecated and should be replaced by sound2 with null-device
        return 0;
    }

    Result GetGroupHash(uint32_t index, dmhash_t* hash)
    {
        // NOTE: Not supported.
        // sound_null is deprecated and should be replaced by sound2 with null-device
        return RESULT_OK;
    }

    Result AddGroup(const char* group)
    {
        // NOTE: Not supported.
        // sound_null is deprecated and should be replaced by sound2 with null-device
        return RESULT_OK;
    }

    Result Update()
    {
        for (uint32_t i = 0; i < g_Instances->Size(); ++i)
            if ((*g_Instances)[i]->m_Playing && !(*g_Instances)[i]->m_Looping)
                (*g_Instances)[i]->m_Playing = 0;
        return RESULT_OK;
    }

    Result Play(HSoundInstance sound_instance)
    {
        sound_instance->m_Playing = 1;
        return RESULT_OK;
    }

    Result Stop(HSoundInstance sound_instance)
    {
        sound_instance->m_Playing = 0;
        return RESULT_OK;
    }

    bool IsPlaying(HSoundInstance sound_instance)
    {
        return sound_instance->m_Playing == 1;
    }

    Result SetLooping(HSoundInstance sound_instance, bool looping)
    {
        sound_instance->m_Looping = looping ? 1 : 0;
        return RESULT_OK;
    }

    Result SetParameter(HSoundInstance sound_instance, Parameter parameter, const Vector4& value)
    {
        sound_instance->m_Parameters[parameter] = value;
        return RESULT_OK;
    }

    Result GetParameter(HSoundInstance sound_instance, Parameter parameter, Vector4& value)
    {
        value = sound_instance->m_Parameters[parameter];
        return RESULT_OK;
    }

    bool IsMusicPlaying()
    {
        return false;
    }

    bool IsPhoneCallActive()
    {
        return false;
    }

    Result RegisterDevice(struct DeviceType* device)
    {
        return RESULT_OK;
    }

    void SetDefaultInitializeParams(InitializeParams* params)
    {
    }
}
