// Copyright 2020-2023 The Defold Foundation
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

#include <glfw/glfw3.h>

#include <dlib/platform.h>
#include <dlib/log.h>

namespace dmPlatform
{
    struct Window
    {
        GLFWwindow* m_Window;
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
        int32_t                       m_Width;
        int32_t                       m_Height;
        /*
        uint32_t                      m_Samples               : 8;
        uint32_t                      m_HighDPI               : 1;
        */

        uint32_t                      m_SwapIntervalSupported : 1;
        uint32_t                      m_WindowOpened          : 1;
    };

    static void glfw_error_callback(int error, const char* description)
    {
        dmLogError("GLFW Error: %s\n", description);
    }

    HWindow NewWindow()
    {
        if (glfwInit() == GL_FALSE)
        {
            dmLogError("Could not initialize glfw.");
            return 0;
        }

        Window* wnd = new Window;
        memset(wnd, 0, sizeof(Window));

        glfwSetErrorCallback(glfw_error_callback);

        return wnd;
    }

    void DeleteWindow(HWindow window)
    {
        delete window;
    }

    PlatformResult OpenWindowOpenGL(Window* wnd, const WindowParams& params)
    {
        // osx
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        wnd->m_Window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);

        if (!wnd->m_Window)
        {
            return PLATFORM_RESULT_WINDOW_OPEN_ERROR;
        }

        wnd->m_SwapIntervalSupported = 1;

        glfwMakeContextCurrent(wnd->m_Window);

        return PLATFORM_RESULT_OK;
    }

    PlatformResult OpenWindowVulkan(Window* wnd, const WindowParams& params)
    {
        return PLATFORM_RESULT_WINDOW_OPEN_ERROR;
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
                res = OpenWindowOpenGL(window, params);
                break;
            case PLATFORM_GRAPHICS_API_VULKAN:
                res = OpenWindowVulkan(window, params);
                break;
            default: assert(0);
        }

        if (res == PLATFORM_RESULT_OK)
        {
            window->m_WindowOpened = 1;
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
    }

    void PollEvents(HWindow window)
    {
        glfwPollEvents();
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

    void SetWindowSize(HWindow window, uint32_t width, uint32_t height)
    {
        glfwSetWindowSize(window->m_Window, (int) width, (int) height);
        int window_width, window_height;
        glfwGetWindowSize(window->m_Window, &window_width, &window_height);
        window->m_Width  = window_width;
        window->m_Height = window_height;

        // The callback is not called from glfw when the size is set manually
        if (window->m_ResizeCallback)
        {
            window->m_ResizeCallback(window->m_ResizeCallbackUserData, window_width, window_height);
        }
    }

    float GetDisplayScaleFactor(HWindow window)
    {
        return 1.0f;
    }

    void* AcquireAuxContext(HWindow window)
    {
        return 0;
    }

    void UnacquireAuxContext(HWindow window, void* aux_context)
    {
    }

    uint32_t GetWindowStateParam(HWindow window, WindowState state)
    {
        switch(state)
        {
            case WINDOW_STATE_OPENED: return window->m_WindowOpened;
        }

        return 0;
        /*
        switch(state)
        {
            case WINDOW_STATE_REFRESH_RATE: return glfwGetWindowRefreshRate();
            case WINDOW_STATE_SAMPLE_COUNT: return window->m_Samples;
            case WINDOW_STATE_HIGH_DPI:     return window->m_HighDPI;
            case WINDOW_STATE_AUX_CONTEXT:  return glfwQueryAuxContext();
            default:break;
        }

        return window->m_WindowOpened ? glfwGetWindowParam(WindowStateToGLFW(state)) : 0;
        */
    }
}
