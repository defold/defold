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

#define GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_EXPOSE_NATIVE_NSGL
#include <glfw/glfw3native.h>

#include <AppKit/AppKit.h>

#include "platform_window_osx.h"

#include "platform_window_glfw3_private.h"

namespace dmPlatform
{
    id GetOSXNSWindow(HWindow window)
    {
        return glfwGetCocoaWindow(window->m_Window);
    }

    id GetOSXNSView(HWindow window)
    {
        return glfwGetCocoaView(window->m_Window);
    }

    id GetOSXNSOpenGLContext(HWindow window)
    {
        return glfwGetNSGLContext(window->m_Window);
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

    void SetWindowedFullscreenFocusNative(HWindow window, bool focused)
    {
        NSWindow* ns_window = (NSWindow*) glfwGetCocoaWindow(window->m_Window);
        if (!ns_window)
        {
            return;
        }

        if (focused)
        {
            [ns_window setLevel:NSMainMenuWindowLevel + 1];
            [ns_window setHasShadow:NO];
            [ns_window makeKeyAndOrderFront:nil];
        }
        else
        {
            [ns_window setLevel:NSNormalWindowLevel];
        }
    }

    void SetWindowedSizeFromSettingsNative(HWindow window, int32_t width, int32_t height)
    {
        glfwSetWindowSize(window->m_Window, width, height);
    }

    // note: on win32 and linux we need to set GLFW_SCALE_TO_MONITOR to GLFW_FALSE
    // but this is not necessary on osx because the GLFW Cocoa backend ignores
    // GLFW_SCALE_TO_MONITOR and treats window dimensions as logical points (https://github.com/glfw/glfw/blob/3.4/src/cocoa_window.m#L734-L757).
    void SetFullscreenWindowModeParamsNative(GLFWmonitor* monitor, const GLFWvidmode* mode, WindowModeParams* mode_params)
    {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwGetMonitorPos(monitor, &mode_params->m_X, &mode_params->m_Y);
        mode_params->m_Width              = mode->width;
        mode_params->m_Height             = mode->height;
        mode_params->m_WindowedFullscreen = true;
    }

    bool CanSetOpenGLCoreProfileHintNative(bool)
    {
        return true;
    }

    void InstallWindowCloseHandlerNative(HWindow)
    {
        // NOP
    }

    void UninstallWindowCloseHandlerNative(HWindow)
    {
        // NOP
    }
}
