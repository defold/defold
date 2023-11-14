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

#include <dmsdk/graphics/glfw/glfw.h>
// #include <graphics/glfw/glfw_native.h>

#include "platform.h"

#include <dlib/platform.h>
#include <dlib/log.h>

namespace dmPlatform
{
    struct Context
    {
        int dummmy;
    };

    struct Window
    {
        WindowParams m_CreateParams;
        uint32_t m_WindowOpened : 1;
    };

    HContext NewContext()
    {
        Context* ctx = new Context;
        return (HContext) ctx;
    }

    void Update(HContext context)
    {
        Context* ctx = (Context*) context;
        glfwPollEvents();
    }

    HWindow NewWindow(HContext context, const WindowParams& params)
    {
        Window* wnd         = new Window;
        wnd->m_CreateParams = params;
        wnd->m_WindowOpened = 0;

        if (glfwInit() == GL_FALSE)
        {
            dmLogError("Could not initialize glfw.");
            return 0;
        }

        return (void*) wnd;
    }

    PlatformResult OpenWindowOpenGL(Context* context, Window* wnd)
    {
        if (wnd->m_CreateParams.m_HighDPI)
        {
            glfwOpenWindowHint(GLFW_WINDOW_HIGH_DPI, 1);
        }

        glfwOpenWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwOpenWindowHint(GLFW_FSAA_SAMPLES, wnd->m_CreateParams.m_Samples);

#if defined(ANDROID)
        // Seems to work fine anyway without any hints
        // which is good, since we want to fallback from OpenGLES 3 to 2
#elif defined(__linux__)
        glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
        glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 3);
#elif defined(_WIN32)
        glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
        glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 3);
#elif defined(__MACH__)
        glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
    #if defined(DM_PLATFORM_IOS)
        glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 0); // 3.0 on iOS
    #else
        glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 2); // 3.2 on macOS (actually picks 4.1 anyways)
    #endif
#endif

        bool is_desktop = false;
#if defined(DM_PLATFORM_WINDOWS) || (defined(DM_PLATFORM_LINUX) && !defined(ANDROID)) || defined(DM_PLATFORM_MACOS)
        is_desktop = true;
#endif
        if (is_desktop) {
            glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
            glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        }

        int mode = GLFW_WINDOW;
        if (wnd->m_CreateParams.m_FullScreen)
        {
            mode = GLFW_FULLSCREEN;
        }
        if (!glfwOpenWindow(wnd->m_CreateParams.m_Width, wnd->m_CreateParams.m_Height, 8, 8, 8, 8, 32, 8, mode))
        {
            if (is_desktop)
            {
                dmLogWarning("Trying OpenGL 3.1 compat mode");

                // Try a second time, this time without core profile, and lower the minor version.
                // And GLFW clears hints, so we have to set them again.
                if (wnd->m_CreateParams.m_HighDPI) {
                    glfwOpenWindowHint(GLFW_WINDOW_HIGH_DPI, 1);
                }
                glfwOpenWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
                glfwOpenWindowHint(GLFW_FSAA_SAMPLES, wnd->m_CreateParams.m_Samples);

                // We currently cannot go lower since we support shader model 140
                glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
                glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 1);

                glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

                if (!glfwOpenWindow(wnd->m_CreateParams.m_Width, wnd->m_CreateParams.m_Height, 8, 8, 8, 8, 32, 8, mode))
                {
                    return PLATFORM_RESULT_WINDOW_OPEN_ERROR;
                }
            }
            else
            {
                return PLATFORM_RESULT_WINDOW_OPEN_ERROR;
            }
        }

        wnd->m_WindowOpened = 1;

        return PLATFORM_RESULT_OK;
    }

    PlatformResult OpenWindowVulkan(Context* context, Window* wnd)
    {
        glfwOpenWindowHint(GLFW_CLIENT_API,   GLFW_NO_API);
        glfwOpenWindowHint(GLFW_FSAA_SAMPLES, wnd->m_CreateParams.m_Samples);

        int mode = wnd->m_CreateParams.m_FullScreen ? GLFW_FULLSCREEN : GLFW_WINDOW;

        if (!glfwOpenWindow(wnd->m_CreateParams.m_Width, wnd->m_CreateParams.m_Height, 8, 8, 8, 8, 32, 8, mode))
        {
            return PLATFORM_RESULT_WINDOW_OPEN_ERROR;
        }

        /*
        if (!InitializeVulkan(context, wnd))
        {
            return PLATFORM_RESULT_WINDOW_OPEN_ERROR;
        }

        glfwSetWindowTitle(wnd->m_CreateParams.m_Title);
        glfwSetWindowBackgroundColor(wnd->m_CreateParams.m_BackgroundColor);

        glfwSetWindowSizeCallback(OnWindowResize);
        glfwSetWindowCloseCallback(OnWindowClose);
        glfwSetWindowFocusCallback(OnWindowFocus);
        */

        wnd->m_WindowOpened = 1;

        return PLATFORM_RESULT_OK;
    }

    PlatformResult OpenWindow(HContext context, HWindow window)
    {
        Window* wnd = (Window*) window;
        Context* ctx = (Context*) context;

        if (wnd->m_WindowOpened)
        {
            return PLATFORM_RESULT_WINDOW_ALREADY_OPENED;
        }

        switch(wnd->m_CreateParams.m_GraphicsApi)
        {
        case PLATFORM_GRAPHICS_API_OPENGL: return OpenWindowOpenGL(ctx, wnd);
        case PLATFORM_GRAPHICS_API_VULKAN: return OpenWindowVulkan(ctx, wnd);
        default: assert(0);
        }

        return PLATFORM_RESULT_WINDOW_OPEN_ERROR;
    }

    void DeleteWindow(HWindow window)
    {
        Window* wnd = (Window*) window;
        delete wnd;
    }
}
