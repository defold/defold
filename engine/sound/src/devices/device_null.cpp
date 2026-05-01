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

#include <stdint.h>
#include "sound.h"

namespace dmDeviceNull
{
    dmSound::Result DeviceNullOpen(const dmSound::OpenDeviceParams* params, dmSound::HDevice* device)
    {
        return dmSound::RESULT_OK;
    }

    void DeviceNullClose(dmSound::HDevice device)
    {
    }

    dmSound::Result DeviceNullQueue(dmSound::HDevice device, const void* samples, uint32_t sample_count)
    {
        return dmSound::RESULT_OK;
    }

    uint32_t DeviceNullFreeBufferSlots(dmSound::HDevice device)
    {
        return 0;
    }

    void DeviceNullDeviceInfo(dmSound::HDevice device, dmSound::DeviceInfo* info)
    {
        info->m_DSPImplementation = dmSound::DSPIMPL_TYPE_CPU;
    }

    void DeviceNullRestart(dmSound::HDevice device)
    {

    }

    void DeviceNullStop(dmSound::HDevice device)
    {

    }

    DM_DECLARE_SOUND_DEVICE(NullSoundDevice, "null", DeviceNullOpen, DeviceNullClose, DeviceNullQueue,
                            DeviceNullFreeBufferSlots, 0, DeviceNullDeviceInfo, DeviceNullRestart, DeviceNullStop);
}

