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
#include "../platform_window_constants.h"
#include "../win32/window_win32.h"

// Verify teardown is safe before the HWND exists.
TEST(dmPlatform, WindowsCloseUnopenedWindow)
{
    HWindow window = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, window);
    dmPlatform::CloseWindow(window);
    ASSERT_EQ(0u, dmPlatform::GetWindowStateParam(window, WINDOW_STATE_OPENED));
    dmPlatform::DeleteWindow(window);
}

// Failed graphics startup and engine teardown may both close the window; reopening must still work afterwards.
TEST(dmPlatform, WindowsCloseWindowTwiceAndReopen)
{
    HWindow window = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, window);

    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    params.m_Hidden = true;
    params.m_FocusOnShow = false;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    dmPlatform::CloseWindow(window);
    dmPlatform::CloseWindow(window);
    ASSERT_EQ(0u, dmPlatform::GetWindowStateParam(window, WINDOW_STATE_OPENED));

    params.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    ASSERT_EQ(1u, dmPlatform::GetWindowStateParam(window, WINDOW_STATE_OPENED));
    dmPlatform::CloseWindow(window);
    dmPlatform::DeleteWindow(window);
}

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
    // Process DPI awareness affects the size of the default icon.
    HINSTANCE instance = (HINSTANCE) GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
    HICON icon = (HICON) LoadImageW(instance, MAKEINTRESOURCEW(100),
                                    IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    ASSERT_NE((HICON) 0, icon);
    ASSERT_EQ((ULONG_PTR) icon, GetClassLongPtrW(hwnd, GCLP_HICON));
    ASSERT_EQ((ULONG_PTR) icon, GetClassLongPtrW(hwnd, GCLP_HICONSM));

    dmPlatform::CloseWindow(window);
    dmPlatform::DeleteWindow(window);
}

struct WindowCallbacks
{
    uint32_t m_CloseCount, m_CharCount, m_Codepoint, m_EmptyMarked;
    bool     m_CloseAllowed;
};

static int CloseRequested(void* data)
{
    WindowCallbacks* callbacks = (WindowCallbacks*)data;
    ++callbacks->m_CloseCount;
    return callbacks->m_CloseAllowed;
}

static void CharInput(void* data, int value)
{
    WindowCallbacks* callbacks = (WindowCallbacks*)data;
    ++callbacks->m_CharCount;
    callbacks->m_Codepoint = value;
}

static void MarkedInput(void* data, char* text)
{
    WindowCallbacks* callbacks = (WindowCallbacks*)data;
    if (!*text)
    {
        ++callbacks->m_EmptyMarked;
    }
}

// Verifies a close veto preserves the HWND and a closed or failed window can reopen with another renderer.
TEST(dmPlatform, WindowsCloseAndReopen)
{
    HWindow window = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, window);
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    WindowCallbacks events = {};
    params.m_Hidden = true;
    params.m_CloseCallback = CloseRequested;
    params.m_CloseCallbackUserData = &events;
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGLES;
    ASSERT_EQ(WINDOW_RESULT_WINDOW_OPEN_ERROR, dmPlatform::OpenWindow(window, params));
    dmPlatform::CloseWindow(window);
    ASSERT_EQ(0u, dmPlatform::GetWindowStateParam(window, WINDOW_STATE_OPENED));
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    HWND hwnd = dmPlatform::GetWindowsHWND(window);
    ASSERT_FALSE(IsWindowVisible(hwnd));
    ASSERT_EQ((uintptr_t)hwnd, dmPlatform::GetNativeWindowId(window).m_Id);
    ASSERT_EQ((HGLRC)0, dmPlatform::GetWindowsHGLRC(window));
    ASSERT_EQ((uintptr_t)0, dmPlatform::GetProcAddress(window, "glGetString"));
    SendMessageW(hwnd, WM_CLOSE, 0, 0);
    ASSERT_EQ(1U, events.m_CloseCount);
    ASSERT_TRUE(IsWindow(hwnd));
    events.m_CloseAllowed = true;
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    dmPlatform::PollEvents(window);
    ASSERT_EQ(2U, events.m_CloseCount);
    ASSERT_FALSE(IsWindow(hwnd));
    ASSERT_EQ((uintptr_t)0, dmPlatform::GetNativeWindowId(window).m_Id);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    ASSERT_EQ((HGLRC)0, dmPlatform::GetWindowsHGLRC(window));
    ASSERT_FALSE(dmPlatform::GetWindowStateParam(window, WINDOW_STATE_AUX_CONTEXT));
    dmPlatform::DeleteWindow(window);
}

