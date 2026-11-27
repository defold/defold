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
#include <stdlib.h>

#include "window_mobile_web_backend.h"

#include <dlib/platform.h>
#include <dlib/log.h>
#include <dlib/array.h>
#include <dlib/math.h>

#include "window.hpp"
#include "platform_window_opengl.h"

#if defined(ANDROID)
#include "platform_window_android.h"
#elif defined(DM_PLATFORM_IOS)
#include "platform_window_ios.h"
#endif

struct dmWindow
{
    FWindowResizeCallback          m_ResizeCallback;
    void*                          m_ResizeCallbackUserData;
    FWindowCloseCallback           m_CloseCallback;
    void*                          m_CloseCallbackUserData;
    FWindowFocusCallback           m_FocusCallback;
    void*                          m_FocusCallbackUserData;
    FWindowIconifyCallback         m_IconifyCallback;
    void*                          m_IconifyCallbackUserData;
    FWindowAddKeyboardCharCallback m_AddKeyboarCharCallBack;
    void*                          m_AddKeyboarCharCallBackUserData;
    FWindowSetMarkedTextCallback   m_SetMarkedTextCallback;
    void*                          m_SetMarkedTextCallbackUserData;
    FWindowDeviceChangedCallback   m_DeviceChangedCallback;
    void*                          m_DeviceChangedCallbackUserData;
    FWindowGamepadEventCallback    m_GamepadEventCallback;
    void*                          m_GamepadEventCallbackUserData;
    dmArray<WindowReservedArea>    m_ReservedAreas;
    dmArray<WindowReservedArea>    m_PendingReservedAreas;
    int32_t                        m_Width;
    int32_t                        m_Height;
    uint32_t                       m_Samples               : 8;
    uint32_t                       m_WindowOpened          : 1;
    uint32_t                       m_SwapIntervalSupported : 1;
    uint32_t                       m_SwapBufferSupported   : 1;
    uint32_t                       m_HighDPI               : 1;
    uint32_t                       m_ObserveReservedAreas  : 1;
};

namespace dmPlatform
{
    bool GetWindowClientSize(HWindow window, uint32_t* width, uint32_t* height)
    {
        if (!GetWindowStateParam(window, WINDOW_STATE_OPENED))
        {
            return false;
        }

        // The native setter and size query use the same units on these backends.
        *width = GetWindowWidth(window);
        *height = GetWindowHeight(window);
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

    // Mobile and web backends own one application window.
    static dmWindow* g_Window = 0;

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

        if (g_Window->m_ResizeCallback != 0x0)
        {
            g_Window->m_ResizeCallback(g_Window->m_ResizeCallbackUserData, (uint32_t)width, (uint32_t)height);
        }
    }

    static int OnWindowClose()
    {
        assert(g_Window);
        if (g_Window->m_CloseCallback != 0x0)
        {
            return g_Window->m_CloseCallback(g_Window->m_CloseCallbackUserData);
        }

        // Close by default
        return 1;
    }

    static void OnWindowFocus(int focus)
    {
        assert(g_Window);
        if (g_Window->m_FocusCallback != 0x0)
        {
            g_Window->m_FocusCallback(g_Window->m_FocusCallbackUserData, focus);
        }
    }

    static void OnWindowIconify(int iconify)
    {
        assert(g_Window);
        if (g_Window->m_IconifyCallback != 0x0)
        {
            g_Window->m_IconifyCallback(g_Window->m_IconifyCallbackUserData, iconify);
        }
    }

    static void OnAddCharacterCallback(int chr, int _)
    {
        if (g_Window->m_AddKeyboarCharCallBack)
        {
            g_Window->m_AddKeyboarCharCallBack(g_Window->m_AddKeyboarCharCallBackUserData, chr);
        }
    }

    static void OnMarkedTextCallback(char* text)
    {
        if (g_Window->m_SetMarkedTextCallback)
        {
            g_Window->m_SetMarkedTextCallback(g_Window->m_SetMarkedTextCallbackUserData, text);
        }
    }

    static void OnDeviceChangedCallback(int status)
    {
        if (g_Window->m_DeviceChangedCallback)
        {
            g_Window->m_DeviceChangedCallback(g_Window->m_DeviceChangedCallbackUserData, status);
        }
    }

    static void OnGamepad(int gamepad_id, int connected)
    {
        if (g_Window->m_GamepadEventCallback)
        {
            g_Window->m_GamepadEventCallback(g_Window->m_GamepadEventCallbackUserData, gamepad_id, connected ? WINDOW_GAMEPAD_EVENT_CONNECTED : WINDOW_GAMEPAD_EVENT_DISCONNECTED);
        }
    }

