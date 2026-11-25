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

#include "window_wayland.h"
#include <dlib/math.h>

namespace dmPlatform
{
    WindowPlacement*& PlacementStorage(HWindow w)
    {
        return w->m_Placement;
    }

    const char* PlacementBackend(HWindow w)
    {
        return w->m_Ops->m_Name;
    }

    bool NormalPlacementWindow(HWindow w)
    {
        const LinuxWayland* wl = w->m_Wayland;
        return !w->m_Params.m_Fullscreen && !w->m_Iconified && !w->m_Maximized &&
               (!wl || (!wl->m_ShellFullscreen && !wl->m_ShellTiled));
    }

    void ApplyPlacementSize(HWindow w, int width, int height)
    {
        w->m_Ops->m_Size(w, width, height);
    }

    static unsigned Containing(const dmWindowState::Rect& rect, const dmArray<dmWindowState::Monitor>& monitors)
    {
        unsigned best = 0;
        int64_t overlap = 0;
        for (unsigned i = 0; i < monitors.Size(); ++i)
        {
            const dmWindowState::Rect& r = monitors[i].m_WorkArea;
            int width = dmMath::Min(rect.m_X + rect.m_Width, r.m_X + r.m_Width) - dmMath::Max(rect.m_X, r.m_X);
            int height = dmMath::Min(rect.m_Y + rect.m_Height, r.m_Y + r.m_Height) - dmMath::Max(rect.m_Y, r.m_Y);
            if (width > 0 && height > 0 && (int64_t)width * height > overlap)
            {
                best = i;
                overlap = (int64_t)width * height;
            }
        }

        return best;
    }

    bool ReadPlacementGeometry(HWindow w, dmWindowState::State* state, bool flush)
    {
        if (flush && w->m_X11)
        {
            X11FlushPlacement(w);
        }
        state->m_Rect.m_Width = w->m_Width;
        state->m_Rect.m_Height = w->m_Height;
        state->m_HasPosition = w->m_X11 != 0;
        if (state->m_HasPosition)
        {
            X11GetPosition(w, &state->m_Rect.m_X, &state->m_Rect.m_Y);
            dmArray<dmWindowState::Monitor> monitors;
            X11Monitors(w, monitors);
            state->m_Monitor = monitors[Containing(state->m_Rect, monitors)];
        }

        state->m_Monitor.m_ScaleX = state->m_Monitor.m_ScaleY = w->m_Scale;
        return state->m_Rect.m_Width > 0 && state->m_Rect.m_Height > 0;
    }

    static void Clamp(HWindow w, dmWindowState::Rect* r, const dmWindowState::Monitor& mon)
    {
        int l = 0, t = 0, right = 0, b = 0;
        X11FrameExtents(w, &l, &t, &right, &b);
        dmWindowState::ClampPosition(r, mon.m_WorkArea, l, t, right, b);
    }

    void ClampWindowPlacement(HWindow w)
    {
        if (!w->m_X11 || !w->m_Placement || !NormalPlacementWindow(w))
        {
            return;
        }

        dmWindowState::State state = {};
        if (!ReadPlacementGeometry(w, &state))
        {
            return;
        }

        dmWindowState::Rect r = state.m_Rect;
        Clamp(w, &r, state.m_Monitor);
        if (r.m_X != state.m_Rect.m_X || r.m_Y != state.m_Rect.m_Y)
        {
            X11SetPosition(w, r.m_X, r.m_Y);
        }
    }

    bool RestorePlacementGeometry(HWindow w, const dmWindowState::State& saved, bool size)
    {
        dmWindowState::Rect r = saved.m_Rect;
        if (w->m_X11 && saved.m_HasPosition)
        {
            dmArray<dmWindowState::Monitor> monitors;
            X11Monitors(w, monitors);
            const dmWindowState::Monitor& mon = monitors[dmWindowState::SelectMonitor(saved, monitors.Begin(), monitors.Size())];
            r = dmWindowState::RestoreRect(saved, mon, true);
            if (!size)
            {
                r.m_Width = w->m_Width;
                r.m_Height = w->m_Height;
            }

            Clamp(w, &r, mon);
            X11SetPosition(w, r.m_X, r.m_Y);
        }

        if (size)
        {
            ApplyPlacementSize(w, r.m_Width, r.m_Height);
        }

        return true;
    }

}
