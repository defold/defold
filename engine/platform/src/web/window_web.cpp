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

#include <EGL/egl.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "platform_window_opengl.h"
#include "window.hpp"
#include "window_mobile_web_constants.h"
#include "window_touch.h"

// Imports implemented by library_platform.js.
extern "C"
{
    int   dmNativeInitJS();
    void  dmNativeTerminate();
    int   dmNativeOpenWindowJS(int alpha_bits, int samples, int fullscreen, int high_dpi, int use_gl, int version);
    void  dmNativeCloseWindow();
    void  dmNativeSetWindowTitle(const char* title);
    void  dmNativeGetWindowSize(int* width, int* height);
    void  dmNativeSetWindowSize(int width, int height);
    int   dmNativeGetWindowParam(WindowState state);
    void  dmNativeSwapBuffers();
    float dmNativeGetDisplayScaleFactor();
    float dmNativeGetDisplayDensity();
    void  dmNativeSetWindowSizeCallback(void (*callback)(int, int));
    void  dmNativeSetWindowFocusCallback(void (*callback)(int));
    int   dmNativeSetCharCallback(void (*callback)(int, int));
    int   dmNativeSetGamepadCallback(void (*callback)(int, int));
    int   dmNativeSetTouchCallback(void (*callback)(int, int, int, int));
    int   dmNativeGetKey(int key);
    int   dmNativeGetMouseButton(int button);
    int   dmNativeIsMouseLeftButtonFromTouch();
    int   dmNativeIsMousePositionFromTouch();
    void  dmNativeGetMousePos(int* x, int* y);
    int   dmNativeGetMouseWheel();
    void  dmNativeSetCursorVisible(int visible);
    int   dmNativeGetMouseLocked();
    void  dmNativeShowKeyboard(int show);
    int   dmNativeGetJoystickParam(int slot, int param);
    int   dmNativeGetJoystickPos(int slot, float* values, int capacity);
    int   dmNativeGetJoystickButtons(int slot, uint8_t* values, int capacity);
    int   dmNativeGetJoystickDeviceId(int slot, char** id);
    int   dmNativeGetJoystickDeviceGuid(int slot, char** guid);
}

struct dmWindow
{
    WindowCreateParams             m_Params;
    FWindowAddKeyboardCharCallback m_CharCallback;
    void*                          m_CharUserData;
    FWindowGamepadEventCallback    m_GamepadCallback;
    void*                          m_GamepadUserData;
    NativeTouch                    m_Touches[NATIVE_MAX_TOUCH];
    int                            m_Width, m_Height;
    bool                           m_Opened;
};

namespace dmPlatform
{
    // DOM callbacks refer to the application's single canvas.
    static HWindow g_Window;

    static NativeTouch* FindTouch(int id)
    {
        NativeTouch* free_touch = 0;
        for (int i = 0; i != NATIVE_MAX_TOUCH; i++)
        {
            if (g_Window->m_Touches[i].Reference != 0 && g_Window->m_Touches[i].Id == id)
            {
                return &g_Window->m_Touches[i];
            }

            if (free_touch == 0 && g_Window->m_Touches[i].Reference == 0)
            {
                free_touch = &g_Window->m_Touches[i];
            }
        }

        if (free_touch != 0)
        {
            free_touch->Reference = free_touch;
        }

        return free_touch;
    }

