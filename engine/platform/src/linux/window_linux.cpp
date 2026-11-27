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

#include "window_linux.h"

#include <dlib/log.h>
#include <math.h>
#include <string.h>

namespace dmPlatform
{
    static HWindow g_Windows;

    HWindow LinuxActiveWindow()
    {
        HWindow w = g_Windows;
        while (w && w->m_DeletePending)
        {
            w = w->m_Next;
        }

        return w;
    }

    HWindow NewWindow()
    {
        HWindow w = new dmWindow();
        if (!LinuxConnectX11(w) && !LinuxConnectWayland(w))
        {
            dmLogError("Could not connect to an X11 or Wayland display.");
            delete w;
            return 0;
        }

        w->m_EventThread = pthread_self();
        w->m_Next = g_Windows;
        g_Windows = w;
        return w;
    }

    void DeleteWindow(HWindow w)
    {
        if (!w)
        {
            return;
        }

        SetGamepadEventCallback(w, 0, 0);
        if (w->m_DispatchDepth)
        {
            // Native dispatch still owns the display and may deliver more events before it returns.
            w->m_DeletePending = true;
            w->m_Params.m_ResizeCallback = 0;
            w->m_Params.m_FocusCallback = 0;
            w->m_Params.m_IconifyCallback = 0;
            w->m_Params.m_CloseCallback = 0;
            w->m_Char = 0;
            return;
        }

        CloseWindow(w);
        HWindow* link = &g_Windows;
        while (*link && *link != w)
        {
            link = &(*link)->m_Next;
        }

        if (*link)
        {
            *link = w->m_Next;
        }

        w->m_Ops->m_Disconnect(w);
        delete w;
        if (!g_Windows)
        {
            LinuxCloseJoysticks();
        }
    }

    static void EndWindowDispatch(HWindow w)
    {
        if (--w->m_DispatchDepth == 0 && w->m_DeletePending)
        {
            DeleteWindow(w);
        }
    }

    void LinuxUpdateSize(HWindow w, int width, int height, float scale)
    {
        if (width <= 0 || height <= 0)
        {
            return;
        }

        // X11 can read ahead of ConfigureNotify. Each callback gets the drawable size of its own event.
        w->m_PixelWidth = w->m_X11 ? width : (int)lround(width * scale);
        w->m_PixelHeight = w->m_X11 ? height : (int)lround(height * scale);
        if (w->m_Width == width && w->m_Height == height && w->m_Scale == scale)
        {
            return;
        }

        const bool update_cursor = w->m_Wayland && w->m_Scale != scale;
        w->m_Width = width;
        w->m_Height = height;
        w->m_Scale = scale;
        if (update_cursor)
        {
            w->m_Ops->m_SetCursor(w);
        }

        LinuxResizeGL(w);
        WindowPlacementChanged(w);
        if (w->m_Open)
        {
            NotifyWindowResize(w, w->m_Params, w->m_ResizeCallbackCount);
        }
    }

    void LinuxFocus(HWindow w, bool focused)
    {
        if (w->m_Focused == focused)
        {
            return;
        }

        w->m_Focused = focused;
        w->m_Ops->m_SetCursor(w);
        if (w->m_Open && w->m_Params.m_FocusCallback)
        {
            w->m_Params.m_FocusCallback(w->m_Params.m_FocusCallbackUserData, focused);
        }

        if (!focused)
        {
            memset(w->m_Keys, 0, sizeof(w->m_Keys));
            memset(w->m_Buttons, 0, sizeof(w->m_Buttons));
        }
    }

    void LinuxIconify(HWindow w, bool iconified)
    {
        if (w->m_Iconified == iconified)
        {
            return;
        }

        w->m_Iconified = iconified;
        if (w->m_Open && w->m_Params.m_IconifyCallback)
        {
            w->m_Params.m_IconifyCallback(w->m_Params.m_IconifyCallbackUserData, iconified);
        }
    }

