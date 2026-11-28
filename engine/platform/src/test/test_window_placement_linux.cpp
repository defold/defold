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

#include "../linux/window_linux.h"
#include "test_window_placement_native.h"

namespace dmWindowTest
{
    const char* Backend()
    {
        return dmPlatform::LinuxActiveWindow()->m_Ops->m_Name;
    }

    void Position(HWindow w, int* x, int* y)
    {
        Window child;
        XTranslateCoordinates(w->m_Display, w->m_Xid, DefaultRootWindow(w->m_Display), 0, 0, x, y, &child);
    }

    void ClientSize(HWindow w, int* width, int* height)
    {
        if (w->m_Xid)
        {
            XWindowAttributes attrs;
            XGetWindowAttributes(w->m_Display, w->m_Xid, &attrs);
            *width = attrs.width;
            *height = attrs.height;
        }
        else
        {
            *width = w->m_Width;
            *height = w->m_Height;
        }
    }

    void DrawableSize(HWindow w, int* width, int* height)
    {
        if (w->m_Xid)
        {
            ClientSize(w, width, height);
        }
        else
        {
            *width = w->m_PixelWidth;
            *height = w->m_PixelHeight;
        }
    }

    int MonitorCount()
    {
        HWindow w = dmPlatform::LinuxActiveWindow();
        // The test counts monitors after connecting to the display and before opening its first window.
        if (!w->m_X11)
        {
            return 1;
        }

        dmArray<dmWindowState::Monitor> monitors;
        dmPlatform::X11Monitors(w, monitors);
        return monitors.Size();
    }

    void WorkArea(int index, int* x, int* y, int* width, int* height)
    {
        dmArray<dmWindowState::Monitor> monitors;
        dmPlatform::X11Monitors(dmPlatform::LinuxActiveWindow(), monitors);
        const dmWindowState::Rect& area = monitors[index].m_WorkArea;
        *x = area.m_X;
        *y = area.m_Y;
        *width = area.m_Width;
        *height = area.m_Height;
    }

}
