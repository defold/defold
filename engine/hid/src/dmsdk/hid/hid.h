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

#ifndef DMSDK_HID_H
#define DMSDK_HID_H

#include <stdint.h>
#include <dmsdk/hid/hid_native_defines.h>

/*# SDK Hid API documentation
 * Used to add input to the engine
 *
 * @document
 * @name Hid
 * @namespace dmHid
 * @path engine/hid/src/dmsdk/hid/hid.h
 */

namespace dmHID
{
    /*# HID context handle
     * @typedef
     * @name dmHID::HContext
     */
    typedef struct Context* HContext;
    /*# gamepad context handle
     * @typedef
     * @name dmHID::HGamepad
     */
    typedef struct Gamepad* HGamepad;
    /*# mouse context handle
     * @typedef
     * @name dmHID::HMouse
     */
    typedef struct Mouse* HMouse;
    /*# keyboard context handle
     * @typedef
     * @name dmHID::HKeyboard
     */
    typedef struct Keyboard* HKeyboard;
    /*# touch device context handle
     * @typedef
     * @name dmHID::HTouchDevice
     */
    typedef struct TouchDevice* HTouchDevice;

    /*# invalid gamepad handle
     * @constant
     * @name dmHID::INVALID_GAMEPAD_HANDLE [type: dmHID::HGamepad]
     */
    const HGamepad INVALID_GAMEPAD_HANDLE = 0;

    /*# invalid keyboard handle
     * @constant
     * @name dmHID::INVALID_KEYBOARD_HANDLE [type: dmHID::HKeyboard]
     */
    const HKeyboard INVALID_KEYBOARD_HANDLE = 0;

    /*# invalid mouse handle
     * @constant
     * @name dmHID::INVALID_MOUSE_HANDLE [type: dmHID::HMouse]
     */
    const HMouse INVALID_MOUSE_HANDLE = 0;

    /*# invalid touch devicehandle
     * @constant
     * @name dmHID::INVALID_TOUCHDEVICE_HANDLE [type: dmHID::HTouchDevice]
     */
    const HTouchDevice INVALID_TOUCH_DEVICE_HANDLE = 0;

    /*# Maximum number of gamepads supported
     * @constant
     * @name dmHID::MAX_GAMEPAD_COUNT [type: uint32_t]
     */
    const static uint32_t MAX_GAMEPAD_COUNT = HID_NATIVE_MAX_GAMEPAD_COUNT;
    /*# Maximum number of gamepad axis supported
     * @constant
     * @name dmHID::MAX_GAMEPAD_AXIS_COUNT [type: uint32_t]
     */
    const static uint32_t MAX_GAMEPAD_AXIS_COUNT = HID_NATIVE_MAX_GAMEPAD_AXIS_COUNT;
    /*# Maximum number of gamepad buttons supported
     * @constant
     * @name dmHID::MAX_GAMEPAD_BUTTON_COUNT [type: uint32_t]
     */
    const static uint32_t MAX_GAMEPAD_BUTTON_COUNT = HID_NATIVE_MAX_GAMEPAD_BUTTON_COUNT;
    /*# Maximum number of gamepad hats supported
     * @constant
     * @name dmHID::MAX_GAMEPAD_HAT_COUNT [type: uint32_t]
     */
    const static uint32_t MAX_GAMEPAD_HAT_COUNT = HID_NATIVE_MAX_GAMEPAD_HAT_COUNT;
    /*# Maximum number of simultaneous touches supported
     * @constant
     * @name dmHID::MAX_TOUCH_COUNT [type: uint32_t]
     */
    const static uint32_t MAX_TOUCH_COUNT = HID_NATIVE_MAX_TOUCH_COUNT;
    /*# Maximum number of keyboards supported
     * @constant
     * @name dmHID::MAX_KEYBOARD_COUNT [type: uint32_t]
     */
    const static uint32_t MAX_KEYBOARD_COUNT = HID_NATIVE_MAX_KEYBOARD_COUNT;
    /*# Maximum number of mice supported
     * @constant
     * @name dmHID::MAX_MOUSE_COUNT [type: uint32_t]
     */
    const static uint32_t MAX_MOUSE_COUNT = HID_NATIVE_MAX_MOUSE_COUNT;
    /*# Maximum number of touch devices supported
     * @constant
     * @name dmHID::MAX_TOUCH_DEVICE_COUNT [type: uint32_t]
     */
    const static uint32_t MAX_TOUCH_DEVICE_COUNT = HID_NATIVE_MAX_TOUCH_DEVICE_COUNT;
    /*# max number of characters
     * @constant
     * @name dmHID::MAX_CHAR_COUNT
     */
    const static uint32_t MAX_CHAR_COUNT = 256;

