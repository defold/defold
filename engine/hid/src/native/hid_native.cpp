// Copyright 2020-2025 The Defold Foundation
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

#include <assert.h>
#include <string.h>

#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>

#include <platform/platform_window.h>

#include "hid.h"
#include "hid_private.h"
#include "hid_native_private.h"

namespace dmHID
{
    static const uint8_t DRIVER_HANDLE_FREE = 0xff;

    struct NativeContextUserData
    {
        dmArray<GamepadDriver*> m_GamepadDrivers;
    };

    static void GLFWAddKeyboardChar(void* ctx, int chr)
    {
        AddKeyboardChar((HContext) ctx, chr);
    }

    static void GLFWSetMarkedText(void* ctx, char* text)
    {
        SetMarkedText((HContext) ctx, text);
    }

    static void GLFWDeviceChangedCallback(void* ctx, int status)
    {
        HContext context = (HContext) ctx;
        NativeContextUserData* user_data = (NativeContextUserData*) context->m_NativeContextUserData;

        for (int i = 0; i < user_data->m_GamepadDrivers.Size(); ++i)
        {
            user_data->m_GamepadDrivers[i]->m_DetectDevices(context, user_data->m_GamepadDrivers[i]);
        }
    }

    static uint8_t DriverToHandle(NativeContextUserData* user_data, GamepadDriver* driver)
    {
        for (int i = 0; i < user_data->m_GamepadDrivers.Size(); ++i)
        {
            if (user_data->m_GamepadDrivers[i] == driver)
            {
                return i;
            }
        }

        return DRIVER_HANDLE_FREE;
    }

    static uint8_t GamepadToIndex(HContext context, Gamepad* gamepad)
    {
        for (int i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            if (&context->m_Gamepads[i] == gamepad)
            {
                return i;
            }
        }
        assert(0);
        return -1;
    }

    static void InstallGamepadDriver(HContext context, GamepadDriver* driver, const char* driver_name)
    {
        NativeContextUserData* user_data = (NativeContextUserData*) context->m_NativeContextUserData;

        if (!driver->m_Initialize(context, driver))
        {
            dmLogError("Unable to initialize gamepad driver '%s'", driver_name);
            return;
        }

        if (user_data->m_GamepadDrivers.Full())
        {
            user_data->m_GamepadDrivers.OffsetCapacity(1);
        }

        user_data->m_GamepadDrivers.Push(driver);

        dmLogDebug("Installed gamepad driver '%s'", driver_name);

        driver->m_DetectDevices(context, driver);
    }

    static void InitializeGamepads(HContext context)
    {
        memset(context->m_Gamepads, 0, sizeof(Gamepad) * MAX_GAMEPAD_COUNT);

        for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            context->m_Gamepads[i].m_Driver = DRIVER_HANDLE_FREE;
        }

