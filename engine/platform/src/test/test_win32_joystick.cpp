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

#define CINTERFACE
#include "../win32/joystick_driver.h"
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../platform_window_win32.h"

static bool g_Connected[XUSER_MAX_COUNT];
static XINPUT_STATE g_State[XUSER_MAX_COUNT];
static DWORD g_StateResult[XUSER_MAX_COUNT];
static IDirectInput8W* g_DirectInput;
static IDirectInputDevice8W g_DirectDevice;
static HRESULT g_DirectStatus, g_DirectRead;

static DWORD WINAPI State(DWORD user, XINPUT_STATE* state)
{
    *state = g_State[user];
    return g_Connected[user] ? g_StateResult[user] : ERROR_DEVICE_NOT_CONNECTED;
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
        if (g_DirectInput)
        {
            drivers->m_Direct = g_DirectInput;
            return;
        }

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

struct DeleteOnGamepad
{
    HWindow            m_Window;
    HWindow            m_Other;
    WindowGamepadEvent m_Event;
};

static void DeleteFromGamepadCallback(void* data, int, WindowGamepadEvent event)
{
    DeleteOnGamepad* targets = (DeleteOnGamepad*)data;
    if (event == targets->m_Event)
    {
        dmPlatform::DeleteWindow(targets->m_Window);
        targets->m_Window = 0;
        if (targets->m_Other)
        {
            dmPlatform::DeleteWindow(targets->m_Other);
            targets->m_Other = 0;
        }
    }
}

// Deleting this callback's window and its successor must still deliver the hotplug to the remaining window.
TEST(Win32Joystick, DeleteWindowsInHotplugCallback)
{
    HWindow survivor = dmPlatform::NewWindow();
    HWindow other = dmPlatform::NewWindow();
    HWindow current = dmPlatform::NewWindow();
    ASSERT_TRUE(survivor && other && current);
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    params.m_Hidden = true;
    GamepadEvents surviving = {}, removed = {};
    DeleteOnGamepad targets = { current, other, WINDOW_GAMEPAD_EVENT_CONNECTED };
    dmPlatform::SetGamepadEventCallback(survivor, OnGamepad, &surviving);
    dmPlatform::SetGamepadEventCallback(other, OnGamepad, &removed);
    dmPlatform::SetGamepadEventCallback(current, DeleteFromGamepadCallback, &targets);
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(survivor, params));
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(other, params));
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(current, params));

    g_Connected[0] = true;
    SendMessageW(dmPlatform::GetWindowsHWND(survivor), WM_DEVICECHANGE, 0, 0);
    dmPlatform::PollEvents(survivor);
    EXPECT_EQ((HWindow)0, targets.m_Window);
    EXPECT_EQ((HWindow)0, targets.m_Other);
    EXPECT_EQ(0u, removed.m_Connected[0]);
    EXPECT_EQ(1u, surviving.m_Connected[0]);

    // A raw presence query can deliver a disconnect without going through PollEvents.
    targets.m_Window = survivor;
    targets.m_Event = WINDOW_GAMEPAD_EVENT_DISCONNECTED;
    dmPlatform::SetGamepadEventCallback(survivor, DeleteFromGamepadCallback, &targets);
    g_Connected[0] = false;
    EXPECT_FALSE(dmPlatform::GetDeviceState(survivor, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, 0));
    EXPECT_EQ((HWindow)0, targets.m_Window);
}

// Deleting the last window on discovery must allow XInput enumeration to finish and the next window to start afresh.
TEST(Win32Joystick, DeleteLastWindowDuringEnumeration)
{
    HWindow window = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, window);
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    params.m_Hidden = true;
    DeleteOnGamepad targets = { window, 0, WINDOW_GAMEPAD_EVENT_CONNECTED };
    dmPlatform::SetGamepadEventCallback(window, DeleteFromGamepadCallback, &targets);
    g_Connected[0] = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    EXPECT_EQ((HWindow)0, targets.m_Window);

    window = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, window);
    GamepadEvents events = {};
    dmPlatform::SetGamepadEventCallback(window, OnGamepad, &events);
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    EXPECT_EQ(1u, events.m_Connected[0]);
    dmPlatform::DeleteWindow(window);
    g_Connected[0] = false;
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
    // Change each control after the last window poll to exercise all three direct reads.
    g_State[3].Gamepad.wButtons = XINPUT_GAMEPAD_A;
    uint8_t buttons[2] = {};
    ASSERT_EQ(2U, dmPlatform::GetJoystickButtons(window, 0, buttons, sizeof(buttons)));
    ASSERT_EQ(1U, buttons[0]);
    g_State[3].Gamepad.sThumbLX = 32767;
    float axis;
    ASSERT_EQ(1U, dmPlatform::GetJoystickAxes(window, 0, &axis, 1));
    ASSERT_EQ(1.0f, axis);
    g_State[3].Gamepad.wButtons = XINPUT_GAMEPAD_DPAD_UP;
    uint8_t hat;
    ASSERT_EQ(1U, dmPlatform::GetJoystickHats(window, 0, &hat, 1));
    ASSERT_EQ(1U, hat);
    g_Connected[1] = false;
    ASSERT_EQ(0U, dmPlatform::GetJoystickButtons(window, 1, buttons, sizeof(buttons)));
    ASSERT_EQ(1U, events.m_Disconnected[1]);
    ASSERT_FALSE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, 1));
    g_Connected[2] = true;
    SendMessageW(hwnd, WM_DEVICECHANGE, 0, 0);
    dmPlatform::PollEvents(window);
    ASSERT_EQ(2U, events.m_Connected[1]);
    ASSERT_EQ(1U, events.m_Connected[0]);
    ASSERT_EQ(0U, events.m_Disconnected[0]);
    // Identity queries must observe removal before the next window poll.
    g_Connected[3] = g_Connected[2] = false;
    EXPECT_FALSE(dmPlatform::GetJoystickDeviceName(window, 0));
    EXPECT_FALSE(dmPlatform::GetJoystickDeviceGuid(window, 1));
    EXPECT_EQ(1U, events.m_Disconnected[0]);
    EXPECT_EQ(2U, events.m_Disconnected[1]);
    dmPlatform::DeleteWindow(window);
}

