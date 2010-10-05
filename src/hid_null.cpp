#include "hid.h"

#include <string.h>

#include <dlib/dstrings.h>

#include "hid_private.h"

namespace dmHID
{
    Context* s_Context = 0x0;

    void Initialize()
    {
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
        delete s_Context;
        s_Context = 0x0;
    }

    void Update()
    {
        s_Context->m_KeyboardConnected = 1;
        s_Context->m_MouseConnected = 1;
        s_Context->m_Gamepads[0].m_Connected = 1;
        s_Context->m_Gamepads[0].m_ButtonCount = MAX_GAMEPAD_BUTTON_COUNT;
        s_Context->m_Gamepads[0].m_AxisCount = MAX_GAMEPAD_AXIS_COUNT;
    }

    void GetGamepadDeviceName(HGamepad gamepad, const char** device_name)
    {
        *device_name = "null_device";
    }
}
