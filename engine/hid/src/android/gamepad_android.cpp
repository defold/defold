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
#include <android/api-level.h>

#include <platform/window.hpp>
#include <platform/platform_window_android.h>

#include "hid_native_private.h"

namespace dmHID
{
    // SDL derives its generic Android mapping from the capabilities stored in the GUID.
    // Nintendo controllers require dedicated profiles rather than this generic fallback.
    static bool GetAndroidGamepadCapabilities(HWindow window, uint32_t joystick_index, uint16_t* button_mask, uint16_t* axis_mask)
    {
        GamepadGuid guid;
        const char* device_guid = dmPlatform::GetJoystickDeviceGuid(window, joystick_index);
        if (device_guid == 0 || !ParseGamepadGuid(device_guid, &guid))
            return false;
        const uint8_t* bytes = (const uint8_t*)&guid;
        *button_mask = bytes[12] | (bytes[13] << 8);
        *axis_mask = bytes[14] | (bytes[15] << 8);
        return guid.m_Bus == 5 && guid.m_Vendor != 0x057e && (*button_mask & 0x0f) != 0;
    }

    static void MapAndroidGamepadPacket(HGamepad gamepad, const AndroidGamepadMapping* mapping, uint8_t* buttons)
    {
        GamepadPacket* packet = &gamepad->m_Packet;
        uint8_t mapped_buttons[GAMEPAD_MAPPED_BUTTON_COUNT] = {};
        const uint8_t button_indices[] = { 0, 1, 2, 3, 4, 6, 7, 8, 9, 10, 5 };
        uint16_t button_mask = mapping->m_ButtonMask;
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(button_indices); ++i)
        {
            uint32_t button = button_indices[i];
            // SDL uses Back as B on TV remotes without a B button.
            if (i == GAMEPAD_MAPPED_BUTTON_B && (button_mask & 2) == 0 && (button_mask & (1 << 4)) != 0)
            {
                mapped_buttons[i] = buttons[4];
                button_mask &= ~(1 << 4);
            }
            else if (button_mask & (1 << button))
            {
                mapped_buttons[i] = buttons[button];
            }
        }
        memset(packet->m_Buttons, 0, sizeof(packet->m_Buttons));
        packet->m_Hat[0] = 0;
        const uint8_t hat_masks[] = { 1, 4, 8, 2 };
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(hat_masks); ++i)
        {
            if ((button_mask & (1 << (11 + i))) != 0 && buttons[11 + i])
                packet->m_Hat[0] |= hat_masks[i];
        }
        for (uint32_t i = 0; i < GAMEPAD_MAPPED_AXIS_COUNT; ++i)
        {
            if ((mapping->m_AxisMask & (1 << i)) == 0)
                packet->m_Axis[i] = i >= GAMEPAD_MAPPED_AXIS_LEFT_TRIGGER ? -1.0f : 0.0f;
        }
        gamepad->m_AxisCount = GAMEPAD_MAPPED_AXIS_COUNT;
        gamepad->m_ButtonCount = GAMEPAD_MAPPED_BUTTON_COUNT;
        gamepad->m_HatCount = 1;
        memcpy(buttons, mapped_buttons, sizeof(mapped_buttons));
    }

    uint32_t GetAndroidGamepadMappingSupport(HWindow window, uint32_t joystick_index)
    {
        uint16_t button_mask;
        uint16_t axis_mask;
        if (GetAndroidGamepadCapabilities(window, joystick_index, &button_mask, &axis_mask))
            return GAMEPAD_MAPPING_SUPPORT_AUTOMATIC;
        return GAMEPAD_MAPPING_SUPPORT_NONE;
    }

    void SetAndroidGamepadMapping(HWindow window, uint32_t joystick_index, HGamepad gamepad, AndroidGamepadMapping* state, const char* mapping)
    {
        state->m_ButtonMask = 0;
        state->m_AxisMask = 0;
        if (gamepad->m_LayoutLegacy)
            return;

        if (mapping == 0)
        {
            uint16_t button_mask;
            uint16_t axis_mask;
            if (!GetAndroidGamepadCapabilities(window, joystick_index, &button_mask, &axis_mask))
                return;
            // Guide generally acts as Android Home before Android 11, as in SDL.
            if (android_get_device_api_level() < 30)
                button_mask &= ~(1 << 5);
            state->m_ButtonMask = button_mask;
            state->m_AxisMask = axis_mask;
            float axes[GAMEPAD_MAPPED_AXIS_COUNT] = {};
            axes[GAMEPAD_MAPPED_AXIS_LEFT_TRIGGER] = -1.0f;
            axes[GAMEPAD_MAPPED_AXIS_RIGHT_TRIGGER] = -1.0f;
            dmPlatform::InitializeAndroidJoystickAxes(joystick_index, axes, GAMEPAD_MAPPED_AXIS_COUNT);
            return;
        }

        // SDL initializes mapped trigger axes to their released endpoint before receiving motion.
        float axes[MAX_GAMEPAD_AXIS_COUNT] = {};
        const char* triggers[] = { ",lefttrigger:", ",righttrigger:" };
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(triggers); ++i)
        {
            const char* target = strstr(mapping, triggers[i]);
            if (target == 0)
                continue;
            target += strlen(triggers[i]);
            // Defold scales signed trigger bindings across the full normalized range too.
            float released = *target == '-' ? 1.0f : -1.0f;
            if (*target == '+' || *target == '-')
            {
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
                released = -released;
                ++end;
            }
            if (*end == ',' || *end == 0)
                axes[axis] = released;
        }
        dmPlatform::InitializeAndroidJoystickAxes(joystick_index, axes, MAX_GAMEPAD_AXIS_COUNT);
    }

    void UpdateAndroidGamepad(HWindow window, uint32_t joystick_index, HGamepad gamepad, const AndroidGamepadMapping* mapping, uint8_t* buttons)
    {
        dmPlatform::SetAndroidJoystickLayoutLegacy(joystick_index, gamepad->m_LayoutLegacy != 0);
        GamepadPacket& packet = gamepad->m_Packet;
        gamepad->m_AxisCount = dmPlatform::GetJoystickAxes(window, joystick_index, packet.m_Axis, MAX_GAMEPAD_AXIS_COUNT);
        gamepad->m_HatCount = dmPlatform::GetJoystickHats(window, joystick_index, packet.m_Hat, MAX_GAMEPAD_HAT_COUNT);
        gamepad->m_ButtonCount = dmPlatform::GetJoystickButtons(window, joystick_index, buttons, MAX_GAMEPAD_BUTTON_COUNT);
        if (!gamepad->m_LayoutLegacy && mapping->m_ButtonMask)
            MapAndroidGamepadPacket(gamepad, mapping, buttons);
    }
}