    /*# touch phase enumeration
     * @note By convention the enumeration corresponds to the iOS values
     * @enum
     * @name Phase
     * @member dmHID::PHASE_BEGAN
     * @member dmHID::PHASE_MOVED
     * @member dmHID::PHASE_STATIONARY
     * @member dmHID::PHASE_ENDED
     * @member dmHID::PHASE_CANCELLED
     */
    enum Phase
    {
        PHASE_BEGAN,
        PHASE_MOVED,
        PHASE_STATIONARY,
        PHASE_ENDED,
        PHASE_CANCELLED,
    };

    /*# keyboard key enumeration
    * @enum
    * @name Key
    * @member dmHID::KEY_SPACE
    * @member dmHID::KEY_EXCLAIM
    * @member dmHID::KEY_QUOTEDBL
    * @member dmHID::KEY_HASH
    * @member dmHID::KEY_DOLLAR
    * @member dmHID::KEY_AMPERSAND
    * @member dmHID::KEY_QUOTE
    * @member dmHID::KEY_LPAREN
    * @member dmHID::KEY_RPAREN
    * @member dmHID::KEY_ASTERISK
    * @member dmHID::KEY_PLUS
    * @member dmHID::KEY_COMMA
    * @member dmHID::KEY_MINUS
    * @member dmHID::KEY_PERIOD
    * @member dmHID::KEY_SLASH
    * @member dmHID::KEY_0
    * @member dmHID::KEY_1
    * @member dmHID::KEY_2
    * @member dmHID::KEY_3
    * @member dmHID::KEY_4
    * @member dmHID::KEY_5
    * @member dmHID::KEY_6
    * @member dmHID::KEY_7
    * @member dmHID::KEY_8
    * @member dmHID::KEY_9
    * @member dmHID::KEY_COLON
    * @member dmHID::KEY_SEMICOLON
    * @member dmHID::KEY_LESS
    * @member dmHID::KEY_EQUALS
    * @member dmHID::KEY_GREATER
    * @member dmHID::KEY_QUESTION
    * @member dmHID::KEY_AT
    * @member dmHID::KEY_A
    * @member dmHID::KEY_B
    * @member dmHID::KEY_C
    * @member dmHID::KEY_D
    * @member dmHID::KEY_E
    * @member dmHID::KEY_F
    * @member dmHID::KEY_G
    * @member dmHID::KEY_H
    * @member dmHID::KEY_I
    * @member dmHID::KEY_J
    * @member dmHID::KEY_K
    * @member dmHID::KEY_L
    * @member dmHID::KEY_M
    * @member dmHID::KEY_N
    * @member dmHID::KEY_O
    * @member dmHID::KEY_P
    * @member dmHID::KEY_Q
    * @member dmHID::KEY_R
    * @member dmHID::KEY_S
    * @member dmHID::KEY_T
    * @member dmHID::KEY_U
    * @member dmHID::KEY_V
    * @member dmHID::KEY_W
    * @member dmHID::KEY_X
    * @member dmHID::KEY_Y
    * @member dmHID::KEY_Z
    * @member dmHID::KEY_LBRACKET
    * @member dmHID::KEY_BACKSLASH
    * @member dmHID::KEY_RBRACKET
    * @member dmHID::KEY_CARET
    * @member dmHID::KEY_UNDERSCORE
    * @member dmHID::KEY_BACKQUOTE
    * @member dmHID::KEY_LBRACE
    * @member dmHID::KEY_PIPE
    * @member dmHID::KEY_RBRACE
    * @member dmHID::KEY_TILDE
    * @member dmHID::KEY_ESC
    * @member dmHID::KEY_F1
    * @member dmHID::KEY_F2
    * @member dmHID::KEY_F3
    * @member dmHID::KEY_F4
    * @member dmHID::KEY_F5
    * @member dmHID::KEY_F6
    * @member dmHID::KEY_F7
    * @member dmHID::KEY_F8
    * @member dmHID::KEY_F9
    * @member dmHID::KEY_F10
    * @member dmHID::KEY_F11
    * @member dmHID::KEY_F12
    * @member dmHID::KEY_UP
    * @member dmHID::KEY_DOWN
    * @member dmHID::KEY_LEFT
    * @member dmHID::KEY_RIGHT
    * @member dmHID::KEY_LSHIFT
    * @member dmHID::KEY_RSHIFT
    * @member dmHID::KEY_LCTRL
    * @member dmHID::KEY_RCTRL
    * @member dmHID::KEY_LALT
    * @member dmHID::KEY_RALT
    * @member dmHID::KEY_TAB
    * @member dmHID::KEY_ENTER
    * @member dmHID::KEY_BACKSPACE
    * @member dmHID::KEY_INSERT
    * @member dmHID::KEY_DEL
    * @member dmHID::KEY_PAGEUP
    * @member dmHID::KEY_PAGEDOWN
    * @member dmHID::KEY_HOME
    * @member dmHID::KEY_END
    * @member dmHID::KEY_KP_0
    * @member dmHID::KEY_KP_1
    * @member dmHID::KEY_KP_2
    * @member dmHID::KEY_KP_3
    * @member dmHID::KEY_KP_4
    * @member dmHID::KEY_KP_5
    * @member dmHID::KEY_KP_6
    * @member dmHID::KEY_KP_7
    * @member dmHID::KEY_KP_8
    * @member dmHID::KEY_KP_9
    * @member dmHID::KEY_KP_DIVIDE
    * @member dmHID::KEY_KP_MULTIPLY
    * @member dmHID::KEY_KP_SUBTRACT
    * @member dmHID::KEY_KP_ADD
    * @member dmHID::KEY_KP_DECIMAL
    * @member dmHID::KEY_KP_EQUAL
    * @member dmHID::KEY_KP_ENTER
    * @member dmHID::KEY_KP_NUM_LOCK
    * @member dmHID::KEY_CAPS_LOCK
    * @member dmHID::KEY_SCROLL_LOCK
    * @member dmHID::KEY_PAUSE
    * @member dmHID::KEY_LSUPER
    * @member dmHID::KEY_RSUPER
    * @member dmHID::KEY_MENU
    * @member dmHID::KEY_BACK
    * @member dmHID::MAX_KEY_COUNT
    */
    enum Key
    {
        KEY_SPACE     = 0,
        KEY_EXCLAIM   = 1,
        KEY_QUOTEDBL  = 2,
        KEY_HASH      = 3,
        KEY_DOLLAR    = 4,
        KEY_AMPERSAND = 5,
        KEY_QUOTE     = 6,
        KEY_LPAREN    = 7,
        KEY_RPAREN    = 8,
        KEY_ASTERISK  = 9,
        KEY_PLUS      = 10,
        KEY_COMMA     = 11,
        KEY_MINUS     = 12,
        KEY_PERIOD    = 13,
        KEY_SLASH     = 14,

