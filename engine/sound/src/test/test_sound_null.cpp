#include <stdlib.h>
#include <gtest/gtest.h>
#include <sound/sound.h>

// Basically just testing that the linkage works
TEST(dmSoundTestNull, Ok)
{
    ASSERT_EQ(dmSound::RESULT_OK, 0);
}


static dmSound::Result DeviceOpen(const dmSound::OpenDeviceParams* params, dmSound::HDevice* device)
{
    (void)params;
    (void)device;
    return dmSound::RESULT_OK;
}

static void DeviceClose(dmSound::HDevice device)
{
    (void)device;
}

static dmSound::Result DeviceQueue(dmSound::HDevice device, const int16_t* samples, uint32_t sample_count)
{
    (void)device;
    (void)samples;
    (void)sample_count;
    return dmSound::RESULT_OK;
}

static uint32_t DeviceFreeBufferSlots(dmSound::HDevice device)
{
    (void)device;
    return 0;
}

static void DeviceDeviceInfo(dmSound::HDevice device, dmSound::DeviceInfo* info)
{
    (void)device;
    info->m_MixRate = 44100;
}

static void DeviceRestart(dmSound::HDevice device)
{
    (void)device;
}

static void DeviceStop(dmSound::HDevice device)
{
    (void)device;
}

DM_DECLARE_SOUND_DEVICE(TestNullDevice, "device", DeviceOpen, DeviceClose, DeviceQueue, DeviceFreeBufferSlots, DeviceDeviceInfo, DeviceRestart, DeviceStop);

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
