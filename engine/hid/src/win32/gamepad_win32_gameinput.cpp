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

#include <GameInput.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/safe_windows.h>

#include "../hid_private.h"

#include "gamepad_win32_private.h"

namespace dmHID
{
    static const uint32_t INVALID_INDEX = 0xffffffff;
    static const uint32_t MAX_PENDING_GAMEINPUT_EVENTS = MAX_GAMEPAD_COUNT * 4;
    static const uint16_t HID_USAGE_PAGE_GENERIC_DESKTOP = 0x01;
    static const uint16_t HID_USAGE_PAGE_BUTTON          = 0x09;
    static const uint16_t HID_USAGE_X                    = 0x30;
    static const uint16_t HID_USAGE_Y                    = 0x31;
    static const uint16_t HID_USAGE_Z                    = 0x32;
    static const uint16_t HID_USAGE_RX                   = 0x33;
    static const uint16_t HID_USAGE_RY                   = 0x34;
    static const uint16_t HID_USAGE_RZ                   = 0x35;
    static const uint16_t HID_USAGE_SLIDER               = 0x36;
    static const uint16_t HID_USAGE_DIAL                 = 0x37;
    static const uint16_t HID_USAGE_WHEEL                = 0x38;
    static const uint16_t HID_USAGE_HAT_SWITCH           = 0x39;
    static const uint16_t NINTENDO_VENDOR_ID             = 0x057e;
    static const uint16_t SWITCH_JOYCON_LEFT_PRODUCT_ID  = 0x2006;
    static const uint16_t SWITCH_JOYCON_RIGHT_PRODUCT_ID = 0x2007;
    static const uint16_t SWITCH_PRO_PRODUCT_ID          = 0x2009;
    static const uint16_t SONY_VENDOR_ID                 = 0x054c;
    static const uint16_t SONY_DUALSENSE_PRODUCT_ID      = 0x0ce6;
    static const uint16_t SONY_DUALSENSE_EDGE_PRODUCT_ID = 0x0df2;

    typedef HRESULT (WINAPI *GameInputCreateFn)(IGameInput** game_input);

    struct GameInputGamepadDevice
    {
        char              m_Name[MAX_GAMEPAD_NAME_LENGTH];
        GamepadGuid       m_Guid;
        Gamepad*          m_Gamepad;
        IGameInputDevice* m_Device;
        GameInputDeviceFamily m_DeviceFamily;
        bool              m_PreferControllerState;
        bool              m_ReadDiagnosticsLogged;
        bool              m_GamepadStateDiagnosticsLogged;
        bool              m_GamepadStateActivityDiagnosticsLogged;
        bool              m_ControllerDiagnosticsLogged;
        bool              m_ControllerActivityDiagnosticsLogged;
    };

    struct PendingGameInputDeviceEvent
    {
        IGameInputDevice* m_Device;
        bool              m_Connected;
    };

    #define SET_BUTTON(packet, state, name)  \
        { \
            int ix = HID_ ## name; \
            if (state.buttons & name) \
                packet.m_Buttons[ix / 32] |= 1 << (ix % 32); \
            else \
                packet.m_Buttons[ix / 32] &= ~(1 << (ix % 32)); \
        }

    static bool PacketHasActivity(const GamepadPacket& packet, uint32_t axis_count, uint32_t button_count, uint32_t hat_count)
    {
        for (uint32_t i = 0; i < axis_count; ++i)
        {
            if (packet.m_Axis[i] > 0.05f || packet.m_Axis[i] < -0.05f)
            {
                return true;
            }
        }

        for (uint32_t i = 0; i < (button_count + 31) / 32; ++i)
        {
            if (packet.m_Buttons[i] != 0)
            {
                return true;
            }
        }

        for (uint32_t i = 0; i < hat_count; ++i)
        {
            if (packet.m_Hat[i] != 0)
            {
                return true;
            }
        }

        return false;
    }

    static bool GamepadHasActivity(Gamepad* gamepad)
    {
        return PacketHasActivity(gamepad->m_Packet, gamepad->m_AxisCount, gamepad->m_ButtonCount, gamepad->m_HatCount);
    }

