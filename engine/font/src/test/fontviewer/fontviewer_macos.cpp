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

#include "fontviewer_macos.h"

#include <stdlib.h>
#include <string.h>

#include <platform_window_constants.h>
#include <platform_window_glfw3_private.h>

static GLFWcursor* g_LinkCursor = 0;

void FontViewerMacOSInstallInput(HWindow                              window,
                                 FontViewerKeyboardCharCallback       char_callback,
                                 FontViewerKeyboardMarkedTextCallback marked_text_callback,
                                 void*                                user_data)
{
    dmPlatform::SetKeyboardCharCallback(window, char_callback, user_data);
    dmPlatform::SetKeyboardMarkedTextCallback(window, marked_text_callback, user_data);
}

void FontViewerMacOSPollInput(HWindow window, uint32_t layout_width, uint32_t layout_height, FontViewerMacOSInput* input)
{
    memset(input, 0, sizeof(*input));
    dmPlatform::GetMousePosition(window, &input->m_MouseX, &input->m_MouseY);
    const uint32_t framebuffer_width = dmPlatform::GetWindowWidth(window);
    const uint32_t framebuffer_height = dmPlatform::GetWindowHeight(window);
    if (framebuffer_width && framebuffer_height)
    {
        // Input arrives in framebuffer pixels; Nuklear and the preview use the
        // fixed logical WINDOW_WIDTH/WINDOW_HEIGHT coordinate system.
        input->m_MouseX = (int32_t)((int64_t)input->m_MouseX * layout_width / framebuffer_width);
        input->m_MouseY = (int32_t)((int64_t)input->m_MouseY * layout_height / framebuffer_height);
    }
    input->m_MouseWheel = dmPlatform::GetMouseWheel(window);
    input->m_LeftMouseDown = dmPlatform::GetMouseButton(window, dmPlatform::PLATFORM_MOUSE_BUTTON_LEFT) != 0;
    input->m_BackspaceDown = dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_BACKSPACE) != 0;
    input->m_DeleteDown = dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_DEL) != 0;
    input->m_EnterDown = dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_ENTER) != 0;
    input->m_EscapeDown = dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_ESC) != 0;
    input->m_LeftDown = dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_LEFT) != 0;
    input->m_RightDown = dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RIGHT) != 0;
    input->m_UpDown = dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_UP) != 0;
    input->m_DownDown = dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_DOWN) != 0;
    input->m_ShiftDown = dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_LSHIFT) != 0 ||
                         dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RSHIFT) != 0;
#if defined(DM_PLATFORM_MACOS)
    bool clipboard_modifier = dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_LSUPER) != 0 ||
                              dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RSUPER) != 0;
#else
    bool clipboard_modifier = dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_LCTRL) != 0 ||
                              dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RCTRL) != 0;
#endif
    input->m_CopyDown = clipboard_modifier && dmPlatform::GetKey(window, GLFW_KEY_C) != 0;
    input->m_SelectAllDown = clipboard_modifier && dmPlatform::GetKey(window, GLFW_KEY_A) != 0;
}

void FontViewerMacOSSetClipboard(HWindow window, const char* text, uint32_t text_length)
{
    char* value = (char*)malloc(text_length + 1);
    memcpy(value, text, text_length);
    value[text_length] = 0;
    glfwSetClipboardString(window->m_Window, value);
    free(value);
}

void FontViewerMacOSSetLinkCursor(HWindow window, bool link_cursor)
{
    if (link_cursor && !g_LinkCursor)
        g_LinkCursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    glfwSetCursor(window->m_Window, link_cursor ? g_LinkCursor : 0);
}

void FontViewerMacOSDestroyLinkCursor(HWindow window)
{
    glfwSetCursor(window->m_Window, 0);
    if (g_LinkCursor)
    {
        glfwDestroyCursor(g_LinkCursor);
        g_LinkCursor = 0;
    }
}
