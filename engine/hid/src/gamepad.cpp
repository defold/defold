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

#include <assert.h>
#include <string.h>

#include <dlib/log.h>

#include "hid.h"
#include "hid_private.h"

namespace dmHID
{
    static uint8_t GamepadToIndex(HContext context, Gamepad* gamepad)
    {
        for (int i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            if (&context->m_Gamepads[i] == gamepad)
            {
                return i;
            }
        }
        assert(0);
        return -1;
    }

    void SetGamepadConnectionStatus(HContext context, Gamepad* gamepad, bool connection_status)
    {
        uint8_t gamepad_index = GamepadToIndex(context, gamepad);

        if (gamepad->m_Connected != connection_status)
        {
            if (!context->m_Finalizing && context->m_GamepadConnectivityCallback)
            {
                if (!context->m_GamepadConnectivityCallback(gamepad_index, connection_status, context->m_GamepadConnectivityUserdata))
                {
                    char device_name[dmHID::MAX_GAMEPAD_NAME_LENGTH];
                    GetGamepadDeviceName(context, gamepad, device_name);
                    dmLogWarning("The connection for '%s' was ignored by the callback function!", device_name);
                    return;
                }
            }
            else if (!context->m_Finalizing)
            {
                dmLogWarning("There was no callback function set to handle the gamepad connection!");
            }

            SetGamepadConnectivity(context, gamepad_index, connection_status);
            gamepad->m_Connected = connection_status;
        }
    }

    Gamepad* CreateGamepad(HContext context)
    {
        for (int i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            if (!context->m_Gamepads[i].m_Allocated)
            {
                context->m_Gamepads[i].m_Allocated = 1;
                context->m_Gamepads[i].m_LayoutLegacy = true; // TODO: For the next task of supporting SDL layouts
                return &context->m_Gamepads[i];
            }
        }

        dmLogError("Unable to allocate a slot for a new gamepad, max capacity reached (%d).", MAX_GAMEPAD_COUNT);
        return 0;
    }

    void ReleaseGamepad(HContext context, Gamepad* gamepad)
    {
        uint8_t gamepad_index = GamepadToIndex(context, gamepad);
        assert(context->m_Gamepads[gamepad_index].m_Allocated);
        context->m_Gamepads[gamepad_index].m_Allocated = 0;
    }

    void GetGamepadDeviceName(HContext context, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH])
    {
        name[0] = 0;
        if (!gamepad->m_Allocated)
        {
            return;
        }

        GamepadGetDeviceName(context, gamepad, name);
    }

    bool GetGamepadDeviceGuid(HContext context, HGamepad gamepad, GamepadGuid* guid)
    {
        if (!gamepad->m_Allocated)
        {
            return false;
        }

        return GamepadGetDeviceGuid(context, gamepad, guid);
    }

    bool GetPlatformGamepadUserId(HContext context, HGamepad gamepad, uint32_t* user_id)
    {
        (void) context;
        (void) gamepad;
        (void) user_id;
        return false;
    }
}
