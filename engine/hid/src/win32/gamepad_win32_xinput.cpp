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

#include <string.h>

#include <dlib/log.h>
#include <dlib/safe_windows.h>

#include <Xinput.h>

#include "../hid_private.h"
#include "gamepad_win32_private.h"

namespace dmHID
{
    static const uint32_t INVALID_XINPUT_INDEX = 0xffffffff;

    typedef DWORD (WINAPI *XInputGetStateFn)(DWORD dwUserIndex, XINPUT_STATE* pState);

    struct XInputGamepadDevice
    {
        IGameInputDevice* m_GameInputDevice;
        uint32_t          m_XInputIndex;
    };

    struct XInputGamepadContext
    {
        HMODULE             m_XInputModule;
        XInputGetStateFn    m_XInputGetState;
        XInputGamepadDevice m_Devices[MAX_GAMEPAD_COUNT];
    };

    static XInputGamepadContext* g_XInputGamepadContext = 0;

    static float NormalizeXInputThumbstick(SHORT value)
    {
        float result = value < 0 ? (float) value / 32768.0f : (float) value / 32767.0f;
        if (result > 1.0f) return 1.0f;
        if (result < -1.0f) return -1.0f;
        return result;
    }

    static bool LoadXInput(XInputGamepadContext* context)
    {
        const char* dll_names[] =
        {
            "xinput1_4.dll",
            "xinput9_1_0.dll",
            "xinput1_3.dll",
        };

        for (uint32_t i = 0; i < sizeof(dll_names) / sizeof(dll_names[0]); ++i)
        {
            context->m_XInputModule = LoadLibraryA(dll_names[i]);
            if (context->m_XInputModule == 0)
            {
                continue;
            }

            context->m_XInputGetState = (XInputGetStateFn) GetProcAddress(context->m_XInputModule, "XInputGetState");
            if (context->m_XInputGetState != 0)
            {
                return true;
            }

            FreeLibrary(context->m_XInputModule);
            context->m_XInputModule = 0;
        }

        return false;
    }

