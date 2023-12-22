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
        /*
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
        uint32_t                      m_Samples               : 8;
        uint32_t                      m_WindowOpened          : 1;
        uint32_t                      m_SwapIntervalSupported : 1;
        uint32_t                      m_HighDPI               : 1;
        */

        uint32_t                      m_WindowOpened          : 1;
    };

    HWindow NewWindow()
    {
        if (glfwInit() == GL_FALSE)
        {
            dmLogError("Could not initialize glfw.");
            return 0;
        }

        Window* wnd = new Window;
        memset(wnd, 0, sizeof(Window));

        return wnd;
    }

    void DeleteWindow(HWindow window)
    {
        delete window;
    }

    PlatformResult OpenWindowOpenGL(Window* wnd, const WindowParams& params)
    {
        wnd->m_Window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);

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
}
