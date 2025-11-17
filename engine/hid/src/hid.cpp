// Copyright 2020-2025 The Defold Foundation
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
#include <dlib/dstrings.h>
#include <dlib/utf8.h>

#include <platform/platform_window.h>
#include <platform/platform_window_constants.h>

#include "hid.h"
#include "hid_private.h"

namespace dmHID
{
    NewContextParams::NewContextParams()
    {
        memset(this, 0, sizeof(NewContextParams));
    }

    Context::Context()
    {
        memset(this, 0, sizeof(Context));
    }

    HContext NewContext(const NewContextParams& params)
    {
        HContext context = new Context();
        context->m_IgnoreMouse = params.m_IgnoreMouse;
        context->m_IgnoreKeyboard = params.m_IgnoreKeyboard;
        context->m_IgnoreGamepads = params.m_IgnoreGamepads;
        context->m_IgnoreTouchDevice = params.m_IgnoreTouchDevice;
        context->m_IgnoreAcceleration = params.m_IgnoreAcceleration;
        context->m_FlipScrollDirection = params.m_FlipScrollDirection;
        return context;
    }

    void SetGamepadConnectivityCallback(HContext context, FHIDGamepadFunc callback, void* callback_ctx)
    {
        context->m_GamepadConnectivityCallback = callback;
        context->m_GamepadConnectivityUserdata = callback_ctx;
    }

    void SetWindow(HContext context, dmPlatform::HWindow window)
    {
        context->m_Window = window;
    }

    void DeleteContext(HContext context)
    {
        delete context;
    }

    HGamepad GetGamepad(HContext context, uint8_t index)
    {
        if (index < MAX_GAMEPAD_COUNT)
            return &context->m_Gamepads[index];
        else
            return INVALID_GAMEPAD_HANDLE;
    }

    HKeyboard GetKeyboard(HContext context, uint8_t index)
    {
        if (index < MAX_KEYBOARD_COUNT)
            return &context->m_Keyboards[index];
        else
            return INVALID_KEYBOARD_HANDLE;
    }

    HMouse GetMouse(HContext context, uint8_t index)
    {
        if (index < MAX_MOUSE_COUNT)
            return &context->m_Mice[index];
        else
            return INVALID_MOUSE_HANDLE;
    }

    HTouchDevice GetTouchDevice(HContext context, uint8_t index)
    {
        if (index < MAX_TOUCH_DEVICE_COUNT)
            return &context->m_TouchDevices[index];
        else
            return INVALID_TOUCH_DEVICE_HANDLE;
    }

    bool GetGamepadUserId(HContext context, HGamepad gamepad, uint32_t* out)
    {
        #if defined(DM_PLATFORM_VENDOR)
            return dmHID::GetPlatformGamepadUserId(context, gamepad, out);
        #else
            return false;
        #endif
    }

    uint32_t GetGamepadButtonCount(HGamepad gamepad)
    {
        return gamepad->m_ButtonCount;
    }

    uint32_t GetGamepadHatCount(HGamepad gamepad)
    {
        return gamepad->m_HatCount;
    }

    uint32_t GetGamepadAxisCount(HGamepad gamepad)
    {
        return gamepad->m_AxisCount;
    }

    bool IsKeyboardConnected(HKeyboard keyboard)
    {
        return keyboard->m_Connected;
    }

    bool IsMouseConnected(HMouse mouse)
    {
        return mouse->m_Connected;
    }

    bool IsGamepadConnected(HGamepad gamepad)
    {
        if (gamepad != 0x0)
            return gamepad->m_Connected;
        else
            return false;
    }

    bool IsTouchDeviceConnected(HTouchDevice device)
    {
        return device->m_Connected;
    }

    bool IsAccelerometerConnected(HContext context)
    {
        return context->m_AccelerometerConnected;
    }