    static bool ReadGamePadState(IGameInputReading* reading, Gamepad* pad, GameInputGamepadDevice* gameinput_device)
    {
        GameInputGamepadState state = {};
        if (!reading->GetGamepadState(&state))
        {
            return false;
        }

        GamepadPacket& packet = pad->m_Packet;

        memset(packet.m_Buttons, 0, sizeof(packet.m_Buttons));
        memset(packet.m_Hat, 0, sizeof(packet.m_Hat));

        SET_BUTTON(packet, state, GameInputGamepadNone);
        SET_BUTTON(packet, state, GameInputGamepadMenu);
        SET_BUTTON(packet, state, GameInputGamepadView);
        SET_BUTTON(packet, state, GameInputGamepadA);
        SET_BUTTON(packet, state, GameInputGamepadB);
        SET_BUTTON(packet, state, GameInputGamepadX);
        SET_BUTTON(packet, state, GameInputGamepadY);
        SET_BUTTON(packet, state, GameInputGamepadDPadUp);
        SET_BUTTON(packet, state, GameInputGamepadDPadDown);
        SET_BUTTON(packet, state, GameInputGamepadDPadLeft);
        SET_BUTTON(packet, state, GameInputGamepadDPadRight);
        SET_BUTTON(packet, state, GameInputGamepadLeftShoulder);
        SET_BUTTON(packet, state, GameInputGamepadRightShoulder);
        SET_BUTTON(packet, state, GameInputGamepadLeftThumbstick);
        SET_BUTTON(packet, state, GameInputGamepadRightThumbstick);

        pad->m_ButtonCount = HID_GameInputGamepad_Max;
        pad->m_HatCount    = 0;

        packet.m_Axis[0] = state.leftThumbstickX;
        packet.m_Axis[1] = state.leftThumbstickY;
        packet.m_Axis[2] = state.rightThumbstickX;
        packet.m_Axis[3] = state.rightThumbstickY;
        packet.m_Axis[4] = state.leftTrigger;
        packet.m_Axis[5] = state.rightTrigger;
        pad->m_AxisCount = 6;

        bool has_activity = state.buttons != 0 ||
            state.leftTrigger > 0.05f ||
            state.rightTrigger > 0.05f ||
            state.leftThumbstickX > 0.05f ||
            state.leftThumbstickX < -0.05f ||
            state.leftThumbstickY > 0.05f ||
            state.leftThumbstickY < -0.05f ||
            state.rightThumbstickX > 0.05f ||
            state.rightThumbstickX < -0.05f ||
            state.rightThumbstickY > 0.05f ||
            state.rightThumbstickY < -0.05f;

        if (gameinput_device != 0 &&
            (!gameinput_device->m_GamepadStateDiagnosticsLogged ||
             (has_activity && !gameinput_device->m_GamepadStateActivityDiagnosticsLogged)))
        {
            if (has_activity)
            {
                gameinput_device->m_GamepadStateActivityDiagnosticsLogged = true;
            }
            gameinput_device->m_GamepadStateDiagnosticsLogged = true;

            dmLogInfo("GameInput gamepad state '%s': kind=0x%08x buttons=0x%08x axes=%1.3f %1.3f %1.3f %1.3f triggers=%1.3f %1.3f active=%u",
                gameinput_device->m_Name,
                reading->GetInputKind(),
                (uint32_t) state.buttons,
                (double) state.leftThumbstickX,
                (double) state.leftThumbstickY,
                (double) state.rightThumbstickX,
                (double) state.rightThumbstickY,
                (double) state.leftTrigger,
                (double) state.rightTrigger,
                has_activity ? 1 : 0);
        }

        return true;
    }

    #undef SET_BUTTON

    static uint8_t GameInputSwitchToHat(GameInputSwitchPosition position)
    {
        switch (position)
        {
            case GameInputSwitchUp:        return 1;
            case GameInputSwitchUpRight:   return 2;
            case GameInputSwitchRight:     return 3;
            case GameInputSwitchDownRight: return 4;
            case GameInputSwitchDown:      return 5;
            case GameInputSwitchDownLeft:  return 6;
            case GameInputSwitchLeft:      return 7;
            case GameInputSwitchUpLeft:    return 8;
            case GameInputSwitchCenter:
            default:                       return 0;
        }
    }

    static bool ReadControllerState(IGameInputReading* reading, Gamepad* pad, GameInputGamepadDevice* gameinput_device)
    {
        uint32_t requested_axis_count   = reading->GetControllerAxisCount();
        uint32_t requested_button_count = reading->GetControllerButtonCount();
        uint32_t requested_switch_count = reading->GetControllerSwitchCount();

        if (requested_axis_count == 0 && requested_button_count == 0 && requested_switch_count == 0)
        {
            return false;
        }

        GamepadPacket& packet = pad->m_Packet;

        memset(packet.m_Axis, 0, sizeof(packet.m_Axis));
        memset(packet.m_Buttons, 0, sizeof(packet.m_Buttons));
        memset(packet.m_Hat, 0, sizeof(packet.m_Hat));

        uint32_t axis_count = requested_axis_count < MAX_GAMEPAD_AXIS_COUNT ? requested_axis_count : MAX_GAMEPAD_AXIS_COUNT;
        uint32_t button_count = requested_button_count < MAX_GAMEPAD_BUTTON_COUNT ? requested_button_count : MAX_GAMEPAD_BUTTON_COUNT;
        uint32_t switch_count = requested_switch_count < MAX_GAMEPAD_HAT_COUNT ? requested_switch_count : MAX_GAMEPAD_HAT_COUNT;

        float axes[MAX_GAMEPAD_AXIS_COUNT] = {};
        bool buttons[MAX_GAMEPAD_BUTTON_COUNT] = {};
        GameInputSwitchPosition switches[MAX_GAMEPAD_HAT_COUNT] = {};

        uint32_t axis_state_count = 0;
        uint32_t button_state_count = 0;
        uint32_t switch_state_count = 0;

        if (axis_count > 0)
        {
            axis_state_count = reading->GetControllerAxisState(axis_count, axes);
            axis_count = axis_state_count < axis_count ? axis_state_count : axis_count;
            for (uint32_t i = 0; i < axis_count; ++i)
            {
                packet.m_Axis[i] = axes[i];
            }
        }

        if (button_count > 0)
        {
            button_state_count = reading->GetControllerButtonState(button_count, buttons);
            button_count = button_state_count < button_count ? button_state_count : button_count;

            for (uint32_t i = 0; i < button_count; ++i)
            {
                if (buttons[i])
                {
                    packet.m_Buttons[i / 32] |= 1 << (i % 32);
                }
            }
        }

        if (switch_count > 0)
        {
            switch_state_count = reading->GetControllerSwitchState(switch_count, switches);
            switch_count = switch_state_count < switch_count ? switch_state_count : switch_count;

            for (uint32_t i = 0; i < switch_count; ++i)
            {
                packet.m_Hat[i] = GameInputSwitchToHat(switches[i]);
            }
        }

        uint32_t pressed_count = 0;
        uint32_t first_pressed = INVALID_INDEX;
        for (uint32_t i = 0; i < button_count; ++i)
        {
            if (buttons[i])
            {
                if (first_pressed == INVALID_INDEX)
                {
                    first_pressed = i;
                }
                ++pressed_count;
            }
        }

        bool has_activity = pressed_count != 0;
        for (uint32_t i = 0; i < axis_count && !has_activity; ++i)
        {
            has_activity = axes[i] > 0.05f || axes[i] < -0.05f;
        }
        for (uint32_t i = 0; i < switch_count && !has_activity; ++i)
        {
            has_activity = switches[i] != GameInputSwitchCenter;
        }

        if (gameinput_device != 0 &&
            (!gameinput_device->m_ControllerDiagnosticsLogged ||
             (has_activity && !gameinput_device->m_ControllerActivityDiagnosticsLogged)))
        {
            if (has_activity)
            {
                gameinput_device->m_ControllerActivityDiagnosticsLogged = true;
            }
            gameinput_device->m_ControllerDiagnosticsLogged = true;

            dmLogInfo("GameInput controller state '%s': kind=0x%08x requested=%u/%u/%u returned=%u/%u/%u axes=%1.3f %1.3f %1.3f %1.3f pressed=%u first_pressed=%u switch0=%u active=%u",
                gameinput_device->m_Name,
                reading->GetInputKind(),
                requested_axis_count,
                requested_button_count,
                requested_switch_count,
                axis_state_count,
                button_state_count,
                switch_state_count,
                (double) axes[0],
                (double) axes[1],
                (double) axes[2],
                (double) axes[3],
                pressed_count,
                first_pressed,
                switch_count > 0 ? (uint32_t) switches[0] : 0,
                has_activity ? 1 : 0);
        }

        pad->m_AxisCount = (uint8_t) axis_count;
        pad->m_ButtonCount = (uint8_t) button_count;
        pad->m_HatCount = (uint8_t) switch_count;

        return axis_count != 0 || button_count != 0 || switch_count != 0;
    }

