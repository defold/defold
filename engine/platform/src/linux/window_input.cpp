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

#include "window_linux.h"
#include "../window_desktop_keys.h"
#include <dlib/utf8.h>
#include <linux/input-event-codes.h>

namespace dmPlatform
{
    int LinuxKeyCode(unsigned code)
    {
        static const char digits[] = "1234567890";
        static const char top[] = "QWERTYUIOP";
        static const char home[] = "ASDFGHJKL";
        static const char bottom[] = "ZXCVBNM";
        if (code >= KEY_1 && code <= KEY_0)
        {
            return digits[code - KEY_1];
        }

        if (code >= KEY_Q && code <= KEY_P)
        {
            return top[code - KEY_Q];
        }

        if (code >= KEY_A && code <= KEY_L)
        {
            return home[code - KEY_A];
        }

        if (code >= KEY_Z && code <= KEY_M)
        {
            return bottom[code - KEY_Z];
        }

        if (code >= KEY_F1 && code <= KEY_F10)
        {
            return PLATFORM_KEY_F1 + code - KEY_F1;
        }

        if (code >= KEY_F13 && code <= KEY_F24)
        {
            return PLATFORM_KEY_F13 + code - KEY_F13;
        }

        switch (code)
        {
            case KEY_SPACE:
            {
                return ' ';
            }
            case KEY_APOSTROPHE:
            {
                return '\'';
            }
            case KEY_COMMA:
            {
                return ',';
            }
            case KEY_MINUS:
            {
                return '-';
            }
            case KEY_DOT:
            {
                return '.';
            }
            case KEY_SLASH:
            {
                return '/';
            }
            case KEY_SEMICOLON:
            {
                return ';';
            }
            case KEY_EQUAL:
            {
                return '=';
            }
            case KEY_LEFTBRACE:
            {
                return '[';
            }
            case KEY_BACKSLASH:
            {
                return '\\';
            }
            case KEY_RIGHTBRACE:
            {
                return ']';
            }
            case KEY_GRAVE:
            {
                return '`';
            }
            case KEY_102ND:
            {
                return PLATFORM_KEY_NON_US_1;
            }
            case KEY_ESC:
            {
                return PLATFORM_KEY_ESC;
            }
            case KEY_ENTER:
            {
                return PLATFORM_KEY_ENTER;
            }
            case KEY_TAB:
            {
                return PLATFORM_KEY_TAB;
            }
            case KEY_BACKSPACE:
            {
                return PLATFORM_KEY_BACKSPACE;
            }
            case KEY_INSERT:
            {
                return PLATFORM_KEY_INSERT;
            }
            case KEY_DELETE:
            {
                return PLATFORM_KEY_DEL;
            }
            case KEY_RIGHT:
            {
                return PLATFORM_KEY_RIGHT;
            }
            case KEY_LEFT:
            {
                return PLATFORM_KEY_LEFT;
            }
            case KEY_DOWN:
            {
                return PLATFORM_KEY_DOWN;
            }
            case KEY_UP:
            {
                return PLATFORM_KEY_UP;
            }
            case KEY_PAGEUP:
            {
                return PLATFORM_KEY_PAGEUP;
            }
            case KEY_PAGEDOWN:
            {
                return PLATFORM_KEY_PAGEDOWN;
            }
            case KEY_HOME:
            {
                return PLATFORM_KEY_HOME;
            }
            case KEY_END:
            {
                return PLATFORM_KEY_END;
            }
            case KEY_CAPSLOCK:
            {
                return PLATFORM_KEY_CAPS_LOCK;
            }
            case KEY_SCROLLLOCK:
            {
                return PLATFORM_KEY_SCROLL_LOCK;
            }
            case KEY_NUMLOCK:
            {
                return PLATFORM_KEY_KP_NUM_LOCK;
            }
            case KEY_SYSRQ:
            case KEY_PRINT:
            {
                return PLATFORM_KEY_PRINT_SCREEN;
            }
            case KEY_PAUSE:
            {
                return PLATFORM_KEY_PAUSE;
            }
            case KEY_F11:
            {
                return PLATFORM_KEY_F11;
            }
            case KEY_F12:
            {
                return PLATFORM_KEY_F12;
            }
            case KEY_KP0:
            {
                return PLATFORM_KEY_KP_0;
            }
            case KEY_KP1:
            {
                return PLATFORM_KEY_KP_1;
            }
            case KEY_KP2:
            {
                return PLATFORM_KEY_KP_2;
            }
            case KEY_KP3:
            {
                return PLATFORM_KEY_KP_3;
            }
            case KEY_KP4:
            {
                return PLATFORM_KEY_KP_4;
            }
            case KEY_KP5:
            {
                return PLATFORM_KEY_KP_5;
            }
            case KEY_KP6:
            {
                return PLATFORM_KEY_KP_6;
            }
            case KEY_KP7:
            {
                return PLATFORM_KEY_KP_7;
            }
            case KEY_KP8:
            {
                return PLATFORM_KEY_KP_8;
            }
            case KEY_KP9:
            {
                return PLATFORM_KEY_KP_9;
            }
            case KEY_KPDOT:
            case KEY_KPCOMMA:
            {
                return PLATFORM_KEY_KP_DECIMAL;
            }
            case KEY_KPSLASH:
            {
                return PLATFORM_KEY_KP_DIVIDE;
            }
            case KEY_KPASTERISK:
            {
                return PLATFORM_KEY_KP_MULTIPLY;
            }
            case KEY_KPMINUS:
            {
                return PLATFORM_KEY_KP_SUBTRACT;
            }
            case KEY_KPPLUS:
            {
                return PLATFORM_KEY_KP_ADD;
            }
            case KEY_KPENTER:
            {
                return PLATFORM_KEY_KP_ENTER;
            }
            case KEY_KPEQUAL:
            {
                return PLATFORM_KEY_KP_EQUAL;
            }
            case KEY_LEFTSHIFT:
            {
                return PLATFORM_KEY_LSHIFT;
            }
            case KEY_LEFTCTRL:
            {
                return PLATFORM_KEY_LCTRL;
            }
            case KEY_LEFTALT:
            {
                return PLATFORM_KEY_LALT;
            }
            case KEY_LEFTMETA:
            {
                return PLATFORM_KEY_LSUPER;
            }
            case KEY_RIGHTSHIFT:
            {
                return PLATFORM_KEY_RSHIFT;
            }
            case KEY_RIGHTCTRL:
            {
                return PLATFORM_KEY_RCTRL;
            }
            case KEY_RIGHTALT:
            {
                return PLATFORM_KEY_RALT;
            }
            case KEY_RIGHTMETA:
            {
                return PLATFORM_KEY_RSUPER;
            }
            case KEY_COMPOSE:
            case KEY_MENU:
            {
                return PLATFORM_KEY_MENU;
            }
            default:
            {
                return -1;
            }
        }
    }

    void LinuxText(HWindow w, const char* text, size_t length)
    {
        // One input-method commit belongs to the window that received it, even if a listener reopens the handle.
        const uint32_t generation = w->m_OpenGeneration;
        while (length && w->m_Open && w->m_OpenGeneration == generation && w->m_Char)
        {
            uint32_t c;
            uint32_t size = dmUtf8::DecodeSequence(text, length, &c);
            if (!size)
            {
                break;
            }

            text += size;
            length -= size;
            if (c >= 32 && !(c >= 127 && c < 160))
            {
                w->m_Char(w->m_CharData, c);
            }
        }
    }

}
