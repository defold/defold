// Copyright 2020-2024 The Defold Foundation
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

#include <glfw/glfw3.h>

// TODO: Wayland?
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#include <glfw/glfw3native.h>

#include "platform_window_linux.h"
#include "platform_window_glfw3_private.h"

namespace dmPlatform
{
    Window GetX11Window(HWindow window)
    {
    	return glfwGetX11Window(window->m_Window);
    }

    GLXContext GetX11GLXContext(HWindow window)
    {
    	return glfwGetGLXContext(window->m_Window);
    }

    void FocusWindowNative(HWindow window)
    {
        glfwFocusWindow(window->m_Window);
    }

    void CenterWindowNative(HWindow wnd, GLFWmonitor* monitor)
    {
        /*
        if (!monitor)
            return;

        const GLFWvidmode* video_mode = glfwGetVideoMode(monitor);
        if (!video_mode)
            return;

        int32_t x = video_mode->width/2 - wnd->m_Width/2;
        int32_t y = video_mode->height/2 - wnd->m_Height/2;
        glfwSetWindowPos(wnd->m_Window, x, y);
        */
    }
}
