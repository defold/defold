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
#include <platform/platform_window_glfw3.h>
#include <glfw/glfw3.h>

static GLFWcursor* g_LinkCursor = 0;

void FontViewerMacOSSetClipboard(HWindow window, const char* text, uint32_t text_length)
{
    char* value = (char*)malloc(text_length + 1);
    memcpy(value, text, text_length);
    value[text_length] = 0;
    glfwSetClipboardString(dmPlatform::GetGLFWWindow(window), value);
    free(value);
}

void FontViewerMacOSSetLinkCursor(HWindow window, bool link_cursor)
{
    if (link_cursor && !g_LinkCursor)
    {
        g_LinkCursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    }

    glfwSetCursor(dmPlatform::GetGLFWWindow(window), link_cursor ? g_LinkCursor : 0);
}

void FontViewerMacOSDestroyLinkCursor(HWindow window)
{
    glfwSetCursor(dmPlatform::GetGLFWWindow(window), 0);

    if (g_LinkCursor)
    {
        glfwDestroyCursor(g_LinkCursor);
        g_LinkCursor = 0;
    }
}
