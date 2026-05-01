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

#ifndef HID_NATIVE_PRIVATE_H
#define HID_NATIVE_PRIVATE_H

#include "hid_private.h"

namespace dmHID
{
    struct GamepadDriver;
    typedef bool (*GamepadDriverInitializeCb)(HContext context, GamepadDriver* driver);
    typedef void (*GamepadDriverDestroyCb)(HContext context, GamepadDriver* driver);
    typedef void (*GamepadDriverUpdateCb)(HContext context, GamepadDriver* driver, Gamepad* gamepad);
    typedef void (*GamepadDriverDetectDevicesCb)(HContext context, GamepadDriver* driver);
    typedef void (*GamepadDriverGetGamepadDeviceNameCb)(HContext context, GamepadDriver*, Gamepad* gamepad, char name[MAX_GAMEPAD_NAME_LENGTH]);

    struct GamepadDriver
    {
        GamepadDriverInitializeCb           m_Initialize;
        GamepadDriverDestroyCb              m_Destroy;
        GamepadDriverUpdateCb               m_Update;
        GamepadDriverDetectDevicesCb        m_DetectDevices;
        GamepadDriverGetGamepadDeviceNameCb m_GetGamepadDeviceName;
    };

    Gamepad*       CreateGamepad(HContext context, GamepadDriver* driver);
    void           ReleaseGamepad(HContext context, Gamepad* gamepad);
    GamepadDriver* CreateGamepadDriverGLFW(HContext context);
    void           SetGamepadConnectionStatus(HContext context, Gamepad* gamepad, bool connection_status);
}

#endif // HID_NATIVE_PRIVATE_H
