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

#include <stdlib.h>
#include <string.h>

#include <platform/window.hpp>
#include <platform/platform_window_android.h>

#include "hid_native_private.h"

namespace dmHID
{
    void SetAndroidGamepadMapping(HWindow window, uint32_t joystick_index, HGamepad gamepad, const char* mapping)
    {
        if (gamepad->m_LayoutLegacy || mapping == 0)
            return;

        // SDL initializes mapped trigger axes to their released endpoint before receiving motion.
        float axes[MAX_GAMEPAD_AXIS_COUNT] = {};
        const char* triggers[] = { ",lefttrigger:", ",righttrigger:" };
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(triggers); ++i)
        {
            const char* target = strstr(mapping, triggers[i]);
            if (target == 0)
                continue;
            target += strlen(triggers[i]);
            float minimum = -1.0f;
            float maximum = 1.0f;
            if (*target == '+' || *target == '-')
            {
                minimum = 0.0f;
                maximum = *target == '+' ? 1.0f : -1.0f;
                ++target;
            }
            if (*target++ != 'a')
                continue;
            char* end = 0;
            unsigned long axis = strtoul(target, &end, 10);
            if (end == target || axis >= MAX_GAMEPAD_AXIS_COUNT)
                continue;
            if (*end == '~')
            {
                minimum = maximum;
                ++end;
            }
            if (*end == ',' || *end == 0)
                axes[axis] = minimum;
        }
        dmPlatform::InitializeAndroidJoystickAxes(joystick_index, axes, MAX_GAMEPAD_AXIS_COUNT);
    }

    void UpdateAndroidGamepad(HWindow window, uint32_t joystick_index, HGamepad gamepad, uint8_t* buttons)
    {
        dmPlatform::SetAndroidJoystickLayoutLegacy(joystick_index, gamepad->m_LayoutLegacy != 0);
        GamepadPacket& packet = gamepad->m_Packet;
        gamepad->m_AxisCount = dmPlatform::GetJoystickAxes(window, joystick_index, packet.m_Axis, MAX_GAMEPAD_AXIS_COUNT);
        gamepad->m_HatCount = dmPlatform::GetJoystickHats(window, joystick_index, packet.m_Hat, MAX_GAMEPAD_HAT_COUNT);
        gamepad->m_ButtonCount = dmPlatform::GetJoystickButtons(window, joystick_index, buttons, MAX_GAMEPAD_BUTTON_COUNT);
    }
}
