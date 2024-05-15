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

#include "sound.h"

#include <string.h>

#include <dlib/array.h>
#include <dmsdk/dlib/vmath.h>

namespace dmSound
{
    using namespace dmVMath;

    dmArray<SoundInstance*>* g_Instances = 0x0;

    struct SoundData
    {
        char* m_Buffer;
        uint32_t m_BufferSize;

        // make sure it's the same size for both 32/64 bit. Makes it easier for tests
        #if defined(DM_PLATFORM_32BIT)
        uint64_t _pad;
        #endif
    };

    struct SoundInstance
    {
        Vector4 m_Parameters[PARAMETER_MAX];
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
        sound_data->m_BufferSize = sound_buffer_size;
        memcpy(sound_data->m_Buffer, sound_buffer, sound_buffer_size);
        return RESULT_OK;
    }

    uint32_t GetSoundResourceSize(HSoundData sound_data)
    {
        return sizeof(SoundData) + sound_data->m_BufferSize;
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

    Result GetGroupHashes(uint32_t* count, dmhash_t* buffer)
    {
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

    Result Pause(bool pause)
    {
        (void)pause;
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

    Result Pause(HSoundInstance sound_instance, bool pause)
    {
        sound_instance->m_Playing = (uint8_t)pause;
        return RESULT_OK;
    }

    uint32_t GetAndIncreasePlayCounter()
    {
        return 0;
    }

    bool IsPlaying(HSoundInstance sound_instance)
    {
        return sound_instance->m_Playing == 1;
    }

    Result SetLooping(HSoundInstance sound_instance, bool looping, int8_t loopcount)
    {
        sound_instance->m_Looping = looping ? 1 : 0;
        // loopcount is ignored
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

    bool IsAudioInterrupted()
    {
        return false;
    }

    void OnWindowFocus(bool focus)
    {
        (void)focus;
    }

    Result RegisterDevice(struct DeviceType* device)
    {
        return RESULT_OK;
    }

    void SetDefaultInitializeParams(InitializeParams* params)
    {
    }
}