// Verifies scan-code modifiers, UTF-16 input, composition end, wheel accumulation and focus cleanup through the HWND.
TEST(dmPlatform, WindowsInputMessages)
{
    HWindow window = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, window);
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_Hidden = true;
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    WindowCallbacks events = {};
    dmPlatform::SetKeyboardCharCallback(window, CharInput, &events);
    dmPlatform::SetKeyboardMarkedTextCallback(window, MarkedInput, &events);
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    HWND hwnd = dmPlatform::GetWindowsHWND(window);
    SendMessageW(hwnd, WM_KEYDOWN, VK_CONTROL, 0x011d0001);
    ASSERT_EQ(1, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RCTRL));
    ASSERT_EQ(0, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_LCTRL));
    SendMessageW(hwnd, WM_KEYDOWN, VK_CANCEL, 0x01460001);
    ASSERT_EQ(1, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_PAUSE));
    ASSERT_EQ(0, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_SCROLL_LOCK));
    SendMessageW(hwnd, WM_KEYUP, VK_CANCEL, 0xc1460001);
    ASSERT_EQ(0, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_PAUSE));
    SendMessageW(hwnd, WM_KEYDOWN, VK_SHIFT, 0x00360001);
    ASSERT_EQ(1, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RSHIFT));
    SendMessageW(hwnd, WM_KEYUP, VK_SHIFT, 0xc1360001);
    ASSERT_EQ(0, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RSHIFT));
    SendMessageW(hwnd, WM_KEYDOWN, VK_SHIFT, 0x01360001);
    ASSERT_EQ(1, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RSHIFT));
    ASSERT_EQ(0, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_LSHIFT));
    SendMessageW(hwnd, WM_KEYDOWN, VK_PROCESSKEY, 0x00390001);
    ASSERT_EQ(0, dmPlatform::GetKey(window, ' '));
    SendMessageW(hwnd, WM_KEYDOWN, VK_SPACE, 0x00390001);
    SendMessageW(hwnd, WM_KEYUP, VK_PROCESSKEY, 0xc0390001);
    ASSERT_EQ(1, dmPlatform::GetKey(window, ' '));
    SendMessageW(hwnd, WM_KEYUP, VK_SPACE, 0xc0390001);
    SendMessageW(hwnd, WM_CHAR, 0xd83d, 0);
    ASSERT_EQ(0U, events.m_CharCount);
    SendMessageW(hwnd, WM_CHAR, 0xde03, 0);
    ASSERT_EQ(1U, events.m_CharCount);
    ASSERT_EQ(0x1f603U, events.m_Codepoint);
    SendMessageW(hwnd, WM_IME_ENDCOMPOSITION, 0, 0);
    ASSERT_EQ(1U, events.m_EmptyMarked);
    SendMessageW(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, 30), 0);
    ASSERT_EQ(0, dmPlatform::GetMouseWheel(window));
    SendMessageW(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, 90), 0);
    ASSERT_EQ(1, dmPlatform::GetMouseWheel(window));
    SendMessageW(hwnd, WM_XBUTTONDOWN, MAKEWPARAM(0, XBUTTON2), 0);
    ASSERT_EQ(1, dmPlatform::GetMouseButton(window, 4));
    SendMessageW(hwnd, WM_KILLFOCUS, 0, 0);
    ASSERT_EQ(0, dmPlatform::GetMouseButton(window, 4));
    ASSERT_EQ(0, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RCTRL));
    ASSERT_EQ(0, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RSHIFT));
    dmPlatform::DeleteWindow(window);
}