    static bool IsXInputIndexAssigned(XInputGamepadContext* context, uint32_t xinput_index)
    {
        for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            XInputGamepadDevice* device = &context->m_Devices[i];
            if (device->m_GameInputDevice != 0 && device->m_XInputIndex == xinput_index)
            {
                return true;
            }
        }
        return false;
    }

    static XInputGamepadDevice* FindXInputDevice(XInputGamepadContext* context, IGameInputDevice* gameinput_device)
    {
        for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            XInputGamepadDevice* device = &context->m_Devices[i];
            if (device->m_GameInputDevice == gameinput_device)
            {
                return device;
            }
        }
        return 0;
    }

    static XInputGamepadDevice* FindFreeXInputDevice(XInputGamepadContext* context)
    {
        for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            XInputGamepadDevice* device = &context->m_Devices[i];
            if (device->m_GameInputDevice == 0)
            {
                return device;
            }
        }
        return 0;
    }

    static void SetXInputButton(GamepadPacket& packet, uint32_t button, WORD buttons, WORD mask)
    {
        if (buttons & mask)
        {
            packet.m_Buttons[button / 32] |= 1 << (button % 32);
        }
        else
        {
            packet.m_Buttons[button / 32] &= ~(1 << (button % 32));
        }
    }

    bool GamepadXInputInitialize(HContext context)
    {
        (void) context;
        if (g_XInputGamepadContext != 0)
        {
            return true;
        }

        XInputGamepadContext* xinput_context = new XInputGamepadContext();
        memset(xinput_context, 0, sizeof(*xinput_context));
        for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            xinput_context->m_Devices[i].m_XInputIndex = INVALID_XINPUT_INDEX;
        }

        LoadXInput(xinput_context);
        g_XInputGamepadContext = xinput_context;
        return true;
    }

    void GamepadXInputFinalize(HContext context)
    {
        (void) context;
        XInputGamepadContext* xinput_context = g_XInputGamepadContext;
        if (xinput_context == 0)
        {
            return;
        }

        if (xinput_context->m_XInputModule != 0)
        {
            FreeLibrary(xinput_context->m_XInputModule);
            xinput_context->m_XInputModule = 0;
        }

        delete xinput_context;
        g_XInputGamepadContext = 0;
    }

    void GamepadXInputBindGameInputDevice(IGameInputDevice* gameinput_device, GameInputDeviceFamily device_family, const char* name)
    {
        XInputGamepadContext* xinput_context = g_XInputGamepadContext;
        if (xinput_context == 0 ||
            xinput_context->m_XInputGetState == 0 ||
            (device_family != GameInputFamilyXbox360 && device_family != GameInputFamilyXboxOne))
        {
            return;
        }

        XInputGamepadDevice* device = FindXInputDevice(xinput_context, gameinput_device);
        if (device == 0)
        {
            device = FindFreeXInputDevice(xinput_context);
        }

        if (device == 0)
        {
            dmLogWarning("Unable to allocate an XInput fallback slot for '%s'", name);
            return;
        }

        device->m_GameInputDevice = 0;
        device->m_XInputIndex = INVALID_XINPUT_INDEX;

        for (uint32_t i = 0; i < XUSER_MAX_COUNT; ++i)
        {
            if (IsXInputIndexAssigned(xinput_context, i))
            {
                continue;
            }

            XINPUT_STATE state = {};
            if (xinput_context->m_XInputGetState(i, &state) == ERROR_SUCCESS)
            {
                device->m_GameInputDevice = gameinput_device;
                device->m_XInputIndex = i;
                dmLogInfo("Using XInput fallback for '%s' at user index %u", name, i);
                return;
            }
        }

        dmLogWarning("Unable to match '%s' to an XInput user index", name);
    }

    void GamepadXInputUnbindGameInputDevice(IGameInputDevice* gameinput_device)
    {
        XInputGamepadContext* xinput_context = g_XInputGamepadContext;
        if (xinput_context == 0)
        {
            return;
        }

        XInputGamepadDevice* device = FindXInputDevice(xinput_context, gameinput_device);
        if (device != 0)
        {
            device->m_GameInputDevice = 0;
            device->m_XInputIndex = INVALID_XINPUT_INDEX;
        }
    }

    bool GamepadXInputUpdate(IGameInputDevice* gameinput_device, Gamepad* gamepad)
    {
        XInputGamepadContext* xinput_context = g_XInputGamepadContext;
        XInputGamepadDevice* device = xinput_context != 0 ? FindXInputDevice(xinput_context, gameinput_device) : 0;
        if (device == 0 || device->m_XInputIndex == INVALID_XINPUT_INDEX || xinput_context->m_XInputGetState == 0)
        {
            return false;
        }

        XINPUT_STATE state = {};
        DWORD result = xinput_context->m_XInputGetState(device->m_XInputIndex, &state);
        if (result != ERROR_SUCCESS)
        {
            return false;
        }

        GamepadPacket& packet = gamepad->m_Packet;
        memset(packet.m_Axis, 0, sizeof(packet.m_Axis));
        memset(packet.m_Buttons, 0, sizeof(packet.m_Buttons));
        memset(packet.m_Hat, 0, sizeof(packet.m_Hat));

        WORD buttons = state.Gamepad.wButtons;
        SetXInputButton(packet, HID_GameInputGamepadMenu, buttons, XINPUT_GAMEPAD_START);
        SetXInputButton(packet, HID_GameInputGamepadView, buttons, XINPUT_GAMEPAD_BACK);
        SetXInputButton(packet, HID_GameInputGamepadA, buttons, XINPUT_GAMEPAD_A);
        SetXInputButton(packet, HID_GameInputGamepadB, buttons, XINPUT_GAMEPAD_B);
        SetXInputButton(packet, HID_GameInputGamepadX, buttons, XINPUT_GAMEPAD_X);
        SetXInputButton(packet, HID_GameInputGamepadY, buttons, XINPUT_GAMEPAD_Y);
        SetXInputButton(packet, HID_GameInputGamepadDPadUp, buttons, XINPUT_GAMEPAD_DPAD_UP);
        SetXInputButton(packet, HID_GameInputGamepadDPadDown, buttons, XINPUT_GAMEPAD_DPAD_DOWN);
        SetXInputButton(packet, HID_GameInputGamepadDPadLeft, buttons, XINPUT_GAMEPAD_DPAD_LEFT);
        SetXInputButton(packet, HID_GameInputGamepadDPadRight, buttons, XINPUT_GAMEPAD_DPAD_RIGHT);
        SetXInputButton(packet, HID_GameInputGamepadLeftShoulder, buttons, XINPUT_GAMEPAD_LEFT_SHOULDER);
        SetXInputButton(packet, HID_GameInputGamepadRightShoulder, buttons, XINPUT_GAMEPAD_RIGHT_SHOULDER);
        SetXInputButton(packet, HID_GameInputGamepadLeftThumbstick, buttons, XINPUT_GAMEPAD_LEFT_THUMB);
        SetXInputButton(packet, HID_GameInputGamepadRightThumbstick, buttons, XINPUT_GAMEPAD_RIGHT_THUMB);

        packet.m_Axis[0] = NormalizeXInputThumbstick(state.Gamepad.sThumbLX);
        packet.m_Axis[1] = NormalizeXInputThumbstick(state.Gamepad.sThumbLY);
        packet.m_Axis[2] = NormalizeXInputThumbstick(state.Gamepad.sThumbRX);
        packet.m_Axis[3] = NormalizeXInputThumbstick(state.Gamepad.sThumbRY);
        packet.m_Axis[4] = (float) state.Gamepad.bLeftTrigger / 255.0f;
        packet.m_Axis[5] = (float) state.Gamepad.bRightTrigger / 255.0f;

        gamepad->m_AxisCount = 6;
        gamepad->m_ButtonCount = HID_GameInputGamepad_Max;
        gamepad->m_HatCount = 0;
        return true;
    }
}