    HWindow NewWindow()
    {
        if (g_Window == 0)
        {
            dmWindow* wnd = new dmWindow;
            memset(wnd, 0, sizeof(dmWindow));

            if (!dmNativeInit())
            {
                dmLogError("Could not initialize the native platform.");
                delete wnd;
                return 0;
            }

            g_Window = wnd;

            return wnd;
        }

        return 0;
    }

    WindowResult OpenWindow(HWindow window, const WindowCreateParams& params)
    {
        if (window->m_WindowOpened)
        {
            return WINDOW_RESULT_WINDOW_ALREADY_OPENED;
        }

        WindowResult res = dmNativeOpenWindow(&params) ? WINDOW_RESULT_OK : WINDOW_RESULT_WINDOW_OPEN_ERROR;
        bool opengl = params.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGL || params.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGLES;
        window->m_SwapIntervalSupported = opengl;
        window->m_SwapBufferSupported = 1;

        if (res == WINDOW_RESULT_OK)
        {
            dmNativeSetWindowBackgroundColor(params.m_BackgroundColor);
            dmNativeSetWindowSizeCallback(OnWindowResize);
            dmNativeSetWindowCloseCallback(OnWindowClose);
            dmNativeSetWindowFocusCallback(OnWindowFocus);
            dmNativeSetWindowIconifyCallback(OnWindowIconify);
            dmNativeSetGamepadCallback(OnGamepad);
            dmNativeSwapInterval(1);
            dmNativeGetWindowSize(&window->m_Width, &window->m_Height);

        #if !defined(__EMSCRIPTEN__)
            dmNativeSetWindowTitle(params.m_Title);
        #endif

            if (dmNativeSetCharCallback(OnAddCharacterCallback) == 0)
            {
                dmLogFatal("could not set dmNative char callback.");
            }

            if (dmNativeSetMarkedTextCallback(OnMarkedTextCallback) == 0)
            {
                dmLogFatal("could not set dmNative marked text callback.");
            }

            if (dmNativeSetDeviceChangedCallback(OnDeviceChangedCallback) == 0)
            {
                dmLogFatal("coult not set dmNative gamepad connection callback.");
            }

            // These callback pointers are set on the window AFTER the dmNative callbacks have been set,
            // This is to make sure dmNative don't call any of the callbacks before everything has been setup in the engine
            window->m_ResizeCallback          = params.m_ResizeCallback;
            window->m_ResizeCallbackUserData  = params.m_ResizeCallbackUserData;
            window->m_CloseCallback           = params.m_CloseCallback;
            window->m_CloseCallbackUserData   = params.m_CloseCallbackUserData;
            window->m_FocusCallback           = params.m_FocusCallback;
            window->m_FocusCallbackUserData   = params.m_FocusCallbackUserData;
            window->m_IconifyCallback         = params.m_IconifyCallback;
            window->m_IconifyCallbackUserData = params.m_IconifyCallbackUserData;
            window->m_HighDPI                 = params.m_HighDPI;
            window->m_Samples                 = params.m_Samples;

            window->m_WindowOpened = 1;
        }

        return res;
    }

    void CloseWindow(HWindow window)
    {
        dmNativeCloseWindow();
        window->m_WindowOpened = 0;
        window->m_ObserveReservedAreas = 0;
        window->m_ReservedAreas.SetSize(0);
        window->m_PendingReservedAreas.SetSize(0);
    }

    void DeleteWindow(HWindow window)
    {
        delete window;
        g_Window = 0;

        dmNativeTerminate();
    }

    void SetWindowTitle(HWindow window, const char* title)
    {
        dmNativeSetWindowTitle(title);
    }

    void SetWindowSize(HWindow window, uint32_t width, uint32_t height)
    {
        dmNativeSetWindowSize((int)width, (int)height);
        int window_width, window_height;
        dmNativeGetWindowSize(&window_width, &window_height);
        window->m_Width  = window_width;
        window->m_Height = window_height;

        // The callback is not called from dmNative when the size is set manually
        if (window->m_ResizeCallback)
        {
            window->m_ResizeCallback(window->m_ResizeCallbackUserData, window_width, window_height);
        }
    }

    void SetWindowPosition(HWindow window, int32_t x, int32_t y)
    {
        dmNativeSetWindowPos(x, y);
    }

    uint32_t GetWindowWidth(HWindow window)
    {
        return (uint32_t) window->m_Width;
    }

    uint32_t GetWindowHeight(HWindow window)
    {
        return (uint32_t) window->m_Height;
    }

