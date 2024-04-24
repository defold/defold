// Copyright 2020-2024 The Defold Foundation
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

#ifndef HID_NATIVE_DEFINES_H
#define HID_NATIVE_DEFINES_H

#define HID_NATIVE_MAX_GAMEPAD_COUNT 16
#define HID_NATIVE_MAX_KEYBOARD_COUNT 1
#define HID_NATIVE_MAX_MOUSE_COUNT 1
#define HID_NATIVE_MAX_TOUCH_DEVICE_COUNT 1

#define HID_NATIVE_MAX_GAMEPAD_AXIS_COUNT 32
#define HID_NATIVE_MAX_GAMEPAD_BUTTON_COUNT 32
#define HID_NATIVE_MAX_GAMEPAD_HAT_COUNT 4
/// Maximum number of simultaneous touches supported
// An iPad supports a maximum of 11 simultaneous touches
#define HID_NATIVE_MAX_TOUCH_COUNT 11


#define HID_KEY_SPACE ' '
#define HID_KEY_EXCLAIM '!'
#define HID_KEY_QUOTEDBL '"'
#define HID_KEY_HASH '#'
#define HID_KEY_DOLLAR '$'
#define HID_KEY_AMPERSAND '&'
#define HID_KEY_QUOTE '\''
#define HID_KEY_LPAREN '('
#define HID_KEY_RPAREN ')'
#define HID_KEY_ASTERISK '*'
#define HID_KEY_PLUS '+'
#define HID_KEY_COMMA ','
#define HID_KEY_MINUS '-'
#define HID_KEY_PERIOD '.'
#define HID_KEY_SLASH '/'

#define HID_KEY_0 '0'
#define HID_KEY_1 '1'
#define HID_KEY_2 '2'
#define HID_KEY_3 '3'
#define HID_KEY_4 '4'
#define HID_KEY_5 '5'
#define HID_KEY_6 '6'
#define HID_KEY_7 '7'
#define HID_KEY_8 '8'
#define HID_KEY_9 '9'

#define HID_KEY_COLON ':'
#define HID_KEY_SEMICOLON ';'
#define HID_KEY_LESS '<'
#define HID_KEY_EQUALS '='
#define HID_KEY_GREATER '>'
#define HID_KEY_QUESTION '?'
#define HID_KEY_AT '@'

#define HID_KEY_A 'A'
#define HID_KEY_B 'B'
#define HID_KEY_C 'C'
#define HID_KEY_D 'D'
#define HID_KEY_E 'E'
#define HID_KEY_F 'F'
#define HID_KEY_G 'G'
#define HID_KEY_H 'H'
#define HID_KEY_I 'I'
#define HID_KEY_J 'J'
#define HID_KEY_K 'K'
#define HID_KEY_L 'L'
#define HID_KEY_M 'M'
#define HID_KEY_N 'N'
#define HID_KEY_O 'O'
#define HID_KEY_P 'P'
#define HID_KEY_Q 'Q'
#define HID_KEY_R 'R'
#define HID_KEY_S 'S'
#define HID_KEY_T 'T'
#define HID_KEY_U 'U'
#define HID_KEY_V 'V'
#define HID_KEY_W 'W'
#define HID_KEY_X 'X'
#define HID_KEY_Y 'Y'
#define HID_KEY_Z 'Z'

#define HID_KEY_LBRACKET '['
#define HID_KEY_BACKSLASH '\\'
#define HID_KEY_RBRACKET ']'
#define HID_KEY_CARET '^'
#define HID_KEY_UNDERSCORE '_'
#define HID_KEY_BACKQUOTE '`'

#define HID_KEY_LBRACE '{'
#define HID_KEY_PIPE '|'
#define HID_KEY_RBRACE '}'
#define HID_KEY_TILDE '~'

