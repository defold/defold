// Copyright 2020-2023 The Defold Foundation
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
#include <dlib/log.h>
#include "hid.h"
#include "hid_private.h"

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/utf8.h>

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

    static int GetKeyValue(Key key)
    {
        static const int translation_table[] = {
            HID_KEY_SPACE,
            HID_KEY_EXCLAIM,
            HID_KEY_QUOTEDBL,
            HID_KEY_HASH,
            HID_KEY_DOLLAR,
            HID_KEY_AMPERSAND,
            HID_KEY_QUOTE,
            HID_KEY_LPAREN,
            HID_KEY_RPAREN,
            HID_KEY_ASTERISK,
            HID_KEY_PLUS,
            HID_KEY_COMMA,
            HID_KEY_MINUS,
            HID_KEY_PERIOD,
            HID_KEY_SLASH,
            HID_KEY_0,
            HID_KEY_1,
            HID_KEY_2,
            HID_KEY_3,
            HID_KEY_4,
            HID_KEY_5,
            HID_KEY_6,
            HID_KEY_7,
            HID_KEY_8,
            HID_KEY_9,
            HID_KEY_COLON,
            HID_KEY_SEMICOLON,
            HID_KEY_LESS,
            HID_KEY_EQUALS,
            HID_KEY_GREATER,
            HID_KEY_QUESTION,
            HID_KEY_AT,
            HID_KEY_A,
            HID_KEY_B,
            HID_KEY_C,
            HID_KEY_D,
            HID_KEY_E,
            HID_KEY_F,
            HID_KEY_G,
            HID_KEY_H,
            HID_KEY_I,
            HID_KEY_J,
            HID_KEY_K,
            HID_KEY_L,
            HID_KEY_M,
            HID_KEY_N,
            HID_KEY_O,
            HID_KEY_P,
            HID_KEY_Q,
            HID_KEY_R,
            HID_KEY_S,
            HID_KEY_T,
            HID_KEY_U,
            HID_KEY_V,
            HID_KEY_W,
            HID_KEY_X,
            HID_KEY_Y,
            HID_KEY_Z,
            HID_KEY_LBRACKET,
            HID_KEY_BACKSLASH,
            HID_KEY_RBRACKET,
            HID_KEY_CARET,
            HID_KEY_UNDERSCORE,
            HID_KEY_BACKQUOTE,
            HID_KEY_LBRACE,
            HID_KEY_PIPE,
            HID_KEY_RBRACE,
            HID_KEY_TILDE,
            HID_KEY_ESC,
            HID_KEY_F1,
            HID_KEY_F2,
            HID_KEY_F3,
            HID_KEY_F4,
            HID_KEY_F5,
            HID_KEY_F6,
            HID_KEY_F7,
            HID_KEY_F8,
            HID_KEY_F9,
            HID_KEY_F10,
            HID_KEY_F11,
            HID_KEY_F12,
            HID_KEY_UP,
            HID_KEY_DOWN,
            HID_KEY_LEFT,
            HID_KEY_RIGHT,
            HID_KEY_LSHIFT,
            HID_KEY_RSHIFT,
            HID_KEY_LCTRL,
            HID_KEY_RCTRL,
            HID_KEY_LALT,
            HID_KEY_RALT,
            HID_KEY_TAB,
            HID_KEY_ENTER,
            HID_KEY_BACKSPACE,
            HID_KEY_INSERT,
            HID_KEY_DEL,
            HID_KEY_PAGEUP,
            HID_KEY_PAGEDOWN,
            HID_KEY_HOME,
            HID_KEY_END,
            HID_KEY_KP_0,
            HID_KEY_KP_1,
            HID_KEY_KP_2,
            HID_KEY_KP_3,
            HID_KEY_KP_4,
            HID_KEY_KP_5,
            HID_KEY_KP_6,
            HID_KEY_KP_7,
            HID_KEY_KP_8,
            HID_KEY_KP_9,
            HID_KEY_KP_DIVIDE,
            HID_KEY_KP_MULTIPLY,
            HID_KEY_KP_SUBTRACT,
            HID_KEY_KP_ADD,
            HID_KEY_KP_DECIMAL,
            HID_KEY_KP_EQUAL,
            HID_KEY_KP_ENTER,
            HID_KEY_KP_NUM_LOCK,
            HID_KEY_CAPS_LOCK,
            HID_KEY_SCROLL_LOCK,
            HID_KEY_PAUSE,
            HID_KEY_LSUPER,
            HID_KEY_RSUPER,
            HID_KEY_MENU,
            HID_KEY_BACK,
        };

        assert((int) key < DM_ARRAY_SIZE(translation_table));
        return translation_table[(int) key];
    }

    bool GetKey(KeyboardPacket* packet, Key key)
    {
        int key_value = GetKeyValue(key);

        if (packet != 0x0)
            return packet->m_Keys[key_value / 32] & (1 << (key_value % 32));
        else
            return false;
    }

    void SetKey(HKeyboard keyboard, Key key, bool value)
    {
        int key_value = GetKeyValue(key);

        if (keyboard != 0x0)
        {
            if (value)
                keyboard->m_Packet.m_Keys[key_value / 32] |= (1 << (key_value % 32));
            else
                keyboard->m_Packet.m_Keys[key_value / 32] &= ~(1 << (key_value % 32));
        }
    }

    static int GetMouseButtonValue(MouseButton button)
    {
        static int translation_table[] = {
            HID_MOUSE_BUTTON_LEFT,
            HID_MOUSE_BUTTON_MIDDLE,
            HID_MOUSE_BUTTON_RIGHT,
            HID_MOUSE_BUTTON_1,
            HID_MOUSE_BUTTON_2,
            HID_MOUSE_BUTTON_3,
            HID_MOUSE_BUTTON_4,
            HID_MOUSE_BUTTON_5,
            HID_MOUSE_BUTTON_6,
            HID_MOUSE_BUTTON_7,
            HID_MOUSE_BUTTON_8,
        };
        assert((int) button < DM_ARRAY_SIZE(translation_table));
        return translation_table[(int) button];
    }

    bool GetMouseButton(MousePacket* packet, MouseButton button)
    {
        int mouse_button_value = GetMouseButtonValue(button);

        if (packet != 0x0)
            return packet->m_Buttons[mouse_button_value / 32] & (1 << (mouse_button_value % 32));
        else
            return false;
    }

    void SetMouseButton(HMouse mouse, MouseButton button, bool value)
    {
        int mouse_button_value = GetMouseButtonValue(button);

        if (mouse != 0x0)
        {
            if (value)
                mouse->m_Packet.m_Buttons[mouse_button_value / 32] |= (1 << (mouse_button_value % 32));
            else
                mouse->m_Packet.m_Buttons[mouse_button_value / 32] &= ~(1 << (mouse_button_value % 32));
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

    void ClearTouches(HTouchDevice device)
    {
        if (device != 0x0 && device->m_Connected)
        {
            device->m_Packet.m_TouchCount = 0;
        }
    }
}