    static bool IsRawAxisUsage(const GameInputRawDeviceReportItemInfo* item)
    {
        if (item->usageCount == 0 || item->usages == 0)
        {
            return false;
        }

        const GameInputUsage& usage = item->usages[0];
        return usage.page == HID_USAGE_PAGE_GENERIC_DESKTOP &&
               usage.id >= HID_USAGE_X &&
               usage.id <= HID_USAGE_WHEEL;
    }

    static bool IsRawHatUsage(const GameInputRawDeviceReportItemInfo* item)
    {
        if (item->usageCount == 0 || item->usages == 0)
        {
            return false;
        }

        const GameInputUsage& usage = item->usages[0];
        return usage.page == HID_USAGE_PAGE_GENERIC_DESKTOP && usage.id == HID_USAGE_HAT_SWITCH;
    }

    static bool IsRawButtonUsage(const GameInputRawDeviceReportItemInfo* item)
    {
        if (item->usageCount == 0 || item->usages == 0)
        {
            return false;
        }

        return item->usages[0].page == HID_USAGE_PAGE_BUTTON;
    }

    static void CountRawInputItems(const GameInputDeviceInfo* info, uint32_t* axis_count, uint32_t* button_count, uint32_t* hat_count)
    {
        *axis_count = 0;
        *button_count = 0;
        *hat_count = 0;

        if (info == 0 || info->inputReportInfo == 0)
        {
            return;
        }

        for (uint32_t report_index = 0; report_index < info->inputReportCount; ++report_index)
        {
            const GameInputRawDeviceReportInfo* report_info = &info->inputReportInfo[report_index];
            for (uint32_t item_index = 0; item_index < report_info->itemCount; ++item_index)
            {
                const GameInputRawDeviceReportItemInfo* item = &report_info->items[item_index];
                if (IsRawAxisUsage(item))
                {
                    ++*axis_count;
                }
                else if (IsRawHatUsage(item))
                {
                    ++*hat_count;
                }
                else if (IsRawButtonUsage(item))
                {
                    ++*button_count;
                }
            }
        }
    }

    static float NormalizeRawAxisValue(int64_t value, const GameInputRawDeviceReportItemInfo* item)
    {
        if (item->logicalMax <= item->logicalMin)
        {
            return 0.0f;
        }

        double normalized = ((double) value - (double) item->logicalMin) / ((double) item->logicalMax - (double) item->logicalMin);
        return (float) (normalized * 2.0 - 1.0);
    }

    static uint8_t RawHatToDefoldHat(int64_t value, const GameInputRawDeviceReportItemInfo* item)
    {
        if (item->logicalMin == 0 && item->logicalMax >= 7)
        {
            if (value >= 0 && value <= 7)
            {
                return (uint8_t) (value + 1);
            }
            return 0;
        }

        if (item->logicalMin == 1 && item->logicalMax >= 8)
        {
            if (value >= 1 && value <= 8)
            {
                return (uint8_t) value;
            }
            return 0;
        }

        return value >= 1 && value <= 8 ? (uint8_t) value : 0;
    }

    static bool ReadRawReportState(IGameInputReading* reading, Gamepad* pad)
    {
        IGameInputRawDeviceReport* report = 0;
        if (!reading->GetRawReport(&report) || report == 0)
        {
            return false;
        }

        const GameInputRawDeviceReportInfo* report_info = report->GetReportInfo();
        if (report_info == 0 || report_info->kind != GameInputRawInputReport || report_info->itemCount == 0 || report_info->items == 0)
        {
            report->Release();
            return false;
        }

        GamepadPacket& packet = pad->m_Packet;

        memset(packet.m_Axis, 0, sizeof(packet.m_Axis));
        memset(packet.m_Buttons, 0, sizeof(packet.m_Buttons));
        memset(packet.m_Hat, 0, sizeof(packet.m_Hat));

        uint32_t axis_count = 0;
        uint32_t button_count = 0;
        uint32_t hat_count = 0;

        for (uint32_t item_index = 0; item_index < report_info->itemCount; ++item_index)
        {
            const GameInputRawDeviceReportItemInfo* item = &report_info->items[item_index];
            int64_t value = 0;
            if (!report->GetItemValue(item_index, &value))
            {
                continue;
            }

            if (IsRawAxisUsage(item))
            {
                if (axis_count < MAX_GAMEPAD_AXIS_COUNT)
                {
                    packet.m_Axis[axis_count++] = NormalizeRawAxisValue(value, item);
                }
            }
            else if (IsRawHatUsage(item))
            {
                if (hat_count < MAX_GAMEPAD_HAT_COUNT)
                {
                    packet.m_Hat[hat_count++] = RawHatToDefoldHat(value, item);
                }
            }
            else if (IsRawButtonUsage(item))
            {
                if (button_count < MAX_GAMEPAD_BUTTON_COUNT)
                {
                    if (value != 0)
                    {
                        packet.m_Buttons[button_count / 32] |= 1 << (button_count % 32);
                    }
                    ++button_count;
                }
            }
        }

        pad->m_AxisCount = (uint8_t) axis_count;
        pad->m_ButtonCount = (uint8_t) button_count;
        pad->m_HatCount = (uint8_t) hat_count;

        report->Release();
        return axis_count != 0 || button_count != 0 || hat_count != 0;
    }