    static void UpdateTouch(NativeTouch* touch, int x, int y, int phase)
    {
        // Ignore updates received before a touch begins.
        if (touch->Phase == NATIVE_PHASE_IDLE)
        {
            touch->Reference = 0;
            return;
        }

        int previous_phase = touch->Phase;
        int new_phase = phase;
        if (phase == NATIVE_PHASE_CANCELLED)
        {
            new_phase = NATIVE_PHASE_ENDED;
        }

        // If previous phase was TAPPED, we need to return early since we currently cannot buffer actions/phases.
        if (previous_phase == NATIVE_PHASE_TAPPED)
        {
            return;
        }

        // A move after a release belongs to no active touch.
        if (previous_phase == NATIVE_PHASE_ENDED && new_phase == NATIVE_PHASE_MOVED)
        {
            return;
        }

        // Accumulate until read, since browsers can send multiple updates with
        // identical positions before the next frame consumes the touch (#10264).
        touch->DX += x - touch->X;
        touch->DY += y - touch->Y;
        touch->X = x;
        touch->Y = y;

        // If a touch begins and moves in one frame,
        // just update the coordinates but leave the phase as began.
        if (previous_phase == NATIVE_PHASE_BEGAN && new_phase == NATIVE_PHASE_MOVED)
        {
            return;
        }

        // Report a same-frame tap as a press, then a release on the following read.
        if (previous_phase == NATIVE_PHASE_BEGAN && new_phase == NATIVE_PHASE_ENDED)
        {
            touch->Phase = NATIVE_PHASE_TAPPED;
            return;
        }

        touch->Phase = phase;
    }

    static void StartTouch(NativeTouch* touch, int id, int x, int y)
    {
        if (touch->Phase != NATIVE_PHASE_IDLE)
        {
            return;
        }

        touch->Phase = NATIVE_PHASE_BEGAN;
        touch->Id = id;
        touch->X = x;
        touch->Y = y;
        touch->DX = 0;
        touch->DY = 0;
    }

    static void OnTouch(int id, int x, int y, int phase)
    {
        NativeTouch* touch = FindTouch(id);
        if (touch != 0)
        {
            if (phase == NATIVE_PHASE_BEGAN)
            {
                StartTouch(touch, id, x, y);
            }
            else
            {
                UpdateTouch(touch, x, y, phase);
            }
        }
    }

    static void OnResize(int width, int height)
    {
        g_Window->m_Width = width;
        g_Window->m_Height = height;
        const WindowCreateParams& params = g_Window->m_Params;
        if (g_Window->m_Opened && params.m_ResizeCallback)
        {
            params.m_ResizeCallback(params.m_ResizeCallbackUserData, width, height);
        }
    }

    static void OnFocus(int focused)
    {
        const WindowCreateParams& params = g_Window->m_Params;
        if (g_Window->m_Opened && params.m_FocusCallback)
        {
            params.m_FocusCallback(params.m_FocusCallbackUserData, focused);
        }
    }

    static void OnCharacter(int codepoint, int)
    {
        if (g_Window->m_Opened && g_Window->m_CharCallback)
        {
            g_Window->m_CharCallback(g_Window->m_CharUserData, codepoint);
        }
    }

    static void OnGamepad(int slot, int connected)
    {
        if (g_Window->m_Opened && g_Window->m_GamepadCallback)
        {
            g_Window->m_GamepadCallback(g_Window->m_GamepadUserData, slot, connected ? WINDOW_GAMEPAD_EVENT_CONNECTED : WINDOW_GAMEPAD_EVENT_DISCONNECTED);
        }
    }

    HWindow NewWindow()
    {
        if (g_Window || !dmNativeInitJS())
        {
            return 0;
        }

        g_Window = new dmWindow();
        dmNativeResetTouches(g_Window->m_Touches);
        dmNativeSetTouchCallback(OnTouch);
        dmNativeSetWindowSizeCallback(OnResize);
        return g_Window;
    }

    WindowResult OpenWindow(HWindow window, const WindowCreateParams& params)
    {
        if (window->m_Opened)
        {
            return WINDOW_RESULT_WINDOW_ALREADY_OPENED;
        }

        dmNativeResetTouches(window->m_Touches);
        bool use_gl = params.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGL || params.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGLES;
        if (!dmNativeOpenWindowJS(params.m_ContextAlphabits, params.m_Samples, params.m_Fullscreen, params.m_HighDPI, use_gl, params.m_GraphicsApiVersionHint))
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        dmNativeGetWindowSize(&window->m_Width, &window->m_Height);
        dmNativeSetWindowFocusCallback(OnFocus);
        dmNativeSetCharCallback(OnCharacter);
        window->m_Params = params;
        window->m_Opened = true;
        dmNativeSetGamepadCallback(OnGamepad);
        return WINDOW_RESULT_OK;
    }

