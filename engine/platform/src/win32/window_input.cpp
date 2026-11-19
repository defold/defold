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

#include "window_win32.h"
#include <imm.h>
#include <windowsx.h>
#include <math.h>

namespace dmPlatform
{
    // Physical PC scan codes map to the desktop key numbers used in compiled input bindings.
    static int KeyCode(WPARAM key, LPARAM flags)
    {
        unsigned scan = (flags >> 16) & 0xff;
        if (scan == 0)
        {
            scan = MapVirtualKeyW((UINT)key, MAPVK_VK_TO_VSC);
        }

        if (key == VK_PAUSE)
        {
            return 284;
        }

        if (key == VK_SNAPSHOT)
        {
            return 283;
        }

        if (key == VK_NUMLOCK)
        {
            return 282;
        }

        if (flags & (1 << 24))
        {
            switch (scan)
            {
                case 0x1c:
                {
                    return 335;        // Keypad Enter
                }
                case 0x1d:
                {
                    return 345;        // Right Control
                }
                case 0x35:
                {
                    return 331;
                }
                case 0x38:
                {
                    return 346;
                }
                case 0x47:
                {
                    return 268;
                }
                case 0x48:
                {
                    return 265;
                }
                case 0x49:
                {
                    return 266;
                }
                case 0x4b:
                {
                    return 263;
                }
                case 0x4d:
                {
                    return 262;
                }
                case 0x4f:
                {
                    return 269;
                }
                case 0x50:
                {
                    return 264;
                }
                case 0x51:
                {
                    return 267;
                }
                case 0x52:
                {
                    return 260;
                }
                case 0x53:
                {
                    return 261;
                }
                case 0x5b:
                {
                    return 343;
                }
                case 0x5c:
                {
                    return 347;
                }
                case 0x5d:
                {
                    return 348;
                }
                default:
                {
                    return -1;
                }
            }
        }

        static const short codes[] = {
            -1, 256, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 259, 258,
            'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', 257, 341, 'A', 'S',
            'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', 39, 96, 340, 92, 'Z', 'X', 'C', 'V',
            'B', 'N', 'M', ',', '.', '/', 344, 332, 342, ' ', 280, 290, 291, 292, 293, 294,
            295, 296, 297, 298, 299, 282, 281, 327, 328, 329, 333, 324, 325, 326, 334, 321,
            322, 323, 320, 330, -1, -1, 162, 300, 301
        };
        if (key >= VK_F13 && key <= VK_F24)
        {
            return 302 + (int)key - VK_F13;
        }

        if (scan == 0x59)
        {
            return 336; // Keypad equals
        }

        return scan < sizeof(codes) / sizeof(codes[0]) ? codes[scan] : -1;
    }

    static bool AltGrControl(UINT message, WPARAM key, LPARAM flags)
    {
        if (key != VK_CONTROL || (flags & (1 << 24)))
        {
            return false;
        }

        MSG next;
        if (!PeekMessageW(&next, 0, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE))
        {
            return false;
        }

        bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        bool matching = down ? next.message == WM_KEYDOWN || next.message == WM_SYSKEYDOWN
                             : next.message == WM_KEYUP || next.message == WM_SYSKEYUP;
        return matching && next.wParam == VK_MENU && (next.lParam & (1 << 24)) && next.time == (DWORD)GetMessageTime();
    }

    static void Character(HWindow window, uint32_t code)
    {
        if (window->m_Callbacks.m_Char && code >= 32 && (code < 127 || code >= 160) &&
            code <= 0x10ffff && (code < 0xd800 || code > 0xdfff))
        {
            window->m_Callbacks.m_Char(window->m_Callbacks.m_CharData, code);
        }
    }

    static void CharacterUnit(HWindow window, uint16_t unit)
    {
        if (unit >= 0xd800 && unit <= 0xdbff)
        {
            window->m_HighSurrogate = unit;
            return;
        }

        uint32_t code = unit;
        if (window->m_HighSurrogate && unit >= 0xdc00 && unit <= 0xdfff)
        {
            code = 0x10000 + ((window->m_HighSurrogate - 0xd800) << 10) + (unit - 0xdc00);
        }

        window->m_HighSurrogate = 0;
        Character(window, code);
    }