    struct GameInputGamepadContext
    {
        HContext                       m_HidContext;
        HMODULE                        m_GameInputModule;
        IGameInput*                    m_GameInput;
        GameInputCallbackToken         m_CallbackToken;
        CRITICAL_SECTION               m_PendingLock;
        bool                           m_PendingLockInitialized;
        PendingGameInputDeviceEvent    m_PendingEvents[MAX_PENDING_GAMEINPUT_EVENTS];
        uint32_t                       m_PendingEventCount;
        GameInputGamepadDevice         m_Devices[MAX_GAMEPAD_COUNT];
        bool                           m_AnyDeviceDiagnosticsLogged;
        bool                           m_AnyDeviceActivityDiagnosticsLogged;
        bool                           m_AnyDeviceMousePositionValid;
        int64_t                        m_AnyDeviceMouseX;
        int64_t                        m_AnyDeviceMouseY;
    };

    static GameInputGamepadContext* g_GameInputGamepadContext = 0;

    static void LogGameInputReadDiagnostics(GameInputGamepadContext* driver, GameInputGamepadDevice* gameinput_device, HRESULT gamepad_hr, HRESULT controller_hr, HRESULT raw_hr)
    {
        if (gameinput_device->m_ReadDiagnosticsLogged)
        {
            return;
        }

        gameinput_device->m_ReadDiagnosticsLogged = true;

        IGameInputReading* reading = 0;
        bool has_gamepad_state = false;
        uint32_t gamepad_reading_kind = 0;
        uint32_t gamepad_axis_count = 0;
        uint32_t gamepad_button_count = 0;
        uint32_t gamepad_switch_count = 0;

        HRESULT hr = driver->m_GameInput->GetCurrentReading(GameInputKindGamepad, gameinput_device->m_Device, &reading);
        if (SUCCEEDED(hr) && reading != 0)
        {
            GameInputGamepadState state = {};
            has_gamepad_state = reading->GetGamepadState(&state);
            gamepad_reading_kind = reading->GetInputKind();
            gamepad_axis_count = reading->GetControllerAxisCount();
            gamepad_button_count = reading->GetControllerButtonCount();
            gamepad_switch_count = reading->GetControllerSwitchCount();
            reading->Release();
            reading = 0;
        }

        bool has_raw_report = false;
        uint32_t raw_reading_kind = 0;
        uint32_t raw_report_id = 0;
        uint32_t raw_report_size = 0;
        uint32_t raw_report_item_count = 0;

        hr = driver->m_GameInput->GetCurrentReading(GameInputKindRawDeviceReport, gameinput_device->m_Device, &reading);
        if (SUCCEEDED(hr) && reading != 0)
        {
            raw_reading_kind = reading->GetInputKind();

            IGameInputRawDeviceReport* report = 0;
            if (reading->GetRawReport(&report) && report != 0)
            {
                has_raw_report = true;
                const GameInputRawDeviceReportInfo* report_info = report->GetReportInfo();
                if (report_info != 0)
                {
                    raw_report_id = report_info->id;
                    raw_report_size = report_info->size;
                    raw_report_item_count = report_info->itemCount;
                }
                report->Release();
            }
            reading->Release();
        }

        dmLogInfo("GameInput read '%s': hr gamepad=0x%08x controller=0x%08x raw=0x%08x gamepad_state=%u gamepad_kind=0x%08x controller_counts=%u/%u/%u raw_report=%u raw_kind=0x%08x raw_id=%u raw_size=%u raw_items=%u",
            gameinput_device->m_Name,
            (uint32_t) gamepad_hr,
            (uint32_t) controller_hr,
            (uint32_t) raw_hr,
            has_gamepad_state ? 1 : 0,
            gamepad_reading_kind,
            gamepad_axis_count,
            gamepad_button_count,
            gamepad_switch_count,
            has_raw_report ? 1 : 0,
            raw_reading_kind,
            raw_report_id,
            raw_report_size,
            raw_report_item_count);
    }

    static void ReleaseDevice(IGameInputDevice*& device)
    {
        if (device != 0)
        {
            device->Release();
            device = 0;
        }
    }