    void CloseWindow(HWindow window)
    {
        dmNativeCloseWindow();
        window->m_Opened = false;
    }

    void DeleteWindow(HWindow window)
    {
        dmNativeTerminate();
        g_Window = 0;
        delete window;
    }

    void SetWindowTitle(HWindow, const char* title)
    {
        dmNativeSetWindowTitle(title);
    }

    void SetWindowSize(HWindow, uint32_t width, uint32_t height)
    {
        dmNativeSetWindowSize(width, height);
    }

    uint32_t GetWindowWidth(HWindow window)
    {
        return window->m_Width;
    }

    uint32_t GetWindowHeight(HWindow window)
    {
        return window->m_Height;
    }

    bool GetWindowClientSize(HWindow window, uint32_t* width, uint32_t* height)
    {
        if (!GetWindowStateParam(window, WINDOW_STATE_OPENED))
        {
            return false;
        }

        *width = window->m_Width;
        *height = window->m_Height;
        return true;
    }

    bool IsWindowResizeSupported(HWindow)
    {
        return false;
    }

    NativeWindowId GetNativeWindowId(HWindow)
    {
        NativeWindowId identity = {};
        return identity;
    }

    void SetWindowPosition(HWindow, int32_t, int32_t)
    {
    }

    void ShowWindow(HWindow)
    {
    }

    void HideWindow(HWindow)
    {
    }

    void IconifyWindow(HWindow)
    {
    }

    bool GetSafeArea(HWindow window, WindowSafeArea* out)
    {
        WindowSafeArea area = {};
        area.m_Width = window->m_Width;
        area.m_Height = window->m_Height;
        *out = area;
        return true;
    }

    void GetReservedAreas(HWindow, dmArray<WindowReservedArea>& areas)
    {
        areas.SetSize(0);
    }

    uint32_t GetReservedAreas(HWindow, const WindowReservedArea** areas)
    {
        *areas = 0;
        return 0;
    }

    uint32_t GetWindowStateParam(HWindow window, WindowState state)
    {
        switch (state)
        {
            case WINDOW_STATE_SAMPLE_COUNT:
            {
                return window->m_Params.m_Samples;
            }
            case WINDOW_STATE_HIGH_DPI:
            {
                return window->m_Params.m_HighDPI;
            }
            default:
            {
                return window->m_Opened ? dmNativeGetWindowParam(state) : 0;
            }
        }
    }

    float GetDisplayScaleFactor(HWindow)
    {
        return dmNativeGetDisplayScaleFactor();
    }

    float GetDisplayDensity(HWindow)
    {
        return dmNativeGetDisplayDensity();
    }

    uintptr_t GetProcAddress(HWindow, const char* name)
    {
        return (uintptr_t)eglGetProcAddress(name);
    }

    void PollEvents(HWindow)
    {
        // DOM event handlers update input as events arrive.
    }

    void SwapBuffers(HWindow window)
    {
        if (window->m_Opened)
        {
            dmNativeSwapBuffers();
        }
    }

    void SetSwapInterval(HWindow, uint32_t)
    {
    }

    void* AcquireAuxContext(HWindow)
    {
        return 0;
    }

    void UnacquireAuxContext(HWindow, void*)
    {
    }

    int32_t OpenGLGetDefaultFramebufferId()
    {
        return 0;
    }

    const char** VulkanGetRequiredInstanceExtensions(uint32_t* count)
    {
        *count = 0;
        return 0;
    }

    uint32_t GetTouchData(HWindow window, WindowTouchData* data, uint32_t capacity)
    {
        return dmNativeReadTouches(window->m_Touches, data, capacity, 0);
    }

    bool GetAcceleration(HWindow, float*, float*, float*)
    {
        return false;
    }

    int32_t GetKey(HWindow, int32_t key)
    {
        return dmNativeGetKey(key);
    }

    int32_t GetMouseButton(HWindow, int32_t button)
    {
        return dmNativeGetMouseButton(button);
    }

