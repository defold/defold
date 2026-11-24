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
    typedef bool (*GamepadDriverGetGamepadDeviceGuidCb)(HContext context, GamepadDriver*, Gamepad* gamepad, GamepadGuid* guid);
    typedef uint32_t (*GamepadDriverGetGamepadMappingSupportCb)(HContext context, GamepadDriver*, Gamepad* gamepad);
    typedef void (*GamepadDriverSetGamepadMappingCb)(HContext context, GamepadDriver*, Gamepad* gamepad, const char* mapping);

    struct GamepadDriver
    {
        GamepadDriverInitializeCb               m_Initialize;
        GamepadDriverDestroyCb                  m_Destroy;
        GamepadDriverUpdateCb                   m_Update;
        GamepadDriverDetectDevicesCb            m_DetectDevices;
        GamepadDriverGetGamepadDeviceNameCb     m_GetGamepadDeviceName;
        GamepadDriverGetGamepadDeviceGuidCb     m_GetGamepadDeviceGuid;
        GamepadDriverGetGamepadMappingSupportCb m_GetGamepadMappingSupport = 0;
        GamepadDriverSetGamepadMappingCb        m_SetGamepadMapping = 0;
    };

    Gamepad*       CreateGamepad(HContext context, GamepadDriver* driver);
    void           ReleaseGamepad(HContext context, Gamepad* gamepad);
    void           SetGamepadConnectionStatus(HContext context, Gamepad* gamepad, bool connection_status);

    GamepadDriver* CreateGamepadDriverPlatform(HContext context);
    GamepadDriver* CreateGamepadDriverApple(HContext context);

#if defined(ANDROID)
    // Owned by the platform device; a zero button mask keeps the raw packet layout.
    struct AndroidGamepadMapping
    {
        uint16_t m_ButtonMask;
        uint16_t m_AxisMask;
    };

    uint32_t GetAndroidGamepadMappingSupport(HWindow window, uint32_t joystick_index);
    void SetAndroidGamepadMapping(HWindow window, uint32_t joystick_index, HGamepad gamepad, AndroidGamepadMapping* state, const char* mapping);
    // Writes up to MAX_GAMEPAD_BUTTON_COUNT entries into the caller's buttons buffer.
    void UpdateAndroidGamepad(HWindow window, uint32_t joystick_index, HGamepad gamepad, const AndroidGamepadMapping* mapping, uint8_t* buttons);
#endif
}

#endif // HID_NATIVE_PRIVATE_H
