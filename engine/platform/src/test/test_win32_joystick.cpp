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
#include "../win32/joystick_driver.h"
#include "../platform_window_win32.h"

static bool g_Connected[XUSER_MAX_COUNT];
static XINPUT_STATE g_State[XUSER_MAX_COUNT];

static DWORD WINAPI State(DWORD user, XINPUT_STATE* state)
{
    *state = g_State[user];
    return g_Connected[user] ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
}

static DWORD WINAPI Capabilities(DWORD user, DWORD, XINPUT_CAPABILITIES* caps)
{
    caps->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
    return g_Connected[user] ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
}

static DWORD WINAPI Extended(DWORD, DWORD user, DWORD, dmPlatform::Win32XInputCapabilities* caps)
{
    if (user != 3)
    {
        return ERROR_NOT_SUPPORTED;
    }

    caps->m_Vendor = 0x045e;
    caps->m_Product = 0x028e;
    return ERROR_SUCCESS;
}

// Replace the OS drivers at link time. Window notifications, slot allocation and polling are real.
namespace dmPlatform
{
    void Win32OpenJoystickDrivers(HINSTANCE, Win32JoystickDrivers* drivers)
    {
        drivers->m_State = State;
        drivers->m_Capabilities = Capabilities;
        drivers->m_Extended = Extended;
    }

}

struct GamepadEvents
{
    unsigned m_Connected[16], m_Disconnected[16];
};

static void OnGamepad(void* data, int slot, WindowGamepadEvent event)
{
    GamepadEvents* events = (GamepadEvents*)data;
    if (event == WINDOW_GAMEPAD_EVENT_CONNECTED)
    {
        ++events->m_Connected[slot];
    }
    else if (event == WINDOW_GAMEPAD_EVENT_DISCONNECTED)
    {
        ++events->m_Disconnected[slot];
    }
}

// Verifies native XInput user IDs survive slot assignment, duplicate arrivals, removal and reuse.
TEST(Win32Joystick, HotplugAndIdentity)
{
    HWindow window = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, window);
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    params.m_Hidden = true;
    GamepadEvents events = {};
    g_Connected[3] = true;
    dmPlatform::SetGamepadEventCallback(window, OnGamepad, &events);
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    ASSERT_EQ(1U, events.m_Connected[0]);
    ASSERT_STREQ("030000005e0400008e02000000000000", dmPlatform::GetJoystickDeviceGuid(window, 0));

    g_Connected[1] = true;
    HWND hwnd = dmPlatform::GetWindowsHWND(window);
    SendMessageW(hwnd, WM_DEVICECHANGE, 0, 0);
    dmPlatform::PollEvents(window);
    ASSERT_EQ(1U, events.m_Connected[0]);
    ASSERT_EQ(1U, events.m_Connected[1]);
    ASSERT_STREQ("78696e70757401000000000000000000", dmPlatform::GetJoystickDeviceGuid(window, 1));
    g_State[3].Gamepad.wButtons = XINPUT_GAMEPAD_A;
    g_Connected[1] = false;
    dmPlatform::PollEvents(window);
    uint8_t buttons[2] = {};
    ASSERT_EQ(2U, dmPlatform::GetJoystickButtons(window, 0, buttons, sizeof(buttons)));
    ASSERT_EQ(1U, buttons[0]);
    ASSERT_EQ(1U, events.m_Disconnected[1]);
    ASSERT_FALSE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, 1));
    g_Connected[2] = true;
    SendMessageW(hwnd, WM_DEVICECHANGE, 0, 0);
    dmPlatform::PollEvents(window);
    ASSERT_EQ(2U, events.m_Connected[1]);
    ASSERT_EQ(1U, events.m_Connected[0]);
    ASSERT_EQ(0U, events.m_Disconnected[0]);
    dmPlatform::DeleteWindow(window);
}

// Registering on an open window must let HID finish installing its driver before reporting existing gamepads.
TEST(Win32Joystick, RegisterWithConnectedGamepad)
{
    memset(g_Connected, 0, sizeof(g_Connected));
    g_Connected[0] = true;
    HWindow window = dmPlatform::NewWindow();
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    params.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));

    GamepadEvents events = {};
    dmPlatform::SetGamepadEventCallback(window, OnGamepad, &events);
    ASSERT_EQ(0U, events.m_Connected[0]);
    ASSERT_TRUE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, 0));
    ASSERT_EQ(1U, events.m_Connected[0]);
    dmPlatform::PollEvents(window);
    ASSERT_EQ(1U, events.m_Connected[0]);
    dmPlatform::DeleteWindow(window);
    g_Connected[0] = false;
}

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
    DIJOYSTATE2 state = {};
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
