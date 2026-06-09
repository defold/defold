// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#ifndef DM_HID_GAMEPAD_WIN32_PRIVATE_H
#define DM_HID_GAMEPAD_WIN32_PRIVATE_H

#include <GameInput.h>

#include "../hid.h"

namespace dmHID
{
    struct Gamepad;

    enum HIDGameInputPadButtons
    {
        // Same ordering as the Xbox GameInput backend.
        HID_GameInputGamepadNone,
        HID_GameInputGamepadMenu,
        HID_GameInputGamepadView,
        HID_GameInputGamepadA,
        HID_GameInputGamepadB,
        HID_GameInputGamepadX,
        HID_GameInputGamepadY,
        HID_GameInputGamepadDPadUp,
        HID_GameInputGamepadDPadDown,
        HID_GameInputGamepadDPadLeft,
        HID_GameInputGamepadDPadRight,
        HID_GameInputGamepadLeftShoulder,
        HID_GameInputGamepadRightShoulder,
        HID_GameInputGamepadLeftThumbstick,
        HID_GameInputGamepadRightThumbstick,
        HID_GameInputGamepad_Max
    };

    bool GamepadGameInputInitialize(HContext context);
    void GamepadGameInputFinalize(HContext context);
    void GamepadGameInputDetectDevices(HContext context);
    void GamepadGameInputUpdate(HContext context, Gamepad* gamepad);
    void GamepadGameInputGetDeviceName(HContext context, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH]);
    bool GamepadGameInputGetDeviceGuid(HContext context, HGamepad gamepad, GamepadGuid* guid);

#if !defined(_GAMING_XBOX)
    bool GamepadDualSenseInitialize(HContext context);
    void GamepadDualSenseFinalize(HContext context);
    void GamepadDualSenseDetectDevices(HContext context);
    bool GamepadDualSenseUpdate(HContext context, Gamepad* gamepad);
    bool GamepadDualSenseGetDeviceName(HContext context, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH]);
    bool GamepadDualSenseGetDeviceGuid(HContext context, HGamepad gamepad, GamepadGuid* guid);
    bool GamepadDualSenseOwnsGamepad(HContext context, Gamepad* gamepad);

    bool GamepadSwitchInitialize(HContext context);
    void GamepadSwitchFinalize(HContext context);
    void GamepadSwitchDetectDevices(HContext context);
    bool GamepadSwitchUpdate(HContext context, Gamepad* gamepad);
    bool GamepadSwitchGetDeviceName(HContext context, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH]);
    bool GamepadSwitchGetDeviceGuid(HContext context, HGamepad gamepad, GamepadGuid* guid);
    bool GamepadSwitchOwnsGamepad(HContext context, Gamepad* gamepad);
#endif

#if defined(_GAMING_XBOX)
    inline bool GamepadXInputInitialize(HContext context) { (void) context; return true; }
    inline void GamepadXInputFinalize(HContext context) { (void) context; }
    inline void GamepadXInputBindGameInputDevice(IGameInputDevice* device, GameInputDeviceFamily device_family, const char* name) { (void) device; (void) device_family; (void) name; }
    inline void GamepadXInputUnbindGameInputDevice(IGameInputDevice* device) { (void) device; }
    inline bool GamepadXInputUpdate(IGameInputDevice* device, Gamepad* gamepad) { (void) device; (void) gamepad; return false; }
#else
    bool GamepadXInputInitialize(HContext context);
    void GamepadXInputFinalize(HContext context);
    void GamepadXInputBindGameInputDevice(IGameInputDevice* device, GameInputDeviceFamily device_family, const char* name);
    void GamepadXInputUnbindGameInputDevice(IGameInputDevice* device);
    bool GamepadXInputUpdate(IGameInputDevice* device, Gamepad* gamepad);
#endif
}

#endif
