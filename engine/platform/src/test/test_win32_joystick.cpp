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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../win32/joystick_packet.h"

// Verifies the raw XInput packet consumed by the existing automatic, database, and legacy mappings.
TEST(Win32Joystick, XInputPacket)
{
    XINPUT_GAMEPAD state = {};
    state.sThumbLX = -32768;
    state.sThumbRY = 32767;
    state.bRightTrigger = 255;
    state.wButtons = XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_RIGHT;
    dmPlatform::Win32JoystickPacket packet = {};
    dmPlatform::DecodeWin32XInput(state, &packet);
    ASSERT_EQ(6U, packet.m_AxisCount);
    ASSERT_EQ(-1.0f, packet.m_Axes[0]);
    ASSERT_EQ(-0.5f / 32767.5f, packet.m_Axes[1]);
    ASSERT_EQ(-1.0f, packet.m_Axes[3]);
    ASSERT_EQ(-1.0f, packet.m_Axes[4]);
    ASSERT_EQ(1.0f, packet.m_Axes[5]);
    ASSERT_EQ(1U, packet.m_HatCount);
    ASSERT_EQ(3U, packet.m_Hats[0]);
    const uint8_t expected[] = { 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0 };
    ASSERT_EQ(sizeof(expected), packet.m_ButtonCount);
    ASSERT_EQ(0, memcmp(expected, packet.m_Buttons, sizeof(expected)));
}

// Verifies sparse DirectInput axes, sliders, POV diagonals, neutral hats, and appended buttons.
TEST(Win32Joystick, DirectInputPacket)
{
    DIJOYSTATE state = {};
    state.lZ = 32767;
    state.rglSlider[0] = -32768;
    state.rgdwPOV[0] = 22500;
    state.rgdwPOV[1] = 0xffffffff;
    state.rgbButtons[1] = 0x80;
    dmPlatform::Win32DirectControls controls = { 0x44, 2, 2 };
    dmPlatform::Win32JoystickPacket packet = {};
    dmPlatform::DecodeWin32DirectInput(state, controls, &packet);
    ASSERT_EQ(2U, packet.m_AxisCount);
    ASSERT_EQ(1.0f, packet.m_Axes[0]);
    ASSERT_EQ(-1.0f, packet.m_Axes[1]);
    ASSERT_EQ(2U, packet.m_HatCount);
    ASSERT_EQ(12U, packet.m_Hats[0]);
    ASSERT_EQ(0U, packet.m_Hats[1]);
    const uint8_t expected[] = { 0, 1, 0, 0, 1, 1, 0, 0, 0, 0 };
    ASSERT_EQ(sizeof(expected), packet.m_ButtonCount);
    ASSERT_EQ(0, memcmp(expected, packet.m_Buttons, sizeof(expected)));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
