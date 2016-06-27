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

    dmSound::Result DeviceNullQueue(dmSound::HDevice device, const int16_t* samples, uint32_t sample_count)
    {
        return dmSound::RESULT_OK;
    }

    uint32_t DeviceNullFreeBufferSlots(dmSound::HDevice device)
    {
        return 0;
    }

    void DeviceNullDeviceInfo(dmSound::HDevice device, dmSound::DeviceInfo* info)
    {
    }

    void DeviceNullRestart(dmSound::HDevice device)
    {

    }

    void DeviceNullStop(dmSound::HDevice device)
    {

    }

    DM_DECLARE_SOUND_DEVICE(NullSoundDevice, "null", DeviceNullOpen, DeviceNullClose, DeviceNullQueue, DeviceNullFreeBufferSlots, DeviceNullDeviceInfo, DeviceNullRestart, DeviceNullStop);
}

