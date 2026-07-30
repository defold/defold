// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0.

#ifndef DM_FONT_VIEWER_MACOS_H
#define DM_FONT_VIEWER_MACOS_H

#include <stdint.h>

#include <platform/window.hpp>

typedef void (*FontViewerKeyboardCharCallback)(void* user_data, int codepoint);
typedef void (*FontViewerKeyboardMarkedTextCallback)(void* user_data, char* text);

struct FontViewerMacOSInput
{
    int32_t m_MouseX;
    int32_t m_MouseY;
    int32_t m_MouseWheel;
    bool    m_LeftMouseDown;
    bool    m_BackspaceDown;
    bool    m_DeleteDown;
    bool    m_EnterDown;
    bool    m_EscapeDown;
    bool    m_LeftDown;
    bool    m_RightDown;
    bool    m_UpDown;
    bool    m_DownDown;
    bool    m_ShiftDown;
};

void FontViewerMacOSInstallInput(HWindow                              window,
                                 FontViewerKeyboardCharCallback       char_callback,
                                 FontViewerKeyboardMarkedTextCallback marked_text_callback,
                                 void*                                user_data);
void FontViewerMacOSPollInput(HWindow window, uint32_t layout_width, uint32_t layout_height, FontViewerMacOSInput* input);

#endif // DM_FONT_VIEWER_MACOS_H