    static void MarkedText(HWindow window, HIMC ime, DWORD kind)
    {
        LONG bytes = ImmGetCompositionStringW(ime, kind, 0, 0);
        if (bytes < 0)
        {
            return;
        }

        dmArray<WCHAR> wide;
        wide.SetCapacity(bytes / sizeof(WCHAR) + 1);
        LONG size = ImmGetCompositionStringW(ime, kind, wide.Begin(), bytes);
        if (size < 0 || size > bytes)
        {
            return;
        }

        wide.Begin()[size / sizeof(WCHAR)] = 0;
        int count = WideCharToMultiByte(CP_UTF8, 0, wide.Begin(), -1, 0, 0, 0, 0);
        dmArray<char> text;
        text.SetCapacity(count);
        if (count && WideCharToMultiByte(CP_UTF8, 0, wide.Begin(), -1, text.Begin(), count, 0, 0))
        {
            window->m_Callbacks.m_Marked(window->m_Callbacks.m_MarkedData, text.Begin());
        }
    }

    static POINT CursorPosition(HWND hwnd)
    {
        POINT cursor = {};
        GetCursorPos(&cursor);
        ScreenToClient(hwnd, &cursor);
        return cursor;
    }

    void Win32CenterCursor(HWindow window)
    {
        if (!window->m_CursorCaptured)
        {
            return;
        }

        RECT rect;
        GetClientRect(window->m_Window, &rect);
        POINT center = { rect.right / 2, rect.bottom / 2 };
        MapWindowPoints(window->m_Window, 0, (POINT*)&rect, 2);
        RECT clip;
        bool moved = !GetClipCursor(&clip) || !EqualRect(&clip, &rect);
        if (moved)
        {
            ClipCursor(&rect);
        }

        // Moving the HWND changes the screen center even if its client size stays the same.
        if (moved || window->m_LastCursor.x != center.x || window->m_LastCursor.y != center.y)
        {
            window->m_LastCursor = center;
            SetCursorPos(rect.left + center.x, rect.top + center.y);
        }
    }

    void Win32UpdateCursor(HWindow window, bool focused)
    {
        bool capture = focused && window->m_CursorLocked;
        if (capture == window->m_CursorCaptured)
        {
            return;
        }

        window->m_CursorCaptured = capture;
        if (capture)
        {
            window->m_LastCursor = CursorPosition(window->m_Window);
            Win32CenterCursor(window);
            SetCursor(0);
        }
        else
        {
            ClipCursor(0);
            POINT pos = window->m_SavedCursor;
            ClientToScreen(window->m_Window, &pos);
            SetCursorPos(pos.x, pos.y);
            SetCursor((HCURSOR)GetClassLongPtrW(window->m_Window, GCLP_HCURSOR));
        }
    }

    static void Button(HWindow window, int button, bool pressed)
    {
        if (pressed)
        {
            SetCapture(window->m_Window);
        }

        window->m_Buttons[button] = pressed;
        if (!pressed && GetCapture() == window->m_Window)
        {
            for (unsigned i = 0; i < sizeof(window->m_Buttons); ++i)
            {
                if (window->m_Buttons[i])
                {
                    return;
                }
            }

            ReleaseCapture();
        }
    }

