#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/time.h>
#include <dlib/profile.h>

#include "sound.h"

extern "C" {
    // Implementation in library_sound.js
    int dmDeviceJSOpen(int buffers);
    int dmGetDeviceSampleRate(int device);
    void dmDeviceJSQueue(int device, const int16_t* samples, uint32_t sample_count);
    int dmDeviceJSFreeBufferSlots(int device);
}


namespace dmDeviceJS
{
    struct JSDevice
    {
        int devId;
    };

    dmSound::Result DeviceJSOpen(const dmSound::OpenDeviceParams* params, dmSound::HDevice* device)
    {
        JSDevice *dev = new JSDevice();
        dev->devId = dmDeviceJSOpen(params->m_BufferCount);
        *device = dev;
        return dmSound::RESULT_OK;
    }

    void DeviceJSClose(dmSound::HDevice device)
    {
        delete (JSDevice*)(device);
    }

    dmSound::Result DeviceJSQueue(dmSound::HDevice device, const int16_t* samples, uint32_t sample_count)
    {
        JSDevice *dev = (JSDevice*) device;
        dmDeviceJSQueue(dev->devId, samples, sample_count);
        return dmSound::RESULT_OK;
    }

    uint32_t DeviceJSFreeBufferSlots(dmSound::HDevice device)
    {
        JSDevice *dev = (JSDevice*) device;
        return dmDeviceJSFreeBufferSlots(dev->devId);
    }

    void DeviceJSDeviceInfo(dmSound::HDevice device, dmSound::DeviceInfo* info)
    {
        JSDevice *dev = (JSDevice*) device;
        info->m_MixRate = dmGetDeviceSampleRate(dev->devId);
    }

    void DeviceJSRestart(dmSound::HDevice device)
    {

    }

    void DeviceJSStop(dmSound::HDevice device)
    {

    }

    DM_DECLARE_SOUND_DEVICE(DefaultSoundDevice, "default", DeviceJSOpen, DeviceJSClose, DeviceJSQueue, DeviceJSFreeBufferSlots, DeviceJSDeviceInfo, DeviceJSRestart, DeviceJSStop);
}
