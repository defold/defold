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

#include "joystick_packet.h"

namespace dmPlatform
{
    static float SignedAxis(LONG value)
    {
        return ((float)value + 0.5f) / 32767.5f;
    }

    static void AppendHat(Win32JoystickPacket* packet, uint8_t mask)
    {
        packet->m_Hats[packet->m_HatCount++] = mask;
        for (int direction = 0; direction < 4; ++direction)
        {
            packet->m_Buttons[packet->m_ButtonCount++] = (mask >> direction) & 1;
        }
    }

    void DecodeWin32XInput(const XINPUT_GAMEPAD& state, Win32JoystickPacket* packet)
    {
        packet->m_AxisCount = 6;
        packet->m_Axes[0] = SignedAxis(state.sThumbLX);
        packet->m_Axes[1] = -SignedAxis(state.sThumbLY);
        packet->m_Axes[2] = SignedAxis(state.sThumbRX);
        packet->m_Axes[3] = -SignedAxis(state.sThumbRY);
        packet->m_Axes[4] = state.bLeftTrigger / 127.5f - 1.0f;
        packet->m_Axes[5] = state.bRightTrigger / 127.5f - 1.0f;

        static const WORD buttons[] = {
            XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
            XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_BACK,
            XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB
        };
        packet->m_ButtonCount = sizeof(buttons) / sizeof(buttons[0]);
        for (unsigned i = 0; i < packet->m_ButtonCount; ++i)
        {
            packet->m_Buttons[i] = (state.wButtons & buttons[i]) != 0;
        }

        uint8_t mask = 0;
        if (state.wButtons & XINPUT_GAMEPAD_DPAD_UP)
        {
            mask |= 1;
        }

        if (state.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
        {
            mask |= 2;
        }

        if (state.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
        {
            mask |= 4;
        }

        if (state.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
        {
            mask |= 8;
        }

        if ((mask & 5) == 5)
        {
            mask &= ~5;
        }

        if ((mask & 10) == 10)
        {
            mask &= ~10;
        }

        packet->m_HatCount = 0;
        AppendHat(packet, mask);
    }

    void DecodeWin32DirectInput(const DIJOYSTATE2& state, const Win32DirectControls& controls, Win32JoystickPacket* packet)
    {
        const LONG axes[] = { state.lX, state.lY, state.lZ, state.lRx, state.lRy, state.lRz, state.rglSlider[0], state.rglSlider[1] };
        packet->m_AxisCount = 0;
        for (unsigned i = 0; i < sizeof(axes) / sizeof(axes[0]); ++i)
        {
            if (controls.m_AxisMask & (1 << i))
            {
                packet->m_Axes[packet->m_AxisCount++] = SignedAxis(axes[i]);
            }
        }

        packet->m_ButtonCount = controls.m_ButtonCount;
        for (unsigned i = 0; i < packet->m_ButtonCount; ++i)
        {
            packet->m_Buttons[i] = (state.rgbButtons[i] & 0x80) != 0;
        }

        static const uint8_t directions[] = { 1, 3, 2, 6, 4, 12, 8, 9 };
        packet->m_HatCount = 0;
        for (unsigned i = 0; i < controls.m_HatCount; ++i)
        {
            unsigned direction = LOWORD(state.rgdwPOV[i]) / 4500;
            AppendHat(packet, direction < sizeof(directions) ? directions[direction] : 0);
        }
    }

}
