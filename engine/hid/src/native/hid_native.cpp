// Copyright 2020-2023 The Defold Foundation
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
#include <dlib/utf8.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>

#include <dmsdk/graphics/glfw/glfw.h>

#include "hid.h"
#include "hid_private.h"
#include "hid_native_private.h"

namespace dmHID
{
    extern const char* KEY_NAMES[MAX_KEY_COUNT];
    extern const char* MOUSE_BUTTON_NAMES[MAX_MOUSE_BUTTON_COUNT];

    static const uint8_t DRIVER_HANDLE_FREE = 0xff;

    struct NativeContextUserData
    {
        dmArray<GamepadDriver*> m_GamepadDrivers;
    };

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

    // This is unfortunately needed for GLFW since we need the context in the callbacks,
    // and there's no userdata pointers available for us to pass it in..
    static HContext g_HidContext = 0;

    static void GLFWCharacterCallback(int chr, int _)
    {
        AddKeyboardChar(g_HidContext, chr);
    }

    static void GLFWMarkedTextCallback(char* text)
    {
        SetMarkedText(g_HidContext, text);
    }

    static void InstallGamepadDriver(GamepadDriver* driver, const char* driver_name)
    {
        NativeContextUserData* user_data = (NativeContextUserData*) g_HidContext->m_NativeContextUserData;

        if (!driver->m_Initialize(g_HidContext, driver))
        {
            dmLogError("Unable to initialize gamepad driver '%s'", driver_name);
            return;
        }

        if (user_data->m_GamepadDrivers.Full())
        {
            user_data->m_GamepadDrivers.OffsetCapacity(1);
        }

        user_data->m_GamepadDrivers.Push(driver);

        dmLogInfo("Installed gamepad driver '%s'", driver_name);

        driver->m_DetectDevices(g_HidContext, driver);
    }

    static void InitializeGamepads(HContext context)
    {
        memset(context->m_Gamepads, 0, sizeof(Gamepad) * MAX_GAMEPAD_COUNT);

        for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            context->m_Gamepads[i].m_Driver = DRIVER_HANDLE_FREE;
        }

        InstallGamepadDriver(CreateGamepadDriverGLFW(context), "GLFW");