    WindowResult OpenWindow(HWindow w, const WindowCreateParams& params)
    {
        if (!w)
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        if (w->m_Open)
        {
            return WINDOW_RESULT_WINDOW_ALREADY_OPENED;
        }

        if (!params.m_Fullscreen && (params.m_Width == 0 || params.m_Height == 0))
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        // Only the headless backend can satisfy a null adapter after graphics fallback.
        if (params.m_GraphicsApi == WINDOW_GRAPHICS_API_NULL)
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        w->m_Params = params;
        w->m_Width = params.m_Width;
        w->m_Height = params.m_Height;
        w->m_PixelWidth = w->m_Width;
        w->m_PixelHeight = w->m_Height;
        w->m_Scale = 1.0f;
        w->m_Density = 96.0f;
        InitWindowPlacement(w, params);
        if (!w->m_Ops->m_Open(w) || !LinuxOpenGL(w))
        {
            LinuxCloseGL(w);
            w->m_Ops->m_Close(w);
            DeleteWindowPlacement(w);
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        if (!params.m_Fullscreen)
        {
            int width = w->m_PixelWidth, height = w->m_PixelHeight;
            RestoreWindowPlacement(w);
            w->m_PendingRestoreResize = width != w->m_PixelWidth || height != w->m_PixelHeight;
        }

        w->m_ResizeCallbackCount = 0;
        ++w->m_OpenGeneration;
        w->m_Open = true;
        SetSwapInterval(w, 1);
        return WINDOW_RESULT_OK;
    }

    void CloseWindow(HWindow w)
    {
        if (!w || !w->m_Open)
        {
            return;
        }

        w->m_Open = false;
        UpdateWindowPlacement(w, true);
        DeleteWindowPlacement(w);
        w->m_PendingRestoreResize = false;
        LinuxCloseGL(w);
        w->m_Ops->m_Close(w);
        w->m_Locked = w->m_Focused = w->m_Iconified = w->m_Shown = w->m_Maximized = w->m_LinkCursor = false;
        w->m_MouseX = w->m_MouseY = w->m_Wheel = 0;
        memset(w->m_Keys, 0, sizeof(w->m_Keys));
        memset(w->m_Buttons, 0, sizeof(w->m_Buttons));
    }

    void PollEvents(HWindow w)
    {
        if (!w)
        {
            return;
        }

        pthread_t thread = pthread_self();
        HWindow current = g_Windows;
        while (current)
        {
            if (!pthread_equal(current->m_EventThread, thread) || current->m_DispatchDepth)
            {
                current = current->m_Next;
                continue;
            }

            ++current->m_DispatchDepth;
            current->m_Ops->m_Poll(current);
            if (current->m_PendingRestoreResize && !current->m_DeletePending)
            {
                current->m_PendingRestoreResize = false;
                // A resize already delivered since open includes the current drawable dimensions.
                if (!current->m_ResizeCallbackCount)
                {
                    NotifyWindowResize(current, current->m_Params, current->m_ResizeCallbackCount);
                }
            }

            if (!current->m_DeletePending)
            {
                UpdateWindowPlacement(current, false);
            }

            HWindow polled = current;
            current = current->m_Next;
            EndWindowDispatch(polled);
        }

        if (LinuxActiveWindow())
        {
            LinuxPollJoysticks();
        }
    }

    void ShowWindow(HWindow w)
    {
        if (!w || !w->m_Open || w->m_DeletePending)
        {
            return;
        }

        // Mapping a hidden Wayland surface dispatches events, including when called from a poll callback.
        ++w->m_DispatchDepth;
        if (w->m_Ops->m_Show(w, true) && !w->m_DeletePending)
        {
            w->m_Shown = true;
            WindowPlacementShown(w);
        }

        EndWindowDispatch(w);
    }

    void HideWindow(HWindow w)
    {
        if (!w || !w->m_Open)
        {
            return;
        }

        w->m_Shown = false;
        w->m_Ops->m_Show(w, false);
    }

    void IconifyWindow(HWindow w)
    {
        if (w && w->m_Open)
        {
            w->m_Ops->m_Iconify(w);
        }
    }

    void SetWindowTitle(HWindow w, const char* title)
    {
        if (w && w->m_Open)
        {
            w->m_Ops->m_Title(w, title);
        }
    }

    void SetWindowSize(HWindow w, uint32_t width, uint32_t height)
    {
        if (!IsWindowResizeSupported(w) || !width || !height)
        {
            return;
        }

        WindowPlacementSizeRequested(w);
        if (w->m_X11 && w->m_Shown)
        {
            // A synchronous size acknowledgement can delete the window from its callback.
            ++w->m_DispatchDepth;
            X11RequestWindowSize(w, width, height);
            EndWindowDispatch(w);
            return;
        }

        const bool same_size = width == (uint32_t)w->m_Width && height == (uint32_t)w->m_Height;
        const uint32_t count = w->m_ResizeCallbackCount;
        w->m_Ops->m_Size(w, width, height);
        if (same_size && w->m_ResizeCallbackCount == count)
        {
            NotifyWindowResize(w, w->m_Params, w->m_ResizeCallbackCount);
        }
    }

    void SetWindowPosition(HWindow w, int32_t x, int32_t y)
    {
        if (w && w->m_Open && w->m_X11)
        {
            X11SetPosition(w, x, y);
        }
    }

    uint32_t GetWindowWidth(HWindow w)
    {
        return w->m_PixelWidth;
    }

    uint32_t GetWindowHeight(HWindow w)
    {
        return w->m_PixelHeight;
    }

    bool GetWindowClientSize(HWindow w, uint32_t* width, uint32_t* height)
    {
        if (!w || !w->m_Open)
        {
            return false;
        }

        *width = w->m_X11 ? w->m_PixelWidth : w->m_Width;
        *height = w->m_X11 ? w->m_PixelHeight : w->m_Height;
        return true;
    }

    bool IsWindowResizeSupported(HWindow w)
    {
        return w && w->m_Open && !w->m_Params.m_Fullscreen;
    }

    float GetDisplayScaleFactor(HWindow w)
    {
        return w->m_Scale;
    }

    float GetDisplayDensity(HWindow w)
    {
        return w->m_Wayland ? WaylandGetDisplayDensity(w->m_Wayland) : w->m_Density;
    }

    Window GetX11Window(HWindow w)
    {
        return w && w->m_Open ? w->m_Xid : None;
    }

    NativeWindowId GetNativeWindowId(HWindow w)
    {
        NativeWindowId id = {};
        if ((id.m_Id = GetX11Window(w)) != 0)
        {
            id.m_Type = NATIVE_WINDOW_TYPE_X11;
        }

        return id;
    }

    uint32_t GetWindowStateParam(HWindow w, WindowState state)
    {
        if (!w)
        {
            return 0;
        }

        switch (state)
        {
            case WINDOW_STATE_OPENED:
            {
                return w->m_Open;
            }
            case WINDOW_STATE_ACTIVE:
            {
                return w->m_Focused;
            }
            case WINDOW_STATE_ICONIFIED:
            {
                return w->m_Iconified;
            }
            case WINDOW_STATE_HIGH_DPI:
            {
                return w->m_Params.m_HighDPI;
            }
            case WINDOW_STATE_REFRESH_RATE:
            {
                return w->m_RefreshRate;
            }
            case WINDOW_STATE_SAMPLE_COUNT:
            {
                // Vulkan negotiates its swapchain using the request after the window opens.
                return w->m_GL ? LinuxGLState(w, state) : w->m_Params.m_Samples;
            }
            case WINDOW_STATE_FSAA_SAMPLES:
            {
                return w->m_Params.m_Samples;
            }
            default:
            {
                return LinuxGLState(w, state);
            }
        }
    }

    int32_t GetKey(HWindow w, int32_t code)
    {
        return code >= 0 && code < (int)sizeof(w->m_Keys) ? w->m_Keys[code] : 0;
    }

    int32_t GetMouseButton(HWindow w, int32_t code)
    {
        return code >= 0 && code < (int)sizeof(w->m_Buttons) ? w->m_Buttons[code] : 0;
    }

    int32_t GetMouseWheel(HWindow w)
    {
        return (int32_t)round(w->m_Wheel);
    }

    void GetMousePosition(HWindow w, int32_t* x, int32_t* y)
    {
        if (w->m_X11)
        {
            if (w->m_Xid && !w->m_Locked && X11QueryPointer(w, x, y))
            {
                return;
            }

            *x = (int32_t)w->m_MouseX;
            *y = (int32_t)w->m_MouseY;
            return;
        }

        *x = w->m_Width ? (int32_t)(w->m_MouseX * w->m_PixelWidth / w->m_Width) : 0;
        *y = w->m_Height ? (int32_t)(w->m_MouseY * w->m_PixelHeight / w->m_Height) : 0;
    }

    bool IsMouseLeftButtonFromTouch(HWindow)
    {
        return false;
    }

    bool IsMousePositionFromTouch(HWindow)
    {
        return false;
    }

    uint32_t GetTouchData(HWindow, WindowTouchData*, uint32_t)
    {
        return 0;
    }

    bool GetAcceleration(HWindow, float*, float*, float*)
    {
        return false;
    }

    bool GetSafeArea(HWindow w, WindowSafeArea* area)
    {
        memset(area, 0, sizeof(*area));
        area->m_Width = GetWindowWidth(w);
        area->m_Height = GetWindowHeight(w);
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

    void SetDeviceState(HWindow w, WindowDeviceState state, bool value, bool)
    {
        if (state == WINDOW_DEVICE_STATE_CURSOR && w->m_Locked == value)
        {
            // X11 can move the window under a stationary pointer. Seed relative motion once, before locking.
            int x, y;
            if (!value && w->m_Xid && X11QueryPointer(w, &x, &y))
            {
                w->m_MouseX = x;
                w->m_MouseY = y;
            }

            w->m_Locked = !value;
            w->m_Ops->m_SetCursor(w);
        }
    }

    void SetDeviceState(HWindow w, WindowDeviceState state, bool value)
    {
        SetDeviceState(w, state, value, false);
    }

    void SetKeyboardCharCallback(HWindow w, FWindowAddKeyboardCharCallback cb, void* data)
    {
        w->m_Char = cb;
        w->m_CharData = data;
    }

    void SetKeyboardMarkedTextCallback(HWindow, FWindowSetMarkedTextCallback, void*)
    {
    }

    void SetKeyboardDeviceChangedCallback(HWindow, FWindowDeviceChangedCallback, void*)
    {
    }

    int32_t TriggerCloseCallback(HWindow w)
    {
        UpdateWindowPlacement(w, true);
        return w->m_Params.m_CloseCallback ? w->m_Params.m_CloseCallback(w->m_Params.m_CloseCallbackUserData) : 1;
    }

    void SetLinuxClipboard(HWindow w, const char* text, unsigned length)
    {
        if (w && w->m_Open)
        {
            w->m_Ops->m_SetClipboard(w, text, length);
        }
    }

    void SetLinuxLinkCursor(HWindow w, bool link)
    {
        if (w && w->m_Open && w->m_LinkCursor != link)
        {
            w->m_LinkCursor = link;
            w->m_Ops->m_SetCursor(w);
        }
    }

}
