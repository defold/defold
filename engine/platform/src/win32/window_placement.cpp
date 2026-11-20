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

#include "window_geometry.h"
#include <dlib/math.h>

namespace dmPlatform
{
    using dmWindowState::Rect;
    using dmWindowState::Monitor;

    const char* PlacementBackend(HWindow)
    {
        return "win32";
    }

    static bool Usable(const Monitor& monitor)
    {
        return monitor.m_WorkArea.m_Width > 0 && monitor.m_WorkArea.m_Height > 0 &&
               monitor.m_ScaleX > 0 && monitor.m_ScaleY > 0;
    }

    static const Monitor* Containing(const dmArray<Win32Display>& displays, const Rect& rect)
    {
        if (displays.Empty())
        {
            return 0;
        }

        uint32_t best = 0;
        int64_t largest = 0;
        for (uint32_t i = 0; i < displays.Size(); ++i)
        {
            const Rect& bounds = displays[i].m_Bounds;
            int width = dmMath::Min(rect.m_X + rect.m_Width, bounds.m_X + bounds.m_Width) - dmMath::Max(rect.m_X, bounds.m_X);
            int height = dmMath::Min(rect.m_Y + rect.m_Height, bounds.m_Y + bounds.m_Height) - dmMath::Max(rect.m_Y, bounds.m_Y);
            int64_t overlap = (int64_t)dmMath::Max(width, 0) * dmMath::Max(height, 0);
            if (overlap > largest)
            {
                best = i;
                largest = overlap;
            }
        }

        return &displays[best].m_Monitor;
    }

    bool ReadPlacementGeometry(HWindow window, dmWindowState::State* state)
    {
        state->m_Rect = Win32ClientRect(window);
        if (state->m_Rect.m_Width <= 0 || state->m_Rect.m_Height <= 0)
        {
            return false;
        }

        dmArray<Win32Display> displays;
        Win32GetDisplays(displays);
        const Monitor* monitor = Containing(displays, state->m_Rect);
        float scale = Win32WindowScale(window);
        if (!monitor || !Usable(*monitor) || scale <= 0)
        {
            return false;
        }

        state->m_Monitor = *monitor;
        state->m_Monitor.m_ScaleX = state->m_Monitor.m_ScaleY = scale;
        state->m_HasPosition = true;
        return true;
    }

    static void Clamp(HWindow window, Rect rect, const Rect& work)
    {
        Win32Frame frame = Win32FrameInsets(window);
        dmWindowState::ClampPosition(&rect, work, frame.m_Left, frame.m_Top, frame.m_Right, frame.m_Bottom);
        Rect current = Win32ClientRect(window);
        if (current.m_X != rect.m_X || current.m_Y != rect.m_Y)
        {
            Win32MoveClient(window, rect.m_X, rect.m_Y);
        }
    }

    bool RestorePlacementGeometry(HWindow window, const dmWindowState::State& saved, bool restore_size)
    {
        if (!saved.m_HasPosition)
        {
            if (restore_size)
            {
                ApplyPlacementSize(window, saved.m_Rect.m_Width, saved.m_Rect.m_Height);
            }

            return true;
        }

        dmArray<Win32Display> displays;
        Win32GetDisplays(displays);
        dmArray<Monitor> candidates;
        candidates.SetCapacity(displays.Size());
        for (uint32_t i = 0; i < displays.Size(); ++i)
        {
            if (Usable(displays[i].m_Monitor))
            {
                candidates.Push(displays[i].m_Monitor);
            }
        }

        if (candidates.Empty())
        {
            return false;
        }

        const Monitor& target = candidates[dmWindowState::SelectMonitor(saved, candidates.Begin(), candidates.Size())];
        Rect rect = dmWindowState::RestoreRect(saved, target, true);
        Win32MoveClient(window, rect.m_X, rect.m_Y);
        if (restore_size)
        {
            ApplyPlacementSize(window, rect.m_Width, rect.m_Height);
        }
        else
        {
            Rect current = Win32ClientRect(window);
            rect.m_Width = current.m_Width;
            rect.m_Height = current.m_Height;
        }

        Clamp(window, rect, target.m_WorkArea);
        // A resize or clamp can synchronously cross a DPI boundary. Reconcile with the final frame.
        Rect current = Win32ClientRect(window);
        if (restore_size && (current.m_Width != rect.m_Width || current.m_Height != rect.m_Height))
        {
            ApplyPlacementSize(window, rect.m_Width, rect.m_Height);
        }

        Clamp(window, rect, target.m_WorkArea);
        return true;
    }

    void ClampWindowPlacement(HWindow window)
    {
        if (!PlacementStorage(window) || !NormalPlacementWindow(window))
        {
            return;
        }

        dmArray<Win32Display> displays;
        Win32GetDisplays(displays);
        Rect rect = Win32ClientRect(window);
        const Monitor* monitor = Containing(displays, rect);
        if (monitor && Usable(*monitor))
        {
            Clamp(window, rect, monitor->m_WorkArea);
            Clamp(window, Win32ClientRect(window), monitor->m_WorkArea);
        }
    }

}