        KEY_0         = 15,
        KEY_1         = 16,
        KEY_2         = 17,
        KEY_3         = 18,
        KEY_4         = 19,
        KEY_5         = 20,
        KEY_6         = 21,
        KEY_7         = 22,
        KEY_8         = 23,
        KEY_9         = 24,

        KEY_COLON     = 25,
        KEY_SEMICOLON = 26,
        KEY_LESS      = 27,
        KEY_EQUALS    = 28,
        KEY_GREATER   = 29,
        KEY_QUESTION  = 30,
        KEY_AT        = 31,

        KEY_A = 32,
        KEY_B = 33,
        KEY_C = 34,
        KEY_D = 35,
        KEY_E = 36,
        KEY_F = 37,
        KEY_G = 38,
        KEY_H = 39,
        KEY_I = 40,
        KEY_J = 41,
        KEY_K = 42,
        KEY_L = 43,
        KEY_M = 44,
        KEY_N = 45,
        KEY_O = 46,
        KEY_P = 47,
        KEY_Q = 48,
        KEY_R = 49,
        KEY_S = 50,
        KEY_T = 51,
        KEY_U = 52,
        KEY_V = 53,
        KEY_W = 54,
        KEY_X = 55,
        KEY_Y = 56,
        KEY_Z = 57,

        KEY_LBRACKET   = 58,
        KEY_BACKSLASH  = 59,
        KEY_RBRACKET   = 60,
        KEY_CARET      = 61,
        KEY_UNDERSCORE = 62,
        KEY_BACKQUOTE  = 63,

