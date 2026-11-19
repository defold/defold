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

#ifndef DM_WIN32_JOYSTICK_PACKET_H
#define DM_WIN32_JOYSTICK_PACKET_H

#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

namespace dmPlatform
{
    struct Win32JoystickPacket
    {
        float   m_Axes[8];
        uint8_t m_Hats[4];
        uint8_t m_Buttons[32 + 4 * 4];
        uint8_t m_AxisCount, m_HatCount, m_ButtonCount;
    };

    struct Win32DirectControls
    {
        uint8_t m_AxisMask, m_ButtonCount, m_HatCount;
    };

    void DecodeWin32XInput(const XINPUT_GAMEPAD& state, Win32JoystickPacket* packet);
    void DecodeWin32DirectInput(const DIJOYSTATE& state, const Win32DirectControls& controls, Win32JoystickPacket* packet);
}
#endif
