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

#include "window_win32.h"

#include <dlib/hash.h>
#include <dlib/math.h>

namespace dmPlatform
{
    typedef UINT    (WINAPI* WindowDpiFn)(HWND);
    typedef HRESULT (WINAPI* MonitorDpiFn)(HMONITOR, int, UINT*, UINT*);
    typedef BOOL    (WINAPI* AdjustFrameFn)(LPRECT, DWORD, BOOL, DWORD, UINT);
    static WindowDpiFn g_WindowDpi;
    static MonitorDpiFn g_MonitorDpi;
    static AdjustFrameFn g_AdjustFrame;

    void Win32InitDpi()
    {
        static bool initialized = false;
        if (initialized)
        {
            return;
        }

        initialized = true;
        HMODULE user = GetModuleHandleW(L"user32.dll");
        HMODULE scaling = LoadLibraryW(L"shcore.dll");
        g_WindowDpi = (WindowDpiFn)::GetProcAddress(user, "GetDpiForWindow");
        g_AdjustFrame = (AdjustFrameFn)::GetProcAddress(user, "AdjustWindowRectExForDpi");
        typedef BOOL (WINAPI* SetContextFn)(HANDLE);
        SetContextFn set_context = (SetContextFn)::GetProcAddress(user, "SetProcessDpiAwarenessContext");
        if (scaling)
        {
            g_MonitorDpi = (MonitorDpiFn)::GetProcAddress(scaling, "GetDpiForMonitor");
        }

        if (set_context && (set_context((HANDLE)(intptr_t)-4) || GetLastError() == ERROR_ACCESS_DENIED))
        {
            return;
        }

        typedef HRESULT (WINAPI* SetAwarenessFn)(int);
        SetAwarenessFn set_awareness = scaling ? (SetAwarenessFn)::GetProcAddress(scaling, "SetProcessDpiAwareness") : 0;
        if (set_awareness)
        {
            set_awareness(2); // PROCESS_PER_MONITOR_DPI_AWARE
        }
        else
        {
            SetProcessDPIAware();
        }
    }

    float Win32MonitorScale(HMONITOR monitor)
    {
        if (g_MonitorDpi)
        {
            UINT x, y;
            return SUCCEEDED(g_MonitorDpi(monitor, 0, &x, &y)) ? x / 96.0f : 0.0f;
        }

        HDC dc = GetDC(0);
        int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 0;
        if (dc)
        {
            ReleaseDC(0, dc);
        }

        return dpi / 96.0f;
    }

    float Win32WindowScale(HWindow window)
    {
        if (g_WindowDpi)
        {
            return g_WindowDpi(window->m_Window) / 96.0f;
        }

        return Win32MonitorScale(MonitorFromWindow(window->m_Window, MONITOR_DEFAULTTONEAREST));
    }

    void Win32AdjustFrame(RECT* rect, DWORD style, DWORD ex_style, UINT dpi)
    {
        if (!g_AdjustFrame || !g_AdjustFrame(rect, style, FALSE, ex_style, dpi))
        {
            AdjustWindowRectEx(rect, style, FALSE, ex_style);
        }
    }

    struct DisplayLayout
    {
        uint64_t m_Hash;
        uint32_t m_Count;
    };

    static BOOL CALLBACK HashMonitorGeometry(HMONITOR monitor, HDC, LPRECT, LPARAM data)
    {
        DisplayLayout* layout = (DisplayLayout*)data;
        MONITORINFO info = { sizeof(info) };
        if (!GetMonitorInfoW(monitor, &info))
        {
            layout->m_Count = 0;
            return FALSE;
        }

        // Monitor bounds, work area and primary status determine placement. Enumeration order can vary.
        layout->m_Hash += dmHashBufferNoReverse64(&info, sizeof(info));
        ++layout->m_Count;
        return TRUE;
    }

    bool Win32UpdateDisplayLayout()
    {
        static DisplayLayout previous;
        DisplayLayout current = {};
        if (!EnumDisplayMonitors(0, 0, HashMonitorGeometry, (LPARAM)&current) || !current.m_Count)
        {
            return false;
        }

        bool changed = current.m_Hash != previous.m_Hash || current.m_Count != previous.m_Count;
        previous = current;
        return changed;
    }

    static dmWindowState::Rect ToRect(const RECT& rect)
    {
        dmWindowState::Rect out = { rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top };
        return out;
    }

