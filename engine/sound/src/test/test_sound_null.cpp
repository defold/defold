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

#include <stdlib.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../sound.h"

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

static dmSound::Result DeviceQueue(dmSound::HDevice device, const void* samples, uint32_t sample_count)
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

DM_DECLARE_SOUND_DEVICE(TestNullDevice, "device", DeviceOpen, DeviceClose, DeviceQueue,
                        DeviceFreeBufferSlots, 0, DeviceDeviceInfo, DeviceRestart, DeviceStop);

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
