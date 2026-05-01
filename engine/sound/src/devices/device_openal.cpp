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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/time.h>
#include <dlib/profile.h>
#include "sound.h"

#if defined(__MACH__)
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

namespace dmDeviceOpenAL
{
    struct OpenALDevice
    {
        ALCdevice*      m_Device;
        ALCcontext*     m_Context;
        dmArray<ALuint> m_AllBuffers;   // All original buffers
        dmArray<ALuint> m_Buffers;      // The pool
        ALuint          m_Source;
        uint32_t        m_MixRate;

        OpenALDevice()
        {
            m_Device = 0;
            m_Context = 0;
            m_Source = 0;
            m_MixRate = 0;
        }
    };

    static ALenum CheckAndPrintError()
    {
        ALenum error = alGetError();
        if (error != AL_NO_ERROR)
        {
            dmLogError("OPENAL error 0x%04x: %s", error, alGetString(error));
        }
        return error;
    }

    dmSound::Result DeviceOpenALOpen(const dmSound::OpenDeviceParams* params, dmSound::HDevice* device)
    {
        assert(params);
        assert(device);
        ALCdevice* al_device;
        ALCcontext* al_context;

        al_device = alcOpenDevice(0);
        if (al_device == 0) {
            dmLogError("Failed to create OpenAL device");
            return dmSound::RESULT_UNKNOWN_ERROR;
        }

        // Since the alcGetIntegerv(al_device, ALC_FREQUENCY, ...) is giving me bogus... /MAWE
        ALCint frequencies[] = {48000, 44100};
        ALCint frequency = 0;
        for (int i = 0; i < DM_ARRAY_SIZE(frequencies); ++i)
        {
            ALCint attrs[] = {
                ALC_FREQUENCY, frequencies[i],
                0 // sentinel
            };

            al_context = alcCreateContext(al_device, attrs);
            if (al_context)
            {
                frequency = frequencies[i];
                break;
            }
        }

        if (al_context == 0) {
            dmLogError("Failed to create OpenAL context");
            alcCloseDevice(al_device);
            return dmSound::RESULT_UNKNOWN_ERROR;
        }

        if (!alcMakeContextCurrent (al_context)) {
            dmLogError("Failed to make OpenAL context current");
            alcDestroyContext(al_context);
            alcCloseDevice(al_device);
            return dmSound::RESULT_UNKNOWN_ERROR;
        }

        OpenALDevice* openal = new OpenALDevice;
        openal->m_Device = al_device;
        openal->m_Context = al_context;

        openal->m_AllBuffers.SetCapacity(params->m_BufferCount);
        openal->m_AllBuffers.SetSize(params->m_BufferCount);
        openal->m_Buffers.SetCapacity(openal->m_AllBuffers.Capacity());
        openal->m_Buffers.SetSize(openal->m_AllBuffers.Capacity());
        alGenBuffers(params->m_BufferCount, openal->m_AllBuffers.Begin());
        CheckAndPrintError();

        for (uint32_t i = 0; i < params->m_BufferCount; ++i)
        {
            openal->m_Buffers[i] = openal->m_AllBuffers[i];
        }

        alGenSources(1, &openal->m_Source);
        CheckAndPrintError();

        openal->m_MixRate = frequency;

        *device = openal;

        alcMakeContextCurrent(NULL);

        return dmSound::RESULT_OK;
    }

    void DeviceOpenALClose(dmSound::HDevice device)
    {
        if (!device)
        {
            return;
        }

        OpenALDevice* openal = (OpenALDevice*) device;

        alcMakeContextCurrent(openal->m_Context);
        alcProcessContext(openal->m_Context);

        alSourceStop(openal->m_Source);
        CheckAndPrintError();

        alDeleteSources(1, &openal->m_Source);
        CheckAndPrintError();

        alDeleteBuffers(openal->m_AllBuffers.Size(), openal->m_AllBuffers.Begin());
        CheckAndPrintError();

        bool active = alcMakeContextCurrent(0);
#if defined(__EMSCRIPTEN__)
        active = true; // the function seems to always return false
#endif

        if (active) {
            alcDestroyContext(openal->m_Context);
            openal->m_Context = 0;
            if (!alcCloseDevice(openal->m_Device)) {
                dmLogError("Failed to close OpenAL device");
            }
            openal->m_Device = 0;
        } else {
            dmLogError("Failed to make OpenAL context current");
        }

        delete openal;
    }

