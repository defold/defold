// Copyright 2020-2024 The Defold Foundation
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

#include <platform/platform_window.h>
#include <platform/platform_window_constants.h>

#include "hid_private.h"

#include "hid_native_private.h"

namespace dmHID
{
    static int GLFW_JOYSTICKS[MAX_GAMEPAD_COUNT] =
    {
        dmPlatform::PLATFORM_JOYSTICK_1,
        dmPlatform::PLATFORM_JOYSTICK_2,
        dmPlatform::PLATFORM_JOYSTICK_3,
        dmPlatform::PLATFORM_JOYSTICK_4,
        dmPlatform::PLATFORM_JOYSTICK_5,
        dmPlatform::PLATFORM_JOYSTICK_6,
        dmPlatform::PLATFORM_JOYSTICK_7,
        dmPlatform::PLATFORM_JOYSTICK_8,
        dmPlatform::PLATFORM_JOYSTICK_9,
        dmPlatform::PLATFORM_JOYSTICK_10,
        dmPlatform::PLATFORM_JOYSTICK_11,
        dmPlatform::PLATFORM_JOYSTICK_12,
        dmPlatform::PLATFORM_JOYSTICK_13,
        dmPlatform::PLATFORM_JOYSTICK_14,
        dmPlatform::PLATFORM_JOYSTICK_15,
        dmPlatform::PLATFORM_JOYSTICK_16
    };

    struct GLFWGamepadDevice
    {
        int      m_Id;
        Gamepad* m_Gamepad;
    };


    // The GLFW driver stores an indirect table to map a GLFW index to our internal representation,
    // this is needed because multiple gamepad drivers can exist at the same time.
    struct GLFWGamepadDriver : GamepadDriver
    {
        HContext                   m_HidContext;
        dmArray<GLFWGamepadDevice> m_Devices;
    };

    static GLFWGamepadDriver* g_GLFWGamepadDriver = 0;

    static Gamepad* GLFWGetGamepad(GLFWGamepadDriver* driver, int gamepad_id)
    {
        for (int i = 0; i < driver->m_Devices.Size(); ++i)
        {
            if (driver->m_Devices[i].m_Id == gamepad_id)
            {
                return driver->m_Devices[i].m_Gamepad;
            }
        }

        return 0;
    }

    static int GLFWGetGamepadId(GLFWGamepadDriver* driver, Gamepad* gamepad)
    {
        for (int i = 0; i < driver->m_Devices.Size(); ++i)
        {
            if (driver->m_Devices[i].m_Gamepad == gamepad)
            {
                return driver->m_Devices[i].m_Id;
            }
        }

        return -1;
    }

    static Gamepad* GLFWEnsureAllocatedGamepad(GLFWGamepadDriver* driver, int gamepad_id)
    {
        Gamepad* gp = GLFWGetGamepad(driver, gamepad_id);
        if (gp != 0)
        {
            return gp;
        }

        gp = CreateGamepad(driver->m_HidContext, driver);
        if (gp == 0)
        {
            return 0;
        }

        GLFWGamepadDevice new_device = {};
        new_device.m_Id              = gamepad_id;
        new_device.m_Gamepad         = gp;

        if (driver->m_Devices.Full())
        {
            driver->m_Devices.OffsetCapacity(1);
        }

        driver->m_Devices.Push(new_device);

        SetGamepadConnectionStatus(driver->m_HidContext, gp, true);

        return gp;
    }

    static void GLFWRemoveGamepad(GLFWGamepadDriver* driver, int gamepad_id)
    {
        for (int i = 0; i < driver->m_Devices.Size(); ++i)
        {
            if (driver->m_Devices[i].m_Id == gamepad_id)
            {
                SetGamepadConnectionStatus(driver->m_HidContext, driver->m_Devices[i].m_Gamepad, false);
                ReleaseGamepad(driver->m_HidContext, driver->m_Devices[i].m_Gamepad);
                driver->m_Devices.EraseSwap(i);
                return;
            }
        }
    }

