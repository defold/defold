// Copyright 2020-2022 The Defold Foundation
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

#include "hid.h"

#include <assert.h>
#include <string.h>

#include <dlib/log.h>
#include <dlib/utf8.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/static_assert.h>

#include <dmsdk/graphics/glfw/glfw.h>

#include "hid_private.h"

namespace dmHID
{
    extern const char* KEY_NAMES[MAX_KEY_COUNT];
    extern const char* MOUSE_BUTTON_NAMES[MAX_MOUSE_BUTTON_COUNT];

    int GLFW_JOYSTICKS[MAX_GAMEPAD_COUNT] =
    {
            GLFW_JOYSTICK_1,
            GLFW_JOYSTICK_2,
            GLFW_JOYSTICK_3,
            GLFW_JOYSTICK_4,
            GLFW_JOYSTICK_5,
            GLFW_JOYSTICK_6,
            GLFW_JOYSTICK_7,
            GLFW_JOYSTICK_8,
            GLFW_JOYSTICK_9,
            GLFW_JOYSTICK_10,
            GLFW_JOYSTICK_11,
            GLFW_JOYSTICK_12,
            GLFW_JOYSTICK_13,
            GLFW_JOYSTICK_14,
            GLFW_JOYSTICK_15,
            GLFW_JOYSTICK_16
    };


    /*
     * TODO: Unfortunately a global variable as glfw has no notion of user-data context
     */
    HContext g_Context = 0;
    static void CharacterCallback(int chr,int)
    {
        AddKeyboardChar(g_Context, chr);
    }

    static void MarkedTextCallback(char* text)
    {
        SetMarkedText(g_Context, text);
    }

    static void GamepadCallback(int gamepad_id, int connected)
    {
        if (g_Context->m_GamepadConnectivityCallback) {
            g_Context->m_GamepadConnectivityCallback(gamepad_id, connected, g_Context->m_GamepadConnectivityUserdata);
        }
        SetGamepadConnectivity(g_Context, gamepad_id, connected);
    }

    bool Init(HContext context)
    {
        if (context != 0x0)
        {
            if (glfwInit() == GL_FALSE)
            {
                dmLogFatal("glfw could not be initialized.");
                return false;
            }
            assert(g_Context == 0);
            g_Context = context;
            if (glfwSetCharCallback(CharacterCallback) == 0) {
                dmLogFatal("could not set glfw char callback.");
            }
            if (glfwSetMarkedTextCallback(MarkedTextCallback) == 0) {
                dmLogFatal("could not set glfw marked text callback.");
            }
            if (glfwSetGamepadCallback(GamepadCallback) == 0) {
                dmLogFatal("could not set glfw gamepad callback.");
            }
            for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
            {
                Gamepad& gamepad = context->m_Gamepads[i];
                gamepad.m_Index = i;
                gamepad.m_Connected = 0;
                gamepad.m_AxisCount = 0;
                gamepad.m_ButtonCount = 0;
                gamepad.m_HatCount = 0;
                memset(&gamepad.m_Packet, 0, sizeof(GamepadPacket));
            }
            return true;
        }
        return false;
    }

    void Final(HContext context)
    {
        g_Context = 0;
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

        // Update gamepads
        if (!context->m_IgnoreGamepads)
        {
            for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
            {
                Gamepad* pad = &context->m_Gamepads[i];
                int glfw_joystick = GLFW_JOYSTICKS[i];
                bool prev_connected = pad->m_Connected;
                pad->m_Connected = glfwGetJoystickParam(glfw_joystick, GLFW_PRESENT) == GL_TRUE;
                if (pad->m_Connected)
                {
                    GamepadPacket& packet = pad->m_Packet;

                    // Workaround to get connectivity packet even if callback
                    // wasn't been set before the gamepad was connected.
                    if (!prev_connected)
                    {
                        packet.m_GamepadConnected = true;
                    }

                    pad->m_AxisCount = glfwGetJoystickParam(glfw_joystick, GLFW_AXES);
                    glfwGetJoystickPos(glfw_joystick, packet.m_Axis, pad->m_AxisCount);

                    pad->m_HatCount = dmMath::Min(MAX_GAMEPAD_HAT_COUNT, (uint32_t) glfwGetJoystickParam(glfw_joystick, GLFW_HATS));
                    glfwGetJoystickHats(glfw_joystick, packet.m_Hat, pad->m_HatCount);

                    pad->m_ButtonCount = dmMath::Min(MAX_GAMEPAD_BUTTON_COUNT, (uint32_t) glfwGetJoystickParam(glfw_joystick, GLFW_BUTTONS));
                    unsigned char buttons[MAX_GAMEPAD_BUTTON_COUNT];
                    glfwGetJoystickButtons(glfw_joystick, buttons, pad->m_ButtonCount);
                    for (uint32_t j = 0; j < pad->m_ButtonCount; ++j)
                    {
                        if (buttons[j] == GLFW_PRESS)
                            packet.m_Buttons[j / 32] |= 1 << (j % 32);
                        else
                            packet.m_Buttons[j / 32] &= ~(1 << (j % 32));
                    }
                }
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

    void GetGamepadDeviceName(HGamepad gamepad, const char** device_name)
    {
        glfwGetJoystickDeviceId(gamepad->m_Index, (char**)device_name);
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
