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

#include <string.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/path.h>
#include <dlib/sys.h>
#include <dlib/time.h>

struct WindowPlacement
{
    dmWindowState::State m_Normal;
    char                m_Path[DMPATH_MAX_PATH];
    char                m_ResetPath[DMPATH_MAX_PATH];
    char                m_ResetToken[40];
    uint64_t            m_LastCheck;
    uint64_t            m_LastWrite;
    int32_t             m_ConfigWidth, m_ConfigHeight;
    bool                m_HaveNormal;
    bool                m_CanWrite;
    bool                m_Ready;
    bool                m_NeedsCapture;
    bool                m_Dirty;
    bool                m_ResetPending;
    bool                m_Warned;
};

namespace dmPlatform
{
    static bool HasPosition()
    {
        return glfwGetPlatform() != GLFW_PLATFORM_WAYLAND;
    }

    static bool NormalWindow(HWindow window)
    {
        return !window->m_FullscreenWindowed && !glfwGetWindowMonitor(window->m_Window)
            && !glfwGetWindowAttrib(window->m_Window, GLFW_ICONIFIED)
            && !glfwGetWindowAttrib(window->m_Window, GLFW_MAXIMIZED);
    }

    static bool SameRect(const dmWindowState::Rect& a, const dmWindowState::Rect& b)
    {
        return a.m_X == b.m_X && a.m_Y == b.m_Y && a.m_Width == b.m_Width && a.m_Height == b.m_Height;
    }

    static bool SameState(const dmWindowState::State& a, const dmWindowState::State& b)
    {
        return SameRect(a.m_Rect, b.m_Rect)
            && SameRect(a.m_Monitor.m_WorkArea, b.m_Monitor.m_WorkArea)
            && a.m_Monitor.m_NameHash == b.m_Monitor.m_NameHash
            && a.m_Monitor.m_ScaleX == b.m_Monitor.m_ScaleX && a.m_Monitor.m_ScaleY == b.m_Monitor.m_ScaleY
            && a.m_ConfigWidth == b.m_ConfigWidth && a.m_ConfigHeight == b.m_ConfigHeight
            && a.m_HasPosition == b.m_HasPosition && !strcmp(a.m_ResetToken, b.m_ResetToken);
    }

    static dmWindowState::Monitor ReadMonitor(GLFWmonitor* monitor)
    {
        dmWindowState::Monitor result = {};
        glfwGetMonitorWorkarea(monitor, &result.m_WorkArea.m_X, &result.m_WorkArea.m_Y,
                              &result.m_WorkArea.m_Width, &result.m_WorkArea.m_Height);
        glfwGetMonitorContentScale(monitor, &result.m_ScaleX, &result.m_ScaleY);
        result.m_NameHash = dmHashString64(glfwGetMonitorName(monitor));
        return result;
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
        dmWindowState::Monitor target = ReadMonitor(monitor);
        int left, top, right, bottom;
        glfwGetWindowFrameSize(window->m_Window, &left, &top, &right, &bottom);
        int x = rect.m_X, y = rect.m_Y;
        dmWindowState::ClampPosition(&rect, target.m_WorkArea, left, top, right, bottom);
        if (rect.m_X != x || rect.m_Y != y)
            glfwSetWindowPos(window->m_Window, rect.m_X, rect.m_Y);
    }

    void InitWindowPlacement(HWindow window, const WindowCreateParams& params)
    {
        const char* prefix = dmSys::GetEnv("DM_WINDOW_STATE");
        if (!prefix || !prefix[0])
            return;
        WindowPlacement* placement = new WindowPlacement;
        memset(placement, 0, sizeof(*placement));
        const char* backend = "cocoa";
        switch (glfwGetPlatform())
        {
            case GLFW_PLATFORM_WIN32: backend = "win32"; break;
            case GLFW_PLATFORM_X11: backend = "x11"; break;
            case GLFW_PLATFORM_WAYLAND: backend = "wayland"; break;
        }
        int length = dmSnPrintf(placement->m_Path, sizeof(placement->m_Path), "%s-%s.ini", prefix, backend);
        if (length < 0 || (uint32_t) length >= sizeof(placement->m_Path))
        {
            dmLogWarning("Editor window state path is too long.");
            delete placement;
            return;
        }
        char directory[DMPATH_MAX_PATH];
        dmPath::Dirname(prefix, directory, sizeof(directory));
        dmPath::Concat(directory, "reset-size", placement->m_ResetPath, sizeof(placement->m_ResetPath));
        dmWindowState::ReadResetToken(placement->m_ResetPath, placement->m_ResetToken, sizeof(placement->m_ResetToken));
        dmWindowState::LoadResult result = dmWindowState::Load(placement->m_Path, &placement->m_Normal);
        placement->m_HaveNormal = result == dmWindowState::LOAD_OK;
        placement->m_CanWrite = result != dmWindowState::LOAD_UNSUPPORTED_VERSION;
        placement->m_ConfigWidth = params.m_Width;
        placement->m_ConfigHeight = params.m_Height;
        placement->m_LastWrite = dmTime::GetMonotonicTime();
        window->m_Placement = placement;
    }