// A newly connected controller must recover from an unavailable first sample without another device change.
TEST(Win32Joystick, DiscoverBeforeFirstStateIsReady)
{
    HWindow window = dmPlatform::NewWindow();
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    params.m_Hidden = true;
    GamepadEvents events = {};
    dmPlatform::SetGamepadEventCallback(window, OnGamepad, &events);
    g_Connected[0] = true;
    g_StateResult[0] = ERROR_NOT_READY;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    EXPECT_EQ(1u, events.m_Connected[0]);
    EXPECT_TRUE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, 0));

    g_StateResult[0] = ERROR_SUCCESS;
    g_State[0].Gamepad.wButtons = XINPUT_GAMEPAD_A;
    dmPlatform::PollEvents(window);
    uint8_t button = 0;
    EXPECT_EQ(1u, dmPlatform::GetJoystickButtons(window, 0, &button, 1));
    EXPECT_EQ(1u, button);
    EXPECT_EQ(1u, events.m_Connected[0]);
    dmPlatform::DeleteWindow(window);
    g_State[0] = XINPUT_STATE();
    g_Connected[0] = false;
}

// Unreadable XInput samples must release sticks, triggers, buttons and hats until a fresh sample arrives.
TEST(Win32Joystick, FailedStateReleasesControls)
{
    HWindow window = dmPlatform::NewWindow();
    g_Connected[0] = true;
    g_State[0] = XINPUT_STATE();
    float centered_axes[6] = {};
    EXPECT_EQ(6u, dmPlatform::GetJoystickAxes(window, 0, centered_axes, 6));
    XINPUT_GAMEPAD& pad = g_State[0].Gamepad;
    pad.sThumbLX = 32767;
    pad.bRightTrigger = 255;
    pad.wButtons = XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_DPAD_UP;

    float axes[6] = {};
    uint8_t button = 0, hat = 0;
    EXPECT_EQ(6u, dmPlatform::GetJoystickAxes(window, 0, axes, 6));
    EXPECT_EQ(1.0f, axes[0]);
    EXPECT_EQ(1.0f, axes[5]);
    EXPECT_EQ(1u, dmPlatform::GetJoystickButtons(window, 0, &button, 1));
    EXPECT_EQ(1u, button);
    EXPECT_EQ(1u, dmPlatform::GetJoystickHats(window, 0, &hat, 1));
    EXPECT_EQ(1u, hat);

    g_StateResult[0] = ERROR_GEN_FAILURE;
    EXPECT_TRUE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, 0));
    EXPECT_EQ(6u, dmPlatform::GetJoystickAxes(window, 0, axes, 6));
    EXPECT_EQ(centered_axes[0], axes[0]);
    EXPECT_EQ(-1.0f, axes[5]);
    EXPECT_EQ(1u, dmPlatform::GetJoystickButtons(window, 0, &button, 1));
    EXPECT_EQ(0u, button);
    EXPECT_EQ(1u, dmPlatform::GetJoystickHats(window, 0, &hat, 1));
    EXPECT_EQ(0u, hat);

    g_StateResult[0] = ERROR_SUCCESS;
    EXPECT_EQ(1u, dmPlatform::GetJoystickButtons(window, 0, &button, 1));
    EXPECT_EQ(1u, button);
    dmPlatform::DeleteWindow(window);
    g_State[0] = XINPUT_STATE();
    g_Connected[0] = false;
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

