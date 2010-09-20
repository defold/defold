#include "hid.h"

#include <string.h>

#include <dlib/dstrings.h>

#include "hid_private.h"

namespace dmHID
{
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
