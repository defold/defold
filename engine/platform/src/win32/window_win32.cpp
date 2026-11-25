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
#include "../platform_window_constants.h"

#include <dbt.h>
#include <dlib/log.h>
#include <stdlib.h>

// Hybrid laptop drivers read these exports from the executable at startup.
extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace dmPlatform
{
    const wchar_t* const WIN32_WINDOW_CLASS = L"DefoldWindow";
    static HWindow g_Windows;
    static void* volatile g_ConsoleWindow;

    static BOOL WINAPI ConsoleControl(DWORD event)
    {
        if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT)
        {
            HWND hwnd = (HWND)InterlockedCompareExchangePointer(&g_ConsoleWindow, 0, 0);
            return hwnd && PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }

        return FALSE;
    }

    static void SelectConsoleWindow()
    {
        HWindow current = g_Windows;
        while (current && !current->m_Ready)
        {
            current = current->m_Next;
        }

        HWND hwnd = current ? current->m_Window : 0;
        HWND previous = (HWND)InterlockedExchangePointer(&g_ConsoleWindow, hwnd);
        if (!!hwnd != !!previous)
        {
            SetConsoleCtrlHandler(ConsoleControl, hwnd != 0);
        }
    }

    static void UpdateSize(HWindow window, bool notify)
    {
        RECT rect = {};
        GetClientRect(window->m_Window, &rect);
        bool changed = window->m_Width != (uint32_t)rect.right || window->m_Height != (uint32_t)rect.bottom ||
                       window->m_LastResizeScale != window->m_Scale;
        window->m_Width = rect.right;
        window->m_Height = rect.bottom;
        window->m_LastResizeScale = window->m_Scale;
        WindowPlacementChanged(window);
        if (changed && notify && window->m_Ready && window->m_Params.m_ResizeCallback)
        {
            window->m_Params.m_ResizeCallback(window->m_Params.m_ResizeCallbackUserData, rect.right, rect.bottom);
        }
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
    {
        HWindow window = (HWindow)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (message == WM_NCCREATE)
        {
            window = (HWindow)((CREATESTRUCTW*)lp)->lpCreateParams;
            if (window)
            {
                window->m_Window = hwnd;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)window);
            }
        }

        if (message == WM_DEVICECHANGE)
        {
            Win32JoysticksChanged();
        }

        if (!window)
        {
            return DefWindowProcW(hwnd, message, wp, lp);
        }

        switch (message)
        {
            case WM_CLOSE:
            {
                UpdateWindowPlacement(window, true);
                if (window->m_Ready && TriggerCloseCallback(window) && IsWindow(hwnd))
                {
                    CloseWindow(window);
                }

                return 0;
            }
            case WM_SIZE:
            {
                bool minimized = wp == SIZE_MINIMIZED;
                bool changed = minimized != window->m_Iconified;
                window->m_Iconified = minimized;
                Win32CenterCursor(window);
                if (changed && window->m_Ready && window->m_Params.m_IconifyCallback)
                {
                    window->m_Params.m_IconifyCallback(window->m_Params.m_IconifyCallbackUserData, minimized);
                    if ((HWindow)GetWindowLongPtrW(hwnd, GWLP_USERDATA) != window)
                    {
                        return 0;
                    }
                }

                UpdateSize(window, true);
                return 0;
            }
            case WM_MOVE:
            {
                Win32CenterCursor(window);
                WindowPlacementChanged(window);
                return 0;
            }
            case WM_DPICHANGED:
            {
                window->m_Scale = HIWORD(wp) / 96.0f;
                RECT rect = *(RECT*)lp;
                if (window->m_Params.m_Fullscreen)
                {
                    MONITORINFO info = { sizeof(info) };
                    if (GetMonitorInfoW(MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST), &info))
                    {
                        rect = info.rcMonitor;
                    }
                }

                SetWindowPos(hwnd, 0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOACTIVATE | SWP_NOZORDER);
                // WM_SIZE handles a simultaneous pixel resize. A scale-only change still needs a callback.
                if ((HWindow)GetWindowLongPtrW(hwnd, GWLP_USERDATA) == window && window->m_LastResizeScale != window->m_Scale)
                {
                    UpdateSize(window, true);
                }
                return 0;
            }
            case WM_DISPLAYCHANGE:
            {
                if (window->m_Ready && window->m_Params.m_Fullscreen && !IsIconic(hwnd))
                {
                    MONITORINFO monitor = { sizeof(monitor) };
                    if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
                    {
                        const RECT& bounds = monitor.rcMonitor;
                        SetWindowPos(hwnd, 0, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top, SWP_NOACTIVATE | SWP_NOZORDER);
                    }
                }

                // A display mode change also invalidates saved windowed placement.
                WindowPlacementMonitorsChanged();
                break;
            }
            case WM_SETTINGCHANGE:
            {
                WindowPlacementMonitorsChanged();
                break;
            }
            case WM_SETFOCUS:
            case WM_KILLFOCUS:
            {
                const bool focused = message == WM_SETFOCUS;
                Win32UpdateCursor(window, focused);
                if (!focused)
                {
                    window->m_CaptionAction = false;
                    memset(window->m_Keys, 0, sizeof(window->m_Keys));
                    memset(window->m_Buttons, 0, sizeof(window->m_Buttons));
                    window->m_HighSurrogate = 0;
                    if (GetCapture() == hwnd)
                    {
                        ReleaseCapture();
                    }
                }

                if (window->m_Ready)
                {
                    WindowFocusChanged(window->m_Focus, window->m_Params, focused);
                }

                break;
            }
            case WM_ERASEBKGND:
            {
                return 1;
            }
        }

        LRESULT result = 0;
        if (window->m_Ready && Win32HandleInput(window, message, wp, lp, &result))
        {
            return result;
        }

        return DefWindowProcW(hwnd, message, wp, lp);
    }

    HWindow NewWindow()
    {
        if (!g_Windows)
        {
            Win32InitDpi();
            WNDCLASSEXW wc = { sizeof(wc) };
            wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = WindowProc;
            wc.hInstance = GetModuleHandleW(0);
            wc.hCursor = LoadCursorW(0, MAKEINTRESOURCEW(32512)); // IDC_ARROW
            wc.hIcon = (HICON)LoadImageW(wc.hInstance, MAKEINTRESOURCEW(100), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
            wc.hIconSm = wc.hIcon;
            wc.lpszClassName = WIN32_WINDOW_CLASS;
            if (!RegisterClassExW(&wc))
            {
                dmLogError("Failed to register the Windows window class (%lu).", GetLastError());
                return 0;
            }
        }

        HWindow window = new dmWindow();
        window->m_Next = g_Windows;
        g_Windows = window;
        return window;
    }

    void DeleteWindow(HWindow window)
    {
        CloseWindow(window);
        HWindow* link = &g_Windows;
        while (*link != window)
        {
            link = &(*link)->m_Next;
        }

        *link = window->m_Next;
        delete window;
        if (!g_Windows)
        {
            Win32ShutdownJoysticks();
            UnregisterClassW(WIN32_WINDOW_CLASS, GetModuleHandleW(0));
        }
    }

    void SetWindowTitle(HWindow window, const char* title)
    {
        int count = MultiByteToWideChar(CP_UTF8, 0, title, -1, 0, 0);
        dmArray<WCHAR> wide;
        wide.SetCapacity(count);
        if (count && MultiByteToWideChar(CP_UTF8, 0, title, -1, wide.Begin(), count))
        {
            SetWindowTextW(window->m_Window, wide.Begin());
        }
    }

    static void ShowNativeWindow(HWindow window)
    {
        // SW_SHOWNA keeps a minimized or maximized window in its current state.
        ::ShowWindow(window->m_Window, window->m_Params.m_FocusOnShow ? SW_SHOW : SW_SHOWNA);
        if (window->m_Params.m_FocusOnShow)
        {
            SetForegroundWindow(window->m_Window);
        }
    }

    WindowResult OpenWindow(HWindow window, const WindowCreateParams& params)
    {
        if (window->m_Window)
        {
            return WINDOW_RESULT_WINDOW_ALREADY_OPENED;
        }

        if (!params.m_Fullscreen && (params.m_Width == 0 || params.m_Height == 0))
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        if (params.m_GraphicsApi != WINDOW_GRAPHICS_API_OPENGL && params.m_GraphicsApi != WINDOW_GRAPHICS_API_VULKAN &&
            params.m_GraphicsApi != WINDOW_GRAPHICS_API_DIRECTX)
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        window->m_Params = params;
        window->m_Samples = params.m_Samples;
        POINT origin = {};
        HMONITOR primary = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO info = { sizeof(info) };
        if (!GetMonitorInfoW(primary, &info))
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        float scale = Win32MonitorScale(primary);
        window->m_Scale = scale > 0 ? scale : 1.0f;
        DWORD style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
                      (params.m_Fullscreen ? (WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX) : WS_OVERLAPPEDWINDOW);
        RECT rect = info.rcMonitor;
        if (!params.m_Fullscreen)
        {
            int width = (int)(params.m_Width * window->m_Scale + 0.5f);
            int height = (int)(params.m_Height * window->m_Scale + 0.5f);
            rect.left = (info.rcWork.left + info.rcWork.right - width) / 2;
            rect.top = (info.rcWork.top + info.rcWork.bottom - height) / 2;
            rect.right = rect.left + width;
            rect.bottom = rect.top + height;
            Win32AdjustFrame(&rect, style, WS_EX_APPWINDOW, (UINT)(window->m_Scale * 96 + 0.5f));
        }

        HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, WIN32_WINDOW_CLASS, L"", style, rect.left, rect.top,
                                    rect.right - rect.left, rect.bottom - rect.top, 0, 0, GetModuleHandleW(0), window);
        if (!hwnd)
        {
            window->m_Window = 0;
            dmLogError("Failed to create the Windows window (%lu).", GetLastError());
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        SetWindowTitle(window, params.m_Title ? params.m_Title : "");
        if (params.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGL && !Win32OpenGLCreate(window))
        {
            CloseWindow(window);
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        SetSwapInterval(window, 1);
        InitWindowPlacement(window, params);
        RestoreWindowPlacement(window);
        UpdateSize(window, false);
        window->m_Ready = true;
        if (window->m_Callbacks.m_Gamepad)
        {
            Win32PollJoysticks(true);
        }

        SelectConsoleWindow();
        if (!params.m_Hidden && params.m_FocusOnShow)
        {
            // The window can take focus now. Only the engine's later ShowWindow call should acknowledge editor placement.
            ShowNativeWindow(window);
        }

        return WINDOW_RESULT_OK;
    }

    void CloseWindow(HWindow window)
    {
        if (!window->m_Window)
        {
            return;
        }

        UpdateWindowPlacement(window, true);
        window->m_Ready = false;
        SelectConsoleWindow();
        Win32UpdateCursor(window, false);
        DeleteWindowPlacement(window);
        Win32OpenGLDestroy(window);
        SetWindowLongPtrW(window->m_Window, GWLP_USERDATA, 0);
        DestroyWindow(window->m_Window);
        if (window->m_HiddenCursor)
        {
            DestroyCursor(window->m_HiddenCursor);
        }

        Win32InputCallbacks callbacks = window->m_Callbacks;
        HWindow next = window->m_Next;
        *window = dmWindow();
        window->m_Callbacks = callbacks;
        window->m_Next = next;
    }

    void ShowWindow(HWindow window)
    {
        ShowNativeWindow(window);
        WindowPlacementShown(window);
    }

    void HideWindow(HWindow window)
    {
        ::ShowWindow(window->m_Window, SW_HIDE);
    }

    void IconifyWindow(HWindow window)
    {
        ::ShowWindow(window->m_Window, SW_MINIMIZE);
    }

    void PollEvents(HWindow window)
    {
        if (window->m_Ready)
        {
            // Extensions can consume startup focus changes once their graphics context exists.
            PollWindowFocus(window->m_Focus, window->m_Params);
        }

        MSG msg;
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                for (HWindow current = g_Windows; current; current = current->m_Next)
                {
                    if (current->m_Ready)
                    {
                        PostMessageW(current->m_Window, WM_CLOSE, 0, 0);
                    }
                }

                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Windows can consume key-up for overlapping Shift keys and the Win+V shortcut.
        HWND focus = GetFocus();
        for (HWindow current = g_Windows; focus && current; current = current->m_Next)
        {
            if (current->m_Window == focus)
            {
                const int modifiers[][2] = {
                    { VK_LSHIFT, PLATFORM_KEY_LSHIFT }, { VK_RSHIFT, PLATFORM_KEY_RSHIFT },
                    { VK_LWIN, PLATFORM_KEY_LSUPER }, { VK_RWIN, PLATFORM_KEY_RSUPER }
                };
                for (unsigned i = 0; i < sizeof(modifiers) / sizeof(modifiers[0]); ++i)
                {
                    if (current->m_Keys[modifiers[i][1]] && !(GetKeyState(modifiers[i][0]) & 0x8000))
                    {
                        current->m_Keys[modifiers[i][1]] = 0;
                    }
                }

                break;
            }
        }

        Win32PollJoysticks(window->m_Callbacks.m_Gamepad != 0);
        Win32CenterCursor(window);
        UpdateWindowPlacement(window, false);
    }

    void SetWindowSize(HWindow window, uint32_t width, uint32_t height)
    {
        WindowPlacementSizeRequested(window);
        ApplyPlacementSize(window, width, height);
        ClampWindowPlacement(window);
    }

    void SetWindowPosition(HWindow window, int32_t x, int32_t y)
    {
        Win32MoveClient(window, x, y);
    }

    uint32_t GetWindowWidth(HWindow window)
    {
        return window->m_Width;
    }

    uint32_t GetWindowHeight(HWindow window)
    {
        return window->m_Height;
    }

    bool GetWindowClientSize(HWindow window, uint32_t* width, uint32_t* height)
    {
        if (!window->m_Ready)
        {
            return false;
        }

        *width = window->m_Width;
        *height = window->m_Height;
        return true;
    }

    bool IsWindowResizeSupported(HWindow window)
    {
        return window->m_Ready && !window->m_Params.m_Fullscreen;
    }

    float GetDisplayScaleFactor(HWindow window)
    {
        return window->m_Scale;
    }

    HWND GetWindowsHWND(HWindow window)
    {
        return window ? window->m_Window : 0;
    }

    HGLRC GetWindowsHGLRC(HWindow window)
    {
        return window ? window->m_Context : 0;
    }

    NativeWindowId GetNativeWindowId(HWindow window)
    {
        NativeWindowId id = {};
        if (window && window->m_Ready)
        {
            id.m_Type = NATIVE_WINDOW_TYPE_WIN32;
            id.m_Id = (uintptr_t)window->m_Window;
        }

        return id;
    }

    uint32_t GetWindowStateParam(HWindow window, WindowState state)
    {
        switch (state)
        {
            case WINDOW_STATE_OPENED:
            {
                return window->m_Ready;
            }
            case WINDOW_STATE_ACTIVE:
            {
                return window->m_Window && GetForegroundWindow() == window->m_Window;
            }
            case WINDOW_STATE_ICONIFIED:
            {
                return window->m_Iconified;
            }
            case WINDOW_STATE_HIGH_DPI:
            {
                return window->m_Params.m_HighDPI;
            }
            case WINDOW_STATE_SAMPLE_COUNT:
            case WINDOW_STATE_FSAA_SAMPLES:
            {
                return window->m_Samples;
            }
            case WINDOW_STATE_REFRESH_RATE:
            {
                MONITORINFOEXW monitor = {};
                monitor.cbSize = sizeof(monitor);
                DEVMODEW mode = {};
                mode.dmSize = sizeof(mode);
                if (GetMonitorInfoW(MonitorFromWindow(window->m_Window, MONITOR_DEFAULTTONEAREST), &monitor) &&
                    EnumDisplaySettingsW(monitor.szDevice, ENUM_CURRENT_SETTINGS, &mode))
                {
                    return mode.dmDisplayFrequency > 1 ? mode.dmDisplayFrequency : 0;
                }

                return 0;
            }
            case WINDOW_STATE_AUX_CONTEXT:
            {
                return window->m_AuxContext != 0;
            }
            default:
            {
                break;
            }
        }

        PIXELFORMATDESCRIPTOR format = {};
        if (window->m_DC)
        {
            DescribePixelFormat(window->m_DC, GetPixelFormat(window->m_DC), sizeof(format), &format);
        }

        switch (state)
        {
            case WINDOW_STATE_RED_BITS:
            {
                return format.cRedBits;
            }
            case WINDOW_STATE_GREEN_BITS:
            {
                return format.cGreenBits;
            }
            case WINDOW_STATE_BLUE_BITS:
            {
                return format.cBlueBits;
            }
            case WINDOW_STATE_ALPHA_BITS:
            {
                return format.cAlphaBits;
            }
            case WINDOW_STATE_DEPTH_BITS:
            {
                return format.cDepthBits;
            }
            case WINDOW_STATE_STENCIL_BITS:
            {
                return format.cStencilBits;
            }
            case WINDOW_STATE_ACCUM_RED_BITS:
            {
                return format.cAccumRedBits;
            }
            case WINDOW_STATE_ACCUM_GREEN_BITS:
            {
                return format.cAccumGreenBits;
            }
            case WINDOW_STATE_ACCUM_BLUE_BITS:
            {
                return format.cAccumBlueBits;
            }
            case WINDOW_STATE_ACCUM_ALPHA_BITS:
            {
                return format.cAccumAlphaBits;
            }
            case WINDOW_STATE_AUX_BUFFERS:
            {
                return format.cAuxBuffers;
            }
            case WINDOW_STATE_STEREO:
            {
                return (format.dwFlags & PFD_STEREO) != 0;
            }
            case WINDOW_STATE_ACCELERATED:
            {
                return window->m_Context && !(format.dwFlags & PFD_GENERIC_FORMAT);
            }
            default:
            {
                return 0;
            }
        }
    }

    bool GetSafeArea(HWindow window, WindowSafeArea* area)
    {
        memset(area, 0, sizeof(*area));
        area->m_Width = window->m_Width;
        area->m_Height = window->m_Height;
        return true;
    }

    uint32_t GetReservedAreas(HWindow, const WindowReservedArea** areas)
    {
        *areas = 0;
        return 0;
    }

    void GetReservedAreas(HWindow, dmArray<WindowReservedArea>& areas)
    {
        areas.SetSize(0);
    }

    uint32_t GetTouchData(HWindow, WindowTouchData*, uint32_t)
    {
        return 0;
    }

    bool IsMouseLeftButtonFromTouch(HWindow)
    {
        return false;
    }

    bool IsMousePositionFromTouch(HWindow)
    {
        return false;
    }

    bool GetAcceleration(HWindow, float*, float*, float*)
    {
        return false;
    }

    int32_t TriggerCloseCallback(HWindow window)
    {
        return window->m_Params.m_CloseCallback ? window->m_Params.m_CloseCallback(window->m_Params.m_CloseCallbackUserData) : 1;
    }

    void Win32NotifyGamepad(int index, WindowGamepadEvent event)
    {
        for (HWindow window = g_Windows; window; window = window->m_Next)
        {
            if (window->m_Ready && window->m_Callbacks.m_Gamepad)
            {
                window->m_Callbacks.m_Gamepad(window->m_Callbacks.m_GamepadData, index, event);
            }
        }
    }

}
