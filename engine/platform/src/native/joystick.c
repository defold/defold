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

#include "internal.h"

static int IsJoystickSlot(int slot)
{
    return dmNativeInitialized && (unsigned)slot <= NATIVE_JOYSTICK_LAST;
}

int dmNativeGetJoystickParam(int slot, int param)
{
    return IsJoystickSlot(slot) ? dmNativeOSGetJoystickParam(slot, param) : 0;
}

int dmNativeGetJoystickPos(int slot, float* values, int capacity)
{
    if (!values || capacity <= 0)
    {
        return 0;
    }

    memset(values, 0, (size_t)capacity * sizeof(*values));
    return IsJoystickSlot(slot) ? dmNativeOSGetJoystickPos(slot, values, capacity) : 0;
}

int dmNativeGetJoystickButtons(int slot, unsigned char* values, int capacity)
{
    if (!values || capacity <= 0)
    {
        return 0;
    }

    memset(values, 0, capacity);
    return IsJoystickSlot(slot) ? dmNativeOSGetJoystickButtons(slot, values, capacity) : 0;
}

int dmNativeGetJoystickHats(int slot, unsigned char* values, int capacity)
{
    if (!values || capacity <= 0)
    {
        return 0;
    }

    memset(values, NATIVE_HAT_CENTERED, capacity);
    return IsJoystickSlot(slot) ? dmNativeOSGetJoystickHats(slot, values, capacity) : 0;
}

int dmNativeGetJoystickDeviceId(int slot, char** id)
{
    if (!id)
    {
        return 0;
    }

    *id = NULL;
    return IsJoystickSlot(slot) ? dmNativeOSGetJoystickDeviceId(slot, id) : 0;
}

int dmNativeSetGamepadCallback(Nativegamepadfun callback)
{
    if (dmNativeWin.opened)
    {
        dmNativeWin.gamepadCallback = callback;
    }

    return dmNativeWin.opened;
}

int dmNativeSetDeviceChangedCallback(Nativedevicechangedfun callback)
{
    return 1;
}
