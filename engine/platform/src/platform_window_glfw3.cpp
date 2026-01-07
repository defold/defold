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

#include "platform_window.h"
#include "platform_window_constants.h"
#include "platform_window_glfw3_private.h"
#include "platform_window_opengl.h"

#include <glfw/glfw3.h>

#include <dlib/platform.h>
#include <dlib/log.h>
#include <dlib/math.h>

namespace dmPlatform
{
    // Gamepad callbacks are shared across all windows, so we need a
    // shared struct to store 'global' data
    static struct PlatformContext
    {
        WindowGamepadEventCallback m_GamepadEventCallback;
        void*                      m_GamepadEventCallbackUserData;
    } g_GLFW3Context;

    static void UpdateWindowSize(HWindow window)
    {
        glfwGetFramebufferSize(window->m_Window, &window->m_Width, &window->m_Height);
        glfwGetWindowSize(window->m_Window, &window->m_WidthScreen, &window->m_HeightScreen);
    }

    static void OnError(int error, const char* description)
    {
        dmLogError("GLFW Error: %s", description);
    }

    static void OnWindowResize(GLFWwindow* glfw_window, int width, int height)
    {
        HWindow window = (HWindow) glfwGetWindowUserPointer(glfw_window);
        UpdateWindowSize(window);

        if (window->m_ResizeCallback != 0x0)
        {
            window->m_ResizeCallback(window->m_ResizeCallbackUserData, window->m_Width, window->m_Height);
        }
    }

    static void OnWindowClose(GLFWwindow* glfw_window)
    {
        HWindow window = (HWindow) glfwGetWindowUserPointer(glfw_window);
        if (window->m_CloseCallback != 0x0)
        {
            window->m_CloseCallback(window->m_CloseCallbackUserData);
        }
    }

    static void OnWindowFocus(GLFWwindow* glfw_window, int focus)
    {
        HWindow window = (HWindow) glfwGetWindowUserPointer(glfw_window);
        if (window->m_FocusCallback != 0x0)
        {
            window->m_FocusCallback(window->m_FocusCallbackUserData, focus);
        }
    }

    static void OnWindowIconify(GLFWwindow* glfw_window, int iconify)
    {
        HWindow window = (HWindow) glfwGetWindowUserPointer(glfw_window);
        if (window->m_IconifyCallback != 0x0)
        {
            window->m_IconifyCallback(window->m_IconifyCallbackUserData, iconify);
        }
    }

    static void OnAddCharacterCallback(GLFWwindow* glfw_window, unsigned int chr)
    {
        HWindow window = (HWindow) glfwGetWindowUserPointer(glfw_window);
        if (window->m_AddKeyboarCharCallBack)
        {
            window->m_AddKeyboarCharCallBack(window->m_AddKeyboarCharCallBackUserData, chr);
        }
    }

    static void OnMarkedTextCallback(GLFWwindow* glfw_window, char* text)
    {
        HWindow window = (HWindow) glfwGetWindowUserPointer(glfw_window);
        if (window->m_SetMarkedTextCallback)
        {
            window->m_SetMarkedTextCallback(window->m_SetMarkedTextCallbackUserData, text);
        }
    }

    static void OnContentScaleCallback(GLFWwindow* glfw_window, float xscale, float yscale)
    {
        HWindow window = (HWindow) glfwGetWindowUserPointer(glfw_window);
        window->m_XScale = xscale;
        window->m_YScale = yscale;

        int width, height;
        glfwGetWindowSize(glfw_window, &width, &height);
        OnWindowResize(glfw_window, width, height);
    }

    static void OnMouseScroll(GLFWwindow* glfw_window, double xoffset, double yoffset)
    {
        HWindow window = (HWindow) glfwGetWindowUserPointer(glfw_window);
        window->m_MouseScrollX += xoffset;
        window->m_MouseScrollY += yoffset;
    }

    static void OnJoystick(int id, int event)
    {
        if (g_GLFW3Context.m_GamepadEventCallback)
        {
            GamepadEvent gp_evt = GAMEPAD_EVENT_UNSUPPORTED;
            switch(event)
            {
                case GLFW_CONNECTED:
                    gp_evt = GAMEPAD_EVENT_CONNECTED;
                    break;
                case GLFW_DISCONNECTED:
                    gp_evt = GAMEPAD_EVENT_DISCONNECTED;
                    break;
                default:break;
            }

            g_GLFW3Context.m_GamepadEventCallback(g_GLFW3Context.m_GamepadEventCallbackUserData, id, gp_evt);
        }
    }

