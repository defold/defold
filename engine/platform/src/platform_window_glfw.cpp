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

#include <dlib/platform.h>
#include <dlib/log.h>

#include <glfw/glfw.h>

#include <dlib/platform.h>
#include <dlib/log.h>
#include <dlib/array.h>
#include <dlib/math.h>

#include "platform_window.h"
#include "platform_window_constants.h"
#include "platform_window_opengl.h"

#if defined(ANDROID)
#include "platform_window_android.h"
#elif defined(DM_PLATFORM_IOS)
#include "platform_window_ios.h"
#endif

namespace dmPlatform
{
    struct dmWindow
    {
        WindowResizeCallback          m_ResizeCallback;
        void*                         m_ResizeCallbackUserData;
        WindowCloseCallback           m_CloseCallback;
        void*                         m_CloseCallbackUserData;
        WindowFocusCallback           m_FocusCallback;
        void*                         m_FocusCallbackUserData;
        WindowIconifyCallback         m_IconifyCallback;
        void*                         m_IconifyCallbackUserData;
        WindowAddKeyboardCharCallback m_AddKeyboarCharCallBack;
        void*                         m_AddKeyboarCharCallBackUserData;
        WindowSetMarkedTextCallback   m_SetMarkedTextCallback;
        void*                         m_SetMarkedTextCallbackUserData;
        WindowDeviceChangedCallback   m_DeviceChangedCallback;
        void*                         m_DeviceChangedCallbackUserData;
        WindowGamepadEventCallback    m_GamepadEventCallback;
        void*                         m_GamepadEventCallbackUserData;
        dmArray<GLFWTouch>            m_TouchData;
        int32_t                       m_Width;
        int32_t                       m_Height;
        uint32_t                      m_Samples               : 8;
        uint32_t                      m_WindowOpened          : 1;
        uint32_t                      m_SwapIntervalSupported : 1;
        uint32_t                      m_SwapBufferSupported   : 1;
        uint32_t                      m_HighDPI               : 1;
    };

    // Needed by glfw2.7
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
            g_Window->m_GamepadEventCallback(g_Window->m_GamepadEventCallbackUserData, gamepad_id, connected ? GAMEPAD_EVENT_CONNECTED : GAMEPAD_EVENT_DISCONNECTED);
        }
    }

    HWindow NewWindow()
    {
        if (g_Window == 0)
        {
            dmWindow* wnd = new dmWindow;
            memset(wnd, 0, sizeof(dmWindow));

            if (glfwInit() == GL_FALSE)
            {
                dmLogError("Could not initialize glfw.");
                return 0;
            }

            g_Window = wnd;

            return wnd;
        }

        return 0;
    }

    static PlatformResult OpenWindowOpenGL(dmWindow* wnd, const WindowParams& params)
    {
        if (params.m_HighDPI)
        {
            glfwOpenWindowHint(GLFW_WINDOW_HIGH_DPI, 1);
        }

        glfwOpenWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwOpenWindowHint(GLFW_FSAA_SAMPLES, params.m_Samples);

#if defined(ANDROID)
        // Seems to work fine anyway without any hints
        // which is good, since we want to fallback from OpenGLES 3 to 2
#elif defined(DM_PLATFORM_IOS)
        glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
        glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 0); // 3.0 on iOS
#endif

        bool is_desktop = false;
#if defined(DM_PLATFORM_LINUX) && !defined(ANDROID)
        is_desktop = true;
