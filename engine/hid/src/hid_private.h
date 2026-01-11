// Copyright 2020-2026 The Defold Foundation
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

#ifndef HID_PRIVATE_H
#define HID_PRIVATE_H

#include "hid.h"

#include <dlib/array.h>
#include <dlib/hash.h>

namespace dmHID
{
    typedef uint8_t HGamepadDriver;

    struct Gamepad
    {
        GamepadPacket  m_Packet;
        HGamepadDriver m_Driver;
        uint8_t        m_AxisCount;
        uint8_t        m_ButtonCount;
        uint8_t        m_HatCount    : 7;
        uint8_t        m_Connected   : 1;
    };

    struct Keyboard
    {
        KeyboardPacket  m_Packet;
        uint32_t        m_Index : 31;
        uint32_t        m_Connected : 1;
    };

    struct Mouse
    {
        MousePacket     m_Packet;
        uint32_t        m_Index : 31;
        uint32_t        m_Connected : 1;
    };

    struct TouchDevice
    {
        TouchDevicePacket   m_Packet;
        uint32_t            m_Index : 31;
        uint32_t            m_Connected : 1;
    };

    struct Context
    {
        Context();

        dmPlatform::HWindow m_Window;
        Gamepad            m_Gamepads[MAX_GAMEPAD_COUNT];
        Keyboard           m_Keyboards[MAX_KEYBOARD_COUNT];
        Mouse              m_Mice[MAX_MOUSE_COUNT];
        TouchDevice        m_TouchDevices[MAX_TOUCH_DEVICE_COUNT];
        TextPacket         m_TextPacket;
        MarkedTextPacket   m_MarkedTextPacket;
        AccelerationPacket m_AccelerationPacket;
        FHIDGamepadFunc    m_GamepadConnectivityCallback;
        void*              m_GamepadConnectivityUserdata;
        void*              m_NativeContext;
        void*              m_NativeContextUserData;
        dmhash_t           m_StateHash;

        uint32_t m_AccelerometerConnected : 1;
        uint32_t m_IgnoreMouse : 1;
        uint32_t m_IgnoreKeyboard : 1;
        uint32_t m_IgnoreGamepads : 1;
        uint32_t m_IgnoreTouchDevice : 1;
        uint32_t m_IgnoreAcceleration : 1;
        uint32_t m_FlipScrollDirection : 1;
        uint32_t : 25;
    };

    // TODO: start using the dmUser namespace
    // TODO: How to represent user id from/to C/Lua
    //      I.e. convert from uint32_t to platform type
    bool GetPlatformGamepadUserId(HContext context, HGamepad gamepad, uint32_t* user_id);
    int  GetKeyValue(Key key);
    int  GetMouseButtonValue(MouseButton button);

    dmhash_t CalcStateHash(HContext context);
}

#endif
