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

#include "hid_native_private.h"

namespace dmHID
{
    static int GLFW_JOYSTICKS[MAX_GAMEPAD_COUNT] =
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

    // static HContext* g_HidContext = 0;

    static void GLFWGamepadCallback(int gamepad_id, int connected)
    {
        /*
        if (g_HidContext->m_GamepadConnectivityCallback) {
            g_HidContext->m_GamepadConnectivityCallback(gamepad_id, connected, g_HidContext->m_GamepadConnectivityUserdata);
        }
        SetGamepadConnectivity(g_HidContext, gamepad_id, connected);
        */
    }

    static void GLFWGamepadDriverUpdate(HContext context, GamepadDriver* driver, Gamepad* gamepad)
    {
        /*
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
        */
    }

    static void GLFWGamepadDriverDetectDevices(HContext context, GamepadDriver* driver)
    {
        // NOP
    }

    static void GLFWGamepadDriverGetGamepadDeviceName(HContext context, GamepadDriver* driver, HGamepad gamepad, const char** out_device_name)
    {
        // need gamepad to glfw index!
        // glfwGetJoystickDeviceId(gamepad->m_Index, (char**) out_device_name);
    }

    static bool GLFWGamepadDriverInitialize(HContext context, GamepadDriver* driver)
    {
        if (glfwSetGamepadCallback(GLFWGamepadCallback) == 0)
        {
            dmLogFatal("could not set glfw gamepad callback.");
            return false;
        }
        return true;
    }

    static void GLFWGamepadDriverDestroy(HContext context, GamepadDriver* driver)
    {
        delete driver;
    }

    GamepadDriver* CreateGamepadDriverGLFW(HContext context)
    {
        GamepadDriver* driver = new GamepadDriver();

        driver->m_Initialize           = GLFWGamepadDriverInitialize;
        driver->m_Destroy              = GLFWGamepadDriverDestroy;
        driver->m_Update               = GLFWGamepadDriverUpdate;
        driver->m_DetectDevices        = GLFWGamepadDriverDetectDevices;
        driver->m_GetGamepadDeviceName = GLFWGamepadDriverGetGamepadDeviceName;

        return driver;
    }
}