        KEY_LBRACE     = 64,
        KEY_PIPE       = 65,
        KEY_RBRACE     = 66,
        KEY_TILDE      = 67,
        // End of ASCII mapped keysyms

        // Special keys
        KEY_ESC         = 68,
        KEY_F1          = 69,
        KEY_F2          = 70,
        KEY_F3          = 71,
        KEY_F4          = 72,
        KEY_F5          = 73,
        KEY_F6          = 74,
        KEY_F7          = 75,
        KEY_F8          = 76,
        KEY_F9          = 77,
        KEY_F10         = 78,
        KEY_F11         = 79,
        KEY_F12         = 80,
        KEY_UP          = 81,
        KEY_DOWN        = 82,
        KEY_LEFT        = 83,
        KEY_RIGHT       = 84,
        KEY_LSHIFT      = 85,
        KEY_RSHIFT      = 86,
        KEY_LCTRL       = 87,
        KEY_RCTRL       = 88,
        KEY_LALT        = 89,
        KEY_RALT        = 90,
        KEY_TAB         = 91,
        KEY_ENTER       = 92,
        KEY_BACKSPACE   = 93,
        KEY_INSERT      = 94,
        KEY_DEL         = 95,
        KEY_PAGEUP      = 96,
        KEY_PAGEDOWN    = 97,
        KEY_HOME        = 98,
        KEY_END         = 99,
        KEY_KP_0        = 100,
        KEY_KP_1        = 101,
        KEY_KP_2        = 102,
        KEY_KP_3        = 103,
        KEY_KP_4        = 104,
        KEY_KP_5        = 105,
        KEY_KP_6        = 106,
        KEY_KP_7        = 107,
        KEY_KP_8        = 108,
        KEY_KP_9        = 109,
        KEY_KP_DIVIDE   = 110,
        KEY_KP_MULTIPLY = 111,
        KEY_KP_SUBTRACT = 112,
        KEY_KP_ADD      = 113,
        KEY_KP_DECIMAL  = 114,
        KEY_KP_EQUAL    = 115,
        KEY_KP_ENTER    = 116,
        KEY_KP_NUM_LOCK = 117,
        KEY_CAPS_LOCK   = 118,
        KEY_SCROLL_LOCK = 119,
        KEY_PAUSE       = 120,
        KEY_LSUPER      = 121,
        KEY_RSUPER      = 122,
        KEY_MENU        = 123,
        KEY_BACK        = 124,

        MAX_KEY_COUNT = KEY_BACK + 1,
    };

    /*# mouse button enumeration
     * @enum
     * @name MouseButton
     * @member dmHID::MOUSE_BUTTON_LEFT
     * @member dmHID::MOUSE_BUTTON_MIDDLE
     * @member dmHID::MOUSE_BUTTON_RIGHT
     * @member dmHID::MOUSE_BUTTON_1
     * @member dmHID::MOUSE_BUTTON_2
     * @member dmHID::MOUSE_BUTTON_3
     * @member dmHID::MOUSE_BUTTON_4
     * @member dmHID::MOUSE_BUTTON_5
     * @member dmHID::MOUSE_BUTTON_6
     * @member dmHID::MOUSE_BUTTON_7
     * @member dmHID::MOUSE_BUTTON_8
     * @member dmHID::MAX_MOUSE_BUTTON_COUNT
     */
    enum MouseButton
    {
        MOUSE_BUTTON_LEFT      = 0,
        MOUSE_BUTTON_MIDDLE    = 1,
        MOUSE_BUTTON_RIGHT     = 2,
        MOUSE_BUTTON_1         = 3,
        MOUSE_BUTTON_2         = 4,
        MOUSE_BUTTON_3         = 5,
        MOUSE_BUTTON_4         = 6,
        MOUSE_BUTTON_5         = 7,
        MOUSE_BUTTON_6         = 8,
        MOUSE_BUTTON_7         = 9,
        MOUSE_BUTTON_8         = 10,
        MAX_MOUSE_BUTTON_COUNT = MOUSE_BUTTON_8 + 1
    };

    /*# Contains the current state of a keyboard
     * @struct
     * @name KeyboardPacket
     * @note implementation is internal, use the proper accessor functions
     */
    struct KeyboardPacket
    {
        uint32_t m_Keys[MAX_KEY_COUNT / 32 + 1];
    };

