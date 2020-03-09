#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dlib/align.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/time.h>
#include <dlib/profile.h>
#include <dlib/memory.h>
#include "sound.h"

#include <nn/nn_Abort.h>
#include <nn/nn_Assert.h>
#include <nn/audio.h>

namespace dmSound
{

struct SoundDevice
{

    nn::audio::AudioOut                 m_AudioOut;
    nn::audio::AudioDeviceName          m_AudioDeviceName;
    nn::os::SystemEvent                 m_SystemEvent;
    dmArray<uint16_t*>                  m_Buffers;
    dmArray<nn::audio::AudioOutBuffer>  m_AudioOutBuffers;
    dmArray<nn::audio::AudioOutBuffer*> m_FreeAudioOutBuffers;
    uint32_t                            m_MixRate;
};

static const char* GetAudioOutStateName(nn::audio::AudioOutState state)
{
    switch (state) {
        case nn::audio::AudioOutState_Started:  return "Started";
        case nn::audio::AudioOutState_Stopped:  return "Stopped";
        default:                                return "Unknown";
    }
}

const char* GetSampleFormatName(nn::audio::SampleFormat format)
{
    switch (format) {
        case nn::audio::SampleFormat_Invalid:   return "Invalid";
        case nn::audio::SampleFormat_PcmInt8:   return "PcmInt8";
        case nn::audio::SampleFormat_PcmInt16:  return "PcmInt16";
        case nn::audio::SampleFormat_PcmInt24:  return "PcmInt24";
        case nn::audio::SampleFormat_PcmInt32:  return "PcmInt32";
        case nn::audio::SampleFormat_PcmFloat:  return "PcmFloat";
        default:                                return "Unknown";
    }
}

static void GetAudioDeviceName(dmSound::HDevice _device)
{
    SoundDevice* device = (SoundDevice*)_device;

    nn::audio::GetActiveAudioDeviceName(&device->m_AudioDeviceName);

    dmLogInfo("Device connected: %s", device->m_AudioDeviceName.name);

}

static dmSound::Result DeviceOpen(const dmSound::OpenDeviceParams* params, dmSound::HDevice* _device)
{
    SoundDevice* device = new SoundDevice;

    nn::audio::AudioOutInfo audioOutInfos[nn::audio::AudioOutCountMax];
    const int count = nn::audio::ListAudioOuts(audioOutInfos, sizeof(audioOutInfos) / sizeof(*audioOutInfos));
    for (int i = 0; i < count; ++i)
    {
        dmLogDebug("AudioOut  %2d: %s\n", i, audioOutInfos[i].name);

        // Verify that open and close can be performed.
        nn::audio::AudioOutParameter parameter;
        nn::audio::InitializeAudioOutParameter(&parameter);
        NN_ABORT_UNLESS(
            nn::audio::OpenAudioOut(&device->m_AudioOut, audioOutInfos[i].name, parameter).IsSuccess(),
            "Failed to open AudioOut."
        );
        nn::audio::CloseAudioOut(&device->m_AudioOut);
    }

    nn::audio::AudioOutParameter parameter;
    nn::audio::InitializeAudioOutParameter(&parameter);
    parameter.sampleRate = 48000;
    parameter.channelCount = 2;
    // parameter.channelCount = 6;  // For 5.1ch output, specify 6 for the number of channels.

    nn::Result result;
    if ((result = nn::audio::OpenDefaultAudioOut(&device->m_AudioOut, &device->m_SystemEvent, parameter)).IsFailure())
    {
        parameter.sampleRate = 0;
        parameter.channelCount = 0;
        NN_ABORT_UNLESS(
            nn::audio::OpenDefaultAudioOut(&device->m_AudioOut, &device->m_SystemEvent, parameter).IsSuccess(),
            "Failed to open AudioOut."
        );
    }

    GetAudioDeviceName(device);

    // Get the audio output properties.
    int channelCount = nn::audio::GetAudioOutChannelCount(&device->m_AudioOut);
    int sampleRate = nn::audio::GetAudioOutSampleRate(&device->m_AudioOut);
    nn::audio::SampleFormat sampleFormat = nn::audio::GetAudioOutSampleFormat(&device->m_AudioOut);

    dmLogDebug("AudioOut is opened\n  State: %s  %d channels @%d hz\n", GetAudioOutStateName(nn::audio::GetAudioOutState(&device->m_AudioOut)), channelCount, sampleRate);

    // This device assumes that the sample format is 16-bit.
    NN_ASSERT(sampleFormat == nn::audio::SampleFormat_PcmInt16);

    const int frameSampleCount = params->m_FrameCount;

    const size_t dataSize = frameSampleCount * channelCount * nn::audio::GetSampleByteSize(sampleFormat);
    const size_t bufferSize = DM_ALIGN(dataSize, nn::audio::AudioOutBuffer::SizeGranularity);
    const uint32_t buffer_count = params->m_BufferCount;

    device->m_MixRate = sampleRate;
    device->m_Buffers.SetCapacity(buffer_count);
    device->m_Buffers.SetSize(buffer_count);
    device->m_AudioOutBuffers.SetCapacity(buffer_count);
    device->m_AudioOutBuffers.SetSize(buffer_count);
    device->m_FreeAudioOutBuffers.SetCapacity(buffer_count);

    for (uint32_t i = 0; i < buffer_count; ++i)
    {
        uint16_t* buffer;
        dmMemory::AlignedMalloc((void**)&buffer, nn::audio::AudioOutBuffer::AddressAlignment, bufferSize);
        NN_ASSERT(buffer !=0);

        memset(buffer, 0, bufferSize);
        device->m_Buffers[i] = buffer;

        nn::audio::SetAudioOutBufferInfo(&device->m_AudioOutBuffers[i], device->m_Buffers[i], bufferSize, dataSize);
        nn::audio::AppendAudioOutBuffer(&device->m_AudioOut, &device->m_AudioOutBuffers[i]);
    }

    *_device = (void*)device;

    return dmSound::RESULT_OK;
}

static void DeviceStart(dmSound::HDevice _device)
{
    SoundDevice* device = (SoundDevice*)_device;
    if (nn::audio::AudioOutState_Stopped == nn::audio::GetAudioOutState(&device->m_AudioOut))
    {
        nn::Result result = nn::audio::StartAudioOut(&device->m_AudioOut);
        if (result.IsFailure())
        {
            dmLogError("Failed to start audio device, could not enable context!");
        }
        dmLogDebug("AudioOut is started\n  State: %s\n", GetAudioOutStateName(nn::audio::GetAudioOutState(&device->m_AudioOut)));
    }
}

static void DeviceStop(dmSound::HDevice _device)
{
    SoundDevice* device = (SoundDevice*)_device;
    if (nn::audio::AudioOutState_Started == nn::audio::GetAudioOutState(&device->m_AudioOut))
    {
        nn::audio::StopAudioOut(&device->m_AudioOut);
        dmLogDebug("AudioOut is stopped\n  State: %s\n", GetAudioOutStateName(nn::audio::GetAudioOutState(&device->m_AudioOut)));
    }
}

static void DeviceClose(dmSound::HDevice _device)
{
    SoundDevice* device = (SoundDevice*)_device;
    if (!device)
        return;

    DeviceStop(_device);

    nn::audio::CloseAudioOut(&device->m_AudioOut);
    nn::os::DestroySystemEvent(device->m_SystemEvent.GetBase());

    for (uint32_t i = 0; i < device->m_Buffers.Size(); ++i)
    {
        free(device->m_Buffers[i]);
    }
    delete device;
}

static dmSound::Result DeviceQueue(dmSound::HDevice _device, const int16_t* samples, uint32_t sample_count)
{
    SoundDevice* device = (SoundDevice*)_device;

    nn::audio::AudioOutBuffer* pAudioOutBuffer = device->m_FreeAudioOutBuffers.Back();
    device->m_FreeAudioOutBuffers.Pop();

    uint16_t* buffer = (uint16_t*)nn::audio::GetAudioOutBufferDataPointer(pAudioOutBuffer);
    memcpy(buffer, samples, sample_count * 2 * sizeof(int16_t));
    nn::audio::AppendAudioOutBuffer(&device->m_AudioOut, pAudioOutBuffer);

    return dmSound::RESULT_OK;
}

static uint32_t DeviceFreeBufferSlots(dmSound::HDevice _device)
{
    SoundDevice* device = (SoundDevice*)_device;

    if (!device->m_SystemEvent.TryWait())
    {
        return 0;
    }

    // Store the free buffers for consumption
    nn::audio::AudioOutBuffer* pAudioOutBuffer = nn::audio::GetReleasedAudioOutBuffer(&device->m_AudioOut);
    while (pAudioOutBuffer) {
        device->m_FreeAudioOutBuffers.Push(pAudioOutBuffer);
        pAudioOutBuffer = nn::audio::GetReleasedAudioOutBuffer(&device->m_AudioOut);
    }

    return device->m_FreeAudioOutBuffers.Size();
}

static void DeviceDeviceInfo(dmSound::HDevice _device, dmSound::DeviceInfo* info)
{
    assert(_device);
    assert(info);
    SoundDevice* device = (SoundDevice*)_device;
    info->m_MixRate = device->m_MixRate;
}

DM_DECLARE_SOUND_DEVICE(DefaultSoundDevice, "default", DeviceOpen, DeviceClose, DeviceQueue, DeviceFreeBufferSlots, DeviceDeviceInfo, DeviceStart, DeviceStop);
}