    bool GetKeyboardPacket(HKeyboard keyboard, KeyboardPacket* out_packet)
    {
        if (out_packet != 0x0 && keyboard != 0x0 && keyboard->m_Connected)
        {
            *out_packet = keyboard->m_Packet;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool GetTextPacket(HContext context, TextPacket* out_packet)
    {
        Keyboard* keyboard = &context->m_Keyboards[0];
        if (out_packet != 0x0 && keyboard->m_Connected)
        {
            *out_packet = context->m_TextPacket;
            context->m_TextPacket.m_Size = 0;
            context->m_TextPacket.m_Text[0] = '\0';
            return true;
        }
        else
        {
            return false;
        }
    }

    void AddKeyboardChar(HContext context, int chr)
    {
        if (context) {
            char buf[5];
            uint32_t n = dmUtf8::ToUtf8((uint16_t) chr, buf);
            buf[n] = '\0';
            TextPacket* p = &context->m_TextPacket;
            p->m_Size = dmStrlCat(p->m_Text, buf, sizeof(p->m_Text));
        }
    }

    bool GetMarkedTextPacket(HContext context, MarkedTextPacket* out_packet)
    {
        Keyboard* keyboard = &context->m_Keyboards[0];
        if (out_packet != 0x0 && keyboard->m_Connected)
        {
            *out_packet = context->m_MarkedTextPacket;
            context->m_MarkedTextPacket.m_Size = 0;
            context->m_MarkedTextPacket.m_HasText = 0;
            context->m_MarkedTextPacket.m_Text[0] = '\0';
            return true;
        }
        else
        {
            return false;
        }
    }

    void SetMarkedText(HContext context, char* text)
    {
        if (context) {
            MarkedTextPacket* p = &context->m_MarkedTextPacket;
            p->m_HasText = 1;
            p->m_Size = dmStrlCpy(p->m_Text, text, sizeof(p->m_Text));
        }
    }

    void SetGamepadConnectivity(HContext context, int gamepad, bool connected)
    {
        assert(context);

        GamepadPacket* p = &context->m_Gamepads[gamepad].m_Packet;
        p->m_GamepadDisconnected = !connected;
        p->m_GamepadConnected = connected;
    }

    bool GetMousePacket(HMouse mouse, MousePacket* out_packet)
    {
        if (out_packet != 0x0 && mouse != 0x0 && mouse->m_Connected)
        {
            *out_packet = mouse->m_Packet;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool GetGamepadPacket(HGamepad gamepad, GamepadPacket* out_packet)
    {
        if (gamepad != 0x0 && out_packet != 0x0)
        {
            *out_packet = gamepad->m_Packet;
            gamepad->m_Packet.m_GamepadDisconnected = false;
            gamepad->m_Packet.m_GamepadConnected = false;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool GetTouchDevicePacket(HTouchDevice device, TouchDevicePacket* out_packet)
    {
        if (out_packet != 0x0 && device != 0x0 && device->m_Connected)
        {
            *out_packet = device->m_Packet;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool GetAccelerationPacket(HContext context, AccelerationPacket* out_packet)
    {
        if (out_packet != 0x0)
        {
            *out_packet = context->m_AccelerationPacket;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool GetKey(KeyboardPacket* packet, Key key)
    {
       int key_index = (int) key - dmPlatform::PLATFORM_KEY_START;

        if (packet != 0x0)
            return packet->m_Keys[key_index / 32] & (1 << (key_index % 32));
        else
            return false;
    }

    void SetKey(HKeyboard keyboard, Key key, bool value)
    {
        if (keyboard != 0x0)
        {
            int key_index = (int) key - dmPlatform::PLATFORM_KEY_START;

            if (value)
                keyboard->m_Packet.m_Keys[key_index / 32] |= (1 << (key_index % 32));
            else
                keyboard->m_Packet.m_Keys[key_index / 32] &= ~(1 << (key_index % 32));
        }
    }

    bool GetMouseButton(MousePacket* packet, MouseButton button)
    {
        if (packet != 0x0)
            return packet->m_Buttons[button / 32] & (1 << (button % 32));
        else
            return false;
    }

    void SetMouseButton(HMouse mouse, MouseButton button, bool value)
    {
        if (mouse != 0x0)
        {
            if (value)
                mouse->m_Packet.m_Buttons[button / 32] |= (1 << (button % 32));
            else
                mouse->m_Packet.m_Buttons[button / 32] &= ~(1 << (button % 32));
        }
    }

    void SetMousePosition(HMouse mouse, int32_t x, int32_t y)
    {
        if (mouse != 0x0)
        {
            MousePacket& packet = mouse->m_Packet;
            packet.m_PositionX = x;
            packet.m_PositionY = y;
        }
    }

    void SetMouseWheel(HMouse mouse, int32_t value)
    {
        if (mouse != 0x0)
        {
            mouse->m_Packet.m_Wheel = value;
        }
    }

    bool GetGamepadButton(GamepadPacket* packet, uint32_t button)
    {
        if (packet != 0x0)
            return packet->m_Buttons[button / 32] & (1 << (button % 32));
        else
            return false;
    }

    void SetGamepadButton(HGamepad gamepad, uint32_t button, bool value)
    {
        if (gamepad != 0x0)
        {
            if (value)
                gamepad->m_Packet.m_Buttons[button / 32] |= (1 << (button % 32));
            else
                gamepad->m_Packet.m_Buttons[button / 32] &= ~(1 << (button % 32));
        }
    }

    bool GetGamepadHat(GamepadPacket* packet, uint32_t hat, uint8_t* out_hat_value)
    {
        if (packet != 0x0)
        {
            *out_hat_value = packet->m_Hat[hat];
            return true;
        } else {
            return false;
        }
    }

    void SetGamepadAxis(HGamepad gamepad, uint32_t axis, float value)
    {
        if (gamepad != 0x0)
        {
            gamepad->m_Packet.m_Axis[axis] = value;
        }
    }

    // NOTE: A bit contrived function only used for unit-tests. See AddTouchPosition
    bool GetTouch(TouchDevicePacket* packet, uint32_t touch_index, int32_t* x, int32_t* y, uint32_t* id, bool* pressed, bool* released)
    {
        if (packet != 0x0
                && x != 0x0
                && y != 0x0
                && id != 0x0)
        {
            if (touch_index < packet->m_TouchCount)
            {
                const Touch& t = packet->m_Touches[touch_index];
                *x = t.m_X;
                *y = t.m_Y;
                *id = t.m_Id;

                if (pressed != 0x0) {
                    *pressed = t.m_Phase == dmHID::PHASE_BEGAN;
                }
                if (released != 0x0) {
                    *released = t.m_Phase == dmHID::PHASE_ENDED || t.m_Phase == dmHID::PHASE_CANCELLED;
                }
                return true;
            }
        }
        return false;
    }

    // NOTE: A bit contrived function only used for unit-tests
    // We should perhaps include additional relevant touch-arguments
    void AddTouch(HTouchDevice device, int32_t x, int32_t y, uint32_t id, Phase phase)
    {
        if (device != 0x0 && device->m_Connected)
        {
            TouchDevicePacket& packet = device->m_Packet;
            if (packet.m_TouchCount < MAX_TOUCH_COUNT)
            {
                Touch& t = packet.m_Touches[packet.m_TouchCount++];
                t.m_X = x;
                t.m_Y = y;
                t.m_Id = id;
                t.m_Phase = phase;
            }
        }
    }

    // Test only
    void ClearTouches(HTouchDevice device)
    {
        if (device != 0x0 && device->m_Connected)
        {
            device->m_Packet.m_TouchCount = 0;
        }
    }

    int GetKeyValue(Key key)
    {
        int key_index = (int) key + dmPlatform::PLATFORM_KEY_START;

        if (key_index >= HID_SPECIAL_START)
        {
            static const int translation_table_special[] = {
                dmPlatform::PLATFORM_KEY_ESC,
                dmPlatform::PLATFORM_KEY_F1,
                dmPlatform::PLATFORM_KEY_F2,
                dmPlatform::PLATFORM_KEY_F3,
                dmPlatform::PLATFORM_KEY_F4,
                dmPlatform::PLATFORM_KEY_F5,
                dmPlatform::PLATFORM_KEY_F6,
                dmPlatform::PLATFORM_KEY_F7,
                dmPlatform::PLATFORM_KEY_F8,
                dmPlatform::PLATFORM_KEY_F9,
                dmPlatform::PLATFORM_KEY_F10,
                dmPlatform::PLATFORM_KEY_F11,
                dmPlatform::PLATFORM_KEY_F12,
                dmPlatform::PLATFORM_KEY_UP,
                dmPlatform::PLATFORM_KEY_DOWN,
                dmPlatform::PLATFORM_KEY_LEFT,
                dmPlatform::PLATFORM_KEY_RIGHT,
                dmPlatform::PLATFORM_KEY_LSHIFT,
                dmPlatform::PLATFORM_KEY_RSHIFT,
                dmPlatform::PLATFORM_KEY_LCTRL,
                dmPlatform::PLATFORM_KEY_RCTRL,
                dmPlatform::PLATFORM_KEY_LALT,
                dmPlatform::PLATFORM_KEY_RALT,
                dmPlatform::PLATFORM_KEY_TAB,
                dmPlatform::PLATFORM_KEY_ENTER,
                dmPlatform::PLATFORM_KEY_BACKSPACE,
                dmPlatform::PLATFORM_KEY_INSERT,
                dmPlatform::PLATFORM_KEY_DEL,
                dmPlatform::PLATFORM_KEY_PAGEUP,
                dmPlatform::PLATFORM_KEY_PAGEDOWN,
                dmPlatform::PLATFORM_KEY_HOME,
                dmPlatform::PLATFORM_KEY_END,
                dmPlatform::PLATFORM_KEY_KP_0,
                dmPlatform::PLATFORM_KEY_KP_1,
                dmPlatform::PLATFORM_KEY_KP_2,
                dmPlatform::PLATFORM_KEY_KP_3,
                dmPlatform::PLATFORM_KEY_KP_4,
                dmPlatform::PLATFORM_KEY_KP_5,
                dmPlatform::PLATFORM_KEY_KP_6,
                dmPlatform::PLATFORM_KEY_KP_7,
                dmPlatform::PLATFORM_KEY_KP_8,
                dmPlatform::PLATFORM_KEY_KP_9,
                dmPlatform::PLATFORM_KEY_KP_DIVIDE,
                dmPlatform::PLATFORM_KEY_KP_MULTIPLY,
                dmPlatform::PLATFORM_KEY_KP_SUBTRACT,
                dmPlatform::PLATFORM_KEY_KP_ADD,
                dmPlatform::PLATFORM_KEY_KP_DECIMAL,
                dmPlatform::PLATFORM_KEY_KP_EQUAL,
                dmPlatform::PLATFORM_KEY_KP_ENTER,
                dmPlatform::PLATFORM_KEY_KP_NUM_LOCK,
                dmPlatform::PLATFORM_KEY_CAPS_LOCK,
                dmPlatform::PLATFORM_KEY_SCROLL_LOCK,
                dmPlatform::PLATFORM_KEY_PAUSE,
                dmPlatform::PLATFORM_KEY_LSUPER,
                dmPlatform::PLATFORM_KEY_RSUPER,
                dmPlatform::PLATFORM_KEY_MENU,
                dmPlatform::PLATFORM_KEY_BACK,
            };

            key_index = key_index - HID_SPECIAL_START;
            if (key_index >= DM_ARRAY_SIZE(translation_table_special))
            {
                return -1;
            }
            return translation_table_special[key_index];
        }
        return key_index;
    }

    int GetMouseButtonValue(MouseButton button)
    {
        static int translation_table[] = {
            dmPlatform::PLATFORM_MOUSE_BUTTON_LEFT,
            dmPlatform::PLATFORM_MOUSE_BUTTON_MIDDLE,
            dmPlatform::PLATFORM_MOUSE_BUTTON_RIGHT,
            dmPlatform::PLATFORM_MOUSE_BUTTON_1,
            dmPlatform::PLATFORM_MOUSE_BUTTON_2,
            dmPlatform::PLATFORM_MOUSE_BUTTON_3,
            dmPlatform::PLATFORM_MOUSE_BUTTON_4,
            dmPlatform::PLATFORM_MOUSE_BUTTON_5,
            dmPlatform::PLATFORM_MOUSE_BUTTON_6,
            dmPlatform::PLATFORM_MOUSE_BUTTON_7,
            dmPlatform::PLATFORM_MOUSE_BUTTON_8,
        };
        assert((int) button < DM_ARRAY_SIZE(translation_table));
        return translation_table[(int) button];
    }

    void ShowKeyboard(HContext context, KeyboardType type, bool autoclose)
    {
        dmPlatform::DeviceState device_state;

        switch (type)
        {
            case KEYBOARD_TYPE_DEFAULT:
                device_state = dmPlatform::DEVICE_STATE_KEYBOARD_DEFAULT;
                break;
            case KEYBOARD_TYPE_NUMBER_PAD:
                device_state = dmPlatform::DEVICE_STATE_KEYBOARD_NUMBER_PAD;
                break;
            case KEYBOARD_TYPE_EMAIL:
                device_state = dmPlatform::DEVICE_STATE_KEYBOARD_EMAIL;
                break;
            case KEYBOARD_TYPE_PASSWORD:
                device_state = dmPlatform::DEVICE_STATE_KEYBOARD_PASSWORD;
                break;
            default:
                dmLogWarning("Unknown keyboard type %d\n", type);
        }

        dmPlatform::SetDeviceState(context->m_Window, device_state, true, autoclose);
    }

    void HideKeyboard(HContext context)
    {
        dmPlatform::SetDeviceState(context->m_Window, dmPlatform::DEVICE_STATE_KEYBOARD_DEFAULT, false);
    }

    void EnableAccelerometer(HContext context)
    {
        dmPlatform::SetDeviceState(context->m_Window, dmPlatform::DEVICE_STATE_ACCELEROMETER, true);
    }

    void ShowMouseCursor(HContext context)
    {
        dmPlatform::SetDeviceState(context->m_Window, dmPlatform::DEVICE_STATE_CURSOR, true);
    }

    void HideMouseCursor(HContext context)
    {
        dmPlatform::SetDeviceState(context->m_Window, dmPlatform::DEVICE_STATE_CURSOR, false);
    }

    bool GetCursorVisible(HContext context)
    {
        return !dmPlatform::GetDeviceState(context->m_Window, dmPlatform::DEVICE_STATE_CURSOR_LOCK);
    }

    dmhash_t CalcStateHash(HContext context)
    {
        HashState64 state;
        dmHashInit64(&state, false);
        dmHashUpdateBuffer64(&state, &context->m_Keyboards[0], MAX_KEYBOARD_COUNT * sizeof(Keyboard));
        dmHashUpdateBuffer64(&state, &context->m_Mice[0], MAX_MOUSE_COUNT * sizeof(Mouse));
        dmHashUpdateBuffer64(&state, &context->m_Gamepads[0], MAX_GAMEPAD_COUNT * sizeof(Gamepad));
        dmHashUpdateBuffer64(&state, &context->m_TouchDevices[0], MAX_TOUCH_DEVICE_COUNT * sizeof(TouchDevice));
        dmHashUpdateBuffer64(&state, &context->m_TextPacket, sizeof(TextPacket));
        dmHashUpdateBuffer64(&state, &context->m_AccelerationPacket, sizeof(context->m_AccelerationPacket));
        return dmHashFinal64(&state);
    }
}