    /*# Contains the current state of a mouse
     * @struct
     * @name MousePacket
     * @note implementation is internal, use the proper accessor functions
     */
    struct MousePacket
    {
        int32_t m_PositionX, m_PositionY;
        int32_t m_Wheel;
        uint32_t m_Buttons[MAX_MOUSE_BUTTON_COUNT / 32 + 1];
    };

    /*# Contains the current state of a gamepad
     * @struct
     * @name GamepadPacket
     * @note implementation is internal, use the proper accessor functions
     */
    struct GamepadPacket
    {
        float m_Axis[MAX_GAMEPAD_AXIS_COUNT];
        uint32_t m_Buttons[MAX_GAMEPAD_BUTTON_COUNT / 32 + 1];
        uint8_t m_Hat[MAX_GAMEPAD_HAT_COUNT];
        uint8_t m_GamepadDisconnected:1;
        uint8_t m_GamepadConnected:1;
        uint8_t :6;
    };

    /*#
     * Data for a single touch, e.g. finger
     * @struct
     * @name Touch
     * @member m_TapCount [type: int32_t] Single-click, double, etc
     * @member m_Phase [type: Phase] Begin, end, etc
     * @member m_X [type: int32_t] Current x
     * @member m_Y [type: int32_t] Current y
     * @member m_ScreenX [type: int32_t] Current x, in screen space
     * @member m_ScreenY [type: int32_t] Current y, in screen space
     * @member m_DX [type: int32_t] Current dx
     * @member m_DY [type: int32_t] Current dy
     * @member m_ScreenDX [type: int32_t] Current dx, in screen space
     * @member m_ScreenDY [type: int32_t] Current dy, in screen space
     * @member m_Id [type: int32_t] Touch id
     */
    struct Touch
    {
        /// Single-click, double, etc
        int32_t m_TapCount;
        /// Begin, end, etc
        Phase   m_Phase;
        /// Current x
        int32_t m_X;
        /// Current y
        int32_t m_Y;
        /// Current x, in screen space
        int32_t m_ScreenX;
        /// Current y, in screen space
        int32_t m_ScreenY;
        /// Current dx
        int32_t m_DX;
        /// Current dy
        int32_t m_DY;
        /// Current dx, in screen space
        int32_t m_ScreenDX;
        /// Current dy, in screen space
        int32_t m_ScreenDY;
        /// Touch id
        int32_t m_Id;
    };

    /*# gets a keyboard handle
     * @name GetKeyboard
     * @param context [type: dmHID::HContext] context in which to find the gamepad
     * @param index [type: uint8_t] device index
     * @return keyboard [type: dmHID::HKeyboard] Handle to keyboard. dmHID::INVALID_KEYBOARD_HANDLE if not available
     */
    HKeyboard GetKeyboard(HContext context, uint8_t index);

    /*# gets a mouse handle
     * @name GetMouse
     * @param context [type: dmHID::HContext] context in which to find the gamepad
     * @param index [type: uint8_t] device index
     * @return mouse [type: dmHID::HMouse] Handle to mouse. dmHID::INVALID_MOUSE_HANDLE if not available
     */
    HMouse GetMouse(HContext context, uint8_t index);

    /*# gets a touch device handle
     * @name GetTouchDevice
     * @param context [type: dmHID::HContext] context in which to find the gamepad
     * @param index [type: uint8_t] device index
     * @return device [type: dmHID::HTouchDevice] Handle to touch device. dmHID::INVALID_TOUCH_DEVICE_HANDLE if not available
     */
    HTouchDevice GetTouchDevice(HContext context, uint8_t index);

    /*# gets a gamepad device handle
     *
     * @name GetGamePad
     * @param context [type: dmHID::HContext] context in which to find the gamepad
     * @param index [type: uint8_t] device index
     * @return gamepad [type: dmHID::HGamepad] Handle to gamepad. dmHID::INVALID_GAMEPAD_HANDLE if not available
     */
    HGamepad GetGamepad(HContext context, uint8_t index);

    /*# gets a gamepad device handle
     *
     * @name GetGamePad
     * @param gamepad [type: dmHID::HGamepad] Handle to gamepad
     * @param out [type: void**] Platform specific user id data
     * @return result [type: boolean] true if gamepad has a user id data assigned to it
     */
    bool GetGamepadUserId(HContext context, HGamepad gamepad, uint32_t* out);

