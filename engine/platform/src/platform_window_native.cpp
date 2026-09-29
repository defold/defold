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

#include "native/native.h"

#include <dlib/platform.h>
#include <dlib/log.h>
#include <dlib/array.h>
#include <dlib/math.h>

#include "window.hpp"
#include "platform_window_constants.h"
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
    dmArray<NativeTouch>             m_TouchData;
    int32_t                        m_Width;
    int32_t                        m_Height;
    uint32_t                       m_Samples               : 8;
    uint32_t                       m_WindowOpened          : 1;
    uint32_t                       m_SwapIntervalSupported : 1;
    uint32_t                       m_SwapBufferSupported   : 1;
    uint32_t                       m_HighDPI               : 1;
};

namespace dmPlatform
{
    // Mobile and web backends own one application window.
    static dmWindow* g_Window = 0;

    static void OnWindowResize(int width, int height)
    {
        assert(g_Window);
        g_Window->m_Width  = (uint32_t) width;
        g_Window->m_Height = (uint32_t) height;

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

            if (dmNativeInit() == GL_FALSE)
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
        switch(state)
        {
            case WINDOW_STATE_REFRESH_RATE: return dmNativeGetWindowRefreshRate();
            case WINDOW_STATE_SAMPLE_COUNT: return window->m_Samples;
            case WINDOW_STATE_HIGH_DPI:     return window->m_HighDPI;
            case WINDOW_STATE_AUX_CONTEXT:  return QueryAuxContextImpl();
            default:break;
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
        return dmNativeGetAcceleration(x,y,z);
    }

    uint32_t GetTouchData(HWindow window, WindowTouchData* touch_data, uint32_t touch_data_count)
    {
        int32_t touch_count = 0;

        if (window->m_TouchData.Capacity() < touch_data_count)
        {
            window->m_TouchData.SetCapacity(touch_data_count);
            window->m_TouchData.SetSize(touch_data_count);
        }

        dmNativeGetTouch(window->m_TouchData.Begin(), touch_data_count, &touch_count);

        for (int i = 0; i < touch_count; ++i)
        {
            touch_data[i].m_TapCount = window->m_TouchData[i].TapCount;
            touch_data[i].m_Phase    = window->m_TouchData[i].Phase;
            touch_data[i].m_X        = window->m_TouchData[i].X;
            touch_data[i].m_Y        = window->m_TouchData[i].Y;
            touch_data[i].m_DX       = window->m_TouchData[i].DX;
            touch_data[i].m_DY       = window->m_TouchData[i].DY;
            touch_data[i].m_Id       = window->m_TouchData[i].Id;
        }

        return (uint32_t) touch_count;
    }

    int32_t GetKey(HWindow window, int32_t code)
    {
         return dmNativeGetKey(code);
    }

    int32_t GetMouseButton(HWindow window, int32_t button)
    {
        return dmNativeGetMouseButton(button);
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
        switch(state)
        {
            case WINDOW_DEVICE_STATE_CURSOR:
                if (op1)
                    dmNativeSetCursorVisible(1);
                else
                    dmNativeSetCursorVisible(0);
                break;
            case WINDOW_DEVICE_STATE_ACCELEROMETER:
                if (op1)
                    dmNativeAccelerometerEnable();
                break;
            case WINDOW_DEVICE_STATE_KEYBOARD_DEFAULT:
                dmNativeShowKeyboard(op1, NATIVE_KEYBOARD_DEFAULT, op2);
                break;
            case WINDOW_DEVICE_STATE_KEYBOARD_NUMBER_PAD:
                dmNativeShowKeyboard(op1, NATIVE_KEYBOARD_NUMBER_PAD, op2);
                break;
            case WINDOW_DEVICE_STATE_KEYBOARD_EMAIL:
                dmNativeShowKeyboard(op1, NATIVE_KEYBOARD_EMAIL, op2);
                break;
            case WINDOW_DEVICE_STATE_KEYBOARD_PASSWORD:
                dmNativeShowKeyboard(op1, NATIVE_KEYBOARD_PASSWORD, op2);
                break;
            case WINDOW_DEVICE_STATE_KEYBOARD_RESET:
                dmNativeResetKeyboard();
                break;
            default:break;
        }
    }

    bool GetDeviceState(HWindow window, WindowDeviceState state)
    {
        return GetDeviceState(window, state, 0);
    }

    bool GetDeviceState(HWindow window, WindowDeviceState state, int32_t op1)
    {
        switch(state)
        {
            case WINDOW_DEVICE_STATE_CURSOR_LOCK:      return dmNativeGetMouseLocked();
            case WINDOW_DEVICE_STATE_JOYSTICK_PRESENT: return dmNativeGetJoystickParam(op1, NATIVE_PRESENT);
            default:break;
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
        uint32_t count = dmMath::Min(dmNativeGetJoystickParam(joystick_index, NATIVE_AXES), (int) values_capacity);
        dmNativeGetJoystickPos(joystick_index, values, count);
        return count;
    }

    uint32_t GetJoystickHats(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity)
    {
        uint32_t count = dmMath::Min(dmNativeGetJoystickParam(joystick_index, NATIVE_HATS), (int) values_capacity);
        dmNativeGetJoystickHats(joystick_index, values, count);
        return count;
    }

    uint32_t GetJoystickButtons(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity)
    {
        uint32_t count = dmMath::Min(dmNativeGetJoystickParam(joystick_index, NATIVE_BUTTONS), (int) values_capacity);
        dmNativeGetJoystickButtons(joystick_index, values, count);
        return count;
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

    const int PLATFORM_KEY_START           = 0;
    const int PLATFORM_JOYSTICK_LAST       = NATIVE_JOYSTICK_LAST;
    const int PLATFORM_KEY_ESC             = NATIVE_KEY_ESC;
    const int PLATFORM_KEY_F1              = NATIVE_KEY_F1;
    const int PLATFORM_KEY_F2              = NATIVE_KEY_F2;
    const int PLATFORM_KEY_F3              = NATIVE_KEY_F3;
    const int PLATFORM_KEY_F4              = NATIVE_KEY_F4;
    const int PLATFORM_KEY_F5              = NATIVE_KEY_F5;
    const int PLATFORM_KEY_F6              = NATIVE_KEY_F6;
    const int PLATFORM_KEY_F7              = NATIVE_KEY_F7;
    const int PLATFORM_KEY_F8              = NATIVE_KEY_F8;
    const int PLATFORM_KEY_F9              = NATIVE_KEY_F9;
    const int PLATFORM_KEY_F10             = NATIVE_KEY_F10;
    const int PLATFORM_KEY_F11             = NATIVE_KEY_F11;
    const int PLATFORM_KEY_F12             = NATIVE_KEY_F12;
    const int PLATFORM_KEY_UP              = NATIVE_KEY_UP;
    const int PLATFORM_KEY_DOWN            = NATIVE_KEY_DOWN;
    const int PLATFORM_KEY_LEFT            = NATIVE_KEY_LEFT;
    const int PLATFORM_KEY_RIGHT           = NATIVE_KEY_RIGHT;
    const int PLATFORM_KEY_LSHIFT          = NATIVE_KEY_LSHIFT;
    const int PLATFORM_KEY_RSHIFT          = NATIVE_KEY_RSHIFT;
    const int PLATFORM_KEY_LCTRL           = NATIVE_KEY_LCTRL;
    const int PLATFORM_KEY_RCTRL           = NATIVE_KEY_RCTRL;
    const int PLATFORM_KEY_LALT            = NATIVE_KEY_LALT;
    const int PLATFORM_KEY_RALT            = NATIVE_KEY_RALT;
    const int PLATFORM_KEY_TAB             = NATIVE_KEY_TAB;
    const int PLATFORM_KEY_ENTER           = NATIVE_KEY_ENTER;
    const int PLATFORM_KEY_BACKSPACE       = NATIVE_KEY_BACKSPACE;
    const int PLATFORM_KEY_INSERT          = NATIVE_KEY_INSERT;
    const int PLATFORM_KEY_DEL             = NATIVE_KEY_DEL;
    const int PLATFORM_KEY_PAGEUP          = NATIVE_KEY_PAGEUP;
    const int PLATFORM_KEY_PAGEDOWN        = NATIVE_KEY_PAGEDOWN;
    const int PLATFORM_KEY_HOME            = NATIVE_KEY_HOME;
    const int PLATFORM_KEY_END             = NATIVE_KEY_END;
    const int PLATFORM_KEY_KP_0            = NATIVE_KEY_KP_0;
    const int PLATFORM_KEY_KP_1            = NATIVE_KEY_KP_1;
    const int PLATFORM_KEY_KP_2            = NATIVE_KEY_KP_2;
    const int PLATFORM_KEY_KP_3            = NATIVE_KEY_KP_3;
    const int PLATFORM_KEY_KP_4            = NATIVE_KEY_KP_4;
    const int PLATFORM_KEY_KP_5            = NATIVE_KEY_KP_5;
    const int PLATFORM_KEY_KP_6            = NATIVE_KEY_KP_6;
    const int PLATFORM_KEY_KP_7            = NATIVE_KEY_KP_7;
    const int PLATFORM_KEY_KP_8            = NATIVE_KEY_KP_8;
    const int PLATFORM_KEY_KP_9            = NATIVE_KEY_KP_9;
    const int PLATFORM_KEY_KP_DIVIDE       = NATIVE_KEY_KP_DIVIDE;
    const int PLATFORM_KEY_KP_MULTIPLY     = NATIVE_KEY_KP_MULTIPLY;
    const int PLATFORM_KEY_KP_SUBTRACT     = NATIVE_KEY_KP_SUBTRACT;
    const int PLATFORM_KEY_KP_ADD          = NATIVE_KEY_KP_ADD;
    const int PLATFORM_KEY_KP_DECIMAL      = NATIVE_KEY_KP_DECIMAL;
    const int PLATFORM_KEY_KP_EQUAL        = NATIVE_KEY_KP_EQUAL;
    const int PLATFORM_KEY_KP_ENTER        = NATIVE_KEY_KP_ENTER;
    const int PLATFORM_KEY_KP_NUM_LOCK     = NATIVE_KEY_KP_NUM_LOCK;
    const int PLATFORM_KEY_CAPS_LOCK       = NATIVE_KEY_CAPS_LOCK;
    const int PLATFORM_KEY_SCROLL_LOCK     = NATIVE_KEY_SCROLL_LOCK;
    const int PLATFORM_KEY_PAUSE           = NATIVE_KEY_PAUSE;
    const int PLATFORM_KEY_LSUPER          = NATIVE_KEY_LSUPER;
    const int PLATFORM_KEY_RSUPER          = NATIVE_KEY_RSUPER;
    const int PLATFORM_KEY_MENU            = NATIVE_KEY_MENU;
    const int PLATFORM_KEY_BACK            = NATIVE_KEY_BACK;

    const int PLATFORM_MOUSE_BUTTON_LEFT   = NATIVE_MOUSE_BUTTON_LEFT;
    const int PLATFORM_MOUSE_BUTTON_MIDDLE = NATIVE_MOUSE_BUTTON_MIDDLE;
    const int PLATFORM_MOUSE_BUTTON_RIGHT  = NATIVE_MOUSE_BUTTON_RIGHT;
    const int PLATFORM_MOUSE_BUTTON_1      = NATIVE_MOUSE_BUTTON_1;
    const int PLATFORM_MOUSE_BUTTON_2      = NATIVE_MOUSE_BUTTON_2;
    const int PLATFORM_MOUSE_BUTTON_3      = NATIVE_MOUSE_BUTTON_3;
    const int PLATFORM_MOUSE_BUTTON_4      = NATIVE_MOUSE_BUTTON_4;
    const int PLATFORM_MOUSE_BUTTON_5      = NATIVE_MOUSE_BUTTON_5;
    const int PLATFORM_MOUSE_BUTTON_6      = NATIVE_MOUSE_BUTTON_6;
    const int PLATFORM_MOUSE_BUTTON_7      = NATIVE_MOUSE_BUTTON_7;
    const int PLATFORM_MOUSE_BUTTON_8      = NATIVE_MOUSE_BUTTON_8;

    const int PLATFORM_JOYSTICK_1          = NATIVE_JOYSTICK_1;
    const int PLATFORM_JOYSTICK_2          = NATIVE_JOYSTICK_2;
    const int PLATFORM_JOYSTICK_3          = NATIVE_JOYSTICK_3;
    const int PLATFORM_JOYSTICK_4          = NATIVE_JOYSTICK_4;
    const int PLATFORM_JOYSTICK_5          = NATIVE_JOYSTICK_5;
    const int PLATFORM_JOYSTICK_6          = NATIVE_JOYSTICK_6;
    const int PLATFORM_JOYSTICK_7          = NATIVE_JOYSTICK_7;
    const int PLATFORM_JOYSTICK_8          = NATIVE_JOYSTICK_8;
    const int PLATFORM_JOYSTICK_9          = NATIVE_JOYSTICK_9;
    const int PLATFORM_JOYSTICK_10         = NATIVE_JOYSTICK_10;
    const int PLATFORM_JOYSTICK_11         = NATIVE_JOYSTICK_11;
    const int PLATFORM_JOYSTICK_12         = NATIVE_JOYSTICK_12;
    const int PLATFORM_JOYSTICK_13         = NATIVE_JOYSTICK_13;
    const int PLATFORM_JOYSTICK_14         = NATIVE_JOYSTICK_14;
    const int PLATFORM_JOYSTICK_15         = NATIVE_JOYSTICK_15;
    const int PLATFORM_JOYSTICK_16         = NATIVE_JOYSTICK_16;
}
