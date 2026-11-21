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

#ifndef DM_NATIVE_WINDOW_WIN32_H
#define DM_NATIVE_WINDOW_WIN32_H

#include "../platform_window_win32.h"
#include "window_geometry.h"

struct Win32InputCallbacks
{
    FWindowAddKeyboardCharCallback m_Char;
    void*                          m_CharData;
    FWindowSetMarkedTextCallback   m_Marked;
    void*                          m_MarkedData;
    FWindowGamepadEventCallback    m_Gamepad;
    void*                          m_GamepadData;
};

struct dmWindow
{
    HWND  m_Window;
    HWND  m_AuxWindow;
    HDC   m_DC, m_AuxDC;
    HGLRC m_Context, m_AuxContext;
    BOOL(WINAPI * m_SwapInterval)(int);
    WindowPlacement*    m_Placement;
    WindowCreateParams  m_Params;
    Win32InputCallbacks m_Callbacks;
    HWindow             m_Next;
    POINT               m_SavedCursor, m_VirtualCursor, m_LastCursor;
    uint32_t            m_Width, m_Height, m_Samples;
    float               m_Scale;
    double              m_Wheel;
    uint16_t            m_HighSurrogate;
    uint8_t             m_Keys[349], m_Buttons[8];
    bool                m_Ready, m_Iconified, m_CursorLocked, m_CursorCaptured, m_CaptionAction;
};

namespace dmPlatform
{
    extern const wchar_t* const WIN32_WINDOW_CLASS;
    float Win32MonitorScale(HMONITOR monitor);
    void  Win32InitDpi();
    void  Win32AdjustFrame(RECT* rect, DWORD style, DWORD ex_style, UINT dpi);
    bool  Win32HandleInput(HWindow window, UINT message, WPARAM wp, LPARAM lp, LRESULT* result);
    void  Win32UpdateCursor(HWindow window, bool focused);
    void  Win32CenterCursor(HWindow window);
    void  Win32JoysticksChanged();
    void  Win32PollJoysticks(bool initialize);
    void  Win32ShutdownJoysticks();
    void  Win32NotifyGamepad(int index, WindowGamepadEvent event);
    bool  Win32JoystickPresent(int index);
    bool  Win32OpenGLCreate(HWindow window);
    void  Win32OpenGLDestroy(HWindow window);
}
#endif