    #ifdef DM_HID_DINPUT
        InstallGamepadDriver(CreateGamepadDriverDInput(context), "Direct Input");
    #endif
    }

    // Called from gamepad drivers
    void SetGamepadConnectionStatus(HContext context, Gamepad* gamepad, bool connection_status)
    {
        uint8_t gamepad_index = GamepadToIndex(context, gamepad);

        if (gamepad->m_Connected != connection_status)
        {
            if (context->m_GamepadConnectivityCallback)
            {
                context->m_GamepadConnectivityCallback(gamepad_index, connection_status, context->m_GamepadConnectivityUserdata);
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
        return 0;
    }

    // Called from gamepad drivers
    void ReleaseGamepad(HContext context, Gamepad* gamepad)
    {
        NativeContextUserData* user_data = (NativeContextUserData*) context->m_NativeContextUserData;
        uint8_t gamepad_index = GamepadToIndex(context, gamepad);
        assert(context->m_Gamepads[gamepad_index].m_Driver != DRIVER_HANDLE_FREE);
        context->m_Gamepads[gamepad_index].m_Driver = DRIVER_HANDLE_FREE;
    }

    bool Init(HContext context)
    {
        if (context != 0x0)
        {
            assert(g_HidContext == 0);

            if (glfwInit() == GL_FALSE)
            {
                dmLogFatal("glfw could not be initialized.");
                return false;
            }
            if (glfwSetCharCallback(GLFWCharacterCallback) == 0) {
                dmLogFatal("could not set glfw char callback.");
            }
            if (glfwSetMarkedTextCallback(GLFWMarkedTextCallback) == 0) {
                dmLogFatal("could not set glfw marked text callback.");
            }

            assert(context->m_NativeContextUserData == 0);
            context->m_NativeContextUserData = new NativeContextUserData();
            g_HidContext = context;

            InitializeGamepads(context);

            return true;
        }
        return false;
    }

    void Final(HContext context)
    {
        NativeContextUserData* user_data = (NativeContextUserData*) g_HidContext->m_NativeContextUserData;

        for (int i = 0; i < user_data->m_GamepadDrivers.Size(); ++i)
        {
            user_data->m_GamepadDrivers[i]->m_Destroy(context, user_data->m_GamepadDrivers[i]);
        }

        delete user_data;
    }

    void Update(HContext context)
    {
        // NOTE: GLFW_AUTO_POLL_EVENTS might be enabled but an application shouldn't have rely on
        // running glfwSwapBuffers for event queue polling
        // Accessing OpenGL isn't permitted on iOS when the application is transitioning to resumed mode either
        glfwPollEvents();

        // Update keyboard
        if (!context->m_IgnoreKeyboard)
        {
            for (uint32_t k = 0; k < MAX_KEYBOARD_COUNT; ++k)
            {
                Keyboard* keyboard = &context->m_Keyboards[k];
                keyboard->m_Connected = 1; // TODO: Actually detect if the keyboard is present

                for (uint32_t i = 0; i < MAX_KEY_COUNT; ++i)
                {
                    uint32_t mask = 1;
                    mask <<= i % 32;
                    int state = glfwGetKey(i);
                    if (state == GLFW_PRESS)
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
                    int state = glfwGetMouseButton(i);
                    if (state == GLFW_PRESS)
                        packet.m_Buttons[i / 32] |= mask;
                    else
                        packet.m_Buttons[i / 32] &= ~mask;
                }
                int32_t wheel = glfwGetMouseWheel();
                if (context->m_FlipScrollDirection)
                {
                    wheel *= -1;
                }
                packet.m_Wheel = wheel;
                glfwGetMousePos(&packet.m_PositionX, &packet.m_PositionY);
            }
        }

        if (!context->m_IgnoreGamepads)
        {
            NativeContextUserData* user_data = (NativeContextUserData*) g_HidContext->m_NativeContextUserData;

            for (int i = 0; i < user_data->m_GamepadDrivers.Size(); ++i)
            {
                user_data->m_GamepadDrivers[i]->m_DetectDevices(g_HidContext, user_data->m_GamepadDrivers[i]);
            }

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

                GLFWTouch glfw_touch[dmHID::MAX_TOUCH_COUNT];
                int n_touch;
                if (glfwGetTouch(glfw_touch, dmHID::MAX_TOUCH_COUNT, &n_touch))
                {
                    device->m_Connected = 1;
                    TouchDevicePacket* packet = &device->m_Packet;
                    packet->m_TouchCount = n_touch;
                    for (int i = 0; i < n_touch; ++i)
                    {
                        packet->m_Touches[i].m_TapCount = glfw_touch[i].TapCount;
                        packet->m_Touches[i].m_Id = glfw_touch[i].Id;
                        packet->m_Touches[i].m_Phase = (dmHID::Phase) glfw_touch[i].Phase;
                        packet->m_Touches[i].m_X = glfw_touch[i].X;
                        packet->m_Touches[i].m_Y = glfw_touch[i].Y;
                        packet->m_Touches[i].m_DX = glfw_touch[i].DX;
                        packet->m_Touches[i].m_DY = glfw_touch[i].DY;
                    }
                }
            }
        }
        if (!context->m_IgnoreAcceleration)
        {
            AccelerationPacket packet;
            context->m_AccelerometerConnected = 0;
            if (glfwGetAcceleration(&packet.m_X, &packet.m_Y, &packet.m_Z))
            {
                context->m_AccelerometerConnected = 1;
                context->m_AccelerationPacket = packet;
            }
        }
    }

    void GetGamepadDeviceName(HContext context, HGamepad gamepad, char* buffer, uint32_t buffer_length)
    {
        assert(buffer_length != 0);
        assert(buffer != 0);
    
        NativeContextUserData* user_data = (NativeContextUserData*) g_HidContext->m_NativeContextUserData;
        if (gamepad->m_Driver == DRIVER_HANDLE_FREE)
        {
            buffer[0] = 0;
            return;
        }

        assert(gamepad->m_Driver < user_data->m_GamepadDrivers.Size());
        GamepadDriver* driver = user_data->m_GamepadDrivers[gamepad->m_Driver];
        driver->m_GetGamepadDeviceName(context, driver, gamepad, buffer, buffer_length);
    }

    void ShowKeyboard(HContext context, KeyboardType type, bool autoclose)
    {
        int t = GLFW_KEYBOARD_DEFAULT;
        switch (type) {
            case KEYBOARD_TYPE_DEFAULT:
                t = GLFW_KEYBOARD_DEFAULT;
                break;
            case KEYBOARD_TYPE_NUMBER_PAD:
                t = GLFW_KEYBOARD_NUMBER_PAD;
                break;
            case KEYBOARD_TYPE_EMAIL:
                t = GLFW_KEYBOARD_EMAIL;
                break;
            case KEYBOARD_TYPE_PASSWORD:
                t = GLFW_KEYBOARD_PASSWORD;
                break;
            default:
                dmLogWarning("Unknown keyboard type %d\n", type);
        }
        glfwShowKeyboard(1, t, (int) autoclose);
    }

    void HideKeyboard(HContext context)
    {
        glfwShowKeyboard(0, GLFW_KEYBOARD_DEFAULT, 0);
    }

    void ResetKeyboard(HContext context)
    {
        glfwResetKeyboard();
    }

    void EnableAccelerometer()
    {
        glfwAccelerometerEnable();
    }

    void ShowMouseCursor(HContext context)
    {
        glfwEnable(GLFW_MOUSE_CURSOR);
    }

    void HideMouseCursor(HContext context)
    {
        glfwDisable(GLFW_MOUSE_CURSOR);
    }

    bool GetCursorVisible(HContext context)
    {
        return !glfwGetMouseLocked();
    }

    const char* GetKeyName(Key key)
    {
        return KEY_NAMES[key];
    }

    const char* GetMouseButtonName(MouseButton input)
    {
        return MOUSE_BUTTON_NAMES[input];
    }

    const char* KEY_NAMES[MAX_KEY_COUNT] =
    {
        "KEY_SPACE",
        "KEY_EXCLAIM",
        "KEY_QUOTEDBL",
        "KEY_HASH",
        "KEY_DOLLAR",
        "KEY_AMPERSAND",
        "KEY_QUOTE",
        "KEY_LPAREN",
        "KEY_RPAREN",
        "KEY_ASTERISK",
        "KEY_PLUS",
        "KEY_COMMA",
        "KEY_MINUS",
        "KEY_PERIOD",
        "KEY_SLASH",
        "KEY_0",
        "KEY_1",
        "KEY_2",
        "KEY_3",
        "KEY_4",
        "KEY_5",
        "KEY_6",
        "KEY_7",
        "KEY_8",
        "KEY_9",
        "KEY_COLON",
        "KEY_SEMICOLON",
        "KEY_LESS",
        "KEY_EQUALS",
        "KEY_GREATER",
        "KEY_QUESTION",
        "KEY_AT",
        "KEY_A",
        "KEY_B",
        "KEY_C",
        "KEY_D",
        "KEY_E",
        "KEY_F",
        "KEY_G",
        "KEY_H",
        "KEY_I",
        "KEY_J",
        "KEY_K",
        "KEY_L",
        "KEY_M",
        "KEY_N",
        "KEY_O",
        "KEY_P",
        "KEY_Q",
        "KEY_R",
        "KEY_S",
        "KEY_T",
        "KEY_U",
        "KEY_V",
        "KEY_W",
        "KEY_X",
        "KEY_Y",
        "KEY_Z",
        "KEY_LBRACKET",
        "KEY_BACKSLASH",
        "KEY_RBRACKET",
        "KEY_CARET",
        "KEY_UNDERSCORE",
        "KEY_BACKQUOTE",
        "KEY_LBRACE",
        "KEY_PIPE",
        "KEY_RBRACE",
        "KEY_TILDE",
        "KEY_ESC",
        "KEY_F1",
        "KEY_F2",
        "KEY_F3",
        "KEY_F4",
        "KEY_F5",
        "KEY_F6",
        "KEY_F7",
        "KEY_F8",
        "KEY_F9",
        "KEY_F10",
        "KEY_F11",
        "KEY_F12",
        "KEY_UP",
        "KEY_DOWN",
        "KEY_LEFT",
        "KEY_RIGHT",
        "KEY_LSHIFT",
        "KEY_RSHIFT",
        "KEY_LCTRL",
        "KEY_RCTRL",
        "KEY_LALT",
        "KEY_RALT",
        "KEY_TAB",
        "KEY_ENTER",
        "KEY_BACKSPACE",
        "KEY_INSERT",
        "KEY_DEL",
        "KEY_PAGEUP",
        "KEY_PAGEDOWN",
        "KEY_HOME",
        "KEY_END",
        "KEY_KP_0",
        "KEY_KP_1",
        "KEY_KP_2",
        "KEY_KP_3",
        "KEY_KP_4",
        "KEY_KP_5",
        "KEY_KP_6",
        "KEY_KP_7",
        "KEY_KP_8",
        "KEY_KP_9",
        "KEY_KP_DIVIDE",
        "KEY_KP_MULTIPLY",
        "KEY_KP_SUBTRACT",
        "KEY_KP_ADD",
        "KEY_KP_DECIMAL",
        "KEY_KP_EQUAL",
        "KEY_KP_ENTER"
    };

    const char* MOUSE_BUTTON_NAMES[MAX_MOUSE_BUTTON_COUNT] =
    {
        "MOUSE_BUTTON_LEFT",
        "MOUSE_BUTTON_MIDDLE",
        "MOUSE_BUTTON_RIGHT"
    };
}