    bool IsMouseLeftButtonFromTouch(HWindow)
    {
        return dmNativeIsMouseLeftButtonFromTouch();
    }

    bool IsMousePositionFromTouch(HWindow)
    {
        return dmNativeIsMousePositionFromTouch();
    }

    int32_t GetMouseWheel(HWindow)
    {
        return dmNativeGetMouseWheel();
    }

    void GetMousePosition(HWindow, int32_t* x, int32_t* y)
    {
        dmNativeGetMousePos(x, y);
    }

    void SetDeviceState(HWindow window, WindowDeviceState state, bool enable)
    {
        SetDeviceState(window, state, enable, false);
    }

    void SetDeviceState(HWindow, WindowDeviceState state, bool enable, bool)
    {
        switch (state)
        {
            case WINDOW_DEVICE_STATE_CURSOR:
            {
                dmNativeSetCursorVisible(enable);
                break;
            }
            case WINDOW_DEVICE_STATE_KEYBOARD_DEFAULT:
            case WINDOW_DEVICE_STATE_KEYBOARD_NUMBER_PAD:
            case WINDOW_DEVICE_STATE_KEYBOARD_EMAIL:
            case WINDOW_DEVICE_STATE_KEYBOARD_PASSWORD:
            {
                dmNativeShowKeyboard(enable);
                break;
            }
            default:
            {
                break;
            }
        }
    }

    bool GetDeviceState(HWindow window, WindowDeviceState state)
    {
        return GetDeviceState(window, state, 0);
    }

    bool GetDeviceState(HWindow, WindowDeviceState state, int32_t index)
    {
        switch (state)
        {
            case WINDOW_DEVICE_STATE_CURSOR_LOCK:
            {
                return dmNativeGetMouseLocked();
            }
            case WINDOW_DEVICE_STATE_JOYSTICK_PRESENT:
            {
                return dmNativeGetJoystickParam(index, NATIVE_PRESENT);
            }
            default:
            {
                dmLogWarning("Unable to get device state (%d), unknown state.", (int)state);
                return false;
            }
        }
    }

    const char* GetJoystickDeviceName(HWindow, uint32_t index)
    {
        char* name;
        dmNativeGetJoystickDeviceId(index, &name);
        return name;
    }

    const char* GetJoystickDeviceGuid(HWindow, uint32_t index)
    {
        char* guid;
        dmNativeGetJoystickDeviceGuid(index, &guid);
        return guid;
    }

    uint32_t GetJoystickAxes(HWindow, uint32_t index, float* values, uint32_t capacity)
    {
        uint32_t count = dmMath::Min((uint32_t)dmNativeGetJoystickParam(index, NATIVE_AXES), capacity);
        return dmNativeGetJoystickPos(index, values, count);
    }

    uint32_t GetJoystickHats(HWindow, uint32_t, uint8_t*, uint32_t)
    {
        return 0;
    }

    uint32_t GetJoystickButtons(HWindow, uint32_t index, uint8_t* values, uint32_t capacity)
    {
        uint32_t count = dmMath::Min((uint32_t)dmNativeGetJoystickParam(index, NATIVE_BUTTONS), capacity);
        return dmNativeGetJoystickButtons(index, values, count);
    }

    int32_t TriggerCloseCallback(HWindow window)
    {
        const WindowCreateParams& params = window->m_Params;
        return params.m_CloseCallback ? params.m_CloseCallback(params.m_CloseCallbackUserData) : 0;
    }

    void SetKeyboardCharCallback(HWindow window, FWindowAddKeyboardCharCallback cb, void* data)
    {
        window->m_CharCallback = cb;
        window->m_CharUserData = data;
    }

    void SetKeyboardMarkedTextCallback(HWindow, FWindowSetMarkedTextCallback, void*)
    {
    }

    void SetKeyboardDeviceChangedCallback(HWindow, FWindowDeviceChangedCallback, void*)
    {
    }

    void SetGamepadEventCallback(HWindow window, FWindowGamepadEventCallback cb, void* data)
    {
        window->m_GamepadCallback = cb;
        window->m_GamepadUserData = data;
    }

}
