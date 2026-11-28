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
#include "../platform_window_constants.h"
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <linux/input-event-codes.h>
#include <string.h>

namespace dmPlatform
{
    struct X11Key
    {
        const char* m_Name;
        KeySym      m_Symbol;
        int         m_Evdev;
    };

    static const X11Key KEYS[] = {
        { "ESC", XK_Escape, KEY_ESC }, { "TAB", XK_Tab, KEY_TAB }, { "RTRN", XK_Return, KEY_ENTER },
        { "BKSP", XK_BackSpace, KEY_BACKSPACE }, { "SPCE", XK_space, KEY_SPACE }, { "TLDE", XK_grave, KEY_GRAVE },
        { "BKSL", XK_backslash, KEY_BACKSLASH }, { "LSGT", XK_less, KEY_102ND }, { "CAPS", XK_Caps_Lock, KEY_CAPSLOCK },
        { "LFSH", XK_Shift_L, KEY_LEFTSHIFT }, { "RTSH", XK_Shift_R, KEY_RIGHTSHIFT },
        { "LCTL", XK_Control_L, KEY_LEFTCTRL }, { "RCTL", XK_Control_R, KEY_RIGHTCTRL },
        { "LALT", XK_Alt_L, KEY_LEFTALT }, { "RALT", XK_Alt_R, KEY_RIGHTALT },
        { "LVL3", XK_ISO_Level3_Shift, KEY_RIGHTALT }, { "LVL5", XK_ISO_Level5_Shift, KEY_RIGHTALT },
        { "LWIN", XK_Super_L, KEY_LEFTMETA }, { "RWIN", XK_Super_R, KEY_RIGHTMETA }, { "MENU", XK_Menu, KEY_MENU },
        { "COMP", XK_Multi_key, KEY_COMPOSE }, { "INS", XK_Insert, KEY_INSERT }, { "DELE", XK_Delete, KEY_DELETE },
        { "HOME", XK_Home, KEY_HOME }, { "END", XK_End, KEY_END }, { "PGUP", XK_Page_Up, KEY_PAGEUP },
        { "PGDN", XK_Page_Down, KEY_PAGEDOWN }, { "LEFT", XK_Left, KEY_LEFT }, { "RGHT", XK_Right, KEY_RIGHT },
        { "UP", XK_Up, KEY_UP }, { "DOWN", XK_Down, KEY_DOWN }, { "PRSC", XK_Print, KEY_SYSRQ },
        { "SCLK", XK_Scroll_Lock, KEY_SCROLLLOCK }, { "PAUS", XK_Pause, KEY_PAUSE }, { "NMLK", XK_Num_Lock, KEY_NUMLOCK },
        { "KPDV", XK_KP_Divide, KEY_KPSLASH }, { "KPMU", XK_KP_Multiply, KEY_KPASTERISK },
        { "KPSU", XK_KP_Subtract, KEY_KPMINUS }, { "KPAD", XK_KP_Add, KEY_KPPLUS },
        { "KPEN", XK_KP_Enter, KEY_KPENTER }, { "KPDL", XK_KP_Delete, KEY_KPDOT }, { "KPEQ", XK_KP_Equal, KEY_KPEQUAL },
        { "KP0", XK_KP_Insert, KEY_KP0 }, { "KP1", XK_KP_End, KEY_KP1 }, { "KP2", XK_KP_Down, KEY_KP2 },
        { "KP3", XK_KP_Page_Down, KEY_KP3 }, { "KP4", XK_KP_Left, KEY_KP4 }, { "KP5", XK_KP_Begin, KEY_KP5 },
        { "KP6", XK_KP_Right, KEY_KP6 }, { "KP7", XK_KP_Home, KEY_KP7 }, { "KP8", XK_KP_Up, KEY_KP8 },
        { "KP9", XK_KP_Page_Up, KEY_KP9 },
    };

