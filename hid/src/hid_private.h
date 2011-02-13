#ifndef HID_PRIVATE_H
#define HID_PRIVATE_H

#include "hid.h"

namespace dmHID
{
    struct Gamepad
    {
        GamepadPacket m_Packet;
        uint32_t m_Index;
        uint32_t m_AxisCount;
        uint32_t m_ButtonCount;
        uint32_t m_Connected : 1;
    };

    struct Context
    {
        KeyboardPacket m_KeyboardPacket;
        MousePacket m_MousePacket;
        Gamepad m_Gamepads[MAX_GAMEPAD_COUNT];
        uint32_t m_KeyboardConnected : 1;
        uint32_t m_MouseConnected : 1;
    };

    extern Context* s_Context;
}

#endif
