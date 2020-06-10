// Copyright 2020 The Defold Foundation
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
        uint8_t m_HatCount;
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
        DMHIDGamepadFunc m_GamepadConnectivityCallback;
        void* m_GamepadConnectivityUserdata;

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
    };
}

#endif
