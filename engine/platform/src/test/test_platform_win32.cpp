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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../platform_window_win32.h"

// Verifies both taskbar fallback icons match the executable before event polling,
// guarding against generic taskbar icons when startup blocks the window thread.
TEST(dmPlatform, WindowsClassIconsBeforePolling)
{
    HWindow window = dmPlatform::NewWindow();
    ASSERT_NE((HWindow) 0, window);

    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    params.m_Hidden = true;
    params.m_FocusOnShow = false;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));

    HWND hwnd = dmPlatform::GetWindowsHWND(window);
    // GLFW sets process DPI awareness, which affects the default icon size.
    HINSTANCE instance = (HINSTANCE) GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
    HICON icon = (HICON) LoadImageW(instance, MAKEINTRESOURCEW(100),
                                  IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    ASSERT_NE((HICON) 0, icon);
    ASSERT_EQ((ULONG_PTR) icon, GetClassLongPtrW(hwnd, GCLP_HICON));
    ASSERT_EQ((ULONG_PTR) icon, GetClassLongPtrW(hwnd, GCLP_HICONSM));

    dmPlatform::CloseWindow(window);
    dmPlatform::DeleteWindow(window);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