    static uint32_t GamePadDeviceToIndex(GameInputGamepadContext* driver, IGameInputDevice* device)
    {
        for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            if (driver->m_Devices[i].m_Device == device)
            {
                return i;
            }
        }
        return INVALID_INDEX;
    }

    static uint32_t GamePadFindFreeIndex(GameInputGamepadContext* driver)
    {
        for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            if (driver->m_Devices[i].m_Device == 0)
            {
                return i;
            }
        }
        return INVALID_INDEX;
    }

    static uint32_t GamepadToIndex(GameInputGamepadContext* driver, Gamepad* gamepad)
    {
        for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            if (driver->m_Devices[i].m_Gamepad == gamepad)
            {
                return i;
            }
        }
        return INVALID_INDEX;
    }

    static const char* GameInputFamilyName(GameInputDeviceFamily family)
    {
        switch (family)
        {
            case GameInputFamilyXboxOne: return "Xbox One Controller";
            case GameInputFamilyXbox360: return "Xbox 360 Controller";
            case GameInputFamilyHid:     return "HID Gamepad";
            default:                     return "GameInput Gamepad";
        }
    }

    static void LogAnyGameInputReadingDiagnostics(GameInputGamepadContext* driver)
    {
        IGameInputReading* reading = 0;
        HRESULT hr = driver->m_GameInput->GetCurrentReading(GameInputKindGamepad | GameInputKindController | GameInputKindMouse, 0, &reading);
        if (FAILED(hr) || reading == 0)
        {
            if (!driver->m_AnyDeviceDiagnosticsLogged)
            {
                driver->m_AnyDeviceDiagnosticsLogged = true;
                dmLogInfo("GameInput any-device read: hr=0x%08x", (uint32_t) hr);
            }
            return;
        }

        IGameInputDevice* reading_device = 0;
        reading->GetDevice(&reading_device);

        const GameInputDeviceInfo* info = reading_device != 0 ? reading_device->GetDeviceInfo() : 0;
        const char* display_name = info != 0 && info->displayName != 0 && info->displayName->data[0] != 0 ? info->displayName->data : 0;

        char name[MAX_GAMEPAD_NAME_LENGTH];
        if (display_name != 0)
        {
            dmStrlCpy(name, display_name, sizeof(name));
        }
        else if (info != 0)
        {
            dmSnPrintf(name, sizeof(name), "%s %04x:%04x", GameInputFamilyName(info->deviceFamily), info->vendorId, info->productId);
        }
        else
        {
            dmStrlCpy(name, "GameInput Device", sizeof(name));
        }

        float axes[MAX_GAMEPAD_AXIS_COUNT] = {};
        bool buttons[MAX_GAMEPAD_BUTTON_COUNT] = {};
        GameInputSwitchPosition switches[MAX_GAMEPAD_HAT_COUNT] = {};

        uint32_t axis_count = reading->GetControllerAxisCount();
        uint32_t button_count = reading->GetControllerButtonCount();
        uint32_t switch_count = reading->GetControllerSwitchCount();
        axis_count = axis_count < MAX_GAMEPAD_AXIS_COUNT ? axis_count : MAX_GAMEPAD_AXIS_COUNT;
        button_count = button_count < MAX_GAMEPAD_BUTTON_COUNT ? button_count : MAX_GAMEPAD_BUTTON_COUNT;
        switch_count = switch_count < MAX_GAMEPAD_HAT_COUNT ? switch_count : MAX_GAMEPAD_HAT_COUNT;

        uint32_t axis_state_count = axis_count > 0 ? reading->GetControllerAxisState(axis_count, axes) : 0;
        uint32_t button_state_count = button_count > 0 ? reading->GetControllerButtonState(button_count, buttons) : 0;
        uint32_t switch_state_count = switch_count > 0 ? reading->GetControllerSwitchState(switch_count, switches) : 0;

        axis_state_count = axis_state_count < MAX_GAMEPAD_AXIS_COUNT ? axis_state_count : MAX_GAMEPAD_AXIS_COUNT;
        button_state_count = button_state_count < MAX_GAMEPAD_BUTTON_COUNT ? button_state_count : MAX_GAMEPAD_BUTTON_COUNT;
        switch_state_count = switch_state_count < MAX_GAMEPAD_HAT_COUNT ? switch_state_count : MAX_GAMEPAD_HAT_COUNT;

        uint32_t pressed_count = 0;
        uint32_t first_pressed = INVALID_INDEX;
        for (uint32_t i = 0; i < button_state_count; ++i)
        {
            if (buttons[i])
            {
                if (first_pressed == INVALID_INDEX)
                {
                    first_pressed = i;
                }
                ++pressed_count;
            }
        }

        bool has_controller_activity = pressed_count != 0;
        for (uint32_t i = 0; i < axis_state_count && !has_controller_activity; ++i)
        {
            has_controller_activity = axes[i] > 0.05f || axes[i] < -0.05f;
        }
        for (uint32_t i = 0; i < switch_state_count && !has_controller_activity; ++i)
        {
            has_controller_activity = switches[i] != GameInputSwitchCenter;
        }

        GameInputMouseState mouse = {};
        bool has_mouse = reading->GetMouseState(&mouse);
        bool has_mouse_activity = has_mouse && (mouse.buttons != 0 || mouse.wheelX != 0 || mouse.wheelY != 0);
        if (has_mouse && driver->m_AnyDeviceMousePositionValid)
        {
            has_mouse_activity = has_mouse_activity || mouse.positionX != driver->m_AnyDeviceMouseX || mouse.positionY != driver->m_AnyDeviceMouseY;
        }
        if (has_mouse)
        {
            driver->m_AnyDeviceMousePositionValid = true;
            driver->m_AnyDeviceMouseX = mouse.positionX;
            driver->m_AnyDeviceMouseY = mouse.positionY;
        }

        bool has_activity = has_controller_activity || has_mouse_activity;
        if (!driver->m_AnyDeviceDiagnosticsLogged ||
            (has_activity && !driver->m_AnyDeviceActivityDiagnosticsLogged))
        {
            if (has_activity)
            {
                driver->m_AnyDeviceActivityDiagnosticsLogged = true;
            }
            driver->m_AnyDeviceDiagnosticsLogged = true;

            uint32_t tracked_index = reading_device != 0 ? GamePadDeviceToIndex(driver, reading_device) : INVALID_INDEX;
            dmLogInfo("GameInput any-device state '%s': tracked=%u family=%d vidpid=%04x:%04x kind=0x%08x returned=%u/%u/%u axes=%1.3f %1.3f %1.3f %1.3f pressed=%u first_pressed=%u switch0=%u mouse=%u mouse_pos=%lld/%lld mouse_buttons=0x%08x active=%u",
                name,
                tracked_index != INVALID_INDEX ? 1 : 0,
                info != 0 ? (int) info->deviceFamily : 0,
                info != 0 ? info->vendorId : 0,
                info != 0 ? info->productId : 0,
                reading->GetInputKind(),
                axis_state_count,
                button_state_count,
                switch_state_count,
                (double) axes[0],
                (double) axes[1],
                (double) axes[2],
                (double) axes[3],
                pressed_count,
                first_pressed,
                switch_state_count > 0 ? (uint32_t) switches[0] : 0,
                has_mouse ? 1 : 0,
                (long long) mouse.positionX,
                (long long) mouse.positionY,
                (uint32_t) mouse.buttons,
                has_activity ? 1 : 0);
        }

        ReleaseDevice(reading_device);
        reading->Release();
    }

    static void SetGameInputDeviceIdentity(GameInputGamepadDevice* gameinput_device)
    {
        const GameInputDeviceInfo* info = gameinput_device->m_Device->GetDeviceInfo();
        const char* display_name = info != 0 && info->displayName != 0 ? info->displayName->data : 0;
        gameinput_device->m_DeviceFamily = info != 0 ? info->deviceFamily : GameInputFamilyAggregate;

        if (display_name != 0 && display_name[0] != 0)
        {
            dmStrlCpy(gameinput_device->m_Name, display_name, MAX_GAMEPAD_NAME_LENGTH);
        }
        else if (info != 0)
        {
            dmSnPrintf(gameinput_device->m_Name, MAX_GAMEPAD_NAME_LENGTH, "%s %04x:%04x",
                GameInputFamilyName(info->deviceFamily), info->vendorId, info->productId);
        }
        else
        {
            dmStrlCpy(gameinput_device->m_Name, "GameInput Gamepad", MAX_GAMEPAD_NAME_LENGTH);
        }

        if (info != 0)
        {
            gameinput_device->m_Guid = CreateGUID(0x0003, info->vendorId, info->productId, info->revisionNumber, 0, gameinput_device->m_Name, 0, 0);
            gameinput_device->m_PreferControllerState = info->deviceFamily == GameInputFamilyHid &&
                (info->controllerAxisCount != 0 || info->controllerButtonCount != 0 || info->controllerSwitchCount != 0);

            uint32_t raw_axis_count = 0;
            uint32_t raw_button_count = 0;
            uint32_t raw_hat_count = 0;
            CountRawInputItems(info, &raw_axis_count, &raw_button_count, &raw_hat_count);

            dmLogInfo("GameInput device '%s': family=%d vidpid=%04x:%04x revision=%u supported=0x%08x input_reports=%u controller axes=%u buttons=%u switches=%u raw axes=%u buttons=%u hats=%u prefer_controller=%u",
                gameinput_device->m_Name,
                (int) info->deviceFamily,
                info->vendorId,
                info->productId,
                info->revisionNumber,
                info->supportedInput,
                info->inputReportCount,
                info->controllerAxisCount,
                info->controllerButtonCount,
                info->controllerSwitchCount,
                raw_axis_count,
                raw_button_count,
                raw_hat_count,
                gameinput_device->m_PreferControllerState ? 1 : 0);
        }
        else
        {
            gameinput_device->m_Guid = CreateGUID(0x0003, 0, 0, 0, 0, gameinput_device->m_Name, 0, 0);
        }
    }

    static bool IsHIDFallbackDevice(IGameInputDevice* device)
    {
        const GameInputDeviceInfo* info = device != 0 ? device->GetDeviceInfo() : 0;
        if (info == 0)
        {
            return false;
        }

        if (info->vendorId == NINTENDO_VENDOR_ID)
        {
            return info->productId == SWITCH_PRO_PRODUCT_ID ||
                   info->productId == SWITCH_JOYCON_LEFT_PRODUCT_ID ||
                   info->productId == SWITCH_JOYCON_RIGHT_PRODUCT_ID;
        }

        if (info->vendorId == SONY_VENDOR_ID)
        {
            return info->productId == SONY_DUALSENSE_PRODUCT_ID ||
                   info->productId == SONY_DUALSENSE_EDGE_PRODUCT_ID;
        }

        if (info->deviceFamily != GameInputFamilyHid)
        {
            return false;
        }

        return false;
    }

    static void QueueGameInputDeviceEvent(GameInputGamepadContext* driver, IGameInputDevice* device, bool connected)
    {
        device->AddRef();

        EnterCriticalSection(&driver->m_PendingLock);
        if (driver->m_PendingEventCount < MAX_PENDING_GAMEINPUT_EVENTS)
        {
            PendingGameInputDeviceEvent* event = &driver->m_PendingEvents[driver->m_PendingEventCount++];
            event->m_Device = device;
            event->m_Connected = connected;
            device = 0;
        }
        LeaveCriticalSection(&driver->m_PendingLock);

        if (device != 0)
        {
            dmLogWarning("Dropped GameInput device event; pending event queue is full");
            device->Release();
        }
    }

    static void CALLBACK GameInputGamepadDeviceCallback(GameInputCallbackToken callback_token,
                                                        void* context,
                                                        IGameInputDevice* device,
                                                        uint64_t timestamp,
                                                        GameInputDeviceStatus current_status,
                                                        GameInputDeviceStatus previous_status)
    {
        (void) callback_token;
        (void) timestamp;
        (void) previous_status;

        GameInputGamepadContext* driver = (GameInputGamepadContext*) context;
        QueueGameInputDeviceEvent(driver, device, (current_status & GameInputDeviceConnected) != 0);
    }

    static void ProcessPendingGameInputDeviceEvents(GameInputGamepadContext* driver)
    {
        PendingGameInputDeviceEvent pending_events[MAX_PENDING_GAMEINPUT_EVENTS];
        uint32_t pending_event_count = 0;

        EnterCriticalSection(&driver->m_PendingLock);
        pending_event_count = driver->m_PendingEventCount;
        memcpy(pending_events, driver->m_PendingEvents, pending_event_count * sizeof(PendingGameInputDeviceEvent));
        driver->m_PendingEventCount = 0;
        LeaveCriticalSection(&driver->m_PendingLock);

        for (uint32_t i = 0; i < pending_event_count; ++i)
        {
            PendingGameInputDeviceEvent* pending_event = &pending_events[i];
            uint32_t index = GamePadDeviceToIndex(driver, pending_event->m_Device);

            if (pending_event->m_Connected)
            {
                if (index == INVALID_INDEX)
                {
#if !defined(_GAMING_XBOX)
                    if (IsHIDFallbackDevice(pending_event->m_Device))
                    {
                        ReleaseDevice(pending_event->m_Device);
                        continue;
                    }
#endif

                    index = GamePadFindFreeIndex(driver);
                    if (index == INVALID_INDEX)
                    {
                        dmLogError("Unable to allocate a slot for a new GameInput gamepad, max capacity reached (%d).", MAX_GAMEPAD_COUNT);
                        ReleaseDevice(pending_event->m_Device);
                        continue;
                    }

                    Gamepad* gamepad = CreateGamepad(driver->m_HidContext);
                    if (gamepad == 0)
                    {
                        ReleaseDevice(pending_event->m_Device);
                        continue;
                    }

                    GameInputGamepadDevice* gameinput_device = &driver->m_Devices[index];
                    gameinput_device->m_Gamepad = gamepad;
                    gameinput_device->m_Device = pending_event->m_Device;
                    pending_event->m_Device = 0;
                    SetGameInputDeviceIdentity(gameinput_device);
                    GamepadXInputBindGameInputDevice(gameinput_device->m_Device, gameinput_device->m_DeviceFamily, gameinput_device->m_Name);
                    SetGamepadConnectionStatus(driver->m_HidContext, gameinput_device->m_Gamepad, true);
                }
            }
            else if (index != INVALID_INDEX)
            {
                GameInputGamepadDevice* gameinput_device = &driver->m_Devices[index];
                GamepadXInputUnbindGameInputDevice(gameinput_device->m_Device);
                SetGamepadConnectionStatus(driver->m_HidContext, gameinput_device->m_Gamepad, false);
                ReleaseGamepad(driver->m_HidContext, gameinput_device->m_Gamepad);
                ReleaseDevice(gameinput_device->m_Device);
                memset(gameinput_device, 0, sizeof(*gameinput_device));
            }

            ReleaseDevice(pending_event->m_Device);
        }
    }

    static void ReleasePendingGameInputDeviceEvents(GameInputGamepadContext* driver)
    {
        EnterCriticalSection(&driver->m_PendingLock);
        for (uint32_t i = 0; i < driver->m_PendingEventCount; ++i)
        {
            ReleaseDevice(driver->m_PendingEvents[i].m_Device);
        }
        driver->m_PendingEventCount = 0;
        LeaveCriticalSection(&driver->m_PendingLock);
    }

    static void ClearGameInputDevices(GameInputGamepadContext* driver)
    {
        for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            GameInputGamepadDevice* gameinput_device = &driver->m_Devices[i];
            if (gameinput_device->m_Device != 0)
            {
                GamepadXInputUnbindGameInputDevice(gameinput_device->m_Device);
                if (gameinput_device->m_Gamepad != 0)
                {
                    SetGamepadConnectionStatus(driver->m_HidContext, gameinput_device->m_Gamepad, false);
                    ReleaseGamepad(driver->m_HidContext, gameinput_device->m_Gamepad);
                }
                ReleaseDevice(gameinput_device->m_Device);
                memset(gameinput_device, 0, sizeof(*gameinput_device));
            }
        }
    }

    bool GamepadGameInputInitialize(HContext context)
    {
        assert(g_GameInputGamepadContext == 0);

        GameInputGamepadContext* driver = new GameInputGamepadContext();
        memset(driver, 0, sizeof(*driver));
        driver->m_HidContext = context;
        g_GameInputGamepadContext = driver;

        InitializeCriticalSection(&driver->m_PendingLock);
        driver->m_PendingLockInitialized = true;

#if defined(_GAMING_XBOX)
        HRESULT hr = GameInputCreate(&driver->m_GameInput);
#else
        driver->m_GameInputModule = LoadLibraryA("GameInput.dll");
        if (driver->m_GameInputModule == 0)
        {
            dmLogError("GameInput.dll is not available");
            GamepadGameInputFinalize(context);
            return false;
        }

        GameInputCreateFn game_input_create = (GameInputCreateFn) GetProcAddress(driver->m_GameInputModule, "GameInputCreate");
        if (game_input_create == 0)
        {
            dmLogError("GameInput.dll does not export GameInputCreate");
            GamepadGameInputFinalize(context);
            return false;
        }

        HRESULT hr = game_input_create(&driver->m_GameInput);
#endif
        if (FAILED(hr))
        {
            dmLogError("Failed to create GameInput context (0x%08x)", (uint32_t) hr);
            GamepadGameInputFinalize(context);
            return false;
        }

        hr = driver->m_GameInput->RegisterDeviceCallback(0,
                                                         GameInputKindGamepad | GameInputKindController | GameInputKindRawDeviceReport,
                                                         GameInputDeviceConnected,
                                                         GameInputBlockingEnumeration,
                                                         driver,
                                                         GameInputGamepadDeviceCallback,
                                                         &driver->m_CallbackToken);
        if (FAILED(hr))
        {
            dmLogError("Failed to register GameInput device callback (0x%08x)", (uint32_t) hr);
            GamepadGameInputFinalize(context);
            return false;
        }

        return true;
    }

    void GamepadGameInputFinalize(HContext context)
    {
        (void) context;
        GameInputGamepadContext* driver = g_GameInputGamepadContext;
        if (driver == 0)
        {
            return;
        }

        if (driver->m_GameInput != 0 && driver->m_CallbackToken != 0)
        {
            driver->m_GameInput->StopCallback(driver->m_CallbackToken);
            driver->m_GameInput->UnregisterCallback(driver->m_CallbackToken, 1000000);
            driver->m_CallbackToken = 0;
        }

        ReleasePendingGameInputDeviceEvents(driver);
        ClearGameInputDevices(driver);

        if (driver->m_GameInput != 0)
        {
            driver->m_GameInput->Release();
            driver->m_GameInput = 0;
        }

        if (driver->m_GameInputModule != 0)
        {
            FreeLibrary(driver->m_GameInputModule);
            driver->m_GameInputModule = 0;
        }

        if (driver->m_PendingLockInitialized)
        {
            DeleteCriticalSection(&driver->m_PendingLock);
            driver->m_PendingLockInitialized = false;
        }

        delete driver;
        g_GameInputGamepadContext = 0;
    }

    void GamepadGameInputUpdate(HContext context, Gamepad* gamepad)
    {
        GameInputGamepadContext* driver = g_GameInputGamepadContext;
        if (driver == 0)
        {
            return;
        }

        (void) context;

        uint32_t index = GamepadToIndex(driver, gamepad);
        assert(index != INVALID_INDEX);

        GameInputGamepadDevice* gameinput_device = &driver->m_Devices[index];
        if (gameinput_device->m_Device == 0)
        {
            return;
        }

        if (GamepadXInputUpdate(gameinput_device->m_Device, gameinput_device->m_Gamepad))
        {
            return;
        }

        IGameInputReading* reading = 0;
        bool read_state = false;
        bool try_raw_report = gameinput_device->m_PreferControllerState;
        HRESULT hr = driver->m_GameInput->GetCurrentReading(GameInputKindGamepad, gameinput_device->m_Device, &reading);
        HRESULT gamepad_hr = hr;
        HRESULT controller_hr = E_FAIL;
        HRESULT raw_hr = E_FAIL;
        if (SUCCEEDED(hr) && reading != 0)
        {
            if (gameinput_device->m_PreferControllerState)
            {
                read_state = ReadControllerState(reading, gamepad, gameinput_device);
            }
            else
            {
                bool read_gamepad_state = ReadGamePadState(reading, gamepad, gameinput_device);
                bool gamepad_has_activity = read_gamepad_state && GamepadHasActivity(gamepad);
                bool read_controller_state = !gamepad_has_activity && ReadControllerState(reading, gamepad, gameinput_device);
                read_state = read_controller_state || read_gamepad_state;
            }
            reading->Release();
            if (read_state && !try_raw_report)
            {
                LogGameInputReadDiagnostics(driver, gameinput_device, gamepad_hr, controller_hr, raw_hr);
                return;
            }
        }

        if (!read_state)
        {
            reading = 0;
            hr = driver->m_GameInput->GetCurrentReading(GameInputKindController, gameinput_device->m_Device, &reading);
            controller_hr = hr;
            if (SUCCEEDED(hr) && reading != 0)
            {
                read_state = ReadControllerState(reading, gamepad, gameinput_device);
                reading->Release();
                if (read_state && !try_raw_report)
                {
                    LogGameInputReadDiagnostics(driver, gameinput_device, gamepad_hr, controller_hr, raw_hr);
                    return;
                }
            }
        }

        if (try_raw_report || !read_state)
        {
            reading = 0;
            hr = driver->m_GameInput->GetCurrentReading(GameInputKindRawDeviceReport, gameinput_device->m_Device, &reading);
            raw_hr = hr;
            if (SUCCEEDED(hr) && reading != 0)
            {
                read_state = ReadRawReportState(reading, gamepad) || read_state;
                reading->Release();
            }
        }

        LogGameInputReadDiagnostics(driver, gameinput_device, gamepad_hr, controller_hr, raw_hr);
        if (!read_state)
        {
            LogAnyGameInputReadingDiagnostics(driver);
        }
    }

    void GamepadGameInputDetectDevices(HContext context)
    {
        (void) context;
        if (g_GameInputGamepadContext != 0)
        {
            ProcessPendingGameInputDeviceEvents(g_GameInputGamepadContext);
        }
    }

    void GamepadGameInputGetDeviceName(HContext context, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH])
    {
        (void) context;

        GameInputGamepadContext* driver = g_GameInputGamepadContext;
        if (driver == 0)
        {
            dmStrlCpy(name, "GameInput Gamepad", MAX_GAMEPAD_NAME_LENGTH);
            return;
        }

        uint32_t index = GamepadToIndex(driver, gamepad);

        if (index != INVALID_INDEX)
        {
            dmStrlCpy(name, driver->m_Devices[index].m_Name, MAX_GAMEPAD_NAME_LENGTH);
        }
        else
        {
            dmStrlCpy(name, "GameInput Gamepad", MAX_GAMEPAD_NAME_LENGTH);
        }
    }

    bool GamepadGameInputGetDeviceGuid(HContext context, HGamepad gamepad, GamepadGuid* guid)
    {
        (void) context;

        GameInputGamepadContext* driver = g_GameInputGamepadContext;
        if (driver == 0)
        {
            return false;
        }

        uint32_t index = GamepadToIndex(driver, gamepad);

        if (index == INVALID_INDEX || guid == 0)
        {
            return false;
        }

        *guid = driver->m_Devices[index].m_Guid;
        return true;
    }
}
