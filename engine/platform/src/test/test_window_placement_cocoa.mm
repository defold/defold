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

#import <AppKit/AppKit.h>

#include "../platform_window_osx.h"
#include "test_window_placement_native.h"

namespace dmWindowTest
{
    const char* Backend()
    {
        return "cocoa";
    }

    void Position(HWindow handle, int* x, int* y)
    {
        NSWindow* window = dmPlatform::GetOSXNSWindow(handle);
        NSRect rect = [window contentRectForFrameRect:window.frame];
        *x = NSMinX(rect);
        *y = NSMaxY([NSScreen screens].firstObject.frame) - NSMaxY(rect);
    }

    void ClientSize(HWindow handle, int* width, int* height)
    {
        NSView* view = dmPlatform::GetOSXNSView(handle);
        *width = NSWidth(view.bounds);
        *height = NSHeight(view.bounds);
    }

    void DrawableSize(HWindow handle, int* width, int* height)
    {
        NSView* view = dmPlatform::GetOSXNSView(handle);
        NSRect bounds = [view convertRectToBacking:view.bounds];
        *width = NSWidth(bounds);
        *height = NSHeight(bounds);
    }

    int MonitorCount()
    {
        return (int)[NSScreen screens].count;
    }

    void WorkArea(int index, int* x, int* y, int* width, int* height)
    {
        NSArray<NSScreen*>* screens = [NSScreen screens];
        NSRect work = screens[index].visibleFrame;
        *x = NSMinX(work);
        *y = NSMaxY(screens.firstObject.frame) - NSMaxY(work);
        *width = NSWidth(work);
        *height = NSHeight(work);
    }

}
