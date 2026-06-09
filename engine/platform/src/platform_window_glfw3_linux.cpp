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

#include <glfw/glfw3.h>

// TODO: Wayland support.
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
        // NOP
    }

    void SetWindowsIconNative(HWindow window)
    {
        // NOP
    }

    void SetWindowedFullscreenFocusNative(HWindow, bool)
    {
        // NOP
    }

    void SetWindowedSizeFromSettingsNative(HWindow, int32_t, int32_t)
    {
        // NOP
    }

    void SetFullscreenWindowModeParamsNative(GLFWmonitor* monitor, const GLFWvidmode* mode, WindowModeParams* mode_params)
    {
        mode_params->m_Width  = mode->width;
        mode_params->m_Height = mode->height;

        if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
        {
            glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
            mode_params->m_Monitor = monitor;
            return;
        }

        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwGetMonitorPos(monitor, &mode_params->m_X, &mode_params->m_Y);
        mode_params->m_WindowedFullscreen = true;
    }

    bool CanSetOpenGLCoreProfileHintNative(bool)
    {
        return true;
    }
}