    HWindow NewWindow()
    {
        glfwSetErrorCallback(OnError);

    #ifdef __MACH__
        glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
    #endif

    #if defined(__linux__) && !defined(ANDROID)
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
        if (glfwInit() == GL_FALSE)
        {
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
            if (glfwInit() == GL_FALSE)
            {
                dmLogError("Could not initialize glfw.");
                return 0;
            }
        }
    #else
        if (glfwInit() == GL_FALSE)
        {
            dmLogError("Could not initialize glfw.");
            return 0;
        }
    #endif

        dmWindow* wnd = new dmWindow;
        memset(wnd, 0, sizeof(dmWindow));

        // Reset context
        memset(&g_GLFW3Context, 0, sizeof(PlatformContext));

        return wnd;
    }

    void DeleteWindow(HWindow window)
    {
        delete window;
        glfwTerminate();
    }

    int32_t OpenGLGetDefaultFramebufferId()
    {
        return 0;
    }

    static PlatformResult OpenWindowOpenGL(dmWindow* wnd, const WindowParams& params)
    {
        glfwWindowHint(GLFW_SAMPLES, params.m_Samples);

        GLFWmonitor* fullscreen_monitor = NULL;
        if (params.m_Fullscreen)
        {
            fullscreen_monitor = glfwGetPrimaryMonitor();
        }

        if (params.m_GraphicsApi == PLATFORM_GRAPHICS_API_OPENGLES)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
            glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

            wnd->m_Window = glfwCreateWindow(params.m_Width, params.m_Height, params.m_Title, fullscreen_monitor, NULL);
        }
        else
        {
            uint32_t major = 3, minor = 3;
            if (!OpenGLGetVersion(params.m_OpenGLVersionHint, &major, &minor))
            {
                dmLogWarning("OpenGL version hint %d is not supported. Using default version (%d.%d)",
                    params.m_OpenGLVersionHint, major, minor);
            }

            bool use_highest_version = major == 0 && minor == 0;
            if (!use_highest_version)
            {
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
            }

            if (params.m_OpenGLUseCoreProfileHint)
            {
                bool can_set_profile_and_fc = true;

            #ifdef _WIN32
                // Not supported on windows when requesting the default OpenGL version (which will equate to the highest available)
                can_set_profile_and_fc = !use_highest_version;
            #endif

                if (can_set_profile_and_fc)
                {
                    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
                }
            }

            wnd->m_Window = glfwCreateWindow(params.m_Width, params.m_Height, params.m_Title, fullscreen_monitor, NULL);
        }

        if (!wnd->m_Window)
        {
            return PLATFORM_RESULT_WINDOW_OPEN_ERROR;
        }

        wnd->m_SwapIntervalSupported = 1;

        glfwMakeContextCurrent(wnd->m_Window);

        // Create aux context
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        // Note: We can't create a 0x0 window
        wnd->m_AuxWindow = glfwCreateWindow(1, 1, "aux_window", NULL, wnd->m_Window);

