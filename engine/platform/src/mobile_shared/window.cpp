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

#include <assert.h>
#include <limits.h>

#include <dlib/log.h>

#include "window.hpp"
#include "window_mobile_private.h"
#include "platform_window_opengl.h"

#if defined(ANDROID)
#include "platform_window_android.h"
#else
#include "platform_window_ios.h"
#endif

struct dmWindow
{
    WindowCreateParams          m_Params;
    MobileInputCallbacks        m_Callbacks;
    dmArray<WindowReservedArea> m_ReservedAreas;
    dmArray<WindowReservedArea> m_PendingReservedAreas;
    uint32_t                    m_Width, m_Height;
    bool                        m_ObserveReservedAreas;
};

namespace dmPlatform
{
    // Native resize notifications refer to the application's single window.
    static HWindow g_Window;

    static int CompareReservedAreas(const void* lhs, const void* rhs)
    {
        const WindowReservedArea* a = (const WindowReservedArea*)lhs;
        const WindowReservedArea* b = (const WindowReservedArea*)rhs;
        if (a->m_X != b->m_X)
        {
            return a->m_X < b->m_X ? -1 : 1;
        }

        if (a->m_Y != b->m_Y)
        {
            return a->m_Y < b->m_Y ? -1 : 1;
        }

        if (a->m_Width != b->m_Width)
        {
            return a->m_Width < b->m_Width ? -1 : 1;
        }

        if (a->m_Height != b->m_Height)
        {
            return a->m_Height < b->m_Height ? -1 : 1;
        }

        return 0;
    }

    static bool UpdateReservedAreas(HWindow window)
    {
        dmArray<WindowReservedArea>& pending = window->m_PendingReservedAreas;
        GetReservedAreas(window, pending);
        if (pending.Size() > 1)
        {
            qsort(pending.Begin(), pending.Size(), sizeof(WindowReservedArea), CompareReservedAreas);
        }

        dmArray<WindowReservedArea>& current = window->m_ReservedAreas;
        if (pending.Size() == current.Size() &&
            (pending.Empty() || memcmp(pending.Begin(), current.Begin(), pending.Size() * sizeof(WindowReservedArea)) == 0))
        {
            return false;
        }

        current.Swap(pending);
        return true;
    }

    static void OnWindowResize(int width, int height)
    {
        assert(g_Window);
        g_Window->m_Width  = (uint32_t) width;
        g_Window->m_Height = (uint32_t) height;

        if (g_Window->m_ObserveReservedAreas)
        {
            UpdateReservedAreas(g_Window);
        }

        if (g_Window->m_Params.m_ResizeCallback)
        {
            g_Window->m_Params.m_ResizeCallback(g_Window->m_Params.m_ResizeCallbackUserData, (uint32_t)width, (uint32_t)height);
        }
    }

    HWindow NewWindow()
    {
        if (g_Window)
        {
            return 0;
        }

        if (!dmNativeInit())
        {
            dmLogError("Could not initialize the native platform.");
            return 0;
        }

        g_Window = new dmWindow();
        return g_Window;
    }