        InstallGamepadDriver(context, CreateGamepadDriverGLFW(context), "GLFW");
    }

    // Called from gamepad drivers
    void SetGamepadConnectionStatus(HContext context, Gamepad* gamepad, bool connection_status)
    {
        uint8_t gamepad_index = GamepadToIndex(context, gamepad);

        if (gamepad->m_Connected != connection_status)
        {
            if (context->m_GamepadConnectivityCallback)
            {
                if (!context->m_GamepadConnectivityCallback(gamepad_index, connection_status, context->m_GamepadConnectivityUserdata))
                {
                    char device_name[dmHID::MAX_GAMEPAD_NAME_LENGTH];
                    GetGamepadDeviceName(context, gamepad, device_name);
                    dmLogWarning("The connection for '%s' was ignored by the callback function!", device_name);
                    return;
                }
            } else {
                dmLogWarning("There was no callback function set to handle the gamepad connection!");
            }

            SetGamepadConnectivity(context, gamepad_index, connection_status);
            gamepad->m_Connected = connection_status;
        }
    }

    // Called from gamepad drivers
    Gamepad* CreateGamepad(HContext context, GamepadDriver* driver)
    {
        NativeContextUserData* user_data = (NativeContextUserData*) context->m_NativeContextUserData;

        for (int i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            if (context->m_Gamepads[i].m_Driver == DRIVER_HANDLE_FREE)
            {
                context->m_Gamepads[i].m_Driver = DriverToHandle(user_data, driver);
                assert(context->m_Gamepads[i].m_Driver != DRIVER_HANDLE_FREE);
                return &context->m_Gamepads[i];
            }
        }

        dmLogError("Unable to allocate a slot for a new gamepad, max capacity reached (%d).", MAX_GAMEPAD_COUNT);
        return 0;
    }

    // Called from gamepad drivers
    void ReleaseGamepad(HContext context, Gamepad* gamepad)
    {
        uint8_t gamepad_index = GamepadToIndex(context, gamepad);
        assert(context->m_Gamepads[gamepad_index].m_Driver != DRIVER_HANDLE_FREE);
        context->m_Gamepads[gamepad_index].m_Driver = DRIVER_HANDLE_FREE;
    }

    bool Init(HContext context)
    {
        if (context != 0x0)
        {
            if (!context->m_Window)
            {
                dmLogFatal("No window has been created.");
                return false;
            }

            assert(context->m_Window && "No window has been set.");
            dmPlatform::SetKeyboardCharCallback(context->m_Window, GLFWAddKeyboardChar, (void*) context);
            dmPlatform::SetKeyboardMarkedTextCallback(context->m_Window, GLFWSetMarkedText, (void*) context);
            dmPlatform::SetKeyboardDeviceChangedCallback(context->m_Window, GLFWDeviceChangedCallback, (void*) context);

            assert(context->m_NativeContextUserData == 0);
            context->m_NativeContextUserData = new NativeContextUserData();

            InitializeGamepads(context);

            return true;
        }
        return false;
    }

    void Final(HContext context)
    {
        if (context)
        {
            NativeContextUserData* user_data = (NativeContextUserData*) context->m_NativeContextUserData;
            if (user_data)
            {
                for (int i = 0; i < user_data->m_GamepadDrivers.Size(); ++i)
                {
                    user_data->m_GamepadDrivers[i]->m_Destroy(context, user_data->m_GamepadDrivers[i]);
                }

                delete user_data;
            }
            context->m_NativeContextUserData = 0;
        }
    }

    bool Update(HContext context)
    {
        dmPlatform::PollEvents(context->m_Window);

        // Update keyboard
        if (!context->m_IgnoreKeyboard)
        {
            for (uint32_t k = 0; k < MAX_KEYBOARD_COUNT; ++k)
            {
                Keyboard* keyboard = &context->m_Keyboards[k];
                keyboard->m_Connected = 1; // TODO: Actually detect if the keyboard is present

                for (uint32_t i = 0; i < MAX_KEY_COUNT; ++i)
                {
                    Key key        = (Key) i;
                    int key_value  = GetKeyValue(key);
                    int state      = dmPlatform::GetKey(context->m_Window, key_value);
                    uint32_t mask  = 1 << (i % 32);

                    if (state)
                        keyboard->m_Packet.m_Keys[i / 32] |= mask;
                    else
                        keyboard->m_Packet.m_Keys[i / 32] &= ~mask;
                }
            }
        }

        // Update mouse
        if (!context->m_IgnoreMouse)
        {
            for (uint32_t m = 0; m < MAX_MOUSE_COUNT; ++m)
            {
                Mouse* mouse = &context->m_Mice[m];
                // TODO: Actually detect if the mouse is present,
                // this is important for mouse input and touch input to not interfere
                mouse->m_Connected = 1;

                MousePacket& packet = mouse->m_Packet;
                for (uint32_t i = 0; i < MAX_MOUSE_BUTTON_COUNT; ++i)
                {
                    uint32_t mask = 1;
                    mask <<= i % 32;

                    int button_value = GetMouseButtonValue((MouseButton) i);
                    int state        = dmPlatform::GetMouseButton(context->m_Window, button_value);

                    if (state)
                        packet.m_Buttons[i / 32] |= mask;
                    else
                        packet.m_Buttons[i / 32] &= ~mask;
                }
                int32_t wheel = dmPlatform::GetMouseWheel(context->m_Window);

                if (context->m_FlipScrollDirection)
                {
                    wheel *= -1;
                }

                packet.m_Wheel = wheel;

                dmPlatform::GetMousePosition(context->m_Window, &packet.m_PositionX, &packet.m_PositionY);
            }
        }

        if (!context->m_IgnoreGamepads)
        {
            NativeContextUserData* user_data = (NativeContextUserData*) context->m_NativeContextUserData;

            for (uint32_t t = 0; t < MAX_GAMEPAD_COUNT; ++t)
            {
                Gamepad* gamepad = &context->m_Gamepads[t];
                if (gamepad->m_Driver == DRIVER_HANDLE_FREE)
                {
                    continue;
                }

                GamepadDriver* driver = user_data->m_GamepadDrivers[gamepad->m_Driver];
                driver->m_Update(context, driver, gamepad);
            }
        }

        if (!context->m_IgnoreTouchDevice)
        {
            for (uint32_t t = 0; t < MAX_TOUCH_DEVICE_COUNT; ++t)
            {
                TouchDevice* device = &context->m_TouchDevices[t];
                TouchDevicePacket* packet = &device->m_Packet;

                dmPlatform::TouchData touch_data[dmHID::MAX_TOUCH_COUNT] = {};
                packet->m_TouchCount = dmPlatform::GetTouchData(context->m_Window, touch_data, dmHID::MAX_TOUCH_COUNT);

                if (packet->m_TouchCount > 0)
                {
                    device->m_Connected = 1;
                    for (int i = 0; i < packet->m_TouchCount; ++i)
                    {
                        packet->m_Touches[i].m_TapCount = touch_data[i].m_TapCount;
                        packet->m_Touches[i].m_Id       = touch_data[i].m_Id;
                        packet->m_Touches[i].m_Phase    = (dmHID::Phase) touch_data[i].m_Phase;
                        packet->m_Touches[i].m_X        = touch_data[i].m_X;
                        packet->m_Touches[i].m_Y        = touch_data[i].m_Y;
                        packet->m_Touches[i].m_DX       = touch_data[i].m_DX;
                        packet->m_Touches[i].m_DY       = touch_data[i].m_DY;
                    }
                }
            }
        }
        if (!context->m_IgnoreAcceleration)
        {
            AccelerationPacket packet;
            context->m_AccelerometerConnected = 0;
            if (dmPlatform::GetAcceleration(context->m_Window, &packet.m_X, &packet.m_Y, &packet.m_Z))
            {
                context->m_AccelerometerConnected = 1;
                context->m_AccelerationPacket = packet;
            }
        }

        dmhash_t prev_state_hash = context->m_StateHash;
        context->m_StateHash = CalcStateHash(context);
        return prev_state_hash != context->m_StateHash;
    }

    void GetGamepadDeviceName(HContext context, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH])
    {
        NativeContextUserData* user_data = (NativeContextUserData*) context->m_NativeContextUserData;

        name[0] = 0;
        if (gamepad->m_Driver == DRIVER_HANDLE_FREE)
        {
            return;
        }

        assert(gamepad->m_Driver < user_data->m_GamepadDrivers.Size());
        GamepadDriver* driver = user_data->m_GamepadDrivers[gamepad->m_Driver];
        driver->m_GetGamepadDeviceName(context, driver, gamepad, name);
    }

    void GetGamepadDeviceGuid(HContext context, HGamepad gamepad, char guid[MAX_GAMEPAD_GUID_LENGTH])
    {
        NativeContextUserData* user_data = (NativeContextUserData*) context->m_NativeContextUserData;

        guid[0] = 0;
        if (gamepad->m_Driver == DRIVER_HANDLE_FREE)
        {
            return;
        }

        assert(gamepad->m_Driver < user_data->m_GamepadDrivers.Size());
        GamepadDriver* driver = user_data->m_GamepadDrivers[gamepad->m_Driver];
        if (driver->m_GetGamepadDeviceGuid)
        {
            driver->m_GetGamepadDeviceGuid(context, driver, gamepad, guid);
        }
    }

    void ResetKeyboard(HContext context)
    {
        dmPlatform::SetDeviceState(context->m_Window, dmPlatform::DEVICE_STATE_KEYBOARD_RESET, true);
    }
}
