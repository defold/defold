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
            return 290 + code - KEY_F1;
        }

        if (code >= KEY_F13 && code <= KEY_F24)
        {
            return 302 + code - KEY_F13;
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
                return 96;
            }
            case KEY_102ND:
            {
                return 161;
            }
            case KEY_ESC:
            {
                return 256;
            }
            case KEY_ENTER:
            {
                return 257;
            }
            case KEY_TAB:
            {
                return 258;
            }
            case KEY_BACKSPACE:
            {
                return 259;
            }
            case KEY_INSERT:
            {
                return 260;
            }
            case KEY_DELETE:
            {
                return 261;
            }
            case KEY_RIGHT:
            {
                return 262;
            }
            case KEY_LEFT:
            {
                return 263;
            }
            case KEY_DOWN:
            {
                return 264;
            }
            case KEY_UP:
            {
                return 265;
            }
            case KEY_PAGEUP:
            {
                return 266;
            }
            case KEY_PAGEDOWN:
            {
                return 267;
            }
            case KEY_HOME:
            {
                return 268;
            }
            case KEY_END:
            {
                return 269;
            }
            case KEY_CAPSLOCK:
            {
                return 280;
            }
            case KEY_SCROLLLOCK:
            {
                return 281;
            }
            case KEY_NUMLOCK:
            {
                return 282;
            }
            case KEY_SYSRQ:
            {
                return 283;
            }
            case KEY_PAUSE:
            {
                return 284;
            }
            case KEY_F11:
            {
                return 300;
            }
            case KEY_F12:
            {
                return 301;
            }
            case KEY_KP0:
            {
                return 320;
            }
            case KEY_KP1:
            {
                return 321;
            }
            case KEY_KP2:
            {
                return 322;
            }
            case KEY_KP3:
            {
                return 323;
            }
            case KEY_KP4:
            {
                return 324;
            }
            case KEY_KP5:
            {
                return 325;
            }
            case KEY_KP6:
            {
                return 326;
            }
            case KEY_KP7:
            {
                return 327;
            }
            case KEY_KP8:
            {
                return 328;
            }
            case KEY_KP9:
            {
                return 329;
            }
            case KEY_KPDOT:
            case KEY_KPCOMMA:
            {
                return 330;
            }
            case KEY_KPSLASH:
            {
                return 331;
            }
            case KEY_KPASTERISK:
            {
                return 332;
            }
            case KEY_KPMINUS:
            {
                return 333;
            }
            case KEY_KPPLUS:
            {
                return 334;
            }
            case KEY_KPENTER:
            {
                return 335;
            }
            case KEY_KPEQUAL:
            {
                return 336;
            }
            case KEY_LEFTSHIFT:
            {
                return 340;
            }
            case KEY_LEFTCTRL:
            {
                return 341;
            }
            case KEY_LEFTALT:
            {
                return 342;
            }
            case KEY_LEFTMETA:
            {
                return 343;
            }
            case KEY_RIGHTSHIFT:
            {
                return 344;
            }
            case KEY_RIGHTCTRL:
            {
                return 345;
            }
            case KEY_RIGHTALT:
            {
                return 346;
            }
            case KEY_RIGHTMETA:
            {
                return 347;
            }
            case KEY_COMPOSE:
            case KEY_MENU:
            {
                return 348;
            }
            default:
            {
                return -1;
            }
        }
    }

    void LinuxText(HWindow w, const char* text, size_t length)
    {
        const char* end = text + length;
        while (w->m_Char && text < end)
        {
            if (!dmUtf8::ValidateSequence(text, end - text))
            {
                break;
            }

            uint32_t c = dmUtf8::NextChar(&text);
            if (c >= 32 && !(c >= 127 && c < 160))
            {
                w->m_Char(w->m_CharData, c);
            }
        }
    }

}