#endif
        if (is_desktop)
        {
            uint32_t major = 3, minor = 3;
            if (!OpenGLGetVersion(params.m_OpenGLVersionHint, &major, &minor))
            {
                dmLogWarning("OpenGL version hint %d is not supported. Using default version (%d.%d)",
                    params.m_OpenGLVersionHint, major, minor);
            }

            // Use specific OpenGL version.
            if (major != 0 && minor != 0)
            {
                glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, major);
                glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, minor);
            }

            if (params.m_OpenGLUseCoreProfileHint)
            {
                glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
                glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            }
        }

        int mode = GLFW_WINDOW;
        if (params.m_Fullscreen)
        {
            mode = GLFW_FULLSCREEN;
        }
        if (!glfwOpenWindow(params.m_Width, params.m_Height, 8, 8, 8, params.m_ContextAlphabits, 32, 8, mode))
        {
            if (is_desktop)
            {
                dmLogWarning("Trying OpenGL 3.1 compat mode");

                // Try a second time, this time without core profile, and lower the minor version.
                // And GLFW clears hints, so we have to set them again.
                if (params.m_HighDPI)
                {
                    glfwOpenWindowHint(GLFW_WINDOW_HIGH_DPI, 1);
                }
                glfwOpenWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
                glfwOpenWindowHint(GLFW_FSAA_SAMPLES, params.m_Samples);

                // We currently cannot go lower since we support shader model 140
                glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
                glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 1);

                glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

                if (!glfwOpenWindow(params.m_Width, params.m_Height, 8, 8, 8, params.m_ContextAlphabits, 32, 8, mode))
                {
                    return PLATFORM_RESULT_WINDOW_OPEN_ERROR;
                }
            }
            else
            {
                return PLATFORM_RESULT_WINDOW_OPEN_ERROR;
            }
        }

        wnd->m_SwapIntervalSupported = 1;
        wnd->m_SwapBufferSupported = 1;

        return PLATFORM_RESULT_OK;
    }

    static PlatformResult OpenWindowNoAPI(dmWindow* wnd, const WindowParams& params)
    {
        glfwOpenWindowHint(GLFW_CLIENT_API,   GLFW_NO_API);
        glfwOpenWindowHint(GLFW_FSAA_SAMPLES, params.m_Samples);

        int mode = params.m_Fullscreen ? GLFW_FULLSCREEN : GLFW_WINDOW;

        if (!glfwOpenWindow(params.m_Width, params.m_Height, 8, 8, 8, 8, 32, 8, mode))
        {
            return PLATFORM_RESULT_WINDOW_OPEN_ERROR;
        }

    #if defined(ANDROID) || defined(DM_PLATFORM_IOS)
        wnd->m_SwapBufferSupported = 1;
    #endif
        if(params.m_GraphicsApi == PLATFORM_GRAPHICS_API_WEBGPU)
            wnd->m_SwapBufferSupported = 1;

        return PLATFORM_RESULT_OK;
    }

    PlatformResult OpenWindow(HWindow window, const WindowParams& params)
    {
        if (window->m_WindowOpened)
        {
            return PLATFORM_RESULT_WINDOW_ALREADY_OPENED;
        }

        PlatformResult res = PLATFORM_RESULT_WINDOW_OPEN_ERROR;

        switch(params.m_GraphicsApi)
        {
            case PLATFORM_GRAPHICS_API_OPENGL:
            case PLATFORM_GRAPHICS_API_OPENGLES:
                res = OpenWindowOpenGL(window, params);
                break;
            case PLATFORM_GRAPHICS_API_WEBGPU:
            case PLATFORM_GRAPHICS_API_VULKAN:
                res = OpenWindowNoAPI(window, params);
                break;
            default: assert(0);
        }

        if (res == PLATFORM_RESULT_OK)
        {
            glfwSetWindowBackgroundColor(params.m_BackgroundColor);
            glfwSetWindowSizeCallback(OnWindowResize);
            glfwSetWindowCloseCallback(OnWindowClose);
            glfwSetWindowFocusCallback(OnWindowFocus);
            glfwSetWindowIconifyCallback(OnWindowIconify);
            glfwSetGamepadCallback(OnGamepad);
            glfwSwapInterval(1);
            glfwGetWindowSize(&window->m_Width, &window->m_Height);

        #if !defined(__EMSCRIPTEN__)
            glfwSetWindowTitle(params.m_Title);
        #endif

            if (glfwSetCharCallback(OnAddCharacterCallback) == 0)
            {
                dmLogFatal("could not set glfw char callback.");
            }
            if (glfwSetMarkedTextCallback(OnMarkedTextCallback) == 0)
            {
                dmLogFatal("could not set glfw marked text callback.");
            }
            if (glfwSetDeviceChangedCallback(OnDeviceChangedCallback) == 0)
            {
                dmLogFatal("coult not set glfw gamepad connection callback.");
            }

            // These callback pointers are set on the window AFTER the glfw callbacks have been set,
            // This is to make sure glfw don't call any of the callbacks before everything has been setup in the engine
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
        glfwCloseWindow();
    }

    void DeleteWindow(HWindow window)
    {
        delete window;
        g_Window = 0;

        glfwTerminate();
    }

    void SetWindowTitle(HWindow window, const char* title)
    {
        glfwSetWindowTitle(title);
    }

    void SetWindowSize(HWindow window, uint32_t width, uint32_t height)
    {
        glfwSetWindowSize((int)width, (int)height);
        int window_width, window_height;
        glfwGetWindowSize(&window_width, &window_height);
        window->m_Width  = window_width;
        window->m_Height = window_height;

        // The callback is not called from glfw when the size is set manually
        if (window->m_ResizeCallback)
        {
            window->m_ResizeCallback(window->m_ResizeCallbackUserData, window_width, window_height);
        }
    }

    void SetWindowPosition(HWindow window, int32_t x, int32_t y)
    {
        glfwSetWindowPos(x, y);
    }

    uint32_t GetWindowWidth(HWindow window)
    {
        return (uint32_t) window->m_Width;
    }
    uint32_t GetWindowHeight(HWindow window)
    {
        return (uint32_t) window->m_Height;
    }

    static void SetSafeAreaFull(HWindow window, SafeArea* out)
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

    bool GetSafeArea(HWindow window, SafeArea* out)
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

    static int WindowStateToGLFW(WindowState state)
    {
        switch(state)
        {
            case WINDOW_STATE_OPENED:           return 0x00020001;
            case WINDOW_STATE_ACTIVE:           return 0x00020002;
            case WINDOW_STATE_ICONIFIED:        return 0x00020003;
            case WINDOW_STATE_ACCELERATED:      return 0x00020004;
            case WINDOW_STATE_RED_BITS:         return 0x00020005;
            case WINDOW_STATE_GREEN_BITS:       return 0x00020006;
            case WINDOW_STATE_BLUE_BITS:        return 0x00020007;
            case WINDOW_STATE_ALPHA_BITS:       return 0x00020008;
            case WINDOW_STATE_DEPTH_BITS:       return 0x00020009;
            case WINDOW_STATE_STENCIL_BITS:     return 0x0002000A;
            case WINDOW_STATE_REFRESH_RATE:     return 0x0002000B;
            case WINDOW_STATE_ACCUM_RED_BITS:   return 0x0002000C;
            case WINDOW_STATE_ACCUM_GREEN_BITS: return 0x0002000D;
            case WINDOW_STATE_ACCUM_BLUE_BITS:  return 0x0002000E;
            case WINDOW_STATE_ACCUM_ALPHA_BITS: return 0x0002000F;
            case WINDOW_STATE_AUX_BUFFERS:      return 0x00020010;
            case WINDOW_STATE_STEREO:           return 0x00020011;
            case WINDOW_STATE_WINDOW_NO_RESIZE: return 0x00020012;
            case WINDOW_STATE_FSAA_SAMPLES:     return 0x00020013;
            default:assert(0);break;
        }
        return -1;
    }

    #ifndef __EMSCRIPTEN__
        #define GLFW_AUX_CONTEXT_SUPPORTED
    #endif

    static inline int32_t QueryAuxContextImpl()
    {
    #if defined(GLFW_AUX_CONTEXT_SUPPORTED)
        return glfwQueryAuxContext();
    #else
        return 0;
    #endif
    }

    void* AcquireAuxContext(HWindow window)
    {
    #if defined(GLFW_AUX_CONTEXT_SUPPORTED)
        return glfwAcquireAuxContext();
    #else
        return 0;
    #endif
    }

    void UnacquireAuxContext(HWindow window, void* aux_context)
    {
    #if defined(GLFW_AUX_CONTEXT_SUPPORTED)
        glfwUnacquireAuxContext(aux_context);
    #endif
    }

    #undef GLFW_AUX_CONTEXT_SUPPORTED

    uint32_t GetWindowStateParam(HWindow window, WindowState state)
    {
        switch(state)
        {
            case WINDOW_STATE_REFRESH_RATE: return glfwGetWindowRefreshRate();
            case WINDOW_STATE_SAMPLE_COUNT: return window->m_Samples;
            case WINDOW_STATE_HIGH_DPI:     return window->m_HighDPI;
            case WINDOW_STATE_AUX_CONTEXT:  return QueryAuxContextImpl();
            default:break;
        }

        return window->m_WindowOpened ? glfwGetWindowParam(WindowStateToGLFW(state)) : 0;
    }

    void IconifyWindow(HWindow window)
    {
        if (window->m_WindowOpened)
        {
            glfwIconifyWindow();
        }
    }

    float GetDisplayScaleFactor(HWindow window)
    {
        return glfwGetDisplayScaleFactor();
    }

    uintptr_t GetProcAddress(HWindow window, const char* proc_name)
    {
        return (uintptr_t) glfwGetProcAddress(proc_name);
    }

    void SetSwapInterval(HWindow window, uint32_t swap_interval)
    {
        if (window->m_SwapIntervalSupported)
        {
            glfwSwapInterval(swap_interval);
        }
    }

    bool GetAcceleration(HWindow window, float* x, float* y, float* z)
    {
        return glfwGetAcceleration(x,y,z);
    }

    uint32_t GetTouchData(HWindow window, TouchData* touch_data, uint32_t touch_data_count)
    {
        int32_t touch_count = 0;

        if (window->m_TouchData.Capacity() < touch_data_count)
        {
            window->m_TouchData.SetCapacity(touch_data_count);
            window->m_TouchData.SetSize(touch_data_count);
        }

        glfwGetTouch(window->m_TouchData.Begin(), touch_data_count, &touch_count);

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
         return glfwGetKey(code);
    }

    int32_t GetMouseButton(HWindow window, int32_t button)
    {
        return glfwGetMouseButton(button);
    }

    int32_t GetMouseWheel(HWindow window)
    {
        return glfwGetMouseWheel();
    }

    void GetMousePosition(HWindow window, int32_t* x, int32_t* y)
    {
        glfwGetMousePos(x, y);
    }

    void SetDeviceState(HWindow window, DeviceState state, bool op1)
    {
        SetDeviceState(window, state, op1, false);
    }

    void SetDeviceState(HWindow window, DeviceState state, bool op1, bool op2)
    {
        switch(state)
        {
            case DEVICE_STATE_CURSOR:
                if (op1)
                    glfwEnable(GLFW_MOUSE_CURSOR);
                else
                    glfwDisable(GLFW_MOUSE_CURSOR);
                break;
            case DEVICE_STATE_ACCELEROMETER:
                if (op1)
                    glfwAccelerometerEnable();
                break;
            case DEVICE_STATE_KEYBOARD_DEFAULT:
                glfwShowKeyboard(op1, GLFW_KEYBOARD_DEFAULT, op2);
                break;
            case DEVICE_STATE_KEYBOARD_NUMBER_PAD:
                glfwShowKeyboard(op1, GLFW_KEYBOARD_NUMBER_PAD, op2);
                break;
            case DEVICE_STATE_KEYBOARD_EMAIL:
                glfwShowKeyboard(op1, GLFW_KEYBOARD_EMAIL, op2);
                break;
            case DEVICE_STATE_KEYBOARD_PASSWORD:
                glfwShowKeyboard(op1, GLFW_KEYBOARD_PASSWORD, op2);
                break;
            case DEVICE_STATE_KEYBOARD_RESET:
                glfwResetKeyboard();
                break;
            default:break;
        }
    }

    bool GetDeviceState(HWindow window, DeviceState state)
    {
        return GetDeviceState(window, state, 0);
    }

    bool GetDeviceState(HWindow window, DeviceState state, int32_t op1)
    {
        switch(state)
        {
            case DEVICE_STATE_CURSOR_LOCK:      return glfwGetMouseLocked();
            case DEVICE_STATE_JOYSTICK_PRESENT: return glfwGetJoystickParam(op1, GLFW_PRESENT);
            default:break;
        }
        dmLogWarning("Unable to get device state (%d), unknown state.", (int) state);
        return false;
    }

    const char* GetJoystickDeviceName(HWindow window, uint32_t joystick_index)
    {
        char* device_name;
        glfwGetJoystickDeviceId(joystick_index, &device_name);
        return (const char*) device_name;
    }

    uint32_t GetJoystickAxes(HWindow window, uint32_t joystick_index, float* values, uint32_t values_capacity)
    {
        uint32_t count = dmMath::Min(glfwGetJoystickParam(joystick_index, GLFW_AXES), (int) values_capacity);
        glfwGetJoystickPos(joystick_index, values, count);
        return count;
    }

    uint32_t GetJoystickHats(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity)
    {
        uint32_t count = dmMath::Min(glfwGetJoystickParam(joystick_index, GLFW_HATS), (int) values_capacity);
        glfwGetJoystickHats(joystick_index, values, count);
        return count;
    }

    uint32_t GetJoystickButtons(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity)
    {
        uint32_t count = dmMath::Min(glfwGetJoystickParam(joystick_index, GLFW_BUTTONS), (int) values_capacity);
        glfwGetJoystickButtons(joystick_index, values, count);
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

    void PollEvents(HWindow window)
    {
        // NOTE: GLFW_AUTO_POLL_EVENTS might be enabled but an application shouldn't have rely on
        // running glfwSwapBuffers for event queue polling
        // Accessing OpenGL isn't permitted on iOS when the application is transitioning to resumed mode either
        glfwPollEvents();
    }

    void SwapBuffers(HWindow window)
    {
        if (window->m_SwapBufferSupported)
        {
            glfwSwapBuffers();
        }
    }

    void SetKeyboardCharCallback(HWindow window, WindowAddKeyboardCharCallback cb, void* user_data)
    {
        window->m_AddKeyboarCharCallBack         = cb;
        window->m_AddKeyboarCharCallBackUserData = user_data;
    }

    void SetKeyboardMarkedTextCallback(HWindow window, WindowSetMarkedTextCallback cb, void* user_data)
    {
        window->m_SetMarkedTextCallback         = cb;
        window->m_SetMarkedTextCallbackUserData = user_data;
    }

    void SetKeyboardDeviceChangedCallback(HWindow window, WindowDeviceChangedCallback cb, void* user_data)
    {
        window->m_DeviceChangedCallback         = cb;
        window->m_DeviceChangedCallbackUserData = user_data;
    }

    void SetGamepadEventCallback(HWindow window, WindowGamepadEventCallback cb, void* user_data)
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
        return glfwGetDefaultFramebuffer();
    }

    const int PLATFORM_KEY_START           = 0;
    const int PLATFORM_JOYSTICK_LAST       = GLFW_JOYSTICK_LAST;
    const int PLATFORM_KEY_ESC             = GLFW_KEY_ESC;
    const int PLATFORM_KEY_F1              = GLFW_KEY_F1;
    const int PLATFORM_KEY_F2              = GLFW_KEY_F2;
    const int PLATFORM_KEY_F3              = GLFW_KEY_F3;
    const int PLATFORM_KEY_F4              = GLFW_KEY_F4;
    const int PLATFORM_KEY_F5              = GLFW_KEY_F5;
    const int PLATFORM_KEY_F6              = GLFW_KEY_F6;
    const int PLATFORM_KEY_F7              = GLFW_KEY_F7;
    const int PLATFORM_KEY_F8              = GLFW_KEY_F8;
    const int PLATFORM_KEY_F9              = GLFW_KEY_F9;
    const int PLATFORM_KEY_F10             = GLFW_KEY_F10;
    const int PLATFORM_KEY_F11             = GLFW_KEY_F11;
    const int PLATFORM_KEY_F12             = GLFW_KEY_F12;
    const int PLATFORM_KEY_UP              = GLFW_KEY_UP;
    const int PLATFORM_KEY_DOWN            = GLFW_KEY_DOWN;
    const int PLATFORM_KEY_LEFT            = GLFW_KEY_LEFT;
    const int PLATFORM_KEY_RIGHT           = GLFW_KEY_RIGHT;
    const int PLATFORM_KEY_LSHIFT          = GLFW_KEY_LSHIFT;
    const int PLATFORM_KEY_RSHIFT          = GLFW_KEY_RSHIFT;
    const int PLATFORM_KEY_LCTRL           = GLFW_KEY_LCTRL;
    const int PLATFORM_KEY_RCTRL           = GLFW_KEY_RCTRL;
    const int PLATFORM_KEY_LALT            = GLFW_KEY_LALT;
    const int PLATFORM_KEY_RALT            = GLFW_KEY_RALT;
    const int PLATFORM_KEY_TAB             = GLFW_KEY_TAB;
    const int PLATFORM_KEY_ENTER           = GLFW_KEY_ENTER;
    const int PLATFORM_KEY_BACKSPACE       = GLFW_KEY_BACKSPACE;
    const int PLATFORM_KEY_INSERT          = GLFW_KEY_INSERT;
    const int PLATFORM_KEY_DEL             = GLFW_KEY_DEL;
    const int PLATFORM_KEY_PAGEUP          = GLFW_KEY_PAGEUP;
    const int PLATFORM_KEY_PAGEDOWN        = GLFW_KEY_PAGEDOWN;
    const int PLATFORM_KEY_HOME            = GLFW_KEY_HOME;
    const int PLATFORM_KEY_END             = GLFW_KEY_END;
    const int PLATFORM_KEY_KP_0            = GLFW_KEY_KP_0;
    const int PLATFORM_KEY_KP_1            = GLFW_KEY_KP_1;
    const int PLATFORM_KEY_KP_2            = GLFW_KEY_KP_2;
    const int PLATFORM_KEY_KP_3            = GLFW_KEY_KP_3;
    const int PLATFORM_KEY_KP_4            = GLFW_KEY_KP_4;
    const int PLATFORM_KEY_KP_5            = GLFW_KEY_KP_5;
    const int PLATFORM_KEY_KP_6            = GLFW_KEY_KP_6;
    const int PLATFORM_KEY_KP_7            = GLFW_KEY_KP_7;
    const int PLATFORM_KEY_KP_8            = GLFW_KEY_KP_8;
    const int PLATFORM_KEY_KP_9            = GLFW_KEY_KP_9;
    const int PLATFORM_KEY_KP_DIVIDE       = GLFW_KEY_KP_DIVIDE;
    const int PLATFORM_KEY_KP_MULTIPLY     = GLFW_KEY_KP_MULTIPLY;
    const int PLATFORM_KEY_KP_SUBTRACT     = GLFW_KEY_KP_SUBTRACT;
    const int PLATFORM_KEY_KP_ADD          = GLFW_KEY_KP_ADD;
    const int PLATFORM_KEY_KP_DECIMAL      = GLFW_KEY_KP_DECIMAL;
    const int PLATFORM_KEY_KP_EQUAL        = GLFW_KEY_KP_EQUAL;
    const int PLATFORM_KEY_KP_ENTER        = GLFW_KEY_KP_ENTER;
    const int PLATFORM_KEY_KP_NUM_LOCK     = GLFW_KEY_KP_NUM_LOCK;
    const int PLATFORM_KEY_CAPS_LOCK       = GLFW_KEY_CAPS_LOCK;
    const int PLATFORM_KEY_SCROLL_LOCK     = GLFW_KEY_SCROLL_LOCK;
    const int PLATFORM_KEY_PAUSE           = GLFW_KEY_PAUSE;
    const int PLATFORM_KEY_LSUPER          = GLFW_KEY_LSUPER;
    const int PLATFORM_KEY_RSUPER          = GLFW_KEY_RSUPER;
    const int PLATFORM_KEY_MENU            = GLFW_KEY_MENU;
    const int PLATFORM_KEY_BACK            = GLFW_KEY_BACK;

    const int PLATFORM_MOUSE_BUTTON_LEFT   = GLFW_MOUSE_BUTTON_LEFT;
    const int PLATFORM_MOUSE_BUTTON_MIDDLE = GLFW_MOUSE_BUTTON_MIDDLE;
    const int PLATFORM_MOUSE_BUTTON_RIGHT  = GLFW_MOUSE_BUTTON_RIGHT;
    const int PLATFORM_MOUSE_BUTTON_1      = GLFW_MOUSE_BUTTON_1;
    const int PLATFORM_MOUSE_BUTTON_2      = GLFW_MOUSE_BUTTON_2;
    const int PLATFORM_MOUSE_BUTTON_3      = GLFW_MOUSE_BUTTON_3;
    const int PLATFORM_MOUSE_BUTTON_4      = GLFW_MOUSE_BUTTON_4;
    const int PLATFORM_MOUSE_BUTTON_5      = GLFW_MOUSE_BUTTON_5;
    const int PLATFORM_MOUSE_BUTTON_6      = GLFW_MOUSE_BUTTON_6;
    const int PLATFORM_MOUSE_BUTTON_7      = GLFW_MOUSE_BUTTON_7;
    const int PLATFORM_MOUSE_BUTTON_8      = GLFW_MOUSE_BUTTON_8;

    const int PLATFORM_JOYSTICK_1          = GLFW_JOYSTICK_1;
    const int PLATFORM_JOYSTICK_2          = GLFW_JOYSTICK_2;
    const int PLATFORM_JOYSTICK_3          = GLFW_JOYSTICK_3;
    const int PLATFORM_JOYSTICK_4          = GLFW_JOYSTICK_4;
    const int PLATFORM_JOYSTICK_5          = GLFW_JOYSTICK_5;
    const int PLATFORM_JOYSTICK_6          = GLFW_JOYSTICK_6;
    const int PLATFORM_JOYSTICK_7          = GLFW_JOYSTICK_7;
    const int PLATFORM_JOYSTICK_8          = GLFW_JOYSTICK_8;
    const int PLATFORM_JOYSTICK_9          = GLFW_JOYSTICK_9;
    const int PLATFORM_JOYSTICK_10         = GLFW_JOYSTICK_10;
    const int PLATFORM_JOYSTICK_11         = GLFW_JOYSTICK_11;
    const int PLATFORM_JOYSTICK_12         = GLFW_JOYSTICK_12;
    const int PLATFORM_JOYSTICK_13         = GLFW_JOYSTICK_13;
    const int PLATFORM_JOYSTICK_14         = GLFW_JOYSTICK_14;
    const int PLATFORM_JOYSTICK_15         = GLFW_JOYSTICK_15;
    const int PLATFORM_JOYSTICK_16         = GLFW_JOYSTICK_16;
}
