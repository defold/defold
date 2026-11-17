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

#include "platform_window_glfw3_private.h"
#include "window_state.h"
#include <dlib/hash.h>
#include <dlib/math.h>

namespace dmPlatform
{
    WindowPlacement*& PlacementStorage(HWindow window) { return window->m_Placement; }
    const char* PlacementBackend()
    {
        switch (glfwGetPlatform())
        {
            case GLFW_PLATFORM_WIN32: return "win32";
            case GLFW_PLATFORM_X11: return "x11";
            case GLFW_PLATFORM_COCOA: return "cocoa";
            default: return "wayland";
        }
    }

    void ApplyPlacementSize(HWindow window, int width, int height)
    {
        glfwSetWindowSize(window->m_Window, width, height);
    }

    static bool HasPosition()
    {
        return glfwGetPlatform() != GLFW_PLATFORM_WAYLAND;
    }

    bool NormalPlacementWindow(HWindow window)
    {
        return !window->m_FullscreenWindowed && !glfwGetWindowMonitor(window->m_Window)
            && !glfwGetWindowAttrib(window->m_Window, GLFW_ICONIFIED)
            && !glfwGetWindowAttrib(window->m_Window, GLFW_MAXIMIZED);
    }

    static bool ReadMonitor(GLFWmonitor* monitor, dmWindowState::Monitor* result)
    {
        glfwGetMonitorWorkarea(monitor, &result->m_WorkArea.m_X, &result->m_WorkArea.m_Y,
                               &result->m_WorkArea.m_Width, &result->m_WorkArea.m_Height);
        if (result->m_WorkArea.m_Width <= 0 || result->m_WorkArea.m_Height <= 0)
        {
            return false;
        }
        glfwGetMonitorContentScale(monitor, &result->m_ScaleX, &result->m_ScaleY);
        // Windows can still enumerate a monitor while it is being activated or
        // disconnected. GLFW returns zero when its DPI query fails.
        if (!(result->m_ScaleX > 0.0f && result->m_ScaleY > 0.0f))
        {
            return false;
        }
        result->m_NameHash = dmHashString64(glfwGetMonitorName(monitor));
        return true;
    }

    static GLFWmonitor* ContainingMonitor(const dmWindowState::Rect& rect)
    {
        int count;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        GLFWmonitor* best = glfwGetPrimaryMonitor();
        int64_t largest_overlap = 0;
        for (int i = 0; i < count; ++i)
        {
            int x, y;
            glfwGetMonitorPos(monitors[i], &x, &y);
            const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
            if (!mode)
                continue;
            int64_t width = dmMath::Max(0, dmMath::Min(rect.m_X + rect.m_Width, x + mode->width) - dmMath::Max(rect.m_X, x));
            int64_t height = dmMath::Max(0, dmMath::Min(rect.m_Y + rect.m_Height, y + mode->height) - dmMath::Max(rect.m_Y, y));
            if (width * height > largest_overlap)
            {
                largest_overlap = width * height;
                best = monitors[i];
            }
        }
        return best;
    }

    static void ClampToMonitor(HWindow window, GLFWmonitor* monitor, dmWindowState::Rect rect)
    {
        dmWindowState::Rect work;
        glfwGetMonitorWorkarea(monitor, &work.m_X, &work.m_Y, &work.m_Width, &work.m_Height);
        if (work.m_Width <= 0 || work.m_Height <= 0)
        {
            return;
        }
        int left, top, right, bottom;
        glfwGetWindowFrameSize(window->m_Window, &left, &top, &right, &bottom);
        int x = rect.m_X, y = rect.m_Y;
        dmWindowState::ClampPosition(&rect, work, left, top, right, bottom);
        if (glfwGetPlatform() == GLFW_PLATFORM_WIN32)
        {
            // Win32 moves are synchronous, but changing DPI can shift the
            // client origin when the title bar and borders change size.
            glfwGetWindowPos(window->m_Window, &x, &y);
        }
        if (rect.m_X != x || rect.m_Y != y)
        {
            glfwSetWindowPos(window->m_Window, rect.m_X, rect.m_Y);
        }
    }

