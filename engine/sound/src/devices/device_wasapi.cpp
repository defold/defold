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

#if !defined(_MSC_VER)
    #error "WASAPI audio driver not supported on this platform!"
#endif

#include <Audioclient.h>
#include <mmdeviceapi.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/time.h>
#include <dlib/profile.h>
#include "sound.h"

namespace dmDeviceWasapi
{
    struct SoundDevice
    {
        IAudioClient2*      m_AudioClient;
        IAudioRenderClient* m_AudioRenderClient;
        WAVEFORMATEX*       m_MixFormat;
        HANDLE              m_ClientBufferEvent;

        uint32_t            m_Format;
        uint32_t            m_NumChannels;
        uint32_t            m_FrameCount;

        SoundDevice()
        {
            memset(this, 0, sizeof(*this));
        }
    };

    static void CheckAndPrintError(HRESULT hr)
    {
        if (FAILED(hr))
        {
            dmLogError("WASAPI error 0x%X: %s", hr, "");
        }
    }

    static void DeleteDevice(SoundDevice* device)
    {
        if (!device)
            return;

        if (!device->m_AudioRenderClient)
        {
            device->m_AudioRenderClient->Release();
            device->m_AudioRenderClient = 0;
        }

        if (!device->m_AudioClient)
        {
            device->m_AudioClient->Release();
            device->m_AudioClient = 0;
        }

        if (INVALID_HANDLE_VALUE != device->m_ClientBufferEvent)
        {
            CloseHandle(device->m_ClientBufferEvent);
            device->m_ClientBufferEvent = INVALID_HANDLE_VALUE;
        }

        delete device;
    }