    bool Win32HandleInput(HWindow window, UINT message, WPARAM wp, LPARAM lp, LRESULT* result)
    {
        switch (message)
        {
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            {
                int code = KeyCode(wp, lp);
                if (code >= 0 && !AltGrControl(message, wp, lp))
                {
                    window->m_Keys[code] = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
                }

                break; // DefWindowProc handles system shortcuts, including Alt+F4.
            }
            case WM_CHAR:
            {
                CharacterUnit(window, (uint16_t)wp);
                return true;
            }
            case WM_UNICHAR:
            {
                if (wp == UNICODE_NOCHAR)
                {
                    *result = TRUE;
                }
                else
                {
                    Character(window, (uint32_t)wp);
                }

                return true;
            }
            case WM_IME_COMPOSITION:
            {
                if (window->m_Callbacks.m_Marked)
                {
                    HIMC ime = ImmGetContext(window->m_Window);
                    if (ime)
                    {
                        if (lp & GCS_COMPSTR)
                        {
                            MarkedText(window, ime, GCS_COMPSTR);
                        }

                        if (lp & GCS_RESULTSTR)
                        {
                            MarkedText(window, ime, GCS_RESULTSTR);
                        }

                        ImmReleaseContext(window->m_Window, ime);
                    }
                }

                break; // The default IME generates WM_CHAR for committed text.
            }
            case WM_IME_ENDCOMPOSITION:
            {
                if (window->m_Callbacks.m_Marked)
                {
                    char empty[] = "";
                    window->m_Callbacks.m_Marked(window->m_Callbacks.m_MarkedData, empty);
                }

                break;
            }
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            {
                Button(window, 0, message == WM_LBUTTONDOWN);
                return true;
            }
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            {
                Button(window, 1, message == WM_RBUTTONDOWN);
                return true;
            }
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            {
                Button(window, 2, message == WM_MBUTTONDOWN);
                return true;
            }
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            {
                Button(window, HIWORD(wp) == XBUTTON1 ? 3 : 4, message == WM_XBUTTONDOWN);
                *result = TRUE;
                return true;
            }
            case WM_CAPTURECHANGED:
            {
                memset(window->m_Buttons, 0, sizeof(window->m_Buttons));
                break;
            }
            case WM_MOUSEWHEEL:
            {
                window->m_Wheel += GET_WHEEL_DELTA_WPARAM(wp) / (double)WHEEL_DELTA;
                return true;
            }
            case WM_MOUSEMOVE:
            {
                if (window->m_CursorCaptured)
                {
                    POINT point = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                    window->m_VirtualCursor.x += point.x - window->m_LastCursor.x;
                    window->m_VirtualCursor.y += point.y - window->m_LastCursor.y;
                    window->m_LastCursor = point;
                }

                break;
            }
            case WM_SETCURSOR:
            {
                if (window->m_CursorCaptured && LOWORD(lp) == HTCLIENT)
                {
                    SetCursor(0);
                    *result = TRUE;
                    return true;
                }

                break;
            }
            case WM_ENTERSIZEMOVE:
            case WM_ENTERMENULOOP:
            {
                Win32UpdateCursor(window, false);
                break;
            }
            case WM_EXITSIZEMOVE:
            case WM_EXITMENULOOP:
            {
                Win32UpdateCursor(window, GetFocus() == window->m_Window);
                break;
            }
        }

        return false;
    }

    int32_t GetKey(HWindow window, int32_t key)
    {
        return (uint32_t)key < sizeof(window->m_Keys) ? window->m_Keys[key] : 0;
    }

    int32_t GetMouseButton(HWindow window, int32_t button)
    {
        return (uint32_t)button < sizeof(window->m_Buttons) ? window->m_Buttons[button] : 0;
    }

    int32_t GetMouseWheel(HWindow window)
    {
        return (int32_t)round(window->m_Wheel);
    }

    void GetMousePosition(HWindow window, int32_t* x, int32_t* y)
    {
        POINT pos = window->m_CursorLocked ? window->m_VirtualCursor : CursorPosition(window->m_Window);
        *x = pos.x;
        *y = pos.y;
    }

    void SetKeyboardCharCallback(HWindow window, FWindowAddKeyboardCharCallback cb, void* data)
    {
        window->m_Callbacks.m_Char = cb;
        window->m_Callbacks.m_CharData = data;
    }

    void SetKeyboardMarkedTextCallback(HWindow window, FWindowSetMarkedTextCallback cb, void* data)
    {
        window->m_Callbacks.m_Marked = cb;
        window->m_Callbacks.m_MarkedData = data;
    }

    void SetKeyboardDeviceChangedCallback(HWindow, FWindowDeviceChangedCallback, void*)
    {
    }

    void SetGamepadEventCallback(HWindow window, FWindowGamepadEventCallback cb, void* data)
    {
        window->m_Callbacks.m_Gamepad = cb;
        window->m_Callbacks.m_GamepadData = data;
        if (cb && window->m_Ready)
        {
            Win32InitJoysticks();
        }
    }

    void SetDeviceState(HWindow window, WindowDeviceState state, bool value, bool)
    {
        if (state == WINDOW_DEVICE_STATE_CURSOR && window->m_CursorLocked == value && window->m_Window)
        {
            window->m_CursorLocked = !value;
            if (!value)
            {
                window->m_VirtualCursor = window->m_SavedCursor = CursorPosition(window->m_Window);
            }

            Win32UpdateCursor(window, GetFocus() == window->m_Window);
        }
    }

    void SetDeviceState(HWindow window, WindowDeviceState state, bool value)
    {
        SetDeviceState(window, state, value, false);
    }

    bool GetDeviceState(HWindow window, WindowDeviceState state, int32_t index)
    {
        if (state == WINDOW_DEVICE_STATE_JOYSTICK_PRESENT)
        {
            return Win32JoystickPresent(index);
        }

        return state == WINDOW_DEVICE_STATE_CURSOR_LOCK && window->m_CursorLocked;
    }

    bool GetDeviceState(HWindow window, WindowDeviceState state)
    {
        return GetDeviceState(window, state, 0);
    }

}
