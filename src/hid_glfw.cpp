#include "hid.h"

#include <assert.h>
#include <string.h>

#include <dlib/log.h>

#include <graphics/glfw/glfw.h>

#include "hid_private.h"

namespace dmHID
{
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

    Context* s_Context = 0x0;

    void Initialize()
    {
        if (glfwInit() == GL_FALSE)
        {
            dmLogFatal("glfw could not be initialized.");
            return;
        }
        if (s_Context == 0x0)
        {
            s_Context = new Context();
            s_Context->m_KeyboardConnected = 0;
            s_Context->m_MouseConnected = 0;
            for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
            {
                Gamepad& gamepad = s_Context->m_Gamepads[i];
                gamepad.m_Index = i;
                gamepad.m_Connected = 0;
                gamepad.m_AxisCount = 0;
                gamepad.m_ButtonCount = 0;
                memset(&gamepad.m_Packet, 0, sizeof(GamepadPacket));
            }
        }
    }

    void Finalize()
    {
        glfwTerminate();
        delete s_Context;
        s_Context = 0x0;
    }

    void Update()
    {
        Context* context = s_Context;
        // Update keyboard
        s_Context->m_KeyboardConnected = 1;
        for (uint32_t i = 0; i < MAX_KEY_COUNT; ++i)
        {
            uint32_t mask = 1;
            mask <<= i % 32;
            int key = glfwGetKey(i);
            if (key == GLFW_PRESS)
                context->m_KeyboardPacket.m_Keys[i / 32] |= mask;
            else
                context->m_KeyboardPacket.m_Keys[i / 32] &= ~mask;
        }

        // Update mouse
        s_Context->m_MouseConnected = 1;
        for (uint32_t i = 0; i < MAX_MOUSE_BUTTON_COUNT; ++i)
        {
            uint32_t mask = 1;
            mask <<= i % 32;
            int button = glfwGetMouseButton(i);
            if (button == GLFW_PRESS)
                context->m_MousePacket.m_Buttons[i / 32] |= mask;
            else
                context->m_MousePacket.m_Buttons[i / 32] &= ~mask;
        }
        context->m_MousePacket.m_Wheel = glfwGetMouseWheel();
        glfwGetMousePos(&context->m_MousePacket.m_PositionX, &context->m_MousePacket.m_PositionY);

        // Update gamepads
        for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            Gamepad* pad = &context->m_Gamepads[i];
            int glfw_joystick = GLFW_JOYSTICKS[i];
            pad->m_Connected = glfwGetJoystickParam(glfw_joystick, GLFW_PRESENT) == GL_TRUE;
            if (pad->m_Connected)
            {
                pad->m_AxisCount = glfwGetJoystickParam(glfw_joystick, GLFW_AXES);
                pad->m_ButtonCount = glfwGetJoystickParam(glfw_joystick, GLFW_BUTTONS);
                glfwGetJoystickPos(glfw_joystick, pad->m_Packet.m_Axis, pad->m_AxisCount);
                unsigned char buttons[MAX_GAMEPAD_BUTTON_COUNT];
                glfwGetJoystickButtons(glfw_joystick, buttons, pad->m_ButtonCount);
                for (uint32_t j = 0; j < pad->m_ButtonCount; ++j)
                {
                    if (buttons[j] == GLFW_PRESS)
                        pad->m_Packet.m_Buttons[j / 32] |= 1 << (j % 32);
                    else
                        pad->m_Packet.m_Buttons[j / 32] &= ~(1 << (j % 32));
                }
            }
        }
    }

    void GetGamepadDeviceName(HGamepad gamepad, const char** device_name)
    {
        glfwGetJoystickDeviceId(gamepad->m_Index, (char**)device_name);
    }
}
