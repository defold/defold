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

#include "test_window_placement_native.h"
#include "../platform_window_win32.h"

namespace dmWindowTest
{
    const char* Backend()
    {
        return "win32";
    }

    void Position(HWindow window, int* x, int* y)
    {
        POINT origin = {};
        ClientToScreen(dmPlatform::GetWindowsHWND(window), &origin);
        *x = origin.x;
        *y = origin.y;
    }

    void ClientSize(HWindow window, int* width, int* height)
    {
        RECT rect = {};
        GetClientRect(dmPlatform::GetWindowsHWND(window), &rect);
        *width = rect.right;
        *height = rect.bottom;
    }

    void DrawableSize(HWindow window, int* width, int* height)
    {
        ClientSize(window, width, height);
    }

    int MonitorCount()
    {
        return GetSystemMetrics(SM_CMONITORS);
    }

    struct WorkAreaQuery
    {
        int  m_Index;
        RECT m_Work;
    };

    static BOOL CALLBACK FindWorkArea(HMONITOR monitor, HDC, LPRECT, LPARAM data)
    {
        WorkAreaQuery* query = (WorkAreaQuery*)data;
        MONITORINFO info = { sizeof(info) };
        GetMonitorInfoW(monitor, &info);
        if (!(info.dwFlags & MONITORINFOF_PRIMARY) && --query->m_Index == 0)
        {
            query->m_Work = info.rcWork;
            return FALSE;
        }

        return TRUE;
    }

    void WorkArea(int index, int* x, int* y, int* width, int* height)
    {
        WorkAreaQuery query = { index };
        if (index == 0)
        {
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &query.m_Work, 0);
        }
        else
        {
            EnumDisplayMonitors(0, 0, FindWorkArea, (LPARAM)&query);
        }

        *x = query.m_Work.left;
        *y = query.m_Work.top;
        *width = query.m_Work.right - *x;
        *height = query.m_Work.bottom - *y;
    }

}