    static dmSound::Result DeviceWasapiOpen(const dmSound::OpenDeviceParams* params, dmSound::HDevice* outdevice)
    {
        assert(params);
        assert(outdevice);

        CoInitializeEx(0, COINIT_APARTMENTTHREADED);

        IMMDeviceEnumerator* imm_enumerator;
        IMMDevice* imm_device;

        // Create a device enumerator
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&imm_enumerator);
        if (FAILED(hr))
        {
            dmLogError("Failed to create device enumerator");
            CheckAndPrintError(hr);
            return dmSound::RESULT_INIT_ERROR;
        }
        hr = imm_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &imm_device);
        if (FAILED(hr))
        {
            dmLogError("Failed to get default audio endpoint");
            CheckAndPrintError(hr);
            imm_enumerator->Release();
            return dmSound::RESULT_INIT_ERROR;
        }

        SoundDevice* device = new SoundDevice;

        // Activate the endpoint
        hr = imm_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&device->m_AudioClient);

        imm_device->Release();
        imm_enumerator->Release();

        if (FAILED(hr))
        {
            dmLogError("Failed to activate audio client");
            CheckAndPrintError(hr);
            DeleteDevice(device);
            return dmSound::RESULT_INIT_ERROR;
        }

        AudioClientProperties audioProps = {};
        audioProps.cbSize = sizeof(AudioClientProperties);
        audioProps.bIsOffload = false;
        audioProps.eCategory = AudioCategory_ForegroundOnlyMedia;

        hr = device->m_AudioClient->SetClientProperties(&audioProps);
        if (FAILED(hr))
        {
            dmLogError("Failed to set audio client properties");
            CheckAndPrintError(hr);
            DeleteDevice(device);
            return dmSound::RESULT_INIT_ERROR;
        }

        // While we can suggest the format, the SHARED mode will return the current format
        WAVEFORMATEX* pformat;
        hr = device->m_AudioClient->GetMixFormat(&pformat);
        if (FAILED(hr))
        {
            dmLogError("Failed to get mix format from device:");
            CheckAndPrintError(hr);
            DeleteDevice(device);
            return dmSound::RESULT_INIT_ERROR;
        }

        dmLogInfo("Mix format:");
        dmLogInfo("  wFormatTag:       %x  IEEE_FLOAT/PCM/EXTENSIBLE: %x / %x / %x", pformat->wFormatTag, WAVE_FORMAT_IEEE_FLOAT, WAVE_FORMAT_PCM, WAVE_FORMAT_EXTENSIBLE);
        dmLogInfo("  nChannels:        %d", pformat->nChannels);
        dmLogInfo("  nSamplesPerSec:   %d", pformat->nSamplesPerSec);
        dmLogInfo("  nAvgBytesPerSec:  %d", pformat->nAvgBytesPerSec);
        dmLogInfo("  nBlockAlign:      %d", pformat->nBlockAlign);
        dmLogInfo("  wBitsPerSample:   %d", pformat->wBitsPerSample);

        WAVEFORMATEX* closest = 0;
        hr = device->m_AudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, pformat, &closest);
        if (FAILED(hr))
        {
            dmLogError("Format not supported:");
            CheckAndPrintError(hr);
            if (closest)
            {
                dmLogInfo("Closest format:");
                dmLogInfo("  wFormatTag:       %x  IEEE_FLOAT/PCM/EXTENSIBLE: %x / %x / %x", pformat->wFormatTag, WAVE_FORMAT_IEEE_FLOAT, WAVE_FORMAT_PCM, WAVE_FORMAT_EXTENSIBLE);
                dmLogInfo("  nChannels:        %d", pformat->nChannels);
                dmLogInfo("  nSamplesPerSec:   %d", pformat->nSamplesPerSec);
                dmLogInfo("  nAvgBytesPerSec:  %d", pformat->nAvgBytesPerSec);
                dmLogInfo("  nBlockAlign:      %d", pformat->nBlockAlign);
                dmLogInfo("  wBitsPerSample:   %d", pformat->wBitsPerSample);
                pformat = closest;
            }
        }

        if (WAVE_FORMAT_EXTENSIBLE == pformat->wFormatTag)
        {
            WAVEFORMATEXTENSIBLE* wfex = (WAVEFORMATEXTENSIBLE*)pformat;
            if (wfex->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
                device->m_Format = WAVE_FORMAT_PCM;
            else if (wfex->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
                device->m_Format = WAVE_FORMAT_IEEE_FLOAT;
        }
        else {
            device->m_Format = pformat->wFormatTag;
        }

        if (device->m_Format != WAVE_FORMAT_PCM &&
            device->m_Format != WAVE_FORMAT_IEEE_FLOAT)
        {
            dmLogError("Data format type not supported: %x", device->m_Format);
            CheckAndPrintError(hr);
            DeleteDevice(device);
            return dmSound::RESULT_INIT_ERROR;
        }

        dmLogInfo("  -> Format:        %x  IEEE_FLOAT/PCM/EXTENSIBLE: %x / %x / %x", device->m_Format, WAVE_FORMAT_IEEE_FLOAT, WAVE_FORMAT_PCM, WAVE_FORMAT_EXTENSIBLE);
        device->m_MixFormat = pformat;

        REFERENCE_TIME buffer_duration = 0;

        // Initialize the AudioClient in Shared Mode with the user specified buffer
        hr = device->m_AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                                                0, // 100-nano second units
                                                0,
                                                device->m_MixFormat,
                                                0);
        if (FAILED(hr))
        {
            dmLogError("Failed to initialize audio client:");
            CheckAndPrintError(hr);
            DeleteDevice(device);
            return dmSound::RESULT_INIT_ERROR;
        }

        // Get the render client
        hr = device->m_AudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&device->m_AudioRenderClient);
        if (FAILED(hr))
        {
            dmLogError("Failed to get audio render client");
            CheckAndPrintError(hr);
            DeleteDevice(device);
            return dmSound::RESULT_INIT_ERROR;
        }

        device->m_ClientBufferEvent = CreateEventEx(0, 0, 0, EVENT_ALL_ACCESS);
        if (0 == device->m_ClientBufferEvent)
        {
            dmLogError("Failed to create audio client event");
            DeleteDevice(device);
            return dmSound::RESULT_INIT_ERROR;
        }

        hr = device->m_AudioClient->SetEventHandle(device->m_ClientBufferEvent);
        if (FAILED(hr))
        {
            dmLogError("Failed to set audio client event");
            CheckAndPrintError(hr);
            DeleteDevice(device);
            return dmSound::RESULT_INIT_ERROR;
        }

        hr = device->m_AudioClient->GetBufferSize(&device->m_FrameCount);
        if (FAILED(hr))
        {
            dmLogError("Failed to get audio device buffer size");
            CheckAndPrintError(hr);
            DeleteDevice(device);
            return dmSound::RESULT_INIT_ERROR;
        }

        *outdevice = device;
        return dmSound::RESULT_OK;
    }

    static void DeviceWasapiClose(dmSound::HDevice device)
    {
        DeleteDevice((SoundDevice*)device);

        CoUninitialize();
    }

    static uint32_t DeviceWasapiFreeBufferSlots(dmSound::HDevice _device)
    {
        assert(_device);
        SoundDevice* device = (SoundDevice*) _device;

        HRESULT hr = WaitForSingleObject(device->m_ClientBufferEvent, 250);
        if (WAIT_OBJECT_0 != hr)
        {
            return 0;
        }
        return 1;
    }

    static uint32_t DeviceWasapiGetAvailableFrames(dmSound::HDevice _device)
    {
        assert(_device);
        SoundDevice* device = (SoundDevice*) _device;

        uint32_t buffer_size = 0;
        uint32_t buffer_pos = 0;
        HRESULT hr = device->m_AudioClient->GetBufferSize(&buffer_size);
        if (FAILED(hr))
        {
            return 0;
        }

        hr = device->m_AudioClient->GetCurrentPadding(&buffer_pos);
        if (FAILED(hr))
        {
            return 0;
        }
        return buffer_size - buffer_pos;
    }

    // We get this call after we've returned a non zero value from the DeviceWasapiGetAvailableFrames()
    static dmSound::Result DeviceWasapiQueue(dmSound::HDevice _device, const int16_t* samples, uint32_t sample_count)
    {
        assert(_device);
        SoundDevice* device = (SoundDevice*) _device;

        uint32_t buffer_size = 0;
        uint32_t buffer_pos = 0;

        HRESULT hr = device->m_AudioClient->GetBufferSize(&buffer_size);
        if (FAILED(hr))
        {
            return dmSound::RESULT_OUT_OF_BUFFERS;
        }

        hr = device->m_AudioClient->GetCurrentPadding(&buffer_pos);
        if (FAILED(hr))
        {
            return dmSound::RESULT_OUT_OF_BUFFERS;
        }

        uint32_t frames_available = buffer_size - buffer_pos;

        if (sample_count < frames_available)
            frames_available = sample_count;

        uint8_t* out;
        hr = device->m_AudioRenderClient->GetBuffer(frames_available, &out);
        if (FAILED(hr))
        {
            return dmSound::RESULT_OUT_OF_BUFFERS;
        }

        for (int i = 0; i < frames_available; ++i)
        {
            if (device->m_Format == WAVE_FORMAT_IEEE_FLOAT)
            {
                float* fout = (float*)out;
                fout[i*2+0] = samples[i*2+0]/32768.0f;
                fout[i*2+1] = samples[i*2+1]/32768.0f;
            }
        }

        device->m_AudioRenderClient->ReleaseBuffer(frames_available, 0);

        return dmSound::RESULT_OK;
    }


    static void DeviceWasapiDeviceInfo(dmSound::HDevice _device, dmSound::DeviceInfo* info)
    {
        assert(_device);
        assert(info);
        SoundDevice* device = (SoundDevice*) _device;

        if (!device->m_AudioClient)
            return;

        info->m_MixRate = device->m_MixFormat->nSamplesPerSec;
        info->m_FrameCount = device->m_FrameCount;
    }

    static void DeviceWasapiStart(dmSound::HDevice _device)
    {
        assert(_device);
        SoundDevice* device = (SoundDevice*) _device;
        if (device->m_AudioClient)
        {
            HRESULT hr = device->m_AudioClient->Start();
            if (FAILED(hr))
            {
                dmLogError("Failed to start audio client");
                CheckAndPrintError(hr);
            }

            uint32_t buffer_size = 0;
            uint32_t buffer_pos = 0;
            device->m_AudioClient->GetBufferSize(&buffer_size);
            device->m_AudioClient->GetCurrentPadding(&buffer_pos);
        }
    }

    static void DeviceWasapiStop(dmSound::HDevice _device)
    {
        assert(_device);
        SoundDevice* device = (SoundDevice*) _device;
        if (device->m_AudioClient)
        {
            uint32_t buffer_size = 0;
            uint32_t buffer_pos = 0;
            uint8_t* out;
            device->m_AudioClient->GetBufferSize(&buffer_size);
            device->m_AudioClient->GetCurrentPadding(&buffer_pos);

            uint32_t frames_available = buffer_size;// buffer_size - buffer_pos;
            device->m_AudioRenderClient->GetBuffer(frames_available, &out);
            device->m_AudioRenderClient->ReleaseBuffer(frames_available, AUDCLNT_BUFFERFLAGS_SILENT);

            HRESULT hr = device->m_AudioClient->Stop();
            if (FAILED(hr))
            {
                dmLogError("Failed to stop audio client");
                CheckAndPrintError(hr);
            }
        }
    }

    DM_DECLARE_SOUND_DEVICE(DefaultSoundDevice, "default", DeviceWasapiOpen, DeviceWasapiClose, DeviceWasapiQueue,
                            DeviceWasapiFreeBufferSlots, DeviceWasapiGetAvailableFrames, DeviceWasapiDeviceInfo, DeviceWasapiStart, DeviceWasapiStop);
}