    static void GLFWGamepadCallback(void* user_Data, int gamepad_id, dmPlatform::GamepadEvent event)
    {
        if (event == dmPlatform::GAMEPAD_EVENT_CONNECTED || event == dmPlatform::GAMEPAD_EVENT_DISCONNECTED)
        {
            Gamepad* gp = GLFWEnsureAllocatedGamepad(g_GLFWGamepadDriver, gamepad_id);
            if (gp == 0)
            {
                return;
            }
            SetGamepadConnectionStatus(g_GLFWGamepadDriver->m_HidContext, gp, event == dmPlatform::GAMEPAD_EVENT_CONNECTED ? 1 : 0);
        }
    }

    static void GLFWGamepadDriverUpdate(HContext context, GamepadDriver* driver, Gamepad* gamepad)
    {
        int id                = GLFWGetGamepadId((GLFWGamepadDriver*) driver, gamepad);
        int glfw_joystick     = GLFW_JOYSTICKS[id];
        GamepadPacket& packet = gamepad->m_Packet;

        unsigned char buttons[MAX_GAMEPAD_BUTTON_COUNT];
        gamepad->m_AxisCount   = dmPlatform::GetJoystickAxes(context->m_Window, glfw_joystick, packet.m_Axis, MAX_GAMEPAD_AXIS_COUNT);
        gamepad->m_HatCount    = dmPlatform::GetJoystickHats(context->m_Window, glfw_joystick, packet.m_Hat, MAX_GAMEPAD_HAT_COUNT);
        gamepad->m_ButtonCount = dmPlatform::GetJoystickButtons(context->m_Window, glfw_joystick, buttons, MAX_GAMEPAD_BUTTON_COUNT);

        for (uint32_t j = 0; j < gamepad->m_ButtonCount; ++j)
        {
            if (buttons[j])
            {
                packet.m_Buttons[j / 32] |= 1 << (j % 32);
            }
            else
            {
                packet.m_Buttons[j / 32] &= ~(1 << (j % 32));
            }
        }
    }

    static void GLFWGamepadDriverDetectDevices(HContext context, GamepadDriver* driver)
    {
        GLFWGamepadDriver* glfw_driver = (GLFWGamepadDriver*) driver;

        for (int i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            if (dmPlatform::GetDeviceState(context->m_Window, dmPlatform::DEVICE_STATE_JOYSTICK_PRESENT, i))
            {
                GLFWEnsureAllocatedGamepad(glfw_driver, i);
            }
            else
            {
                GLFWRemoveGamepad(glfw_driver, i);
            }
        }
    }

    static void GLFWGamepadDriverGetGamepadDeviceName(HContext context, GamepadDriver* driver, HGamepad gamepad, char* buffer, uint32_t buffer_length)
    {
        uint32_t gamepad_index = GLFWGetGamepadId((GLFWGamepadDriver*) driver, gamepad);
        const char* device_name = dmPlatform::GetJoystickDeviceName(context->m_Window, gamepad_index);

        dmStrlCpy(buffer, device_name, buffer_length);
    }

    static bool GLFWGamepadDriverInitialize(HContext context, GamepadDriver* driver)
    {
        if (!dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            return false;
        }

        dmPlatform::SetGamepadEventCallback(context->m_Window, GLFWGamepadCallback, 0);
        return true;
    }

    static void GLFWGamepadDriverDestroy(HContext context, GamepadDriver* driver)
    {
        GLFWGamepadDriver* glfw_driver = (GLFWGamepadDriver*) driver;
        assert(g_GLFWGamepadDriver == glfw_driver);
        delete glfw_driver;
        g_GLFWGamepadDriver = 0;
    }

    GamepadDriver* CreateGamepadDriverGLFW(HContext context)
    {
        GLFWGamepadDriver* driver = new GLFWGamepadDriver();

        driver->m_Initialize           = GLFWGamepadDriverInitialize;
        driver->m_Destroy              = GLFWGamepadDriverDestroy;
        driver->m_Update               = GLFWGamepadDriverUpdate;
        driver->m_DetectDevices        = GLFWGamepadDriverDetectDevices;
        driver->m_GetGamepadDeviceName = GLFWGamepadDriverGetGamepadDeviceName;

        assert(g_GLFWGamepadDriver == 0);
        g_GLFWGamepadDriver               = driver;
        g_GLFWGamepadDriver->m_HidContext = context;

        return driver;
    }
}
