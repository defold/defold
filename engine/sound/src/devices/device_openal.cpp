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
        dmArray<ALuint> m_Buffers;
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

    static void CheckAndPrintError()
    {
        ALenum error = alGetError();
        if (error != AL_NO_ERROR)
        {
            dmLogError("%s", alGetString(error));
        }
    }

    dmSound::Result DeviceOpenALOpen(const dmSound::OpenDeviceParams* params, dmSound::HDevice* device)
    {
        ALCdevice* al_device;
        ALCcontext* al_context;

        al_device = alcOpenDevice(0);
        if (al_device == 0) {
            dmLogError("Failed to create OpenAL device");
            return dmSound::RESULT_UNKNOWN_ERROR;
        }

        al_context = alcCreateContext(al_device, 0);
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

        openal->m_Buffers.SetCapacity(params->m_BufferCount);
        openal->m_Buffers.SetSize(params->m_BufferCount);
        alGenBuffers(params->m_BufferCount, openal->m_Buffers.Begin());
        CheckAndPrintError();

        alGenSources(1, &openal->m_Source);
        CheckAndPrintError();

        openal->m_MixRate = 44100;

        *device = openal;

        return dmSound::RESULT_OK;
    }

    void DeviceOpenALClose(dmSound::HDevice device)
    {
        OpenALDevice* openal = (OpenALDevice*) device;

        int iter = 0;
        while (openal->m_Buffers.Size() != openal->m_Buffers.Capacity())
        {
            int processed;
            alGetSourcei(openal->m_Source, AL_BUFFERS_PROCESSED, &processed);
            while (processed > 0) {
                ALuint buffer;
                alSourceUnqueueBuffers(openal->m_Source, 1, &buffer);
                CheckAndPrintError();
                openal->m_Buffers.Push(buffer);
                --processed;
            }

            if ((iter + 1) % 10 == 0) {
                dmLogInfo("Waiting for OpenAL device to complete");
            }
            ++iter;
            dmTime::Sleep(10 * 1000);

            if (iter > 1000) {
                dmLogError("Still buffers in OpenAL. Bailing.");
            }
        }

        alSourceStop(openal->m_Source);
        alDeleteSources(1, &openal->m_Source);

        alDeleteBuffers(openal->m_Buffers.Size(), openal->m_Buffers.Begin());
        CheckAndPrintError();

        if (alcMakeContextCurrent(0)) {
            alcDestroyContext(openal->m_Context);
            if (!alcCloseDevice(openal->m_Device)) {
                dmLogError("Failed to close OpenAL device");
            }
        } else {
            dmLogError("Failed to make OpenAL context current");
        }

        delete openal;
    }

    dmSound::Result DeviceOpenALQueue(dmSound::HDevice device, const int16_t* samples, uint32_t sample_count)
    {
        OpenALDevice* openal = (OpenALDevice*) device;
        ALuint buffer = openal->m_Buffers[0];
        openal->m_Buffers.EraseSwap(0);
        alBufferData(buffer, AL_FORMAT_STEREO16, samples, sample_count * 2 * sizeof(int16_t), openal->m_MixRate);
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
        OpenALDevice* openal = (OpenALDevice*) device;
        info->m_MixRate = openal->m_MixRate;
    }

    void DeviceOpenALRestart(dmSound::HDevice device)
    {
        OpenALDevice* openal = (OpenALDevice*) device;
        if (!alcMakeContextCurrent(openal->m_Context)) {
            dmLogError("Failed to restart OpenAL device, could not enable context!");
        }
    }

    void DeviceOpenALStop(dmSound::HDevice device)
    {
        if (!alcMakeContextCurrent(NULL)) {
            dmLogError("Failed to stop OpenAL device, could not disable context!");
        }
    }

    DM_DECLARE_SOUND_DEVICE(DefaultSoundDevice, "default", DeviceOpenALOpen, DeviceOpenALClose, DeviceOpenALQueue, DeviceOpenALFreeBufferSlots, DeviceOpenALDeviceInfo, DeviceOpenALRestart, DeviceOpenALStop);
}