    static void SetSafeAreaFull(HWindow window, WindowSafeArea* out)
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
    }

    bool GetSafeArea(HWindow window, WindowSafeArea* out)
    {
        SetSafeAreaFull(window, out);

#if defined(ANDROID)
        if (GetSafeAreaAndroid(window, out))
        {
            return true;
        }

#elif defined(DM_PLATFORM_IOS)
        if (GetSafeAreaiOS(window, out))
        {
            return true;
        }

#endif

        return true;
    }

#if defined(__EMSCRIPTEN__)
    void GetReservedAreas(HWindow window, dmArray<WindowReservedArea>& areas)
    {
        areas.SetSize(0);
    }

#endif

    uint32_t GetReservedAreas(HWindow window, const WindowReservedArea** areas)
    {
        if (!window->m_ObserveReservedAreas)
        {
            window->m_ObserveReservedAreas = 1;
            UpdateReservedAreas(window);
        }

        *areas = window->m_ReservedAreas.Empty() ? 0 : window->m_ReservedAreas.Begin();
        return window->m_ReservedAreas.Size();
    }

    #ifndef __EMSCRIPTEN__
        #define NATIVE_AUX_CONTEXT_SUPPORTED
    #endif

    static inline int32_t QueryAuxContextImpl()
    {
    #if defined(NATIVE_AUX_CONTEXT_SUPPORTED)
        return dmNativeQueryAuxContext();
    #else
        return 0;
    #endif
    }

    void* AcquireAuxContext(HWindow window)
    {
    #if defined(NATIVE_AUX_CONTEXT_SUPPORTED)
        return dmNativeAcquireAuxContext();
    #else
        return 0;
    #endif
    }

    void UnacquireAuxContext(HWindow window, void* aux_context)
    {
    #if defined(NATIVE_AUX_CONTEXT_SUPPORTED)
        dmNativeUnacquireAuxContext(aux_context);
    #endif
    }

    #undef NATIVE_AUX_CONTEXT_SUPPORTED

    uint32_t GetWindowStateParam(HWindow window, WindowState state)
    {
        switch (state)
        {
            case WINDOW_STATE_REFRESH_RATE:
            {
                return dmNativeGetWindowRefreshRate();
            }
            case WINDOW_STATE_SAMPLE_COUNT:
            {
                return window->m_Samples;
            }
            case WINDOW_STATE_HIGH_DPI:
            {
                return window->m_HighDPI;
            }
            case WINDOW_STATE_AUX_CONTEXT:
            {
                return QueryAuxContextImpl();
            }
            default:
            {
                break;
            }
        }

        return window->m_WindowOpened ? dmNativeGetWindowParam(state) : 0;
    }

    void IconifyWindow(HWindow window)
    {
        if (window->m_WindowOpened)
        {
            dmNativeIconifyWindow();
        }
    }

    float GetDisplayScaleFactor(HWindow window)
    {
        return dmNativeGetDisplayScaleFactor();
    }

    float GetDisplayDensity(HWindow window)
    {
        (void) window;
        return dmNativeGetDisplayDensity();
    }

    uintptr_t GetProcAddress(HWindow window, const char* proc_name)
    {
        return (uintptr_t) dmNativeGetProcAddress(proc_name);
    }

    void SetSwapInterval(HWindow window, uint32_t swap_interval)
    {
        if (window->m_SwapIntervalSupported)
        {
            dmNativeSwapInterval(swap_interval);
        }
    }

    bool GetAcceleration(HWindow window, float* x, float* y, float* z)
    {
        return dmNativeGetAcceleration(x, y, z);
    }

    uint32_t GetTouchData(HWindow window, WindowTouchData* touch_data, uint32_t touch_data_count)
    {
        return dmNativeGetTouch(touch_data, touch_data_count);
    }

    int32_t GetKey(HWindow window, int32_t code)
    {
        return dmNativeGetKey(code);
    }

    int32_t GetMouseButton(HWindow window, int32_t button)
    {
        return dmNativeGetMouseButton(button);
    }

    bool IsMouseLeftButtonFromTouch(HWindow window)
    {
        return dmNativeIsMouseLeftButtonFromTouch();
    }

    bool IsMousePositionFromTouch(HWindow window)
    {
        return dmNativeIsMousePositionFromTouch();
    }

    int32_t GetMouseWheel(HWindow window)
    {
        return dmNativeGetMouseWheel();
    }

    void GetMousePosition(HWindow window, int32_t* x, int32_t* y)
    {
        dmNativeGetMousePos(x, y);
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
                if (op1)
                {
                    dmNativeSetCursorVisible(1);
                }
                else
                {
                    dmNativeSetCursorVisible(0);
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
                return dmNativeGetMouseLocked();
            }
            case WINDOW_DEVICE_STATE_JOYSTICK_PRESENT:
            {
                return dmNativeGetJoystickParam(op1, NATIVE_PRESENT);
            }
            default:
            {
                break;
            }
        }

        dmLogWarning("Unable to get device state (%d), unknown state.", (int) state);
        return false;
    }

    const char* GetJoystickDeviceName(HWindow window, uint32_t joystick_index)
    {
        char* device_name;
        dmNativeGetJoystickDeviceId(joystick_index, &device_name);
        return (const char*) device_name;
    }

    const char* GetJoystickDeviceGuid(HWindow window, uint32_t joystick_index)
    {
#if defined(__EMSCRIPTEN__) || defined(ANDROID)
        char* device_guid = 0;
        if (dmNativeGetJoystickDeviceGuid(joystick_index, &device_guid)) // Defold addition
        {
            return (const char*) device_guid;
        }

#endif
        return 0; // unsupported
    }

    uint32_t GetJoystickAxes(HWindow window, uint32_t joystick_index, float* values, uint32_t values_capacity)
    {
        uint32_t count = dmMath::Min((uint32_t)dmNativeGetJoystickParam(joystick_index, NATIVE_AXES), values_capacity);
        return dmNativeGetJoystickPos(joystick_index, values, count);
    }

    uint32_t GetJoystickHats(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity)
    {
        uint32_t count = dmMath::Min((uint32_t)dmNativeGetJoystickParam(joystick_index, NATIVE_HATS), values_capacity);
        return dmNativeGetJoystickHats(joystick_index, values, count);
    }

    uint32_t GetJoystickButtons(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity)
    {
        uint32_t count = dmMath::Min((uint32_t)dmNativeGetJoystickParam(joystick_index, NATIVE_BUTTONS), values_capacity);
        return dmNativeGetJoystickButtons(joystick_index, values, count);
    }

    int32_t TriggerCloseCallback(HWindow window)
    {
        if (window->m_CloseCallback)
        {
            return window->m_CloseCallback(window->m_CloseCallbackUserData);
        }

        return 0;
    }

    void ShowWindow(HWindow window)
    {
    }

    void HideWindow(HWindow window)
    {
    }

    void PollEvents(HWindow window)
    {
        // Poll events independently of buffer swaps. Accessing OpenGL isn't permitted
        // on iOS when the application is transitioning to resumed mode.
        dmNativePollEvents();
#if defined(DM_PLATFORM_IOS)
        // UIKit occlusions can change independently of size and safe-area insets.
        // Only observe after the first query. Resize callbacks above already
        // refresh the snapshot, so a transition produces no duplicate event here.
        if (window->m_ObserveReservedAreas && UpdateReservedAreas(window) && window->m_ResizeCallback)
        {
            window->m_ResizeCallback(window->m_ResizeCallbackUserData, window->m_Width, window->m_Height);
        }

#endif
    }

    void SwapBuffers(HWindow window)
    {
        if (window->m_SwapBufferSupported)
        {
            dmNativeSwapBuffers();
        }
    }

    void SetKeyboardCharCallback(HWindow window, FWindowAddKeyboardCharCallback cb, void* user_data)
    {
        window->m_AddKeyboarCharCallBack         = cb;
        window->m_AddKeyboarCharCallBackUserData = user_data;
    }

    void SetKeyboardMarkedTextCallback(HWindow window, FWindowSetMarkedTextCallback cb, void* user_data)
    {
        window->m_SetMarkedTextCallback         = cb;
        window->m_SetMarkedTextCallbackUserData = user_data;
    }

    void SetKeyboardDeviceChangedCallback(HWindow window, FWindowDeviceChangedCallback cb, void* user_data)
    {
        window->m_DeviceChangedCallback         = cb;
        window->m_DeviceChangedCallbackUserData = user_data;
    }

    void SetGamepadEventCallback(HWindow window, FWindowGamepadEventCallback cb, void* user_data)
    {
        window->m_GamepadEventCallback         = cb;
        window->m_GamepadEventCallbackUserData = user_data;
    }

    const char** VulkanGetRequiredInstanceExtensions(uint32_t* count)
    {
        *count = 0;
        return 0;
    }

    int32_t OpenGLGetDefaultFramebufferId()
    {
        return dmNativeGetDefaultFramebuffer();
    }
}
