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

#include "window_placement.h"
#include "window_state.h"
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/sys.h>
#include <dlib/time.h>
#include <string.h>

struct WindowPlacement
{
    dmWindowState::State m_Normal;
    char                 m_Path[DMPATH_MAX_PATH];
    char                 m_ResetPath[DMPATH_MAX_PATH];
    char                 m_ResetToken[40];
    uint64_t             m_LastCheck;
    uint64_t             m_LastWrite;
    uint32_t             m_MonitorRevision;
    int32_t              m_ConfigWidth, m_ConfigHeight;
    bool                 m_HaveNormal;
    bool                 m_CanWrite;
    bool                 m_Ready;
    bool                 m_NeedsCapture;
    bool                 m_Dirty;
    bool                 m_ResetPending;
    bool                 m_Warned;
};

namespace dmPlatform
{
    static uint32_t g_MonitorRevision;

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

    void InitWindowPlacement(HWindow window, const WindowCreateParams& params)
    {
        const char* prefix = dmSys::GetEnv("DM_WINDOW_STATE");
        if (!prefix || !prefix[0])
        {
            return;
        }

        WindowPlacement* placement = new WindowPlacement;
        memset(placement, 0, sizeof(*placement));
        int length = dmSnPrintf(placement->m_Path, sizeof(placement->m_Path), "%s-%s.ini", prefix, PlacementBackend(window));
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
        placement->m_MonitorRevision = g_MonitorRevision;
        PlacementStorage(window) = placement;
    }

    bool RestoreWindowPlacement(HWindow window)
    {
        WindowPlacement* placement = PlacementStorage(window);
        if (!placement || !placement->m_HaveNormal || !NormalPlacementWindow(window))
        {
            return false;
        }

        const dmWindowState::State& saved = placement->m_Normal;
        bool restore_size = dmWindowState::RestoreSize(saved, placement->m_ConfigWidth, placement->m_ConfigHeight, placement->m_ResetToken);
        return RestorePlacementGeometry(window, saved, restore_size);
    }

    void WindowPlacementMonitorsChanged()
    {
        ++g_MonitorRevision;
    }

    void WindowPlacementChanged(HWindow window)
    {
        if (PlacementStorage(window))
        {
            PlacementStorage(window)->m_NeedsCapture = true;
        }
    }

    void WindowPlacementSizeRequested(HWindow window)
    {
        WindowPlacement* placement = PlacementStorage(window);
        // Renderer setup may adjust a hidden window before it is ready. That
        // must not acknowledge a reset requested during startup.
        if (!placement || !placement->m_Ready)
        {
            return;
        }

        // The editor writes the reset token before sending its resize message.
        // Consume it here so a delayed token check cannot undo that resize, or
        // a later explicit size request, with the dimensions cached at launch.
        dmWindowState::ReadResetToken(placement->m_ResetPath, placement->m_ResetToken, sizeof(placement->m_ResetToken));
        placement->m_ResetPending = false;
        placement->m_NeedsCapture = true;
    }

    void WindowPlacementShown(HWindow window)
    {
        if (PlacementStorage(window))
        {
            PlacementStorage(window)->m_Ready = true;
            PlacementStorage(window)->m_NeedsCapture = true;
        }
    }

    void UpdateWindowPlacement(HWindow window, bool flush)
    {
        WindowPlacement* placement = PlacementStorage(window);
        if (!placement || !placement->m_Ready || !placement->m_CanWrite)
        {
            return;
        }

        uint64_t now = dmTime::GetMonotonicTime();
        // Work area changes do not necessarily emit a window callback.
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

        if ((check || placement->m_NeedsCapture) && NormalPlacementWindow(window))
        {
            placement->m_NeedsCapture = false;
            if (placement->m_ResetPending)
            {
                ApplyPlacementSize(window, placement->m_ConfigWidth, placement->m_ConfigHeight);
                ClampWindowPlacement(window);
                placement->m_ResetPending = false;
            }

            if (placement->m_MonitorRevision != g_MonitorRevision)
            {
                // A window left outside the remaining displays must stay
                // reachable, including when a display is unplugged at runtime.
                ClampWindowPlacement(window);
            }

            dmWindowState::State state = {};
            if (!ReadPlacementGeometry(window, &state))
            {
                return;
            }

            state.m_ConfigWidth = placement->m_ConfigWidth;
            state.m_ConfigHeight = placement->m_ConfigHeight;
            dmStrlCpy(state.m_ResetToken, placement->m_ResetToken, sizeof(state.m_ResetToken));
            if (!placement->m_HaveNormal || !SameState(state, placement->m_Normal))
            {
                placement->m_Normal = state;
                placement->m_HaveNormal = true;
                placement->m_Dirty = true;
            }

            placement->m_MonitorRevision = g_MonitorRevision;
        }

        if (placement->m_Dirty && (flush || now - placement->m_LastWrite >= 500000))
        {
            if (dmWindowState::Save(placement->m_Path, placement->m_Normal))
            {
                placement->m_Dirty = false;
            }
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
        delete PlacementStorage(window);
        PlacementStorage(window) = 0;
    }

}