// Alt+Space must reach the game without entering the modal system menu. Alt+F4 must still reach Windows.
TEST(dmPlatform, WindowsKeyboardMenu)
{
    dmWindow window = {};
    LRESULT result = 0;
    EXPECT_FALSE(dmPlatform::Win32HandleInput(&window, WM_SYSKEYDOWN, VK_SPACE, 0x20390001, &result));
    EXPECT_TRUE(dmPlatform::GetKey(&window, ' '));
    EXPECT_TRUE(dmPlatform::Win32HandleInput(&window, WM_SYSCHAR, ' ', 0x20390001, &result));
    EXPECT_TRUE(dmPlatform::Win32HandleInput(&window, WM_SYSCOMMAND, SC_KEYMENU, ' ', &result));
    EXPECT_FALSE(dmPlatform::Win32HandleInput(&window, WM_SYSKEYDOWN, VK_F4, 0x203e0001, &result));
    EXPECT_FALSE(dmPlatform::Win32HandleInput(&window, WM_SYSCOMMAND, SC_CLOSE, 0, &result));
}

// Activating a locked window through a caption button must leave the pointer free until the click completes.
TEST(dmPlatform, WindowsCaptionActivationDefersCursorCapture)
{
    HWindow window = dmPlatform::NewWindow();
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    HWND hwnd = window->m_Window;
    SetFocus(0);
    dmPlatform::SetDeviceState(window, WINDOW_DEVICE_STATE_CURSOR, false);
    SendMessageW(hwnd, WM_MOUSEACTIVATE, (WPARAM)hwnd, MAKELPARAM(HTCLOSE, WM_LBUTTONDOWN));
    SetFocus(hwnd);
    EXPECT_FALSE(window->m_CursorCaptured);
    SendMessageW(hwnd, WM_CAPTURECHANGED, 0, 0);
    EXPECT_TRUE(window->m_CursorCaptured);
    dmPlatform::DeleteWindow(window);
}

// Polling must reconcile missing key-up events from overlapping Shifts and Win+V while preserving held modifiers.
TEST(dmPlatform, WindowsMissingModifierReleases)
{
    HWindow window = dmPlatform::NewWindow();
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    HWND hwnd = dmPlatform::GetWindowsHWND(window);
    SetFocus(hwnd);
    ASSERT_EQ(hwnd, GetFocus());
    dmPlatform::PollEvents(window);

    BYTE saved[256], state[256];
    ASSERT_TRUE(GetKeyboardState(saved));
    memcpy(state, saved, sizeof(state));
    state[VK_SHIFT] = state[VK_RSHIFT] = 0x80;
    state[VK_LSHIFT] = 0;
    state[VK_LWIN] = 0;
    state[VK_RWIN] = 0x80;
    SendMessageW(hwnd, WM_KEYDOWN, VK_SHIFT, 0x002a0001);
    SendMessageW(hwnd, WM_KEYDOWN, VK_SHIFT, 0x00360001);
    SendMessageW(hwnd, WM_KEYDOWN, VK_LWIN, 0x015b0001);
    SendMessageW(hwnd, WM_KEYDOWN, VK_RWIN, 0x015c0001);
    ASSERT_TRUE(SetKeyboardState(state));
    dmPlatform::PollEvents(window);
    EXPECT_EQ(0, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_LSHIFT));
    EXPECT_EQ(1, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RSHIFT));
    EXPECT_EQ(0, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_LSUPER));
    EXPECT_EQ(1, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RSUPER));
    state[VK_SHIFT] = state[VK_RSHIFT] = 0;
    state[VK_RWIN] = 0;
    ASSERT_TRUE(SetKeyboardState(state));
    dmPlatform::PollEvents(window);
    EXPECT_EQ(0, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RSHIFT));
    EXPECT_EQ(0, dmPlatform::GetKey(window, dmPlatform::PLATFORM_KEY_RSUPER));
    SetKeyboardState(saved);
    dmPlatform::DeleteWindow(window);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
