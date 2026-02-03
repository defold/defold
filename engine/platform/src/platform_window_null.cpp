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

#include <dlib/log.h>

#include "platform_window.h"
#include "platform_window_constants.h"

namespace dmPlatform
{
    struct dmWindow
    {
        WindowParams m_CreateParams;
        uint32_t     m_WindowWidth;
        uint32_t     m_WindowHeight;
        uint32_t     m_WindowOpened             : 1;
        uint32_t     m_StateCursor              : 1;
        uint32_t     m_StateCursorLock          : 1;
        uint32_t     m_StateCursorAccelerometer : 1;
        uint32_t     m_StateKeyboard            : 3;
    };

    HWindow NewWindow()
    {
        dmWindow* wnd = new dmWindow();
        memset(wnd, 0, sizeof(dmWindow));
        return wnd;
    }

    void DeleteWindow(HWindow window)
    {
        delete window;
    }

    PlatformResult OpenWindow(HWindow window, const WindowParams& params)
    {
        if (window->m_WindowOpened)
        {
            return PLATFORM_RESULT_WINDOW_ALREADY_OPENED;
        }

        window->m_CreateParams = params;
        window->m_WindowOpened = 1;
        window->m_WindowWidth  = params.m_Width;
        window->m_WindowHeight = params.m_Height;

        return PLATFORM_RESULT_OK;
    }

    void CloseWindow(HWindow window)
    {
        window->m_WindowOpened = 0;
        window->m_WindowWidth  = 0;
        window->m_WindowHeight = 0;
    }

    uint32_t GetWindowWidth(HWindow window)
    {
        return window->m_WindowWidth;
    }

    uint32_t GetWindowHeight(HWindow window)
    {
        return window->m_WindowHeight;
    }

    bool GetSafeArea(HWindow window, SafeArea* out)
    {
        const uint32_t width = GetWindowWidth(window);
        const uint32_t height = GetWindowHeight(window);

        out->m_X = 0;
        out->m_Y = 0;
        out->m_Width = width;
        out->m_Height = height;
        out->m_InsetLeft = 0;
        out->m_InsetTop = 0;
        out->m_InsetRight = 0;
        out->m_InsetBottom = 0;

        return true;
    }

    uint32_t GetWindowStateParam(HWindow window, WindowState state)
    {
        switch(state)
        {
            case WINDOW_STATE_OPENED: return window->m_WindowOpened;
            case WINDOW_STATE_FSAA_SAMPLES: return window->m_CreateParams.m_Samples;
            default:break;
        }

        return 0;
    }

    float GetDisplayScaleFactor(HWindow window)
    {
        return 1.0f;
    }

    void SetWindowTitle(HWindow window, const char* title)
    {}

    void SetWindowSize(HWindow window, uint32_t width, uint32_t height)
    {
        window->m_WindowWidth  = width;
        window->m_WindowHeight = height;

        if (window->m_CreateParams.m_ResizeCallback)
        {
            window->m_CreateParams.m_ResizeCallback(window->m_CreateParams.m_ResizeCallbackUserData, width, height);
        }
    }

    void SetWindowPosition(HWindow window, int32_t x, int32_t y)
    {}

    void ShowWindow(HWindow window)
    {}

    void SetSwapInterval(HWindow window, uint32_t swap_interval)
    {}

    void IconifyWindow(HWindow window)
    {}

    void PollEvents(HWindow window)
    {}

    void SwapBuffers(HWindow window)
    {}

    void SetDeviceState(HWindow window, DeviceState state, bool op1)
    {
        SetDeviceState(window, state, op1, false);
    }

    void SetDeviceState(HWindow window, DeviceState state, bool op1, bool op2)
    {
        switch(state)
        {
            case DEVICE_STATE_CURSOR:
                window->m_StateCursor = op1;
                window->m_StateCursorLock = !op1;
                break;
            case DEVICE_STATE_CURSOR_LOCK:
                // We don't distinguish between locked state and visible state on the null implementation
                break;
            case DEVICE_STATE_ACCELEROMETER:
                window->m_StateCursorAccelerometer = op1;
                break;
            case DEVICE_STATE_KEYBOARD_DEFAULT:
                window->m_StateKeyboard = op1 ? 1 : 0;
                break;
            case DEVICE_STATE_KEYBOARD_NUMBER_PAD:
                window->m_StateKeyboard = op1 ? 2 : 0;
                break;
            case DEVICE_STATE_KEYBOARD_EMAIL:
                window->m_StateKeyboard = op1 ? 3 : 0;
                break;
            case DEVICE_STATE_KEYBOARD_PASSWORD:
                window->m_StateKeyboard = op1 ? 4 : 0;
                break;
            default:
                dmLogWarning("Unable to set device state (%d), unknown state.", (int) state);
                break;
        }
    }

    bool GetDeviceState(HWindow window, DeviceState state)
    {
        switch(state)
        {
            case DEVICE_STATE_CURSOR:              return window->m_StateCursor;
            case DEVICE_STATE_CURSOR_LOCK:         return window->m_StateCursorLock;
            case DEVICE_STATE_ACCELEROMETER:       return window->m_StateCursorAccelerometer;
            case DEVICE_STATE_KEYBOARD_DEFAULT:    return window->m_StateKeyboard == 1;
            case DEVICE_STATE_KEYBOARD_NUMBER_PAD: return window->m_StateKeyboard == 2;
            case DEVICE_STATE_KEYBOARD_EMAIL:      return window->m_StateKeyboard == 3;
            case DEVICE_STATE_KEYBOARD_PASSWORD:   return window->m_StateKeyboard == 4;
            default:
                dmLogWarning("Unable to set device state (%d), unknown state.", (int) state);
                break;
        }
        return false;
    }