#define HID_SPECIAL_START 256
#define HID_KEY_ESC (HID_SPECIAL_START+0)
#define HID_KEY_F1 (HID_SPECIAL_START+1)
#define HID_KEY_F2 (HID_SPECIAL_START+2)
#define HID_KEY_F3 (HID_SPECIAL_START+3)
#define HID_KEY_F4 (HID_SPECIAL_START+4)
#define HID_KEY_F5 (HID_SPECIAL_START+5)
#define HID_KEY_F6 (HID_SPECIAL_START+6)
#define HID_KEY_F7 (HID_SPECIAL_START+7)
#define HID_KEY_F8 (HID_SPECIAL_START+8)
#define HID_KEY_F9 (HID_SPECIAL_START+9)
#define HID_KEY_F10 (HID_SPECIAL_START+10)
#define HID_KEY_F11 (HID_SPECIAL_START+11)
#define HID_KEY_F12 (HID_SPECIAL_START+12)
#define HID_KEY_UP (HID_SPECIAL_START+13)
#define HID_KEY_DOWN (HID_SPECIAL_START+14)
#define HID_KEY_LEFT (HID_SPECIAL_START+15)
#define HID_KEY_RIGHT (HID_SPECIAL_START+16)
#define HID_KEY_LSHIFT (HID_SPECIAL_START+17)
#define HID_KEY_RSHIFT (HID_SPECIAL_START+18)
#define HID_KEY_LCTRL (HID_SPECIAL_START+19)
#define HID_KEY_RCTRL (HID_SPECIAL_START+20)
#define HID_KEY_LALT (HID_SPECIAL_START+21)
#define HID_KEY_RALT (HID_SPECIAL_START+22)
#define HID_KEY_TAB (HID_SPECIAL_START+23)
#define HID_KEY_ENTER (HID_SPECIAL_START+24)
#define HID_KEY_BACKSPACE (HID_SPECIAL_START+25)
#define HID_KEY_INSERT (HID_SPECIAL_START+26)
#define HID_KEY_DEL (HID_SPECIAL_START+27)
#define HID_KEY_PAGEUP (HID_SPECIAL_START+28)
#define HID_KEY_PAGEDOWN (HID_SPECIAL_START+29)
#define HID_KEY_HOME (HID_SPECIAL_START+30)
#define HID_KEY_END (HID_SPECIAL_START+31)

#define HID_KEY_KP_0 (HID_SPECIAL_START+32)
#define HID_KEY_KP_1 (HID_SPECIAL_START+33)
#define HID_KEY_KP_2 (HID_SPECIAL_START+34)
#define HID_KEY_KP_3 (HID_SPECIAL_START+35)
#define HID_KEY_KP_4 (HID_SPECIAL_START+36)
#define HID_KEY_KP_5 (HID_SPECIAL_START+37)
#define HID_KEY_KP_6 (HID_SPECIAL_START+38)
#define HID_KEY_KP_7 (HID_SPECIAL_START+39)
#define HID_KEY_KP_8 (HID_SPECIAL_START+40)
#define HID_KEY_KP_9 (HID_SPECIAL_START+41)
#define HID_KEY_KP_DIVIDE (HID_SPECIAL_START+42)
#define HID_KEY_KP_MULTIPLY (HID_SPECIAL_START+43)
#define HID_KEY_KP_SUBTRACT (HID_SPECIAL_START+44)
#define HID_KEY_KP_ADD (HID_SPECIAL_START+45)
#define HID_KEY_KP_DECIMAL (HID_SPECIAL_START+46)
#define HID_KEY_KP_EQUAL (HID_SPECIAL_START+47)
#define HID_KEY_KP_ENTER (HID_SPECIAL_START+48)
#define HID_KEY_KP_NUM_LOCK (HID_SPECIAL_START+49)
#define HID_KEY_CAPS_LOCK (HID_SPECIAL_START+50)
#define HID_KEY_SCROLL_LOCK (HID_SPECIAL_START+51)
#define HID_KEY_PAUSE (HID_SPECIAL_START+52)
#define HID_KEY_LSUPER (HID_SPECIAL_START+53)
#define HID_KEY_RSUPER (HID_SPECIAL_START+54)
#define HID_KEY_MENU (HID_SPECIAL_START+55)
#define HID_KEY_BACK (HID_SPECIAL_START+56)

#define HID_MOUSE_BUTTON_LEFT   0
#define HID_MOUSE_BUTTON_MIDDLE 1
#define HID_MOUSE_BUTTON_RIGHT  2
#define HID_MOUSE_BUTTON_1      3
#define HID_MOUSE_BUTTON_2      4
#define HID_MOUSE_BUTTON_3      5
#define HID_MOUSE_BUTTON_4      6
#define HID_MOUSE_BUTTON_5      7
#define HID_MOUSE_BUTTON_6      8
#define HID_MOUSE_BUTTON_7      9
#define HID_MOUSE_BUTTON_8      10

#endif
