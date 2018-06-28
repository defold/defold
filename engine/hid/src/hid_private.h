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
        Context();

        KeyboardPacket m_KeyboardPacket;
        TextPacket     m_TextPacket;
        MarkedTextPacket m_MarkedTextPacket;
        MousePacket m_MousePacket;
        Gamepad m_Gamepads[MAX_GAMEPAD_COUNT];
        TouchDevicePacket m_TouchDevicePacket;
        AccelerationPacket m_AccelerationPacket;
        uint32_t m_KeyboardConnected : 1;
        uint32_t m_MouseConnected : 1;
        uint32_t m_TouchDeviceConnected : 1;
        uint32_t m_AccelerometerConnected : 1;
        uint32_t m_IgnoreMouse : 1;
        uint32_t m_IgnoreKeyboard : 1;
        uint32_t m_IgnoreGamepads : 1;
        uint32_t m_IgnoreTouchDevice : 1;
        uint32_t m_IgnoreAcceleration : 1;
        uint32_t m_FlipScrollDirection : 1;

        DMHIDGamepadFunc m_GamepadConnectivityCallback;
        void* m_GamepadConnectivityUserdata;
    };
}

#endif
