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

#ifndef DM_WINDOW_MACOS_H
#define DM_WINDOW_MACOS_H

#import <AppKit/AppKit.h>
#include "../window_placement.h"
#include "../window_egl.h"

struct WindowPlacement;
@class DMWindowDelegate;

struct WindowInputCallbacks
{
    FWindowAddKeyboardCharCallback m_CharCallback;
    void*                          m_CharUserData;
    FWindowSetMarkedTextCallback   m_MarkedCallback;
    void*                          m_MarkedUserData;
    FWindowGamepadEventCallback    m_GamepadCallback;
    void*                          m_GamepadUserData;
};

struct dmWindow
{
    NSWindow*                      m_Window;
    NSView*                        m_View;
    DMWindowDelegate*              m_Delegate;
    NSOpenGLContext*               m_Context;
    NSOpenGLContext*               m_AuxContext;
    WindowEGL*                     m_EGL;
    WindowPlacement*               m_Placement;
    WindowCreateParams             m_Params;
    WindowInputCallbacks           m_InputCallbacks;
    dmPlatform::WindowFocusState   m_Focus;
    double                         m_Scroll;
    NSPoint                        m_VirtualCursor;
    NSPoint                        m_SavedCursor;
    NSPoint                        m_WarpDelta;
    NSSize                         m_ClientSize;
    NSSize                         m_DrawableSize;
    CGFloat                        m_Scale;
    uint32_t                       m_ResizeCallbackCount;
    uint32_t                       m_Samples;
    dmPlatform::WindowPressedInput m_Input;
    bool                           m_CursorLocked;
    bool                           m_CursorDisabled;
    bool                           m_Ready;
};

namespace dmPlatform
{
    NSView* NewContentView(HWindow window, NSRect frame);
    void    DetachContentView(NSView* view);
    void    UpdateCursorMode(HWindow window, bool focused);
    void    CenterLockedCursor(HWindow window);
    bool    OpenGLCreateContexts(HWindow window);
    void    OpenGLDestroyContexts(HWindow window);
    void    UpdateWindowMetrics(HWindow window, bool notify);
    void    SetCocoaWindowPosition(HWindow window, int x, int y);
    void    ShutdownJoysticks();
    bool    JoystickPresent(int index);
}
#endif
