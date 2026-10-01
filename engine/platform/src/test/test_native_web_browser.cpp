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

#include <emscripten.h>
#include <GL/gl.h>

#include "window.hpp"
#include "platform_window_constants.h"

// Browser tests drive the public window API and observe callbacks from compiled C++.
static HWindow g_Window;
static int g_ResizeCount;
static int g_FocusCount;
static int g_CloseCount;
static int g_CharCount;
static int g_LastChar;
static int g_Focused;

static void OnResize(void* user_data, uint32_t width, uint32_t height)
{
    ++g_ResizeCount;
}

static void OnFocus(void* user_data, uint32_t focused)
{
    ++g_FocusCount;
    g_Focused = focused;
}

static int OnClose(void* user_data)
{
    ++g_CloseCount;
    return 1;
}

static void OnCharacter(void* user_data, int character)
{
    ++g_CharCount;
    g_LastChar = character;
}

extern "C"
{
    EMSCRIPTEN_KEEPALIVE int TestOpen()
    {
        g_Window = dmPlatform::NewWindow();
        if (!g_Window)
            return -1;

        WindowCreateParams params;
        WindowCreateParamsInitialize(&params);
        params.m_Width = 640;
        params.m_Height = 480;
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGLES;
        params.m_GraphicsApiVersionHint = 2;
        params.m_ContextAlphabits = 8;
        params.m_HighDPI = 1;
        params.m_ResizeCallback = OnResize;
        params.m_FocusCallback = OnFocus;
        params.m_CloseCallback = OnClose;
        WindowResult result = dmPlatform::OpenWindow(g_Window, params);
        if (result == WINDOW_RESULT_OK)
            dmPlatform::SetKeyboardCharCallback(g_Window, OnCharacter, 0);
        return result;
    }

    EMSCRIPTEN_KEEPALIVE void TestClose()
    {
        dmPlatform::CloseWindow(g_Window);
        dmPlatform::DeleteWindow(g_Window);
        g_Window = 0;
    }

    EMSCRIPTEN_KEEPALIVE void TestFrame()
    {
        dmPlatform::PollEvents(g_Window);
        dmPlatform::SwapBuffers(g_Window);
    }

    EMSCRIPTEN_KEEPALIVE int TestRender()
    {
        if (!glGetString(GL_VERSION))
            return 0;
        unsigned char pixel[4];
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        return glGetError() == GL_NO_ERROR && pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 255;
    }

    EMSCRIPTEN_KEEPALIVE void TestSnapshot()
    {
        int32_t x = 0;
        int32_t y = 0;
        if (g_Window)
            dmPlatform::GetMousePosition(g_Window, &x, &y);
        EM_ASM({
            Module['testState'] = ({
                opened: $0, width: $1, height: $2, scale: $3, key: $4, button: $5,
                x: $6, y: $7, locked: $8, fullscreen: DefoldPlatform.isFullscreen,
                resizes: $9, focuses: $10, closes: $11, characters: $12, lastChar: $13, focused: $14
            });
        }, g_Window ? dmPlatform::GetWindowStateParam(g_Window, WINDOW_STATE_OPENED) : 0,
           g_Window ? dmPlatform::GetWindowWidth(g_Window) : 0,
           g_Window ? dmPlatform::GetWindowHeight(g_Window) : 0,
           g_Window ? dmPlatform::GetDisplayScaleFactor(g_Window) : 0.0f,
           g_Window ? dmPlatform::GetKey(g_Window, 'A') : 0,
           g_Window ? dmPlatform::GetMouseButton(g_Window, dmPlatform::PLATFORM_MOUSE_BUTTON_LEFT) : 0,
           x, y, g_Window ? dmPlatform::GetDeviceState(g_Window, WINDOW_DEVICE_STATE_CURSOR_LOCK) : 0,
           g_ResizeCount, g_FocusCount, g_CloseCount, g_CharCount, g_LastChar, g_Focused);
    }

    EMSCRIPTEN_KEEPALIVE void TestFullscreen()
    {
        // Fullscreen is requested by the JavaScript loader through this backend helper.
        EM_ASM({ DefoldPlatform.requestFullScreen(); });
    }

    EMSCRIPTEN_KEEPALIVE void TestSetCursorVisible(int visible)
    {
        dmPlatform::SetDeviceState(g_Window, WINDOW_DEVICE_STATE_CURSOR, visible != 0);
    }
}

int main(int argc, char** argv)
{
    return 0;
}
