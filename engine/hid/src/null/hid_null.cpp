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

#include <dlib/dstrings.h>

#include "../hid_private.h"

namespace dmHID
{
    bool Init(HContext context)
    {
        if (context == 0x0)
        {
            return false;
        }

        memset(context->m_Gamepads, 0, sizeof(Gamepad) * MAX_GAMEPAD_COUNT);
        CreateGamepad(context);
        return true;
    }

    void Final(HContext context)
    {
        if (context != 0x0)
        {
            context->m_Finalizing = 1;
        }
    }

    bool Update(HContext context)
    {
        dmhash_t prev_state_hash = context->m_StateHash;

        context->m_Keyboards[0].m_Connected = !context->m_IgnoreKeyboard;
        context->m_Mice[0].m_Connected = !context->m_IgnoreMouse;
        context->m_TouchDevices[0].m_Connected = !context->m_IgnoreTouchDevice;

        Gamepad* gamepad = &context->m_Gamepads[0];
        gamepad->m_Connected = !context->m_IgnoreGamepads;
        gamepad->m_ButtonCount = MAX_GAMEPAD_BUTTON_COUNT;
        gamepad->m_AxisCount = MAX_GAMEPAD_AXIS_COUNT;

        context->m_StateHash = CalcStateHash(context);
        return prev_state_hash != context->m_StateHash;
    }

    void ShowKeyboard(HContext context, KeyboardType type, bool autoclose)
    {
        (void) context;
        (void) type;
        (void) autoclose;
    }

    void HideKeyboard(HContext context)
    {
        (void) context;
    }

    void ResetKeyboard(HContext context)
    {
        (void) context;
    }

    void EnableAccelerometer(HContext context)
    {
        (void) context;
    }

    void ShowMouseCursor(HContext context)
    {
        (void) context;
    }

    void HideMouseCursor(HContext context)
    {
        (void) context;
    }

    bool GetCursorVisible(HContext context)
    {
        (void) context;
        return true;
    }

    bool GamepadInitialize(HContext context)
    {
        return CreateGamepad(context) != 0;
    }

    void GamepadFinalize(HContext context)
    {
        (void) context;
    }

    void GamepadDetectDevices(HContext context)
    {
        (void) context;
    }

    void GamepadUpdate(HContext context, Gamepad* gamepad)
    {
        (void) context;
        gamepad->m_Connected = 1;
        gamepad->m_ButtonCount = MAX_GAMEPAD_BUTTON_COUNT;
        gamepad->m_AxisCount = MAX_GAMEPAD_AXIS_COUNT;
    }

    void GamepadGetDeviceName(HContext context, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH])
    {
        (void) context;
        (void) gamepad;
        dmStrlCpy(name, "null_device", MAX_GAMEPAD_NAME_LENGTH);
    }

    bool GamepadGetDeviceGuid(HContext context, HGamepad gamepad, GamepadGuid* guid)
    {
        (void) context;
        (void) gamepad;
        memset(guid, 0, sizeof(*guid));
        return true;
    }
}