    int32_t TriggerCloseCallback(HWindow window)
    {
        if (window->m_CreateParams.m_CloseCallback)
        {
            return window->m_CreateParams.m_CloseCallback(window->m_CreateParams.m_CloseCallbackUserData);
        }
        return 0;
    }

    const int PLATFORM_KEY_START           = 0;
    const int PLATFORM_JOYSTICK_LAST       = 0;
    const int PLATFORM_KEY_ESC             = 1;
    const int PLATFORM_KEY_F1              = 2;
    const int PLATFORM_KEY_F2              = 3;
    const int PLATFORM_KEY_F3              = 4;
    const int PLATFORM_KEY_F4              = 5;
    const int PLATFORM_KEY_F5              = 6;
    const int PLATFORM_KEY_F6              = 7;
    const int PLATFORM_KEY_F7              = 8;
    const int PLATFORM_KEY_F8              = 9;
    const int PLATFORM_KEY_F9              = 10;
    const int PLATFORM_KEY_F10             = 11;
    const int PLATFORM_KEY_F11             = 12;
    const int PLATFORM_KEY_F12             = 13;
    const int PLATFORM_KEY_UP              = 14;
    const int PLATFORM_KEY_DOWN            = 15;
    const int PLATFORM_KEY_LEFT            = 16;
    const int PLATFORM_KEY_RIGHT           = 17;
    const int PLATFORM_KEY_LSHIFT          = 18;
    const int PLATFORM_KEY_RSHIFT          = 19;
    const int PLATFORM_KEY_LCTRL           = 20;
    const int PLATFORM_KEY_RCTRL           = 21;
    const int PLATFORM_KEY_LALT            = 22;
    const int PLATFORM_KEY_RALT            = 23;
    const int PLATFORM_KEY_TAB             = 24;
    const int PLATFORM_KEY_ENTER           = 25;
    const int PLATFORM_KEY_BACKSPACE       = 26;
    const int PLATFORM_KEY_INSERT          = 27;
    const int PLATFORM_KEY_DEL             = 28;
    const int PLATFORM_KEY_PAGEUP          = 29;
    const int PLATFORM_KEY_PAGEDOWN        = 30;
    const int PLATFORM_KEY_HOME            = 31;
    const int PLATFORM_KEY_END             = 32;
    const int PLATFORM_KEY_KP_0            = 33;
    const int PLATFORM_KEY_KP_1            = 34;
    const int PLATFORM_KEY_KP_2            = 35;
    const int PLATFORM_KEY_KP_3            = 36;
    const int PLATFORM_KEY_KP_4            = 37;
    const int PLATFORM_KEY_KP_5            = 38;
    const int PLATFORM_KEY_KP_6            = 39;
    const int PLATFORM_KEY_KP_7            = 40;
    const int PLATFORM_KEY_KP_8            = 41;
    const int PLATFORM_KEY_KP_9            = 42;
    const int PLATFORM_KEY_KP_DIVIDE       = 43;
    const int PLATFORM_KEY_KP_MULTIPLY     = 44;
    const int PLATFORM_KEY_KP_SUBTRACT     = 45;
    const int PLATFORM_KEY_KP_ADD          = 46;
    const int PLATFORM_KEY_KP_DECIMAL      = 47;
    const int PLATFORM_KEY_KP_EQUAL        = 48;
    const int PLATFORM_KEY_KP_ENTER        = 49;
    const int PLATFORM_KEY_KP_NUM_LOCK     = 50;
    const int PLATFORM_KEY_CAPS_LOCK       = 51;
    const int PLATFORM_KEY_SCROLL_LOCK     = 52;
    const int PLATFORM_KEY_PAUSE           = 53;
    const int PLATFORM_KEY_LSUPER          = 54;
    const int PLATFORM_KEY_RSUPER          = 55;
    const int PLATFORM_KEY_MENU            = 56;
    const int PLATFORM_KEY_BACK            = 57;

    const int PLATFORM_MOUSE_BUTTON_LEFT   = 0;
    const int PLATFORM_MOUSE_BUTTON_MIDDLE = 1;
    const int PLATFORM_MOUSE_BUTTON_RIGHT  = 2;
    const int PLATFORM_MOUSE_BUTTON_1      = 3;
    const int PLATFORM_MOUSE_BUTTON_2      = 4;
    const int PLATFORM_MOUSE_BUTTON_3      = 5;
    const int PLATFORM_MOUSE_BUTTON_4      = 6;
    const int PLATFORM_MOUSE_BUTTON_5      = 7;
    const int PLATFORM_MOUSE_BUTTON_6      = 8;
    const int PLATFORM_MOUSE_BUTTON_7      = 9;
    const int PLATFORM_MOUSE_BUTTON_8      = 10;

    const int PLATFORM_JOYSTICK_1          = 0;
    const int PLATFORM_JOYSTICK_2          = 1;
    const int PLATFORM_JOYSTICK_3          = 2;
    const int PLATFORM_JOYSTICK_4          = 3;
    const int PLATFORM_JOYSTICK_5          = 4;
    const int PLATFORM_JOYSTICK_6          = 5;
    const int PLATFORM_JOYSTICK_7          = 6;
    const int PLATFORM_JOYSTICK_8          = 7;
    const int PLATFORM_JOYSTICK_9          = 8;
    const int PLATFORM_JOYSTICK_10         = 9;
    const int PLATFORM_JOYSTICK_11         = 10;
    const int PLATFORM_JOYSTICK_12         = 11;
    const int PLATFORM_JOYSTICK_13         = 12;
    const int PLATFORM_JOYSTICK_14         = 13;
    const int PLATFORM_JOYSTICK_15         = 14;
    const int PLATFORM_JOYSTICK_16         = 15;
}
