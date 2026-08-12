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
    bool    m_CopyDown;
    bool    m_SelectAllDown;
};

void FontViewerMacOSInstallInput(HWindow                              window,
                                 FontViewerKeyboardCharCallback       char_callback,
                                 FontViewerKeyboardMarkedTextCallback marked_text_callback,
                                 void*                                user_data);
void FontViewerMacOSPollInput(HWindow window, uint32_t layout_width, uint32_t layout_height, FontViewerMacOSInput* input);
void FontViewerMacOSSetClipboard(HWindow window, const char* text, uint32_t text_length);

#endif // DM_FONT_VIEWER_MACOS_H
