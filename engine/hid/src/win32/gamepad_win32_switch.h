// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#ifndef DM_HID_GAMEPAD_WIN32_SWITCH_H
#define DM_HID_GAMEPAD_WIN32_SWITCH_H

#include "../hid.h"

namespace dmHID
{
    struct Gamepad;

    bool GamepadSwitchInitialize(HContext context);
    void GamepadSwitchFinalize(HContext context);
    void GamepadSwitchDetectDevices(HContext context);
    bool GamepadSwitchUpdate(HContext context, Gamepad* gamepad);
    bool GamepadSwitchGetDeviceName(HContext context, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH]);
    bool GamepadSwitchGetDeviceGuid(HContext context, HGamepad gamepad, GamepadGuid* guid);
    bool GamepadSwitchOwnsGamepad(HContext context, Gamepad* gamepad);
}

#endif
