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

extern "C" {
    // Implementation in library_sound.js
    int dmDeviceJSOpen(int buffers);
    int dmGetDeviceSampleRate(int device);
    void dmDeviceJSQueue(int device, const float* samples, uint32_t sample_count);
    int dmDeviceJSFreeBufferSlots(int device);
}


namespace dmDeviceJS
{
    struct JSDevice
    {
        int devId;
        bool isStarted;
    };

    dmSound::Result DeviceJSOpen(const dmSound::OpenDeviceParams* params, dmSound::HDevice* device)
    {
        assert(params);
        assert(device);
        JSDevice *dev = new JSDevice();
        int deviceId = dmDeviceJSOpen(params->m_BufferCount);
        if (deviceId < 0)
        {
            return dmSound::RESULT_DEVICE_NOT_FOUND;
        }
        dev->devId = deviceId;
        dev->isStarted = false;
        *device = dev;

        dmLogInfo("Info");
        dmLogInfo("  nSamplesPerSec:   %d", dmGetDeviceSampleRate(deviceId));

        return dmSound::RESULT_OK;
    }

    void DeviceJSClose(dmSound::HDevice device)
    {
        assert(device);
        delete (JSDevice*)(device);
    }

    dmSound::Result DeviceJSQueue(dmSound::HDevice device, const void* samples, uint32_t sample_count)
    {
        assert(device);
        JSDevice *dev = (JSDevice*) device;
        if (!dev->isStarted)
        {
            return dmSound::RESULT_INIT_ERROR;
        }
        dmDeviceJSQueue(dev->devId, (const float*)samples, sample_count);
        return dmSound::RESULT_OK;
    }

    uint32_t DeviceJSFreeBufferSlots(dmSound::HDevice device)
    {
        assert(device);
        JSDevice *dev = (JSDevice*) device;
        return dmDeviceJSFreeBufferSlots(dev->devId);
    }

    void DeviceJSDeviceInfo(dmSound::HDevice device, dmSound::DeviceInfo* info)
    {
        assert(device);
        assert(info);
        JSDevice *dev = (JSDevice*) device;
        info->m_MixRate = dmGetDeviceSampleRate(dev->devId);

        info->m_UseNonInterleaved = 1;
        info->m_UseFloats = 1;
        info->m_UseNormalized = 1;
    }

    void DeviceJSStart(dmSound::HDevice device)
    {
        assert(device);
        JSDevice *dev = (JSDevice*) device;
        dev->isStarted = true;
    }

    void DeviceJSStop(dmSound::HDevice device)
    {
        assert(device);
        JSDevice *dev = (JSDevice*) device;
        dev->isStarted = false;
    }

    DM_DECLARE_SOUND_DEVICE(DefaultSoundDevice, "default", DeviceJSOpen, DeviceJSClose, DeviceJSQueue,
                            DeviceJSFreeBufferSlots, 0, DeviceJSDeviceInfo, DeviceJSStart, DeviceJSStop);
}