    /*# Adds a touch event touch.
     * @name AddTouch
     * @param device [type: dmHID::HTouchDevice] device handle
     * @param x [type: int32_t] x-coordinate of the position
     * @param y [type: int32_t] y-coordinate of the position
     * @param id [type: uint32_t] identifier of touch
     * @param phase [type: dmHID::Phase] phase of touch
     */
    void AddTouch(HTouchDevice device, int32_t x, int32_t y, uint32_t id, Phase phase);

    /*# Sets the state of a gamepad button.
     * @name SetGamepadButton
     * @param gamepad [type: dmHID::HGamepad] device handle
     * @param button [type: uint32_t] The requested button [0, dmHID::MAX_GAMEPAD_BUTTON_COUNT)
     * @param value [type: bool] Button state
     */
    void SetGamepadButton(HGamepad gamepad, uint32_t button, bool value);

    /*# Sets the state of a gamepad axis.
     * @name SetGamepadAxis
     * @param gamepad [type: dmHID::HGamepad] device handle
     * @param axis [type: uint32_t] The requested axis [0, dmHID::MAX_GAMEPAD_AXIS_COUNT)
     * @param value [type: float] axis value [-1, 1]
     */
    void SetGamepadAxis(HGamepad gamepad, uint32_t axis, float value);

    /*# Sets the state of a mouse button.
     * @name SetMouseButton
     * @param mouse [type: dmHID::HMouse] device handle
     * @param button [type: dmHID::MouseButton] The requested button
     * @param value [type: bool] Button state
     */
    void SetMouseButton(HMouse mouse, MouseButton button, bool value);

    /*# Sets the position of a mouse.
     * @name SetMousePosition
     * @param mouse [type: dmHID::HMouse] device handle
     * @param x [type: int32_t] x-coordinate of the position
     * @param y [type: int32_t] y-coordinate of the position
     */
    void SetMousePosition(HMouse mouse, int32_t x, int32_t y);

    /*# Sets the mouse wheel.
     * @name SetMouseWheel
     * @param mouse [type: dmHID::HMouse] device handle
     * @param value [type: int32_t] wheel value
     */
    void SetMouseWheel(HMouse mouse, int32_t value);

    /*#
     * Obtain a mouse packet reflecting the current input state of a HID context.
     *
     * @name GetMousePacket
     * @param mouse [type: dmHID::HMouse] context from which to retrieve the packet
     * @param out_packet [type: dmHID::MousePacket*] Mouse packet out argument
     * @return result [type: bool] If the packet was successfully updated or not.
     */
    bool GetMousePacket(HMouse mouse, MousePacket* out_packet);

    /*#
     * Convenience function to retrieve the state of a mouse button from a mouse packet.
     *
     * @name GetMouseButton
     * @param packet [type: dmHID::MousePacket*] Mouse packet
     * @param button [type: dmHID::MouseButton] The requested button
     * @return result [type: bool] If the button was pressed or not
     */
    bool GetMouseButton(MousePacket* packet, MouseButton button);

    /*# Sets the state of a key.
     * @name SetKey
     * @param keyboard [type: dmHID::HKeyboard] context handle
     * @param key [type: dmHID::Key] The requested key
     * @param value [type: bool] Key state
     */
    void SetKey(HKeyboard keyboard, Key key, bool value);

    /*# Add text input
     * @name AddKeyboardChar
     * @param keyboard [type: dmHID::HContext] context handle
     * @param chr [type: int] The character (unicode)
     */
    void AddKeyboardChar(HContext context, int chr);

    /**
     * Obtain a gamepad packet reflecting the current input state of the gamepad in a  HID context.
     *
     * @param gamepad gamepad handle
     * @param out_packet Gamepad packet out argument
     * @return True if the packet was successfully updated.
     */
    bool GetGamepadPacket(HGamepad gamepad, GamepadPacket* out_packet);

    /**
     * Convenience function to retrieve the state of a gamepad button from a gamepad packet.
     * @param packet Gamepad packet
     * @param button The requested button
     * @return True if the button is currently pressed down.
     */
    bool GetGamepadButton(GamepadPacket* packet, uint32_t button);

    /**
     * Convenience function to retrieve the state of a gamepad hat from a gamepad packet.
     * @param packet Gamepad packet
     * @param hat The requested hat index
     * @param out_hat_value Hat value out argument
     * @return True if the hat has data.
     */
    bool GetGamepadHat(GamepadPacket* packet, uint32_t hat, uint8_t* out_hat_value);
}

#endif // DMSDK_HID_H
