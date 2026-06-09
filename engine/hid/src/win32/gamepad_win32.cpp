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

#include <dlib/dstrings.h>

#include "../hid_private.h"
#include "gamepad_win32_private.h"

namespace dmHID
{
    bool GamepadInitialize(HContext context)
    {
        if (!GamepadXInputInitialize(context))
        {
            return false;
        }

        if (!GamepadGameInputInitialize(context))
        {
            GamepadFinalize(context);
            return false;
        }

#if !defined(_GAMING_XBOX)
        if (!GamepadSwitchInitialize(context))
        {
            GamepadFinalize(context);
            return false;
        }

        if (!GamepadDualSenseInitialize(context))
        {
            GamepadFinalize(context);
            return false;
        }
#endif

        return true;
    }

    void GamepadFinalize(HContext context)
    {
#if !defined(_GAMING_XBOX)
        GamepadDualSenseFinalize(context);
        GamepadSwitchFinalize(context);
#endif
        GamepadGameInputFinalize(context);
        GamepadXInputFinalize(context);
    }

    void GamepadDetectDevices(HContext context)
    {
        GamepadGameInputDetectDevices(context);

#if !defined(_GAMING_XBOX)
        GamepadSwitchDetectDevices(context);
        GamepadDualSenseDetectDevices(context);
#endif
    }

    void GamepadUpdate(HContext context, Gamepad* gamepad)
    {
#if !defined(_GAMING_XBOX)
        if (GamepadSwitchUpdate(context, gamepad))
        {
            return;
        }
        if (GamepadDualSenseUpdate(context, gamepad))
        {
            return;
        }
#endif

        GamepadGameInputUpdate(context, gamepad);
    }

    void GamepadGetDeviceName(HContext context, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH])
    {
#if !defined(_GAMING_XBOX)
        if (GamepadSwitchGetDeviceName(context, gamepad, name))
        {
            return;
        }
        if (GamepadDualSenseGetDeviceName(context, gamepad, name))
        {
            return;
        }
#endif

        GamepadGameInputGetDeviceName(context, gamepad, name);
    }

    bool GamepadGetDeviceGuid(HContext context, HGamepad gamepad, GamepadGuid* guid)
    {
#if !defined(_GAMING_XBOX)
        if (GamepadSwitchGetDeviceGuid(context, gamepad, guid))
        {
            return true;
        }
        if (GamepadDualSenseGetDeviceGuid(context, gamepad, guid))
        {
            return true;
        }
#endif

        return GamepadGameInputGetDeviceGuid(context, gamepad, guid);
    }
}