    WindowResult OpenWindow(HWindow window, const WindowCreateParams& params)
    {
        if (dmNativeWin.opened)
        {
            return WINDOW_RESULT_WINDOW_ALREADY_OPENED;
        }

        if (!dmNativeInitialized)
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        // Mobile apps may specify just one dimension when selecting the initial orientation.
        uint64_t width = params.m_Width;
        uint64_t height = params.m_Height;
        if (!width)
        {
            width = height ? height * 4 / 3 : 640;
        }

        if (!height)
        {
            height = width * 3 / 4;
        }

        if (width > INT_MAX || height > INT_MAX)
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        bool use_gl = params.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGL || params.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGLES;
        dmNativeClearInput();
        dmNativeWin.active = 1;
        dmNativeWin.iconified = 0;
        dmNativeWin.width = (int)width;
        dmNativeWin.height = (int)height;
        dmNativeWin.opened = dmNativeOSOpenWindow((int)width, (int)height, use_gl ? NATIVE_OPENGL_API : NATIVE_NO_API);
        if (!dmNativeWin.opened || (use_gl && !dmNativeHasRequiredGLESVersion()))
        {
            dmNativeOSCloseWindow();
            memset(&dmNativeWin, 0, sizeof(dmNativeWin));
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        dmNativeOSSetWindowBackgroundColor(params.m_BackgroundColor);
        dmNativeOSSwapInterval(1);

        // Publish callbacks after native setup, including when reopening for another graphics API.
        window->m_Params = params;
        window->m_Width = dmNativeWin.width;
        window->m_Height = dmNativeWin.height;
        dmNativeWin.windowSizeCallback = OnWindowResize;
        dmNativeWin.windowFocusCallback = params.m_FocusCallback;
        dmNativeWin.windowFocusUserData = params.m_FocusCallbackUserData;
        dmNativeWin.inputCallbacks = &window->m_Callbacks;
        return WINDOW_RESULT_OK;
    }

    void CloseWindow(HWindow window)
    {
        dmNativeCloseWindow();
        window->m_ObserveReservedAreas = false;
        window->m_ReservedAreas.SetSize(0);
        window->m_PendingReservedAreas.SetSize(0);
    }

    void DeleteWindow(HWindow window)
    {
        dmNativeWin.inputCallbacks = 0;
        dmNativeWin.windowSizeCallback = 0;
        dmNativeWin.windowFocusCallback = 0;
        dmNativeTerminate();
        g_Window = 0;
        delete window;
    }

    void SetWindowTitle(HWindow, const char*)
    {
    }

    void SetWindowSize(HWindow window, uint32_t width, uint32_t height)
    {
        if (dmNativeWin.opened)
        {
            if (!dmNativeWin.iconified)
            {
                dmNativeOSSetWindowSize(width, height);
            }

            OnWindowResize(dmNativeWin.width, dmNativeWin.height);
        }
    }

    void SetWindowPosition(HWindow, int32_t, int32_t)
    {
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
        if (!dmNativeWin.opened)
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

    bool GetSafeArea(HWindow window, WindowSafeArea* out)
    {
        WindowSafeArea full = {};
        full.m_Width = window->m_Width;
        full.m_Height = window->m_Height;
        *out = full;
#if defined(ANDROID)
        GetSafeAreaAndroid(window, out);
#else
        GetSafeAreaiOS(window, out);
#endif
        return true;
    }

    uint32_t GetReservedAreas(HWindow window, const WindowReservedArea** areas)
    {
        if (!window->m_ObserveReservedAreas)
        {
            window->m_ObserveReservedAreas = true;
            UpdateReservedAreas(window);
        }

        *areas = window->m_ReservedAreas.Empty() ? 0 : window->m_ReservedAreas.Begin();
        return window->m_ReservedAreas.Size();
    }

    void* AcquireAuxContext(HWindow)
    {
        return dmNativeOSAcquireAuxContext();
    }

    void UnacquireAuxContext(HWindow, void* context)
    {
        dmNativeOSUnacquireAuxContext(context);
    }

    uint32_t GetWindowStateParam(HWindow window, WindowState state)
    {
        switch (state)
        {
            case WINDOW_STATE_OPENED:
            {
                return dmNativeWin.opened;
            }
            case WINDOW_STATE_ACTIVE:
            {
                return dmNativeWin.opened && dmNativeWin.active;
            }
            case WINDOW_STATE_ICONIFIED:
            {
                return dmNativeWin.opened && dmNativeWin.iconified;
            }
            case WINDOW_STATE_REFRESH_RATE:
            {
                return dmNativeWin.opened ? dmNativeOSGetWindowRefreshRate() : 0;
            }
            case WINDOW_STATE_SAMPLE_COUNT:
            {
                return window->m_Params.m_Samples;
            }
            case WINDOW_STATE_HIGH_DPI:
            {
                return window->m_Params.m_HighDPI;
            }
            case WINDOW_STATE_AUX_CONTEXT:
            {
                return dmNativeOSQueryAuxContext();
            }
            default:
            {
                return 0;
            }
        }
    }

    void IconifyWindow(HWindow)
    {
        if (dmNativeWin.opened && !dmNativeWin.iconified)
        {
            dmNativeOSIconifyWindow();
        }
    }

    float GetDisplayScaleFactor(HWindow)
    {
        return 1.0f;
    }

    void SetSwapInterval(HWindow window, uint32_t interval)
    {
        WindowsGraphicsApi api = window->m_Params.m_GraphicsApi;
        if (dmNativeWin.opened && (api == WINDOW_GRAPHICS_API_OPENGL || api == WINDOW_GRAPHICS_API_OPENGLES))
        {
            dmNativeOSSwapInterval(interval);
        }
    }

    bool GetAcceleration(HWindow, float* x, float* y, float* z)
    {
        return dmNativeOSGetAcceleration(x, y, z);
    }

    uint32_t GetTouchData(HWindow, WindowTouchData* data, uint32_t capacity)
    {
        return dmNativeReadTouches(dmNativeInput.Touch, data, capacity, 1);
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
        return dmNativeInput.MouseLeftButtonFromTouch;
    }

    bool IsMousePositionFromTouch(HWindow)
    {
        return dmNativeInput.MousePositionFromTouch;
    }

    int32_t GetMouseWheel(HWindow)
    {
        return 0;
    }

    void GetMousePosition(HWindow, int32_t* x, int32_t* y)
    {
        if (dmNativeWin.opened)
        {
            if (x)
            {
                *x = dmNativeInput.MousePosX;
            }

            if (y)
            {
                *y = dmNativeInput.MousePosY;
            }
        }
    }

    void SetDeviceState(HWindow window, WindowDeviceState state, bool op1)
    {
        SetDeviceState(window, state, op1, false);
    }

    void SetDeviceState(HWindow window, WindowDeviceState state, bool op1, bool op2)
    {
        switch (state)
        {
            case WINDOW_DEVICE_STATE_CURSOR:
            {
                if (dmNativeWin.opened)
                {
                    dmNativeWin.cursorHidden = !op1;
                }

                break;
            }
            case WINDOW_DEVICE_STATE_ACCELEROMETER:
            {
                if (op1)
                {
                    dmNativeAccelerometerEnable();
                }

                break;
            }
            case WINDOW_DEVICE_STATE_KEYBOARD_DEFAULT:
            {
                dmNativeShowKeyboard(op1, NATIVE_KEYBOARD_DEFAULT, op2);
                break;
            }
            case WINDOW_DEVICE_STATE_KEYBOARD_NUMBER_PAD:
            {
                dmNativeShowKeyboard(op1, NATIVE_KEYBOARD_NUMBER_PAD, op2);
                break;
            }
            case WINDOW_DEVICE_STATE_KEYBOARD_EMAIL:
            {
                dmNativeShowKeyboard(op1, NATIVE_KEYBOARD_EMAIL, op2);
                break;
            }
            case WINDOW_DEVICE_STATE_KEYBOARD_PASSWORD:
            {
                dmNativeShowKeyboard(op1, NATIVE_KEYBOARD_PASSWORD, op2);
                break;
            }
            case WINDOW_DEVICE_STATE_KEYBOARD_RESET:
            {
                dmNativeResetKeyboard();
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

    bool GetDeviceState(HWindow window, WindowDeviceState state, int32_t op1)
    {
        switch (state)
        {
            case WINDOW_DEVICE_STATE_CURSOR_LOCK:
            {
                return dmNativeWin.cursorHidden;
            }
            case WINDOW_DEVICE_STATE_JOYSTICK_PRESENT:
            {
#if defined(ANDROID)
                return dmNativeInitialized && dmNativeOSGetJoystickParam(op1, NATIVE_PRESENT);
#else
                return false;
#endif
            }
            default:
            {
                break;
            }
        }

        dmLogWarning("Unable to get device state (%d), unknown state.", (int) state);
        return false;
    }

    int32_t TriggerCloseCallback(HWindow window)
    {
        const WindowCreateParams& params = window->m_Params;
        return params.m_CloseCallback ? params.m_CloseCallback(params.m_CloseCallbackUserData) : 0;
    }

    void ShowWindow(HWindow)
    {
    }

    void HideWindow(HWindow)
    {
    }

    void PollEvents(HWindow window)
    {
        if (dmNativeWin.opened)
        {
            dmNativeOSPollEvents();
        }

#if defined(DM_PLATFORM_IOS)
        // UIKit occlusions can change independently of size and safe-area insets.
        if (g_Window == window && window->m_ObserveReservedAreas && UpdateReservedAreas(window) && window->m_Params.m_ResizeCallback)
        {
            window->m_Params.m_ResizeCallback(window->m_Params.m_ResizeCallbackUserData, window->m_Width, window->m_Height);
        }

#endif
    }

    void SwapBuffers(HWindow)
    {
        if (dmNativeWin.opened)
        {
            dmNativeOSSwapBuffers();
            // Mobile buffer swaps also drain native events.
            if (dmNativeWin.opened)
            {
                dmNativeOSPollEvents();
            }
        }
    }

    void SetKeyboardCharCallback(HWindow window, FWindowAddKeyboardCharCallback cb, void* data)
    {
        window->m_Callbacks.m_CharCallback = cb;
        window->m_Callbacks.m_CharUserData = data;
    }

    void SetKeyboardMarkedTextCallback(HWindow window, FWindowSetMarkedTextCallback cb, void* data)
    {
        window->m_Callbacks.m_MarkedCallback = cb;
        window->m_Callbacks.m_MarkedUserData = data;
    }

    void SetKeyboardDeviceChangedCallback(HWindow, FWindowDeviceChangedCallback, void*)
    {
    }

    void SetGamepadEventCallback(HWindow window, FWindowGamepadEventCallback cb, void* data)
    {
        window->m_Callbacks.m_GamepadCallback = cb;
        window->m_Callbacks.m_GamepadUserData = data;
    }

    const char** VulkanGetRequiredInstanceExtensions(uint32_t* count)
    {
        *count = 0;
        return 0;
    }

    int32_t OpenGLGetDefaultFramebufferId()
    {
        return dmNativeOSGetDefaultFramebuffer();
    }

}