    bool RestoreWindowPlacement(HWindow window)
    {
        WindowPlacement* placement = window->m_Placement;
        if (!placement || !placement->m_HaveNormal || !NormalWindow(window))
            return false;
        const dmWindowState::State& saved = placement->m_Normal;
        dmWindowState::Rect rect = saved.m_Rect;
        bool restore_size = dmWindowState::RestoreSize(saved, placement->m_ConfigWidth, placement->m_ConfigHeight, placement->m_ResetToken);
        if (HasPosition() && saved.m_HasPosition)
        {
            int count;
            GLFWmonitor** handles = glfwGetMonitors(&count);
            if (!count)
                return false;
            dmArray<dmWindowState::Monitor> monitors;
            monitors.SetCapacity(count);
            for (int i = 0; i < count; ++i)
                monitors.Push(ReadMonitor(handles[i]));
            uint32_t monitor_index = dmWindowState::SelectMonitor(saved, monitors.Begin(), count);
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
            ClampToMonitor(window, handles[monitor_index], rect);
        }
        else if (restore_size)
        {
            glfwSetWindowSize(window->m_Window, rect.m_Width, rect.m_Height);
        }
        return true;
    }

    void ClampWindowPlacement(HWindow window)
    {
        if (!window->m_Placement || !HasPosition() || !NormalWindow(window))
            return;
        dmWindowState::Rect rect;
        glfwGetWindowPos(window->m_Window, &rect.m_X, &rect.m_Y);
        glfwGetWindowSize(window->m_Window, &rect.m_Width, &rect.m_Height);
        GLFWmonitor* handle = ContainingMonitor(rect);
        if (handle)
            ClampToMonitor(window, handle, rect);
    }

    void WindowPlacementChanged(HWindow window)
    {
        if (window->m_Placement)
            window->m_Placement->m_NeedsCapture = true;
    }

    void WindowPlacementSizeRequested(HWindow window)
    {
        WindowPlacement* placement = window->m_Placement;
        // Renderer setup may adjust a hidden window before it is ready. That
        // must not acknowledge a reset requested during startup.
        if (!placement || !placement->m_Ready)
            return;
        // The editor writes the reset token before sending its resize message.
        // Consume it here so a delayed token check cannot undo that resize, or
        // a later explicit size request, with the dimensions cached at launch.
        dmWindowState::ReadResetToken(placement->m_ResetPath, placement->m_ResetToken, sizeof(placement->m_ResetToken));
        placement->m_ResetPending = false;
        placement->m_NeedsCapture = true;
    }

    void WindowPlacementShown(HWindow window)
    {
        if (window->m_Placement)
        {
            window->m_Placement->m_Ready = true;
            window->m_Placement->m_NeedsCapture = true;
        }
    }

    void UpdateWindowPlacement(HWindow window, bool flush)
    {
        WindowPlacement* placement = window->m_Placement;
        if (!placement || !placement->m_Ready || !placement->m_CanWrite)
            return;
        uint64_t now = dmTime::GetMonotonicTime();
        // Work area changes do not necessarily emit a GLFW window callback.
        bool check = flush || now - placement->m_LastCheck >= 250000;
        if (check)
        {
            char token[40];
            dmStrlCpy(token, placement->m_ResetToken, sizeof(token));
            dmWindowState::ReadResetToken(placement->m_ResetPath, token, sizeof(token));
            if (strcmp(token, placement->m_ResetToken))
            {
                dmStrlCpy(placement->m_ResetToken, token, sizeof(placement->m_ResetToken));
                placement->m_ResetPending = true;
            }
            placement->m_LastCheck = now;
        }
        if ((check || placement->m_NeedsCapture) && NormalWindow(window))
        {
            if (placement->m_ResetPending)
            {
                glfwSetWindowSize(window->m_Window, placement->m_ConfigWidth, placement->m_ConfigHeight);
                ClampWindowPlacement(window);
                placement->m_ResetPending = false;
            }
            dmWindowState::State state = {};
            glfwGetWindowSize(window->m_Window, &state.m_Rect.m_Width, &state.m_Rect.m_Height);
            if (state.m_Rect.m_Width <= 0 || state.m_Rect.m_Height <= 0)
                return;
            state.m_HasPosition = HasPosition();
            if (state.m_HasPosition)
            {
                glfwGetWindowPos(window->m_Window, &state.m_Rect.m_X, &state.m_Rect.m_Y);
                GLFWmonitor* monitor = ContainingMonitor(state.m_Rect);
                if (!monitor)
                    return;
                state.m_Monitor = ReadMonitor(monitor);
            }
            glfwGetWindowContentScale(window->m_Window, &state.m_Monitor.m_ScaleX, &state.m_Monitor.m_ScaleY);
            state.m_ConfigWidth = placement->m_ConfigWidth;
            state.m_ConfigHeight = placement->m_ConfigHeight;
            dmStrlCpy(state.m_ResetToken, placement->m_ResetToken, sizeof(state.m_ResetToken));
            if (!placement->m_HaveNormal || !SameState(state, placement->m_Normal))
            {
                placement->m_Normal = state;
                placement->m_HaveNormal = true;
                placement->m_Dirty = true;
            }
            placement->m_NeedsCapture = false;
        }
        if (placement->m_Dirty && (flush || now - placement->m_LastWrite >= 500000))
        {
            if (dmWindowState::Save(placement->m_Path, placement->m_Normal))
                placement->m_Dirty = false;
            else if (!placement->m_Warned)
            {
                dmLogWarning("Could not save editor window state to '%s'.", placement->m_Path);
                placement->m_Warned = true;
            }
            placement->m_LastWrite = now;
        }
    }

    void DeleteWindowPlacement(HWindow window)
    {
        delete window->m_Placement;
        window->m_Placement = 0;
    }
}