    dmSound::Result DeviceOpenALQueue(dmSound::HDevice device, const void* samples, uint32_t sample_count)
    {
        assert(device);
        OpenALDevice* openal = (OpenALDevice*) device;
        ALCcontext* current_context = alcGetCurrentContext();
        CheckAndPrintError();
        if (current_context != openal->m_Context)
        {
            return dmSound::RESULT_INIT_ERROR;
        }
        ALuint buffer = openal->m_Buffers[0];
        openal->m_Buffers.EraseSwap(0);
        alBufferData(buffer, AL_FORMAT_STEREO16, (const int16_t*)samples, sample_count * 2 * sizeof(int16_t), openal->m_MixRate);
        CheckAndPrintError();

        alSourceQueueBuffers(openal->m_Source, 1, &buffer);

        ALint state;
        alGetSourcei(openal->m_Source, AL_SOURCE_STATE, &state);
        CheckAndPrintError();
        if(state != AL_PLAYING)
        {
            alSourcePlay(openal->m_Source);
        }

        return dmSound::RESULT_OK;
    }

    uint32_t DeviceOpenALFreeBufferSlots(dmSound::HDevice device)
    {
        assert(device);
        OpenALDevice* openal = (OpenALDevice*) device;
        int processed;
        alGetSourcei(openal->m_Source, AL_BUFFERS_PROCESSED, &processed);

        while (processed > 0)
        {
            ALuint buffer;
            alSourceUnqueueBuffers(openal->m_Source, 1, &buffer);
            CheckAndPrintError();
            openal->m_Buffers.Push(buffer);
            --processed;
        }

        return openal->m_Buffers.Size();
    }

    void DeviceOpenALDeviceInfo(dmSound::HDevice device, dmSound::DeviceInfo* info)
    {
        assert(device);
        assert(info);
        OpenALDevice* openal = (OpenALDevice*) device;
        info->m_MixRate = openal->m_MixRate;
        info->m_DSPImplementation = dmSound::DSPIMPL_TYPE_CPU;
    }

    void DeviceOpenALStart(dmSound::HDevice device)
    {
        assert(device);
        OpenALDevice* openal = (OpenALDevice*) device;
        if (!alcMakeContextCurrent(openal->m_Context)) {
            dmLogError("Failed to restart OpenAL device, could not enable context!");
        }
    }

    void DeviceOpenALStop(dmSound::HDevice device)
    {
        assert(device);
        OpenALDevice* openal = (OpenALDevice*) device;

        ALint state;
        int iter = 0;
        alGetSourcei(openal->m_Source, AL_SOURCE_STATE, &state);
        // When apps goes to background OS gives us some time to finish our processing
        // Let use this time to finish OpenAL buffer processing
        while (state == AL_PLAYING && iter < 20)
        {
            ++iter;
            dmTime::Sleep(10 * 1000);
            alGetSourcei(openal->m_Source, AL_SOURCE_STATE, &state);
        }

        if (!alcMakeContextCurrent(NULL)) {
            dmLogError("Failed to stop OpenAL device, could not disable context!");
        }
    }

    DM_DECLARE_SOUND_DEVICE(DefaultSoundDevice, "default", DeviceOpenALOpen, DeviceOpenALClose, DeviceOpenALQueue,
                            DeviceOpenALFreeBufferSlots, 0, DeviceOpenALDeviceInfo, DeviceOpenALStart, DeviceOpenALStop);
}
