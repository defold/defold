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

#include "window_macos.h"
#include "../window_state.h"
#include <dlib/hash.h>
#include <math.h>

namespace dmPlatform
{
    WindowPlacement*& PlacementStorage(HWindow window)
    {
        return window->m_Placement;
    }

    const char* PlacementBackend(HWindow)
    {
        return "cocoa";
    }

    // Stored desktop coordinates start at the top left of the primary display.
    static dmWindowState::Rect ScreenRect(NSRect rect)
    {
        dmWindowState::Rect result;
        result.m_X = (int)NSMinX(rect);
        result.m_Y = (int)(NSMaxY([NSScreen screens].firstObject.frame) - NSMaxY(rect));
        result.m_Width = (int)NSWidth(rect);
        result.m_Height = (int)NSHeight(rect);
        return result;
    }

    static dmWindowState::Monitor Monitor(NSScreen* screen)
    {
        dmWindowState::Monitor result;
        result.m_WorkArea = ScreenRect(screen.visibleFrame);
        result.m_NameHash = dmHashString64(screen.localizedName.UTF8String);
        result.m_ScaleX = result.m_ScaleY = screen.backingScaleFactor;
        return result;
    }

    static NSRect ContentRect(HWindow window)
    {
        return [window->m_Window contentRectForFrameRect:window->m_Window.frame];
    }

    void SetCocoaWindowPosition(HWindow window, int x, int y)
    {
        NSRect content = ContentRect(window);
        content.origin.x = x;
        content.origin.y = NSMaxY([NSScreen screens].firstObject.frame) - y - NSHeight(content);
        [window->m_Window setFrameOrigin:[window->m_Window frameRectForContentRect:content].origin];
    }

    void ApplyPlacementSize(HWindow window, int width, int height)
    {
        [window->m_Window setContentSize:NSMakeSize(width, height)];
    }

    bool NormalPlacementWindow(HWindow window)
    {
        NSWindow* native = window->m_Window;
        return native && !window->m_Params.m_Fullscreen && !(native.styleMask & NSWindowStyleMaskFullScreen) && !native.miniaturized && !native.zoomed;
    }

    bool ReadPlacementGeometry(HWindow window, dmWindowState::State* state)
    {
        NSScreen* screen = window->m_Window.screen;
        if (!screen)
        {
            return false;
        }

        state->m_Rect = ScreenRect(ContentRect(window));
        state->m_HasPosition = true;
        state->m_Monitor = Monitor(screen);
        return state->m_Rect.m_Width > 0 && state->m_Rect.m_Height > 0;
    }

    static void ClampToScreen(HWindow window, NSScreen* screen)
    {
        if (!screen)
        {
            return;
        }

        NSRect frame = window->m_Window.frame;
        NSRect content = ContentRect(window);
        dmWindowState::Rect rect = ScreenRect(content);
        int x = rect.m_X;
        int y = rect.m_Y;
        dmWindowState::ClampPosition(&rect, ScreenRect(screen.visibleFrame),
                                     NSMinX(content) - NSMinX(frame), NSMaxY(frame) - NSMaxY(content),
                                     NSMaxX(frame) - NSMaxX(content), NSMinY(content) - NSMinY(frame));
        if (rect.m_X != x || rect.m_Y != y)
        {
            SetCocoaWindowPosition(window, rect.m_X, rect.m_Y);
        }
    }

    void ClampWindowPlacement(HWindow window)
    {
        if (window->m_Placement && NormalPlacementWindow(window))
        {
            ClampToScreen(window, window->m_Window.screen ?: [NSScreen screens].firstObject);
        }
    }

    bool RestorePlacementGeometry(HWindow window, const dmWindowState::State& saved, bool restore_size)
    {
        NSArray<NSScreen*>* screens = [NSScreen screens];
        if (!screens.count)
        {
            return false;
        }

        dmWindowState::Rect rect = saved.m_Rect;
        NSScreen* screen = nil;
        if (saved.m_HasPosition)
        {
            dmArray<dmWindowState::Monitor> monitors;
            monitors.SetCapacity(screens.count);
            for (NSScreen* candidate in screens)
            {
                monitors.Push(Monitor(candidate));
            }

            uint32_t index = dmWindowState::SelectMonitor(saved, monitors.Begin(), monitors.Size());
            screen = screens[index];
            rect = dmWindowState::RestoreRect(saved, monitors[index], false);
        }

        if (restore_size)
        {
            ApplyPlacementSize(window, rect.m_Width, rect.m_Height);
        }

        if (screen)
        {
            SetCocoaWindowPosition(window, rect.m_X, rect.m_Y);
            ClampToScreen(window, screen);
        }

        return true;
    }

}
