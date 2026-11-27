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

#ifndef DM_WINDOW_LINUX_PRIVATE_H
#define DM_WINDOW_LINUX_PRIVATE_H

#include "../platform_window_linux.h"
#include "../window_placement.h"
#include "../window_state.h"
#include <pthread.h>

struct wl_display;
struct wl_surface;
struct LinuxGL;
struct LinuxX11;
struct LinuxWayland;
struct LinuxWindowOps;

struct dmWindow
{
    const LinuxWindowOps*          m_Ops;
    HWindow                        m_Next;
    pthread_t                      m_EventThread;
    Display*                       m_Display;
    ::Window                       m_Xid;
    wl_display*                    m_WlDisplay;
    wl_surface*                    m_Surface;
    LinuxX11*                      m_X11;
    LinuxWayland*                  m_Wayland;
    LinuxGL*                       m_GL;
    WindowPlacement*               m_Placement;
    WindowCreateParams             m_Params;
    FWindowAddKeyboardCharCallback m_Char;
    void*                          m_CharData;
    int                            m_Width, m_Height, m_PixelWidth, m_PixelHeight, m_RefreshRate;
    uint32_t                       m_ResizeCallbackCount, m_DispatchDepth, m_OpenGeneration;
    float                          m_Scale, m_Density;
    double                         m_MouseX, m_MouseY, m_Wheel;
    unsigned char                  m_Keys[349], m_Buttons[8];
    bool                           m_Open, m_Shown, m_Focused, m_Iconified, m_Maximized, m_Locked, m_LinkCursor;
    bool                           m_PendingRestoreResize, m_DeletePending;
};

struct LinuxWindowOps
{
    const char* m_Name;
    bool        (*m_Open)(HWindow);
    void        (*m_Close)(HWindow);
    void        (*m_Disconnect)(HWindow);
    void        (*m_Poll)(HWindow);
    void        (*m_Show)(HWindow, bool);
    void        (*m_Iconify)(HWindow);
    void        (*m_Title)(HWindow, const char*);
    void        (*m_Size)(HWindow, int, int);
    void        (*m_SetCursor)(HWindow);
    void        (*m_SetClipboard)(HWindow, const char*, unsigned);
};

namespace dmPlatform
{
    bool    LinuxConnectX11(HWindow window);
    bool    LinuxConnectWayland(HWindow window);
    float   WaylandGetDisplayDensity(const LinuxWayland* wl);
    HWindow LinuxActiveWindow();
    void    LinuxUpdateSize(HWindow window, int width, int height, float scale);
    void    LinuxFocus(HWindow window, bool focused);
    void    LinuxIconify(HWindow window, bool iconified);
    void    LinuxText(HWindow window, const char* text, size_t length);
    int     LinuxKeyCode(unsigned evdev_code);
    void    LinuxPollJoysticks();
    void    LinuxCloseJoysticks();

    void     X11RequestWindowSize(HWindow window, int width, int height);
    void     X11SetPosition(HWindow window, int x, int y);
    void     X11GetPosition(HWindow window, int* x, int* y);
    bool     X11QueryPointer(HWindow window, int* x, int* y);
    void     X11ReadKeymap(Display* display, int16_t* keys);
    unsigned X11LookupCharacter(XKeyEvent* event);
    void     X11Monitors(HWindow window, dmArray<dmWindowState::Monitor>& monitors);
    void     X11FrameExtents(HWindow window, int* left, int* top, int* right, int* bottom);
    void     X11ReadSize(HWindow window);
    void     X11FlushPlacement(HWindow window);
    bool     LinuxPrepareGL(HWindow window, VisualID* visual);
    bool     LinuxOpenGL(HWindow window);
    void     LinuxCloseGL(HWindow window);
    void     LinuxResizeGL(HWindow window);
    int  LinuxGLState(HWindow window, WindowState state);
}

#endif
