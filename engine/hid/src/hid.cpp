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

#include <string.h>

#include <dlib/log.h>

#include <platform/window.hpp>
#include <platform/platform_window_constants.h>

#include "hid.h"
#include "hid_private.h"

namespace dmHID
{
    static bool InitializeGamepads(HContext context)
    {
        memset(context->m_Gamepads, 0, sizeof(Gamepad) * MAX_GAMEPAD_COUNT);

        if (context->m_IgnoreGamepads)
        {
            return true;
        }

        if (!GamepadInitialize(context))
        {
            dmLogError("Unable to initialize gamepad input");
            return false;
        }

        GamepadDetectDevices(context);
        return true;
    }

    static void UpdateGamepads(HContext context)
    {
        GamepadDetectDevices(context);

        for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            Gamepad* gamepad = &context->m_Gamepads[i];
            if (!gamepad->m_Allocated)
            {
                continue;
            }

            GamepadUpdate(context, gamepad);
        }
    }

    bool Init(HContext context)
    {
        if (context == 0x0)
        {
            return false;
        }

        context->m_Finalizing = 0;
        if (!KeyboardInitialize(context) ||
            !MouseInitialize(context) ||
            !TouchInitialize(context))
        {
            return false;
        }

        if (!InitializeGamepads(context))
        {
            TouchFinalize(context);
            MouseFinalize(context);
            KeyboardFinalize(context);
            return false;
        }

        return true;
    }

    void Final(HContext context)
    {
        if (context != 0x0)
        {
            context->m_Finalizing = 1;
            GamepadFinalize(context);
            TouchFinalize(context);
            MouseFinalize(context);
            KeyboardFinalize(context);
        }
    }

    bool Update(HContext context)
    {
        dmhash_t prev_state_hash = context->m_StateHash;
        dmPlatform::PollEvents(context->m_Window);
        if (!context->m_IgnoreKeyboard)
        {
            KeyboardUpdate(context);
        }
        if (!context->m_IgnoreMouse)
        {
            MouseUpdate(context);
        }
        if (!context->m_IgnoreGamepads)
        {
            UpdateGamepads(context);
        }
        if (!context->m_IgnoreTouchDevice)
        {
            TouchUpdate(context);
        }
        if (!context->m_IgnoreAcceleration)
        {
            AccelerationPacket packet;
            context->m_AccelerometerConnected = 0;
            if (dmPlatform::GetAcceleration(context->m_Window, &packet.m_X, &packet.m_Y, &packet.m_Z))
            {
                context->m_AccelerometerConnected = 1;
                context->m_AccelerationPacket = packet;
            }
        }
        context->m_StateHash = CalcStateHash(context);
        return prev_state_hash != context->m_StateHash;
    }

    void ShowKeyboard(HContext context, KeyboardType type, bool autoclose)
    {
        WindowDeviceState device_state;

        switch (type)
        {
            case KEYBOARD_TYPE_DEFAULT:
                device_state = WINDOW_DEVICE_STATE_KEYBOARD_DEFAULT;
                break;
            case KEYBOARD_TYPE_NUMBER_PAD:
                device_state = WINDOW_DEVICE_STATE_KEYBOARD_NUMBER_PAD;
                break;
            case KEYBOARD_TYPE_EMAIL:
                device_state = WINDOW_DEVICE_STATE_KEYBOARD_EMAIL;
                break;
            case KEYBOARD_TYPE_PASSWORD:
                device_state = WINDOW_DEVICE_STATE_KEYBOARD_PASSWORD;
                break;
            default:
                dmLogWarning("Unknown keyboard type %d\n", type);
        }

        dmPlatform::SetDeviceState(context->m_Window, device_state, true, autoclose);
    }

    void HideKeyboard(HContext context)
    {
        dmPlatform::SetDeviceState(context->m_Window, WINDOW_DEVICE_STATE_KEYBOARD_DEFAULT, false);
    }

    void ResetKeyboard(HContext context)
    {
        KeyboardReset(context);
    }

    void EnableAccelerometer(HContext context)
    {
        dmPlatform::SetDeviceState(context->m_Window, WINDOW_DEVICE_STATE_ACCELEROMETER, true);
    }

    void ShowMouseCursor(HContext context)
    {
        dmPlatform::SetDeviceState(context->m_Window, WINDOW_DEVICE_STATE_CURSOR, true);
    }

    void HideMouseCursor(HContext context)
    {
        dmPlatform::SetDeviceState(context->m_Window, WINDOW_DEVICE_STATE_CURSOR, false);
    }

    bool GetCursorVisible(HContext context)
    {
        return !dmPlatform::GetDeviceState(context->m_Window, WINDOW_DEVICE_STATE_CURSOR_LOCK);
    }
}
