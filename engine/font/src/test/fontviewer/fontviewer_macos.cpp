// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0.

#include "fontviewer_macos.h"

#include <string.h>

#include <platform_window_constants.h>

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
}