    static uint64_t MonitorNameHash(const WCHAR* adapter_name)
    {
        // Version-one player state hashes the UTF-8 display description, with the adapter as fallback.
        DISPLAY_DEVICEW display = { sizeof(display) };
        for (DWORD i = 0; EnumDisplayDevicesW(adapter_name, i, &display, 0); ++i)
        {
            if (display.StateFlags & DISPLAY_DEVICE_ACTIVE)
            {
                char name[512];
                if (WideCharToMultiByte(CP_UTF8, 0, display.DeviceString, -1, name, sizeof(name), 0, 0))
                {
                    return dmHashString64(name);
                }
            }
        }

        for (DWORD i = 0; EnumDisplayDevicesW(0, i, &display, 0); ++i)
        {
            if (wcscmp(display.DeviceName, adapter_name) == 0)
            {
                char name[512];
                if (WideCharToMultiByte(CP_UTF8, 0, display.DeviceString, -1, name, sizeof(name), 0, 0))
                {
                    return dmHashString64(name);
                }

                break;
            }
        }

        return 0;
    }

    static BOOL CALLBACK CollectDisplay(HMONITOR monitor, HDC, LPRECT, LPARAM data)
    {
        MONITORINFOEXW info = {};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(monitor, &info))
        {
            Win32Display display = {};
            display.m_Bounds = ToRect(info.rcMonitor);
            display.m_Monitor.m_WorkArea = ToRect(info.rcWork);
            display.m_Monitor.m_NameHash = MonitorNameHash(info.szDevice);
            display.m_Monitor.m_ScaleX = display.m_Monitor.m_ScaleY = Win32MonitorScale(monitor);
            dmArray<Win32Display>& displays = *(dmArray<Win32Display>*)data;
            if (displays.Full())
            {
                displays.OffsetCapacity(4);
            }

            displays.Push(display);
            if (info.dwFlags & MONITORINFOF_PRIMARY)
            {
                displays.Back() = displays[0];
                displays[0] = display;
            }
        }

        return TRUE;
    }

    void Win32GetDisplays(dmArray<Win32Display>& displays)
    {
        EnumDisplayMonitors(0, 0, CollectDisplay, (LPARAM)&displays);
    }

    dmWindowState::Rect Win32ClientRect(HWindow window)
    {
        RECT rect = {};
        GetClientRect(window->m_Window, &rect);
        POINT point = {};
        ClientToScreen(window->m_Window, &point);
        dmWindowState::Rect result = { point.x, point.y, rect.right, rect.bottom };
        return result;
    }

    Win32Frame Win32FrameInsets(HWindow window)
    {
        RECT frame = {};
        GetWindowRect(window->m_Window, &frame);
        dmWindowState::Rect client = Win32ClientRect(window);
        Win32Frame insets = { client.m_X - frame.left, client.m_Y - frame.top,
                              frame.right - client.m_X - client.m_Width, frame.bottom - client.m_Y - client.m_Height };
        return insets;
    }

    void Win32MoveClient(HWindow window, int x, int y)
    {
        Win32Frame frame = Win32FrameInsets(window);
        SetWindowPos(window->m_Window, 0, x - frame.m_Left, y - frame.m_Top, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
    }

    void ApplyPlacementSize(HWindow window, int width, int height)
    {
        RECT rect = { 0, 0, width, height };
        Win32AdjustFrame(&rect, (DWORD)GetWindowLongPtrW(window->m_Window, GWL_STYLE),
                         (DWORD)GetWindowLongPtrW(window->m_Window, GWL_EXSTYLE), (UINT)(window->m_Scale * 96.0f + 0.5f));
        SetWindowPos(window->m_Window, 0, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
    }

    bool NormalPlacementWindow(HWindow window)
    {
        return window->m_Window && !window->m_Params.m_Fullscreen && !IsIconic(window->m_Window) && !IsZoomed(window->m_Window);
    }

    WindowPlacement*& PlacementStorage(HWindow window)
    {
        return window->m_Placement;
    }

    float GetDisplayDensity(HWindow window)
    {
        MONITORINFOEXW info = {};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(MonitorFromWindow(window->m_Window, MONITOR_DEFAULTTOPRIMARY), &info))
        {
            return 96.0f;
        }

        HDC dc = CreateDCW(L"DISPLAY", info.szDevice, 0, 0);
        if (!dc)
        {
            return 96.0f;
        }

        int mm = GetDeviceCaps(dc, HORZSIZE);
        DeleteDC(dc);
        // Per-monitor-aware Win32 monitor rectangles are already in physical pixels.
        return CalculateDisplayDensity(mm, (float)(info.rcMonitor.right - info.rcMonitor.left));
    }

}