// Failed reads must release held controls while preserving the device until it is unplugged.
TEST(Win32Joystick, DirectInputDisconnectAndReconnect)
{
    IDirectInputDevice8WVtbl device = {};
    device.SetDataFormat = [](IDirectInputDevice8W*, LPCDIDATAFORMAT) {
                               return DI_OK;
                           };
    device.SetCooperativeLevel = [](IDirectInputDevice8W*, HWND, DWORD) {
                                     return DI_OK;
                                 };
    device.EnumObjects = [](IDirectInputDevice8W*, LPDIENUMDEVICEOBJECTSCALLBACKW callback, void* data, DWORD) {
                             DIDEVICEOBJECTINSTANCEW control = {};
                             control.dwType = DIDFT_ABSAXIS;
                             control.guidType = GUID_XAxis;
                             callback(&control, data);
                             control.dwType = DIDFT_BUTTON;
                             callback(&control, data);
                             control.dwType = DIDFT_POV;
                             callback(&control, data);
                             return DI_OK;
                         };
    device.SetProperty = [](IDirectInputDevice8W*, REFGUID, LPCDIPROPHEADER) {
                             return DI_OK;
                         };
    device.Acquire = device.Unacquire = device.Poll = [](IDirectInputDevice8W*) {
                                                          return DI_OK;
                                                      };
    device.Release = [](IDirectInputDevice8W*) {
                         return 0UL;
                     };
    device.GetDeviceState = [](IDirectInputDevice8W*, DWORD size, void* data) {
                                memset(data, 0, size);
                                DIJOYSTATE2* state = (DIJOYSTATE2*)data;
                                state->lX = 32767;
                                state->rgbButtons[0] = 0x80;
                                state->rgdwPOV[0] = 9000;
                                return g_DirectRead;
                            };
    g_DirectDevice.lpVtbl = &device;
    IDirectInput8WVtbl driver = {};
    driver.CreateDevice = [](IDirectInput8W*, REFGUID, IDirectInputDevice8W** result, IUnknown*) {
                              *result = &g_DirectDevice;
                              return DI_OK;
                          };
    driver.EnumDevices = [](IDirectInput8W*, DWORD, LPDIENUMDEVICESCALLBACKW callback, void* data, DWORD) {
                             if (g_DirectStatus == DI_OK)
                             {
                                 DIDEVICEINSTANCEW instance = {};
                                 instance.dwSize = sizeof(instance);
                                 instance.tszInstanceName[0] = L'J';
                                 callback(&instance, data);
                             }

                             return DI_OK;
                         };
    driver.GetDeviceStatus = [](IDirectInput8W*, REFGUID) {
                                 return g_DirectStatus;
                             };
    driver.Release = [](IDirectInput8W*) {
                         return 0UL;
                     };
    IDirectInput8W direct = { &driver };
    g_DirectInput = &direct;
    g_DirectRead = g_DirectStatus = DI_OK;

    HWindow window = dmPlatform::NewWindow();
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    params.m_Hidden = true;
    GamepadEvents events = {};
    dmPlatform::SetGamepadEventCallback(window, OnGamepad, &events);
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    EXPECT_EQ(1U, events.m_Connected[0]);
    float axis = 0.0f;
    uint8_t button = 0, hat = 0;
    EXPECT_EQ(1U, dmPlatform::GetJoystickAxes(window, 0, &axis, 1));
    EXPECT_EQ(1.0f, axis);
    EXPECT_EQ(1U, dmPlatform::GetJoystickButtons(window, 0, &button, 1));
    EXPECT_EQ(1U, button);
    EXPECT_EQ(1U, dmPlatform::GetJoystickHats(window, 0, &hat, 1));
    EXPECT_EQ(2U, hat);
    g_DirectRead = DIERR_INPUTLOST;
    dmPlatform::PollEvents(window);
    EXPECT_TRUE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, 0));
    EXPECT_EQ(1U, dmPlatform::GetJoystickAxes(window, 0, &axis, 1));
    EXPECT_EQ(0.0f, axis);
    EXPECT_EQ(1U, dmPlatform::GetJoystickButtons(window, 0, &button, 1));
    EXPECT_EQ(0U, button);
    EXPECT_EQ(1U, dmPlatform::GetJoystickHats(window, 0, &hat, 1));
    EXPECT_EQ(0U, hat);
    g_DirectRead = DI_OK;
    EXPECT_EQ(1U, dmPlatform::GetJoystickButtons(window, 0, &button, 1));
    EXPECT_EQ(1U, button);
    g_DirectRead = DIERR_INPUTLOST;
    g_DirectStatus = DI_NOTATTACHED;
    EXPECT_FALSE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, 0));
    dmPlatform::PollEvents(window);
    EXPECT_EQ(1U, events.m_Disconnected[0]);
    g_DirectRead = g_DirectStatus = DI_OK;
    SendMessageW(dmPlatform::GetWindowsHWND(window), WM_DEVICECHANGE, 0, 0);
    dmPlatform::PollEvents(window);
    EXPECT_EQ(2U, events.m_Connected[0]);
    dmPlatform::DeleteWindow(window);
    g_DirectInput = 0;
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