    static int PhysicalKey(const char* name)
    {
        // XKB numbers physical keys from left to right in each alphanumeric row.
        if (name[2] >= '0' && name[2] <= '9' && name[3] >= '0' && name[3] <= '9')
        {
            int number = 10 * (name[2] - '0') + name[3] - '0';
            if (name[0] == 'A' && name[1] >= 'B' && name[1] <= 'E')
            {
                static const char* const rows[] = { "ZXCVBNM,./", "ASDFGHJKL;'", "QWERTYUIOP[]", "1234567890-=" };
                const char* row = rows[name[1] - 'B'];
                return number > 0 && (size_t)number <= strlen(row) ? row[number - 1] : -1;
            }

            if (name[0] == 'F' && name[1] == 'K' && number >= 1 && number <= 25)
            {
                return PLATFORM_KEY_F1 + number - 1;
            }
        }

        for (const X11Key& key : KEYS)
        {
            if (!strncmp(name, key.m_Name, XkbKeyNameLength))
            {
                return LinuxKeyCode(key.m_Evdev);
            }
        }

        return -1;
    }

    static int SymbolKey(KeySym symbol)
    {
        if (symbol >= XK_a && symbol <= XK_z)
        {
            return 'A' + symbol - XK_a;
        }

        if (symbol > 0 && symbol < 128 && strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-=[];',./", (int)symbol))
        {
            return (int)symbol;
        }

        if (symbol >= XK_F1 && symbol <= XK_F25)
        {
            return PLATFORM_KEY_F1 + symbol - XK_F1;
        }

        if (symbol >= XK_KP_0 && symbol <= XK_KP_9)
        {
            return PLATFORM_KEY_KP_0 + symbol - XK_KP_0;
        }

        if (symbol == XK_KP_Decimal || symbol == XK_KP_Separator)
        {
            return PLATFORM_KEY_KP_DECIMAL;
        }

        if (symbol == XK_Meta_L || symbol == XK_Meta_R || symbol == XK_Mode_switch)
        {
            return symbol == XK_Meta_L ? PLATFORM_KEY_LALT : PLATFORM_KEY_RALT;
        }

        for (const X11Key& key : KEYS)
        {
            if (symbol == key.m_Symbol)
            {
                return LinuxKeyCode(key.m_Evdev);
            }
        }

        return -1;
    }

    void X11ReadKeymap(Display* display, int16_t* keys)
    {
        int first, last, width;
        XDisplayKeycodes(display, &first, &last);
        KeySym* symbols = XGetKeyboardMapping(display, first, last - first + 1, &width);
        XkbDescPtr keyboard = XkbGetMap(display, 0, XkbUseCoreKbd);
        XkbNamesPtr names = 0;
        if (keyboard && XkbGetNames(display, XkbKeyNamesMask | XkbKeyAliasesMask, keyboard) == Success)
        {
            names = keyboard->names;
        }

        memset(keys, -1, 256 * sizeof(*keys));
        for (int native = first; native <= last; ++native)
        {
            int key = -1;
            if (names && names->keys && native >= keyboard->min_key_code && native <= keyboard->max_key_code)
            {
                const char* name = names->keys[native].name;
                key = PhysicalKey(name);
                for (int i = 0; key < 0 && i < names->num_key_aliases; ++i)
                {
                    const XkbKeyAliasRec& alias = names->key_aliases[i];
                    if (!memcmp(alias.real, name, XkbKeyNameLength))
                    {
                        key = PhysicalKey(alias.alias);
                    }
                }
            }

            if (key < 0 && symbols && width > 0)
            {
                KeySym* levels = symbols + (native - first) * width;
                // With Num Lock off, the first keypad symbol can be a navigation key.
                if (width > 1 && IsKeypadKey(levels[1]))
                {
                    key = SymbolKey(levels[1]);
                }

                if (key < 0)
                {
                    key = SymbolKey(levels[0]);
                }
            }

            keys[native] = key;
        }

        if (keyboard)
        {
            XkbFreeKeyboard(keyboard, XkbAllComponentsMask, True);
        }

        XFree(symbols);
    }

}
