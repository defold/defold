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
#include "../win32/window_win32.h"
#include <jc_test/jc_test.h>

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

static void CountResize(void* data, uint32_t, uint32_t)
{
    ++*(unsigned*)data;
}

// DPI changes must notify once, including when the window retains its pixel dimensions.
TEST(Win32Desktop, DpiResizeCallback)
{
    for (int fullscreen = 0; fullscreen < 2; ++fullscreen)
    {
        unsigned count = 0;
        HWindow window = dmPlatform::NewWindow();
        WindowCreateParams params;
        WindowCreateParamsInitialize(&params);
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
        params.m_Fullscreen = fullscreen != 0;
        params.m_Hidden = true;
        params.m_ResizeCallback = CountResize;
        params.m_ResizeCallbackUserData = &count;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        HWND hwnd = dmPlatform::GetWindowsHWND(window);
        RECT bounds;
        ASSERT_TRUE(GetWindowRect(hwnd, &bounds));
        UINT dpi = dmPlatform::GetDisplayScaleFactor(window) == 1.0f ? 144 : 96;
        count = 0;
        SendMessageW(hwnd, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), (LPARAM)&bounds);
        EXPECT_EQ(1u, count);
        EXPECT_EQ(dpi / 96.0f, dmPlatform::GetDisplayScaleFactor(window));
        if (!fullscreen)
        {
            bounds.right += 64;
            dpi = dpi == 96 ? 144 : 96;
            SendMessageW(hwnd, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), (LPARAM)&bounds);
            EXPECT_EQ(2u, count);
        }

        dmPlatform::DeleteWindow(window);
    }
}

// Showing without focus must preserve the maximized or minimized placement across a hide.
TEST(Win32Desktop, ShowWithoutFocusPreservesPlacement)
{
    HWindow window = dmPlatform::NewWindow();
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    params.m_FocusOnShow = false;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    HWND hwnd = dmPlatform::GetWindowsHWND(window);
    ::ShowWindow(hwnd, SW_MAXIMIZE);
    dmPlatform::HideWindow(window);
    dmPlatform::ShowWindow(window);
    EXPECT_TRUE(IsZoomed(hwnd));
    ::ShowWindow(hwnd, SW_MINIMIZE);
    dmPlatform::HideWindow(window);
    dmPlatform::ShowWindow(window);
    EXPECT_TRUE(IsIconic(hwnd));
    dmPlatform::DeleteWindow(window);
}

// Borderless fullscreen must keep its system menu and cover its monitor again after a display change.
TEST(Win32Desktop, FullscreenDisplayChange)
{
    HWindow window = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, window);
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    params.m_Fullscreen = true;
    params.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    HWND hwnd = dmPlatform::GetWindowsHWND(window);
    MONITORINFO monitor = { sizeof(monitor) };
    ASSERT_TRUE(GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor));
    RECT bounds;
    GetWindowRect(hwnd, &bounds);
    ASSERT_TRUE(EqualRect(&bounds, &monitor.rcMonitor));
    ASSERT_FALSE(GetWindowLongW(hwnd, GWL_STYLE) & WS_CAPTION);
    ASSERT_TRUE(GetWindowLongW(hwnd, GWL_STYLE) & WS_MINIMIZEBOX);
    HMENU menu = GetSystemMenu(hwnd, FALSE);
    ASSERT_NE((HMENU)0, menu);
    EXPECT_NE((UINT)-1, GetMenuState(menu, SC_MINIMIZE, MF_BYCOMMAND));
    EXPECT_NE((UINT)-1, GetMenuState(menu, SC_CLOSE, MF_BYCOMMAND));
    SetWindowPos(hwnd, 0, bounds.left, bounds.top, 64, 64, SWP_NOACTIVATE | SWP_NOZORDER);
    SendMessageW(hwnd, WM_DISPLAYCHANGE, 32, 0);
    GetWindowRect(hwnd, &bounds);
    ASSERT_TRUE(EqualRect(&bounds, &monitor.rcMonitor));
    dmPlatform::DeleteWindow(window);
}

// Capture must install an invisible cursor for Remote Desktop and restore the pointer on release.
TEST(Win32Desktop, CursorRestoresAfterRefocus)
{
    POINT original;
    ASSERT_TRUE(GetCursorPos(&original));
    HWindow window = dmPlatform::NewWindow();
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    params.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    HWND hwnd = dmPlatform::GetWindowsHWND(window);
    POINT first = { 35, 45 }, second = { 170, 125 };
    ClientToScreen(hwnd, &first);
    ClientToScreen(hwnd, &second);
    ASSERT_TRUE(SetCursorPos(first.x, first.y));
    dmPlatform::SetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR, false);
    SendMessageW(hwnd, WM_SETFOCUS, 0, 0);
    HCURSOR hidden = GetCursor();
    HCURSOR arrow = (HCURSOR)GetClassLongPtrW(hwnd, GCLP_HCURSOR);
    EXPECT_NE((HCURSOR)0, hidden);
    EXPECT_NE(arrow, hidden);
    SetCursor(arrow);
    SendMessageW(hwnd, WM_SETCURSOR, (WPARAM)hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    EXPECT_EQ(hidden, GetCursor());
    SendMessageW(hwnd, WM_KILLFOCUS, 0, 0);
    EXPECT_EQ(arrow, GetCursor());
    EXPECT_TRUE(SetCursorPos(second.x, second.y));
    SendMessageW(hwnd, WM_SETFOCUS, 0, 0);
    dmPlatform::SetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR, true);
    POINT actual = {};
    EXPECT_TRUE(GetCursorPos(&actual));
    dmPlatform::DeleteWindow(window);
    SetCursorPos(original.x, original.y);
    EXPECT_EQ(second.x, actual.x);
    EXPECT_EQ(second.y, actual.y);
}

// Moving a cursor-locked window must recenter on the new client area without changing virtual coordinates.
TEST(Win32Desktop, CursorLockFollowsWindow)
{
    POINT original;
    ASSERT_TRUE(GetCursorPos(&original));
    HWindow window = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, window);
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    params.m_Hidden = true;
    params.m_Width = params.m_Height = 100;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    HWND hwnd = dmPlatform::GetWindowsHWND(window);
    POINT origin = {};
    ClientToScreen(hwnd, &origin);
    dmPlatform::SetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR, false);
    SendMessageW(hwnd, WM_SETFOCUS, 0, 0);
    int x, y;
    dmPlatform::GetMousePosition(window, &x, &y);
    dmPlatform::SetWindowPosition(window, origin.x + 20, origin.y + 10);
    RECT client;
    GetClientRect(hwnd, &client);
    POINT center = { client.right / 2, client.bottom / 2 };
    ClientToScreen(hwnd, &center);
    POINT actual;
    GetCursorPos(&actual);
    int after_x, after_y;
    dmPlatform::GetMousePosition(window, &after_x, &after_y);
    dmPlatform::DeleteWindow(window);
    SetCursorPos(original.x, original.y);
    ASSERT_EQ(center.x, actual.x);
    ASSERT_EQ(center.y, actual.y);
    ASSERT_EQ(x, after_x);
    ASSERT_EQ(y, after_y);
}