        return PLATFORM_RESULT_OK;
    }

    static PlatformResult OpenWindowNoApi(dmWindow* wnd, const WindowParams& params)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_SAMPLES, params.m_Samples);

        GLFWmonitor* fullscreen_monitor = NULL;
        if (params.m_Fullscreen)
        {
            fullscreen_monitor = glfwGetPrimaryMonitor();
        }

        wnd->m_Window = glfwCreateWindow(params.m_Width, params.m_Height, params.m_Title, fullscreen_monitor, NULL);

        if (!wnd->m_Window)
        {
            return PLATFORM_RESULT_WINDOW_OPEN_ERROR;
        }

        return PLATFORM_RESULT_OK;
    }

    PlatformResult OpenWindow(HWindow window, const WindowParams& params)
    {
        if (window->m_WindowOpened)
        {
            return PLATFORM_RESULT_WINDOW_ALREADY_OPENED;
        }

        // A reboot doesn't shut down GLFW, so we reset all window hints here to defaults first.
        glfwDefaultWindowHints();

        // This hint saves both size and position, but we need only position
        // That's why we hide window here and then later set default size and show window
        glfwWindowHintString(GLFW_COCOA_FRAME_NAME, params.m_Title);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

        PlatformResult res = PLATFORM_RESULT_WINDOW_OPEN_ERROR;

        switch(params.m_GraphicsApi)
        {
            case PLATFORM_GRAPHICS_API_OPENGL:
            case PLATFORM_GRAPHICS_API_OPENGLES:
                res = OpenWindowOpenGL(window, params);
                break;
            case PLATFORM_GRAPHICS_API_DIRECTX:
            case PLATFORM_GRAPHICS_API_VULKAN:
                res = OpenWindowNoApi(window, params);
                break;
            default: assert(0);
        }

        if (res == PLATFORM_RESULT_OK)
        {
            FocusWindowNative(window);
            SetWindowsIconNative(window);

        #ifdef __MACH__
            // Set size from settings
            glfwSetWindowSize(window->m_Window, params.m_Width, params.m_Height);
        #endif

            glfwGetWindowContentScale(window->m_Window, &window->m_XScale, &window->m_YScale);
            glfwSetWindowUserPointer(window->m_Window, (void*) window);
            glfwSetWindowSizeCallback(window->m_Window, OnWindowResize);
            glfwSetWindowCloseCallback(window->m_Window, OnWindowClose);
            glfwSetWindowFocusCallback(window->m_Window, OnWindowFocus);
            glfwSetWindowIconifyCallback(window->m_Window, OnWindowIconify);
            glfwSetScrollCallback(window->m_Window, OnMouseScroll);
            glfwSetCharCallback(window->m_Window, OnAddCharacterCallback);
            glfwSetMarkedTextCallback(window->m_Window, OnMarkedTextCallback);
            glfwSetWindowContentScaleCallback(window->m_Window, OnContentScaleCallback);

            glfwSetJoystickCallback(OnJoystick);

            UpdateWindowSize(window);

            SetSwapInterval(window, 1);

            if (!params.m_Fullscreen)
            {
                CenterWindowNative(window, glfwGetPrimaryMonitor());
            }

            // glfwSetWindowBackgroundColor does not exist in GLFW3, but we could potentially
            // set glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE); to get a transparent framebuffer
            // glfwSetWindowBackgroundColor(params.m_BackgroundColor);

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
            window->m_WindowOpened            = 1;
        }

        return res;
    }

    uintptr_t GetProcAddress(HWindow window, const char* proc_name)
    {
        return (uintptr_t) glfwGetProcAddress(proc_name);
    }

    void CloseWindow(HWindow window)
    {
        glfwDestroyWindow(window->m_Window);
        if (window->m_AuxWindow)
            glfwDestroyWindow(window->m_AuxWindow);
    }

    void PollEvents(HWindow window)
    {
        glfwPollEvents();
    }

    void ShowWindow(HWindow window)
    {
        glfwShowWindow(window->m_Window);
    }

    void SwapBuffers(HWindow window)
    {
        glfwSwapBuffers(window->m_Window);
    }

    void IconifyWindow(HWindow window)
    {
        glfwIconifyWindow(window->m_Window);
    }

    uint32_t GetWindowWidth(HWindow window)
    {
        return (uint32_t) window->m_Width;
    }
    uint32_t GetWindowHeight(HWindow window)
    {
        return (uint32_t) window->m_Height;
    }

    void SetSwapInterval(HWindow window, uint32_t swap_interval)
    {
        if (window->m_SwapIntervalSupported)
        {
            glfwSwapInterval(swap_interval);
        }
    }

    void SetWindowTitle(HWindow window, const char* title)
    {
        glfwSetWindowTitle(window->m_Window, title);
    }

    void SetWindowSize(HWindow window, uint32_t width, uint32_t height)
    {
        glfwSetWindowSize(window->m_Window, (int) width, (int) height);
        UpdateWindowSize(window);

        // The callback is not called from glfw when the size is set manually
        if (window->m_ResizeCallback)
        {
            window->m_ResizeCallback(window->m_ResizeCallbackUserData, window->m_Width, window->m_Height);
        }
    }

    void SetWindowPosition(HWindow window, int32_t x, int32_t y)
    {
        glfwSetWindowPos(window->m_Window, x, y);
    }

    float GetDisplayScaleFactor(HWindow window)
    {
        return dmMath::Max(window->m_XScale, window->m_YScale);
    }

    void* AcquireAuxContext(HWindow window)
    {
        glfwMakeContextCurrent(window->m_AuxWindow);
        return (void*) window->m_AuxWindow;
    }

    void UnacquireAuxContext(HWindow window, void* aux_context)
    {
        glfwMakeContextCurrent(0);
    }

    static int WindowStateToGLFW(WindowState state)
    {
        switch(state)
        {
            case WINDOW_STATE_ACTIVE:           return GLFW_FOCUSED;
            case WINDOW_STATE_ICONIFIED:        return GLFW_ICONIFIED;
            case WINDOW_STATE_RED_BITS:         return GLFW_RED_BITS;
            case WINDOW_STATE_GREEN_BITS:       return GLFW_GREEN_BITS;
            case WINDOW_STATE_BLUE_BITS:        return GLFW_BLUE_BITS;
            case WINDOW_STATE_ALPHA_BITS:       return GLFW_ALPHA_BITS;
            case WINDOW_STATE_DEPTH_BITS:       return GLFW_DEPTH_BITS;
            case WINDOW_STATE_STENCIL_BITS:     return GLFW_STENCIL_BITS;
            case WINDOW_STATE_REFRESH_RATE:     return GLFW_REFRESH_RATE;
            case WINDOW_STATE_ACCUM_RED_BITS:   return GLFW_ACCUM_RED_BITS;
            case WINDOW_STATE_ACCUM_GREEN_BITS: return GLFW_ACCUM_GREEN_BITS;
            case WINDOW_STATE_ACCUM_BLUE_BITS:  return GLFW_ACCUM_BLUE_BITS;
            case WINDOW_STATE_ACCUM_ALPHA_BITS: return GLFW_ACCUM_ALPHA_BITS;
            case WINDOW_STATE_AUX_BUFFERS:      return GLFW_AUX_BUFFERS;
            case WINDOW_STATE_STEREO:           return GLFW_STEREO;
            case WINDOW_STATE_FSAA_SAMPLES:     return GLFW_SAMPLES;
            default:break;
        }
        return -1;
    }

    uint32_t GetWindowStateParam(HWindow window, WindowState state)
    {
        switch(state)
        {
            case WINDOW_STATE_OPENED:       return window->m_WindowOpened;
            case WINDOW_STATE_SAMPLE_COUNT: return window->m_Samples;
            case WINDOW_STATE_HIGH_DPI:     return window->m_HighDPI;
            case WINDOW_STATE_AUX_CONTEXT:  return window->m_AuxWindow ? 1 : 0;
            default: break;
        }

        return glfwGetWindowAttrib(window->m_Window, WindowStateToGLFW(state));
    }

    uint32_t GetTouchData(HWindow window, TouchData* touch_data, uint32_t touch_data_count)
    {
        return 0;
    }

    bool GetAcceleration(HWindow window, float* x, float* y, float* z)
    {
        return false;
    }

    int32_t GetKey(HWindow window, int32_t code)
    {
        if (code < 0 || code > GLFW_KEY_LAST)
        {
            return 0;
        }
        return glfwGetKey(window->m_Window, code);
    }

    int32_t GetMouseWheel(HWindow window)
    {
        return (int32_t) round(window->m_MouseScrollY);
    }

    int32_t GetMouseButton(HWindow window, int32_t button)
    {
        return glfwGetMouseButton(window->m_Window, button);
    }

    void GetMousePosition(HWindow window, int32_t* x, int32_t* y)
    {
        double xpos, ypos;
        glfwGetCursorPos(window->m_Window, &xpos, &ypos);

        // We have to account for a window scaling factor here because
        // when we output input actions we do a similar scaling (see engine.cpp -> GOActionCallback)
        // And it seems like GLFW3 has changed the coordinate system into "screen coordinates"
        // instead of pixel coordinates.
        float w_scale = (float) window->m_Width / (float) window->m_WidthScreen;
        float h_scale = (float) window->m_Height / (float) window->m_HeightScreen;

        *x = (int32_t) (xpos * w_scale);
        *y = (int32_t) (ypos * h_scale);
    }

    bool GetDeviceState(HWindow window, DeviceState state, int32_t op1)
    {
        switch(state)
        {
            case DEVICE_STATE_CURSOR_LOCK:      return glfwGetInputMode(window->m_Window, GLFW_CURSOR) != GLFW_CURSOR_NORMAL;
            case DEVICE_STATE_JOYSTICK_PRESENT: return glfwJoystickPresent(op1);
            default:break;
        }
        assert(0 && "Not supported.");
        return false;
    }

    const char* GetJoystickDeviceName(HWindow window, uint32_t gamepad_index)
    {
        return glfwGetJoystickName((int) gamepad_index);
    }

    uint32_t GetJoystickAxes(HWindow window, uint32_t joystick_index, float* values, uint32_t values_capacity)
    {
        int32_t count = 0;
        const float* axes_values = glfwGetJoystickAxes(joystick_index, &count);

        count = dmMath::Min(count, (int32_t) values_capacity);
        if (count > 0)
        {
            memcpy(values, axes_values, sizeof(float) * count);
        }
        return count;
    }

    uint32_t GetJoystickHats(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity)
    {
        int32_t count = 0;
        const unsigned char* hats_values = glfwGetJoystickHats(joystick_index, &count);

        count = dmMath::Min(count, (int32_t) values_capacity);
        if (count > 0)
        {
            memcpy(values, hats_values, sizeof(uint8_t) * count);
        }
        return count;
    }

    uint32_t GetJoystickButtons(HWindow window, uint32_t joystick_index, uint8_t* values, uint32_t values_capacity)
    {
        int32_t count = 0;
        const unsigned char* buttons_values = glfwGetJoystickButtons(joystick_index, &count);

        count = dmMath::Min(count, (int32_t) values_capacity);
        if (count > 0)
        {
            memcpy(values, buttons_values, sizeof(uint8_t) * count);
        }
        return count;
    }

    bool GetDeviceState(HWindow window, DeviceState state)
    {
        return GetDeviceState(window, state, 0);
    }

    void SetDeviceState(HWindow window, DeviceState state, bool op1)
    {
        SetDeviceState(window, state, op1, false);
    }

    void SetDeviceState(HWindow window, DeviceState state, bool op1, bool op2)
    {
        if (state == DEVICE_STATE_CURSOR)
        {
            if (op1)
            {
                glfwSetInputMode(window->m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            else
            {
                glfwSetInputMode(window->m_Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
                glfwSetInputMode(window->m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
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
        g_GLFW3Context.m_GamepadEventCallback         = cb;
        g_GLFW3Context.m_GamepadEventCallbackUserData = user_data;
    }

    const int PLATFORM_KEY_START           = 32;
    const int PLATFORM_JOYSTICK_LAST       = GLFW_JOYSTICK_LAST;
    const int PLATFORM_KEY_ESC             = GLFW_KEY_ESCAPE;
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
    const int PLATFORM_KEY_LSHIFT          = GLFW_KEY_LEFT_SHIFT;
    const int PLATFORM_KEY_RSHIFT          = GLFW_KEY_RIGHT_SHIFT;
    const int PLATFORM_KEY_LCTRL           = GLFW_KEY_LEFT_CONTROL;
    const int PLATFORM_KEY_RCTRL           = GLFW_KEY_RIGHT_CONTROL;
    const int PLATFORM_KEY_LALT            = GLFW_KEY_LEFT_ALT;
    const int PLATFORM_KEY_RALT            = GLFW_KEY_RIGHT_ALT;
    const int PLATFORM_KEY_TAB             = GLFW_KEY_TAB;
    const int PLATFORM_KEY_ENTER           = GLFW_KEY_ENTER;
    const int PLATFORM_KEY_BACKSPACE       = GLFW_KEY_BACKSPACE;
    const int PLATFORM_KEY_INSERT          = GLFW_KEY_INSERT;
    const int PLATFORM_KEY_DEL             = GLFW_KEY_DELETE;
    const int PLATFORM_KEY_PAGEUP          = GLFW_KEY_PAGE_UP;
    const int PLATFORM_KEY_PAGEDOWN        = GLFW_KEY_PAGE_DOWN;
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
    const int PLATFORM_KEY_KP_NUM_LOCK     = GLFW_KEY_NUM_LOCK;
    const int PLATFORM_KEY_CAPS_LOCK       = GLFW_KEY_CAPS_LOCK;
    const int PLATFORM_KEY_SCROLL_LOCK     = GLFW_KEY_SCROLL_LOCK;
    const int PLATFORM_KEY_PAUSE           = GLFW_KEY_PAUSE;
    const int PLATFORM_KEY_LSUPER          = GLFW_KEY_LEFT_SUPER;
    const int PLATFORM_KEY_RSUPER          = GLFW_KEY_RIGHT_SUPER;
    const int PLATFORM_KEY_MENU            = GLFW_KEY_MENU;
    const int PLATFORM_KEY_BACK            = -1; // Android only - back button support
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