    bool ReadPlacementGeometry(HWindow window, dmWindowState::State* state)
    {
        glfwGetWindowSize(window->m_Window, &state->m_Rect.m_Width, &state->m_Rect.m_Height);
        if (state->m_Rect.m_Width <= 0 || state->m_Rect.m_Height <= 0)
            return false;
        state->m_HasPosition = HasPosition();
        if (state->m_HasPosition)
        {
            glfwGetWindowPos(window->m_Window, &state->m_Rect.m_X, &state->m_Rect.m_Y);
            GLFWmonitor* monitor = ContainingMonitor(state->m_Rect);
            if (!monitor)
            {
                return false;
            }
            if (!ReadMonitor(monitor, &state->m_Monitor))
            {
                return false;
            }
        }
        glfwGetWindowContentScale(window->m_Window, &state->m_Monitor.m_ScaleX, &state->m_Monitor.m_ScaleY);
        if (!(state->m_Monitor.m_ScaleX > 0.0f && state->m_Monitor.m_ScaleY > 0.0f))
        {
            // Keep the last valid geometry and retry on the next check.
            return false;
        }
        return true;
    }

    bool RestorePlacementGeometry(HWindow window, const dmWindowState::State& saved, bool restore_size)
    {
        dmWindowState::Rect rect = saved.m_Rect;
        GLFWmonitor* restored_monitor = 0;
        if (HasPosition() && saved.m_HasPosition)
        {
            int count;
            GLFWmonitor** handles = glfwGetMonitors(&count);
            if (!count)
                return false;
            dmArray<dmWindowState::Monitor> monitors;
            dmArray<GLFWmonitor*> available_handles;
            monitors.SetCapacity(count);
            available_handles.SetCapacity(count);
            for (int i = 0; i < count; ++i)
            {
                dmWindowState::Monitor monitor;
                if (ReadMonitor(handles[i], &monitor))
                {
                    monitors.Push(monitor);
                    available_handles.Push(handles[i]);
                }
            }
            if (monitors.Empty())
            {
                return false;
            }
            uint32_t monitor_index = dmWindowState::SelectMonitor(saved, monitors.Begin(), monitors.Size());
            restored_monitor = available_handles[monitor_index];
            const dmWindowState::Monitor& monitor = monitors[monitor_index];
            bool scale_coordinates = glfwGetPlatform() != GLFW_PLATFORM_COCOA;
            rect = dmWindowState::RestoreRect(saved, monitor, scale_coordinates);
            // Move first so GLFW can perform its monitor/DPI transition. Saved
            // dimensions are already scaled screen units, so apply them through
            // the setter rather than feeding them back through creation hints.
            glfwSetWindowPos(window->m_Window, rect.m_X, rect.m_Y);
            if (restore_size)
                glfwSetWindowSize(window->m_Window, rect.m_Width, rect.m_Height);
            else
                glfwGetWindowSize(window->m_Window, &rect.m_Width, &rect.m_Height);
            // Keep the selected monitor even if a smaller work area puts the
            // restored offset completely outside its current bounds. Clamp the
            // requested rectangle: X11 may still report the previous position
            // while the window manager processes the move asynchronously.
            ClampToMonitor(window, restored_monitor, rect);
        }
        else if (restore_size)
        {
            glfwSetWindowSize(window->m_Window, rect.m_Width, rect.m_Height);
        }
        if (glfwGetPlatform() == GLFW_PLATFORM_WIN32)
        {
            // The initial move can leave the larger startup window mostly on
            // another monitor. Resizing or clamping it can then trigger a DPI
            // transition that scales the saved screen dimensions again. Once
            // those transitions have completed, enforce the requested size.
            if (restore_size)
            {
                int width, height;
                glfwGetWindowSize(window->m_Window, &width, &height);
                if (width != rect.m_Width || height != rect.m_Height)
                {
                    glfwSetWindowSize(window->m_Window, rect.m_Width, rect.m_Height);
                }
            }
            if (restored_monitor)
            {
                // Reapply the client position with the final DPI's frame
                // insets, including when a reset discards the saved size.
                ClampToMonitor(window, restored_monitor, rect);
            }
        }
        return true;
    }

    void ClampWindowPlacement(HWindow window)
    {
        if (!window->m_Placement || !HasPosition() || !NormalPlacementWindow(window))
            return;
        dmWindowState::Rect rect;
        glfwGetWindowPos(window->m_Window, &rect.m_X, &rect.m_Y);
        glfwGetWindowSize(window->m_Window, &rect.m_Width, &rect.m_Height);
        GLFWmonitor* handle = ContainingMonitor(rect);
        if (handle)
        {
            ClampToMonitor(window, handle, rect);
            if (glfwGetPlatform() == GLFW_PLATFORM_WIN32)
            {
                // Moving off a disconnected monitor can change both the DPI
                // and frame insets. Clamp again using the resulting bounds.
                glfwGetWindowPos(window->m_Window, &rect.m_X, &rect.m_Y);
                glfwGetWindowSize(window->m_Window, &rect.m_Width, &rect.m_Height);
                ClampToMonitor(window, handle, rect);
            }
        }
    }

}
