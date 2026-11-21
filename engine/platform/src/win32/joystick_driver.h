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

#ifndef DM_WIN32_JOYSTICK_DRIVER_H
#define DM_WIN32_JOYSTICK_DRIVER_H

#include "joystick_packet.h"

namespace dmPlatform
{
    struct Win32XInputCapabilities
    {
        XINPUT_CAPABILITIES m_Capabilities;
        WORD                m_Vendor, m_Product, m_Version, m_Reserved;
        DWORD               m_Reserved2;
    };

    struct Win32JoystickDrivers
    {
        IDirectInput8W* m_Direct;
        HMODULE         m_XInput;
        DWORD(WINAPI * m_State)(DWORD, XINPUT_STATE*);
        DWORD(WINAPI * m_Capabilities)(DWORD, DWORD, XINPUT_CAPABILITIES*);
        DWORD(WINAPI * m_Extended)(DWORD, DWORD, DWORD, Win32XInputCapabilities*);
    };

    void Win32OpenJoystickDrivers(HINSTANCE instance, Win32JoystickDrivers* drivers);
}
#endif
