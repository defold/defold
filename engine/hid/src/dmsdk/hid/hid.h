// Copyright 2020-2022 The Defold Foundation
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
        KEY_SPACE = HID_KEY_SPACE,
        KEY_EXCLAIM = HID_KEY_EXCLAIM,
        KEY_QUOTEDBL = HID_KEY_QUOTEDBL,
        KEY_HASH = HID_KEY_HASH,
        KEY_DOLLAR = HID_KEY_DOLLAR,
        KEY_AMPERSAND = HID_KEY_AMPERSAND,
        KEY_QUOTE = HID_KEY_QUOTE,
        KEY_LPAREN = HID_KEY_LPAREN,
        KEY_RPAREN = HID_KEY_RPAREN,
        KEY_ASTERISK = HID_KEY_ASTERISK,
        KEY_PLUS = HID_KEY_PLUS,
        KEY_COMMA = HID_KEY_COMMA,
        KEY_MINUS = HID_KEY_MINUS,
        KEY_PERIOD = HID_KEY_PERIOD,
        KEY_SLASH = HID_KEY_SLASH,

        KEY_0 = HID_KEY_0,
        KEY_1 = HID_KEY_1,
        KEY_2 = HID_KEY_2,
        KEY_3 = HID_KEY_3,
        KEY_4 = HID_KEY_4,
        KEY_5 = HID_KEY_5,
        KEY_6 = HID_KEY_6,
        KEY_7 = HID_KEY_7,
        KEY_8 = HID_KEY_8,
        KEY_9 = HID_KEY_9,

        KEY_COLON = HID_KEY_COLON,
        KEY_SEMICOLON = HID_KEY_SEMICOLON,
        KEY_LESS = HID_KEY_LESS,
        KEY_EQUALS = HID_KEY_EQUALS,
        KEY_GREATER = HID_KEY_GREATER,
        KEY_QUESTION = HID_KEY_QUESTION,
        KEY_AT = HID_KEY_AT,

        KEY_A = HID_KEY_A,
        KEY_B = HID_KEY_B,
        KEY_C = HID_KEY_C,
        KEY_D = HID_KEY_D,
        KEY_E = HID_KEY_E,
        KEY_F = HID_KEY_F,
        KEY_G = HID_KEY_G,
        KEY_H = HID_KEY_H,
        KEY_I = HID_KEY_I,
        KEY_J = HID_KEY_J,
        KEY_K = HID_KEY_K,
        KEY_L = HID_KEY_L,
        KEY_M = HID_KEY_M,
        KEY_N = HID_KEY_N,
        KEY_O = HID_KEY_O,
        KEY_P = HID_KEY_P,
        KEY_Q = HID_KEY_Q,
        KEY_R = HID_KEY_R,
        KEY_S = HID_KEY_S,
        KEY_T = HID_KEY_T,
        KEY_U = HID_KEY_U,
        KEY_V = HID_KEY_V,
        KEY_W = HID_KEY_W,
        KEY_X = HID_KEY_X,
        KEY_Y = HID_KEY_Y,
        KEY_Z = HID_KEY_Z,

        KEY_LBRACKET = HID_KEY_LBRACKET,
        KEY_BACKSLASH = HID_KEY_BACKSLASH,
        KEY_RBRACKET = HID_KEY_RBRACKET,
        KEY_CARET = HID_KEY_CARET,
        KEY_UNDERSCORE = HID_KEY_UNDERSCORE,
        KEY_BACKQUOTE = HID_KEY_BACKQUOTE,

        KEY_LBRACE = HID_KEY_LBRACE,
        KEY_PIPE = HID_KEY_PIPE,
        KEY_RBRACE = HID_KEY_RBRACE,
        KEY_TILDE = HID_KEY_TILDE,
        // End of ASCII mapped keysyms

        // Special keys
        KEY_ESC = HID_KEY_ESC,
        KEY_F1 = HID_KEY_F1,
        KEY_F2 = HID_KEY_F2,
        KEY_F3 = HID_KEY_F3,
        KEY_F4 = HID_KEY_F4,
        KEY_F5 = HID_KEY_F5,
        KEY_F6 = HID_KEY_F6,
        KEY_F7 = HID_KEY_F7,
        KEY_F8 = HID_KEY_F8,
        KEY_F9 = HID_KEY_F9,
        KEY_F10 = HID_KEY_F10,
        KEY_F11 = HID_KEY_F11,
        KEY_F12 = HID_KEY_F12,
        KEY_UP = HID_KEY_UP,
        KEY_DOWN = HID_KEY_DOWN,
        KEY_LEFT = HID_KEY_LEFT,
        KEY_RIGHT = HID_KEY_RIGHT,
        KEY_LSHIFT = HID_KEY_LSHIFT,
        KEY_RSHIFT = HID_KEY_RSHIFT,
        KEY_LCTRL = HID_KEY_LCTRL,
        KEY_RCTRL = HID_KEY_RCTRL,
        KEY_LALT = HID_KEY_LALT,
        KEY_RALT = HID_KEY_RALT,
        KEY_TAB = HID_KEY_TAB,
        KEY_ENTER = HID_KEY_ENTER,
        KEY_BACKSPACE = HID_KEY_BACKSPACE,
        KEY_INSERT = HID_KEY_INSERT,
        KEY_DEL = HID_KEY_DEL,
        KEY_PAGEUP = HID_KEY_PAGEUP,
        KEY_PAGEDOWN = HID_KEY_PAGEDOWN,
        KEY_HOME = HID_KEY_HOME,
        KEY_END = HID_KEY_END,
        KEY_KP_0 = HID_KEY_KP_0,
        KEY_KP_1 = HID_KEY_KP_1,
        KEY_KP_2 = HID_KEY_KP_2,
        KEY_KP_3 = HID_KEY_KP_3,
        KEY_KP_4 = HID_KEY_KP_4,
        KEY_KP_5 = HID_KEY_KP_5,
        KEY_KP_6 = HID_KEY_KP_6,
        KEY_KP_7 = HID_KEY_KP_7,
        KEY_KP_8 = HID_KEY_KP_8,
        KEY_KP_9 = HID_KEY_KP_9,
        KEY_KP_DIVIDE = HID_KEY_KP_DIVIDE,
        KEY_KP_MULTIPLY = HID_KEY_KP_MULTIPLY,
        KEY_KP_SUBTRACT = HID_KEY_KP_SUBTRACT,
        KEY_KP_ADD = HID_KEY_KP_ADD,
        KEY_KP_DECIMAL = HID_KEY_KP_DECIMAL,
        KEY_KP_EQUAL = HID_KEY_KP_EQUAL,
        KEY_KP_ENTER = HID_KEY_KP_ENTER,
        KEY_KP_NUM_LOCK = HID_KEY_KP_NUM_LOCK,
        KEY_CAPS_LOCK = HID_KEY_CAPS_LOCK,
        KEY_SCROLL_LOCK = HID_KEY_SCROLL_LOCK,
        KEY_PAUSE = HID_KEY_PAUSE,
        KEY_LSUPER = HID_KEY_LSUPER,
        KEY_RSUPER = HID_KEY_RSUPER,
        KEY_MENU = HID_KEY_MENU,
        KEY_BACK = HID_KEY_BACK,

        MAX_KEY_COUNT = HID_KEY_BACK + 1
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
        MOUSE_BUTTON_LEFT = HID_MOUSE_BUTTON_LEFT,
        MOUSE_BUTTON_MIDDLE = HID_MOUSE_BUTTON_MIDDLE,
        MOUSE_BUTTON_RIGHT = HID_MOUSE_BUTTON_RIGHT,
        MOUSE_BUTTON_1 = HID_MOUSE_BUTTON_1,
        MOUSE_BUTTON_2 = HID_MOUSE_BUTTON_2,
        MOUSE_BUTTON_3 = HID_MOUSE_BUTTON_3,
        MOUSE_BUTTON_4 = HID_MOUSE_BUTTON_4,
        MOUSE_BUTTON_5 = HID_MOUSE_BUTTON_5,
        MOUSE_BUTTON_6 = HID_MOUSE_BUTTON_6,
        MOUSE_BUTTON_7 = HID_MOUSE_BUTTON_7,
        MOUSE_BUTTON_8 = HID_MOUSE_BUTTON_8,

        MAX_MOUSE_BUTTON_COUNT = HID_MOUSE_BUTTON_8 + 1
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
        bool m_GamepadDisconnected;
        bool m_GamepadConnected;
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
