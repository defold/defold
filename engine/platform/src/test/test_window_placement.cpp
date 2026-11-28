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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlib/dstrings.h>
#include <dlib/path.h>
#include <dlib/sys.h>
#include <dlib/time.h>
#include "../window_placement.h"
#include "test_window_placement_native.h"
#include "../window_state.h"

#if defined(__MACH__)
#include <CoreGraphics/CoreGraphics.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
#include "../platform_window_win32.h"
#endif
#if defined(__linux__)
#include "../linux/window_linux.h"
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#endif

static void SetStateEnvironment(const char* value)
{
#if defined(_WIN32)
    _putenv_s("DM_WINDOW_STATE", value ? value : "");
#else
    if (value)
    {
        setenv("DM_WINDOW_STATE", value, 1);
    }
    else
    {
        unsetenv("DM_WINDOW_STATE");
    }

#endif
}

class WindowPlacementTest : public jc_test_base_class
{
    protected:
    HWindow            m_Window;
    char*              m_PreviousEnvironment;
    char               m_Path[DMPATH_MAX_PATH];
    WindowCreateParams m_Params;
    struct ResizeEvents
    {
        uint32_t m_Count, m_Width, m_Height;
    } m_ResizeEvents;

    static void OnResize(void* data, uint32_t width, uint32_t height)
    {
        ResizeEvents* events = (ResizeEvents*)data;
        ++events->m_Count;
        events->m_Width = width;
        events->m_Height = height;
    }

    void SetUp()
    {
        const char* value = dmSys::GetEnv("DM_WINDOW_STATE");
        m_PreviousEnvironment = value ? strdup(value) : 0;
        SetStateEnvironment("test-window-placement/player-0");
        dmSys::Mkdir("test-window-placement", 0755);
        remove("test-window-placement/reset-size");
        m_Window = dmPlatform::NewWindow();
        ASSERT_NE((HWindow)0, m_Window);
        const char* expected_backend = dmSys::GetEnv("DM_TEST_WINDOW_BACKEND");
        if (expected_backend)
        {
            ASSERT_STREQ(expected_backend, dmWindowTest::Backend());
        }

        dmSnPrintf(m_Path, sizeof(m_Path), "test-window-placement/player-0-%s.ini", dmWindowTest::Backend());
        remove(m_Path);
        WindowCreateParamsInitialize(&m_Params);
        m_Params.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN; // This window needs no graphics context.
        m_Params.m_Title = "Defold window placement test";
        m_Params.m_Width = 400;
        m_Params.m_Height = 300;
        m_Params.m_FocusOnShow = false;
        m_Params.m_Hidden = true;
    }

    void OpenAndShow()
    {
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, m_Params));
        dmPlatform::ShowWindow(m_Window);
        dmPlatform::PollEvents(m_Window);
    }

    void SetPosition(int x, int y)
    {
        if (!strcmp(dmWindowTest::Backend(), "wayland"))
        {
            return;
        }

        dmPlatform::SetWindowPosition(m_Window, x, y);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        int actual_x, actual_y;
        for (;;)
        {
            dmPlatform::PollEvents(m_Window);
            dmWindowTest::Position(m_Window, &actual_x, &actual_y);
            if ((actual_x == x && actual_y == y) || dmTime::GetMonotonicTime() >= deadline)
            {
                break;
            }
        }

        ASSERT_EQ(x, actual_x);
        ASSERT_EQ(y, actual_y);
    }

    void DestroyWindow()
    {
        if (m_Window)
        {
            if (dmPlatform::GetWindowStateParam(m_Window, WINDOW_STATE_OPENED))
            {
                dmPlatform::CloseWindow(m_Window);
            }

            dmPlatform::DeleteWindow(m_Window);
            m_Window = 0;
        }
    }

    void Recreate()
    {
        DestroyWindow();
        m_Window = dmPlatform::NewWindow();
        ASSERT_NE((HWindow)0, m_Window);
        OpenAndShow();
    }

    void TearDown()
    {
        DestroyWindow();
        remove(m_Path);
        remove("test-window-placement/reset-size");
        dmSys::Rmdir("test-window-placement");
        SetStateEnvironment(m_PreviousEnvironment);
        free(m_PreviousEnvironment);
    }

};

// Desktop windows must reject the null graphics API and remain usable for a later Vulkan open.
TEST_F(WindowPlacementTest, RejectsNullGraphicsApi)
{
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, m_Params));
    dmPlatform::CloseWindow(m_Window);
    m_Params.m_GraphicsApi = WINDOW_GRAPHICS_API_NULL;
    ASSERT_EQ(WINDOW_RESULT_WINDOW_OPEN_ERROR, dmPlatform::OpenWindow(m_Window, m_Params));
    ASSERT_EQ(0u, dmPlatform::GetWindowStateParam(m_Window, WINDOW_STATE_OPENED));
    ASSERT_EQ(dmPlatform::NATIVE_WINDOW_TYPE_NONE, dmPlatform::GetNativeWindowId(m_Window).m_Type);
    dmPlatform::CloseWindow(m_Window);
    m_Params.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, m_Params));
}

// A windowed drawable needs both dimensions. Failed opens must leave the handle usable for a retry.
TEST_F(WindowPlacementTest, RejectsZeroWindowedDimensions)
{
    m_Params.m_Width = 0;
    ASSERT_EQ(WINDOW_RESULT_WINDOW_OPEN_ERROR, dmPlatform::OpenWindow(m_Window, m_Params));
    ASSERT_EQ(0u, dmPlatform::GetWindowStateParam(m_Window, WINDOW_STATE_OPENED));
    dmPlatform::CloseWindow(m_Window);

    m_Params.m_Width = 400;
    m_Params.m_Height = 0;
    ASSERT_EQ(WINDOW_RESULT_WINDOW_OPEN_ERROR, dmPlatform::OpenWindow(m_Window, m_Params));
    ASSERT_EQ(0u, dmPlatform::GetWindowStateParam(m_Window, WINDOW_STATE_OPENED));
    dmPlatform::CloseWindow(m_Window);

    m_Params.m_Height = 300;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, m_Params));
}

// Native IDs are stable while the window exists and unavailable before opening or after closing it.
TEST_F(WindowPlacementTest, NativeWindowIdentity)
{
    dmPlatform::NativeWindowId identity = dmPlatform::GetNativeWindowId(m_Window);
    ASSERT_EQ(dmPlatform::NATIVE_WINDOW_TYPE_NONE, identity.m_Type);
    ASSERT_EQ(0U, identity.m_Id);
    identity = dmPlatform::GetNativeWindowId(0);
    ASSERT_EQ(dmPlatform::NATIVE_WINDOW_TYPE_NONE, identity.m_Type);
    ASSERT_EQ(0U, identity.m_Id);
    OpenAndShow();
    dmPlatform::NativeWindowType expected_type = dmPlatform::NATIVE_WINDOW_TYPE_NONE;
    const char* backend = dmWindowTest::Backend();
    if (!strcmp(backend, "cocoa"))
    {
        expected_type = dmPlatform::NATIVE_WINDOW_TYPE_COCOA;
    }
    else if (!strcmp(backend, "win32"))
    {
        expected_type = dmPlatform::NATIVE_WINDOW_TYPE_WIN32;
    }
    else if (!strcmp(backend, "x11"))
    {
        expected_type = dmPlatform::NATIVE_WINDOW_TYPE_X11;
    }

    identity = dmPlatform::GetNativeWindowId(m_Window);
    ASSERT_EQ(expected_type, identity.m_Type);
    ASSERT_EQ(expected_type != dmPlatform::NATIVE_WINDOW_TYPE_NONE, identity.m_Id != 0);
#if defined(_WIN32)
    HWND hwnd = (HWND)(uintptr_t)identity.m_Id;
    ASSERT_EQ(dmPlatform::GetWindowsHWND(m_Window), hwnd);
    ASSERT_TRUE(IsWindow(hwnd));
#elif defined(__MACH__)
    // Resolve the ID through WindowServer to catch accidentally exposing an NSWindow pointer.
    ASSERT_LE(identity.m_Id, (uint64_t)UINT32_MAX);
    CFArrayRef windows = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, (CGWindowID)identity.m_Id);
    ASSERT_NE((CFArrayRef)0, windows);
    ASSERT_EQ((CFIndex)1, CFArrayGetCount(windows));
    CFDictionaryRef info = (CFDictionaryRef)CFArrayGetValueAtIndex(windows, 0);
    CFNumberRef owner = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowOwnerPID);
    ASSERT_NE((CFNumberRef)0, owner);
    int pid = 0;
    ASSERT_TRUE(CFNumberGetValue(owner, kCFNumberIntType, &pid));
    CFRelease(windows);
    ASSERT_EQ(getpid(), pid);
#endif
    dmPlatform::SetWindowSize(m_Window, 501, 351);
    dmPlatform::PollEvents(m_Window);
    dmPlatform::NativeWindowId resized = dmPlatform::GetNativeWindowId(m_Window);
    ASSERT_EQ(identity.m_Type, resized.m_Type);
    ASSERT_EQ(identity.m_Id, resized.m_Id);
    dmPlatform::CloseWindow(m_Window);
    identity = dmPlatform::GetNativeWindowId(m_Window);
    ASSERT_EQ(dmPlatform::NATIVE_WINDOW_TYPE_NONE, identity.m_Type);
    ASSERT_EQ(0U, identity.m_Id);
}

#if defined(_WIN32)
// Wallpaper and same-layout display notifications must preserve saved placement until a work-area change.
TEST_F(WindowPlacementTest, OnlyWorkAreaSettingsClampPosition)
{
    OpenAndShow();
    HWND hwnd = dmPlatform::GetWindowsHWND(m_Window);
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    ASSERT_TRUE(GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor));
    const int x = monitor.rcWork.left - 64;
    const int y = monitor.rcWork.top + 80;
    SetPosition(x, y);

    SendMessageW(hwnd, WM_SETTINGCHANGE, SPI_SETDESKWALLPAPER, 0);
    dmPlatform::UpdateWindowPlacement(m_Window, true);
    int actual_x, actual_y;
    dmWindowTest::Position(m_Window, &actual_x, &actual_y);
    EXPECT_EQ(x, actual_x);
    EXPECT_EQ(y, actual_y);

    DEVMODEW mode = {};
    mode.dmSize = sizeof(mode);
    ASSERT_TRUE(EnumDisplaySettingsW(0, ENUM_CURRENT_SETTINGS, &mode));
    SendMessageW(hwnd, WM_DISPLAYCHANGE, mode.dmBitsPerPel, MAKELPARAM(mode.dmPelsWidth, mode.dmPelsHeight));
    dmPlatform::UpdateWindowPlacement(m_Window, true);
    dmWindowState::State saved = {};
    ASSERT_EQ(dmWindowState::LOAD_OK, dmWindowState::Load(m_Path, &saved));
    EXPECT_EQ(x, saved.m_Rect.m_X);
    EXPECT_EQ(y, saved.m_Rect.m_Y);

    SendMessageW(hwnd, WM_SETTINGCHANGE, SPI_SETWORKAREA, 0);
    dmPlatform::UpdateWindowPlacement(m_Window, true);
    dmWindowTest::Position(m_Window, &actual_x, &actual_y);
    EXPECT_GE(actual_x, monitor.rcWork.left);
    EXPECT_EQ(y, actual_y);
}
#endif

#if defined(__linux__)
// On a private X server, RandR metadata must preserve a partial offscreen position until the work area changes.
TEST_F(WindowPlacementTest, X11ClampOnlyOnLayoutChange)
{
    if (!getenv("DM_TEST_X11_NO_WM") || !m_Window->m_X11)
    {
        SKIP();
    }

    OpenAndShow();
    SetPosition(-70, 80);
    Display* display = m_Window->m_Display;
    Window root = DefaultRootWindow(display);
    int opcode, event_base, error_base;
    ASSERT_TRUE(XQueryExtension(display, "RANDR", &opcode, &event_base, &error_base));
    XEvent event = {};
    event.xany.type = event_base + RRNotify;
    event.xany.display = display;
    event.xany.window = root;
    ((XRRNotifyEvent*)&event)->subtype = RRNotify_OutputProperty;
    XPutBackEvent(display, &event);
    dmPlatform::PollEvents(m_Window);
    dmPlatform::UpdateWindowPlacement(m_Window, true);
    int x, y;
    dmWindowTest::Position(m_Window, &x, &y);
    EXPECT_EQ(-70, x);
    EXPECT_EQ(80, y);

    Atom work_area = XInternAtom(display, "_NET_WORKAREA", False);
    int screen = DefaultScreen(display);
    unsigned long bounds[] = { 24, 0, (unsigned long)DisplayWidth(display, screen) - 24, (unsigned long)DisplayHeight(display, screen) };
    XChangeProperty(display, root, work_area, XA_CARDINAL, 32, PropModeReplace, (unsigned char*)bounds, 4);
    XSync(display, False);
    dmPlatform::PollEvents(m_Window);
    dmPlatform::UpdateWindowPlacement(m_Window, true);
    dmWindowTest::Position(m_Window, &x, &y);
    EXPECT_EQ(24, x);
    EXPECT_EQ(80, y);
    XDeleteProperty(display, root, work_area);
}
#endif

// Client metrics must use native setter units, independently of high-DPI drawable dimensions.
TEST_F(WindowPlacementTest, ClientAndDrawableMetrics)
{
    m_Params.m_HighDPI = true;
    OpenAndShow();
    ASSERT_TRUE(dmPlatform::IsWindowResizeSupported(m_Window));
    dmPlatform::SetWindowSize(m_Window, 501, 351);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    int client_width, client_height, drawable_width, drawable_height;
    uint32_t width, height;
    for (;;)
    {
        dmPlatform::PollEvents(m_Window);
        dmWindowTest::ClientSize(m_Window, &client_width, &client_height);
        ASSERT_TRUE(dmPlatform::GetWindowClientSize(m_Window, &width, &height));
        if ((client_width == 501 && client_height == 351 && width == 501 && height == 351) || dmTime::GetMonotonicTime() >= deadline)
        {
            break;
        }
    }

    dmWindowTest::DrawableSize(m_Window, &drawable_width, &drawable_height);
    ASSERT_EQ((uint32_t)client_width, width);
    ASSERT_EQ((uint32_t)client_height, height);
    ASSERT_EQ(501U, width);
    ASSERT_EQ(351U, height);
    ASSERT_EQ((uint32_t)drawable_width, dmPlatform::GetWindowWidth(m_Window));
    ASSERT_EQ((uint32_t)drawable_height, dmPlatform::GetWindowHeight(m_Window));
}

// An explicit resize reports drawable pixels once, including when the client already has the requested size.
TEST_F(WindowPlacementTest, ExplicitResizeCallback)
{
    ResizeEvents& events = m_ResizeEvents;
    events.m_Count = 0;
    m_Params.m_HighDPI = true;
    m_Params.m_ResizeCallback = OnResize;
    m_Params.m_ResizeCallbackUserData = &events;
    OpenAndShow();
    uint32_t width, height;
    ASSERT_TRUE(dmPlatform::GetWindowClientSize(m_Window, &width, &height));
    events.m_Count = 0;
    WindowSetSize(m_Window, width + 17, height + 11);
    const uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!events.m_Count && dmTime::GetMonotonicTime() < deadline)
    {
        dmPlatform::PollEvents(m_Window);
    }

    ASSERT_EQ(1u, events.m_Count);
    EXPECT_EQ(WindowGetWidth(m_Window), events.m_Width);
    EXPECT_EQ(WindowGetHeight(m_Window), events.m_Height);

    ASSERT_TRUE(dmPlatform::GetWindowClientSize(m_Window, &width, &height));
    WindowSetSize(m_Window, width, height);
    ASSERT_EQ(2u, events.m_Count);
    EXPECT_EQ(WindowGetWidth(m_Window), events.m_Width);
    EXPECT_EQ(WindowGetHeight(m_Window), events.m_Height);
    dmPlatform::PollEvents(m_Window);
    EXPECT_EQ(2u, events.m_Count);
}

// Verifies close captures the actual geometry and X11 restoration preserves the requested position.
TEST_F(WindowPlacementTest, RecreateRestoresActualGeometry)
{
    OpenAndShow();
    dmPlatform::SetWindowSize(m_Window, 501, 351);
    SetPosition(170, 190);
    dmWindowState::Rect expected;
    dmWindowTest::ClientSize(m_Window, &expected.m_Width, &expected.m_Height);
    if (strcmp(dmWindowTest::Backend(), "wayland") != 0)
    {
        dmWindowTest::Position(m_Window, &expected.m_X, &expected.m_Y);
    }

    Recreate();
    int width, height;
    dmWindowTest::ClientSize(m_Window, &width, &height);
    ASSERT_EQ(expected.m_Width, width);
    ASSERT_EQ(expected.m_Height, height);
    if (strcmp(dmWindowTest::Backend(), "wayland") != 0)
    {
        int x, y;
        dmWindowTest::Position(m_Window, &x, &y);
        ASSERT_EQ(expected.m_X, x);
        ASSERT_EQ(expected.m_Y, y);
    }

    // Repeat to detect cumulative scaling during creation.
    Recreate();
    dmWindowTest::ClientSize(m_Window, &width, &height);
    ASSERT_EQ(expected.m_Width, width);
    ASSERT_EQ(expected.m_Height, height);
}

#if defined(__linux__)
// An X11 placement save must capture pending resizes without calling application code during close.
TEST_F(WindowPlacementTest, X11SavePendingResizeWithoutCallback)
{
    if (!m_Window->m_X11)
    {
        SKIP();
    }

    m_ResizeEvents.m_Count = 0;
    m_Params.m_ResizeCallback = OnResize;
    m_Params.m_ResizeCallbackUserData = &m_ResizeEvents;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, m_Params));
    XSetWindowAttributes attributes = {};
    attributes.override_redirect = True;
    XChangeWindowAttributes(m_Window->m_Display, m_Window->m_Xid, CWOverrideRedirect, &attributes);
    dmPlatform::ShowWindow(m_Window);
    XSync(m_Window->m_Display, False);
    dmPlatform::PollEvents(m_Window);
    m_ResizeEvents.m_Count = 0;

    dmPlatform::SetWindowSize(m_Window, 501, 351);
    dmPlatform::UpdateWindowPlacement(m_Window, true);
    EXPECT_EQ(0u, m_ResizeEvents.m_Count);
    dmWindowState::State saved;
    ASSERT_EQ(dmWindowState::LOAD_OK, dmWindowState::Load(m_Path, &saved));
    EXPECT_EQ(501, saved.m_Rect.m_Width);
    EXPECT_EQ(351, saved.m_Rect.m_Height);
    dmPlatform::PollEvents(m_Window);
    EXPECT_EQ(1u, m_ResizeEvents.m_Count);

    dmPlatform::SetWindowSize(m_Window, 517, 363);
    dmPlatform::CloseWindow(m_Window);
    EXPECT_EQ(1u, m_ResizeEvents.m_Count);
    ASSERT_EQ(dmWindowState::LOAD_OK, dmWindowState::Load(m_Path, &saved));
    EXPECT_EQ(517, saved.m_Rect.m_Width);
    EXPECT_EQ(363, saved.m_Rect.m_Height);
}

static bool SaveResizedPlacement(const char* path, const WindowCreateParams& params)
{
    dmWindowState::State saved = {};
    saved.m_ConfigWidth = params.m_Width;
    saved.m_ConfigHeight = params.m_Height;
    saved.m_Rect = { 0, 0, 501, 351 };
    saved.m_Monitor.m_WorkArea = { 0, 0, 1280, 1024 };
    saved.m_Monitor.m_ScaleX = saved.m_Monitor.m_ScaleY = 1;
    return dmWindowState::Save(path, saved);
}

// Restored drawable dimensions are available during startup, but resize callbacks require an initialized renderer.
TEST_F(WindowPlacementTest, DeferredRestoreResize)
{
    ASSERT_TRUE(SaveResizedPlacement(m_Path, m_Params));
    m_Params.m_ResizeCallback = OnResize;
    m_Params.m_ResizeCallbackUserData = &m_ResizeEvents;
    m_ResizeEvents.m_Count = 0;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, m_Params));
    uint32_t width, height;
    ASSERT_TRUE(dmPlatform::GetWindowClientSize(m_Window, &width, &height));
    EXPECT_EQ(501u, width);
    EXPECT_EQ(351u, height);
    EXPECT_EQ(0u, m_ResizeEvents.m_Count);
    dmPlatform::PollEvents(m_Window);
    EXPECT_EQ(1u, m_ResizeEvents.m_Count);
    EXPECT_EQ(WindowGetWidth(m_Window), m_ResizeEvents.m_Width);
    EXPECT_EQ(WindowGetHeight(m_Window), m_ResizeEvents.m_Height);
    dmPlatform::PollEvents(m_Window);
    EXPECT_EQ(1u, m_ResizeEvents.m_Count);

    dmPlatform::CloseWindow(m_Window);
    m_ResizeEvents.m_Count = 0;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, m_Params));
    WindowSetSize(m_Window, 480, 320);
    dmPlatform::PollEvents(m_Window);
    EXPECT_EQ(1u, m_ResizeEvents.m_Count);
    EXPECT_EQ(WindowGetWidth(m_Window), m_ResizeEvents.m_Width);
}

struct WindowsToDelete
{
    HWindow m_Current;
    HWindow m_Next;
};

static void DeleteOnResize(void* data, uint32_t, uint32_t)
{
    WindowsToDelete* windows = (WindowsToDelete*)data;
    dmPlatform::DeleteWindow(windows->m_Current);
    dmPlatform::DeleteWindow(windows->m_Next);
    windows->m_Current = windows->m_Next = 0;
}

// Deleting both the dispatching window and its successor must still deliver the surviving window's restore event.
TEST_F(WindowPlacementTest, DeleteWindowsDuringRestoreResize)
{
    ASSERT_TRUE(SaveResizedPlacement(m_Path, m_Params));
    m_ResizeEvents.m_Count = 0;
    m_Params.m_ResizeCallback = OnResize;
    m_Params.m_ResizeCallbackUserData = &m_ResizeEvents;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, m_Params));

    WindowsToDelete windows;
    windows.m_Next = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, windows.m_Next);
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(windows.m_Next, m_Params));
    windows.m_Current = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, windows.m_Current);
    m_Params.m_ResizeCallback = DeleteOnResize;
    m_Params.m_ResizeCallbackUserData = &windows;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(windows.m_Current, m_Params));

    dmPlatform::PollEvents(windows.m_Current);
    EXPECT_EQ((HWindow)0, windows.m_Current);
    EXPECT_EQ(m_Window, dmPlatform::LinuxActiveWindow());
    EXPECT_EQ(1u, m_ResizeEvents.m_Count);
    dmPlatform::PollEvents(m_Window);
    EXPECT_EQ(1u, m_ResizeEvents.m_Count);
}
#endif

// Verifies repeated launches preserve size and position on secondary monitors, including mixed DPI.
TEST_F(WindowPlacementTest, RecreateRestoresSecondaryMonitorGeometry)
{
    if (!strcmp(dmWindowTest::Backend(), "wayland"))
    {
        SKIP();
    }

    int count = dmWindowTest::MonitorCount();
    if (count < 2)
    {
        SKIP();
    }

    m_Params.m_Width = 1200;
    m_Params.m_Height = 800;
    OpenAndShow();
    for (int i = 1; i < count; ++i)
    {
        int x, y, width, height;
        dmWindowTest::WorkArea(i, &x, &y, &width, &height);
        // Establish the target DPI before arranging a smaller window near its
        // bottom-right corner. At this position, the larger startup window can
        // overlap a different monitor until the saved size is applied.
        dmPlatform::SetWindowSize(m_Window, 401, 301);
        dmPlatform::SetWindowPosition(m_Window, x + width / 2 - 200, y + height / 2 - 150);
        dmPlatform::PollEvents(m_Window);
        dmPlatform::SetWindowSize(m_Window, 401, 301);
        SetPosition(x + width - 465, y + height - 365);
        int expected_x, expected_y, expected_width, expected_height;
        dmWindowTest::Position(m_Window, &expected_x, &expected_y);
        dmWindowTest::ClientSize(m_Window, &expected_width, &expected_height);
        for (int restart = 0; restart < 5; ++restart)
        {
            Recreate();
            int actual_x, actual_y, actual_width, actual_height;
            dmWindowTest::Position(m_Window, &actual_x, &actual_y);
            dmWindowTest::ClientSize(m_Window, &actual_width, &actual_height);
            ASSERT_EQ(expected_width, actual_width);
            ASSERT_EQ(expected_height, actual_height);
            ASSERT_EQ(expected_x, actual_x);
            ASSERT_EQ(expected_y, actual_y);
        }
    }
}

// Verifies reset requests are observed while running, and survive closing before the next poll.
TEST_F(WindowPlacementTest, ResetSize)
{
    OpenAndShow();
    dmPlatform::SetWindowSize(m_Window, 501, 351);
    dmPlatform::UpdateWindowPlacement(m_Window, true);
    FILE* file = fopen("test-window-placement/reset-size", "wb");
    ASSERT_NE((FILE*)0, file);
    fputs("deadbeef-1234", file);
    fclose(file);
    dmPlatform::UpdateWindowPlacement(m_Window, true);
    dmWindowState::State state;
    ASSERT_EQ(dmWindowState::LOAD_OK, dmWindowState::Load(m_Path, &state));
    ASSERT_EQ(400, state.m_Rect.m_Width);
    ASSERT_EQ(300, state.m_Rect.m_Height);
    ASSERT_STREQ("deadbeef-1234", state.m_ResetToken);
    Recreate();
    int width, height;
    dmWindowTest::ClientSize(m_Window, &width, &height);
    ASSERT_EQ(400, width);
    ASSERT_EQ(300, height);
}

// Verifies the editor's HTTP resize and a later preset cannot be undone by a delayed reset check.
TEST_F(WindowPlacementTest, ResetDoesNotOverrideExplicitSize)
{
    OpenAndShow();
    dmPlatform::SetWindowSize(m_Window, 501, 351);
    dmPlatform::UpdateWindowPlacement(m_Window, true);
    FILE* file = fopen("test-window-placement/reset-size", "wb");
    ASSERT_NE((FILE*)0, file);
    fputs("deadbeef-1234", file);
    fclose(file);
    // This is the same setter used by the HTTP resize message. Its requested
    // dimensions can differ from the project dimensions cached at launch.
    dmPlatform::SetWindowSize(m_Window, 620, 420);
    dmPlatform::UpdateWindowPlacement(m_Window, true);
    dmWindowState::State state;
    ASSERT_EQ(dmWindowState::LOAD_OK, dmWindowState::Load(m_Path, &state));
    ASSERT_EQ(620, state.m_Rect.m_Width);
    ASSERT_EQ(420, state.m_Rect.m_Height);
    ASSERT_STREQ("deadbeef-1234", state.m_ResetToken);

    // A second reset followed immediately by a new preset must also respect
    // the newer explicit size, even before any window event polling occurs.
    file = fopen("test-window-placement/reset-size", "wb");
    ASSERT_NE((FILE*)0, file);
    fputs("deadbeef-5678", file);
    fclose(file);
    dmPlatform::SetWindowSize(m_Window, 720, 480);
    Recreate();
    int width, height;
    dmWindowTest::ClientSize(m_Window, &width, &height);
    ASSERT_EQ(720, width);
    ASSERT_EQ(480, height);
}

// Verifies hidden renderer initialization cannot acknowledge a reset before the player is shown.
TEST_F(WindowPlacementTest, ResetDuringStartup)
{
#if defined(_WIN32)
    // Exercise the player's normal Win32 startup, including its request for focus on first show.
    m_Params.m_Hidden = false;
    m_Params.m_FocusOnShow = true;
#endif
    OpenAndShow();
    dmPlatform::SetWindowSize(m_Window, 501, 351);
    DestroyWindow();
    m_Window = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, m_Window);
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, m_Params));
    FILE* file = fopen("test-window-placement/reset-size", "wb");
    ASSERT_NE((FILE*)0, file);
    fputs("deadbeef-1234", file);
    fclose(file);
    dmPlatform::SetWindowSize(m_Window, 501, 351);
    dmPlatform::ShowWindow(m_Window);
    dmPlatform::UpdateWindowPlacement(m_Window, true);
    int width, height;
    dmWindowTest::ClientSize(m_Window, &width, &height);
    ASSERT_EQ(400, width);
    ASSERT_EQ(300, height);
}

// Verifies a reset made with no player running preserves position but discards the saved size.
TEST_F(WindowPlacementTest, ResetWhileStopped)
{
    OpenAndShow();
    int default_width, default_height;
    dmWindowTest::ClientSize(m_Window, &default_width, &default_height);
    dmPlatform::SetWindowSize(m_Window, 501, 351);
    SetPosition(170, 190);
    DestroyWindow();
    dmWindowState::State saved;
    ASSERT_EQ(dmWindowState::LOAD_OK, dmWindowState::Load(m_Path, &saved));
    FILE* file = fopen("test-window-placement/reset-size", "wb");
    ASSERT_NE((FILE*)0, file);
    fputs("deadbeef-5678", file);
    fclose(file);
    Recreate();
    int width, height;
    dmWindowTest::ClientSize(m_Window, &width, &height);
    ASSERT_EQ(default_width, width);
    ASSERT_EQ(default_height, height);
    if (saved.m_HasPosition)
    {
        int x, y;
        dmWindowTest::Position(m_Window, &x, &y);
        ASSERT_EQ(saved.m_Rect.m_X, x);
        ASSERT_EQ(saved.m_Rect.m_Y, y);
    }
}

// Verifies startup failure before ShowWindow cannot replace valid geometry with a transient size.
TEST_F(WindowPlacementTest, HiddenWindowPreservesState)
{
    OpenAndShow();
    dmPlatform::SetWindowSize(m_Window, 501, 351);
    DestroyWindow();
    m_Window = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, m_Window);
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, m_Params));
    dmPlatform::SetWindowSize(m_Window, 123, 234);
    dmPlatform::PollEvents(m_Window);
    DestroyWindow();
    dmWindowState::State saved;
    ASSERT_EQ(dmWindowState::LOAD_OK, dmWindowState::Load(m_Path, &saved));
    ASSERT_EQ(501, saved.m_Rect.m_Width);
    ASSERT_EQ(351, saved.m_Rect.m_Height);
}

#if defined(__MACH__)
static void ReopenHiddenOnFocus(void* data, uint32_t focused)
{
    if (focused)
    {
        HWindow window = (HWindow)data;
        dmPlatform::CloseWindow(window);
        WindowCreateParams replacement;
        WindowCreateParamsInitialize(&replacement);
        replacement.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
        replacement.m_Hidden = true;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, replacement));
    }
}

// A Cocoa focus callback can reopen the handle during ShowWindow. The hidden replacement must not save its placement.
TEST_F(WindowPlacementTest, CocoaShowPreservesHiddenReplacement)
{
    // Activate the app first so showing the next window delivers focus synchronously.
    m_Params.m_FocusOnShow = true;
    m_Params.m_Hidden = false;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, m_Params));
    uint64_t deadline = dmTime::GetMonotonicTime() + 3000000;
    while (!dmPlatform::GetWindowStateParam(m_Window, WINDOW_STATE_ACTIVE) && dmTime::GetMonotonicTime() < deadline)
    {
        dmPlatform::PollEvents(m_Window);
    }

    ASSERT_TRUE(dmPlatform::GetWindowStateParam(m_Window, WINDOW_STATE_ACTIVE));
    dmPlatform::CloseWindow(m_Window);

    m_Params.m_Hidden = true;
    m_Params.m_FocusCallback = ReopenHiddenOnFocus;
    m_Params.m_FocusCallbackUserData = m_Window;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, m_Params));
    dmPlatform::PollEvents(m_Window);
    uint64_t original = dmPlatform::GetNativeWindowId(m_Window).m_Id;
    dmPlatform::ShowWindow(m_Window);
    ASSERT_NE(original, dmPlatform::GetNativeWindowId(m_Window).m_Id);
    DestroyWindow();
    EXPECT_FALSE(dmSys::Exists(m_Path));
}
#endif

// Verifies a fullscreen reboot never overwrites the last normal rectangle.
TEST_F(WindowPlacementTest, FullscreenPreservesNormalGeometry)
{
    OpenAndShow();
    dmPlatform::SetWindowSize(m_Window, 501, 351);
    m_Params.m_Fullscreen = true;
    Recreate();
    dmPlatform::UpdateWindowPlacement(m_Window, true);
    dmWindowState::State state;
    ASSERT_EQ(dmWindowState::LOAD_OK, dmWindowState::Load(m_Path, &state));
    ASSERT_EQ(501, state.m_Rect.m_Width);
    ASSERT_EQ(351, state.m_Rect.m_Height);
    m_Params.m_Fullscreen = false;
    Recreate();
    int width, height;
    dmWindowTest::ClientSize(m_Window, &width, &height);
    ASSERT_EQ(501, width);
    ASSERT_EQ(351, height);
}

// Verifies standalone launches do not initialize or write the new persistence mechanism.
TEST_F(WindowPlacementTest, Disabled)
{
    SetStateEnvironment(0);
    OpenAndShow();
    dmPlatform::SetWindowSize(m_Window, 501, 351);
    dmPlatform::UpdateWindowPlacement(m_Window, true);
    ASSERT_FALSE(dmSys::Exists(m_Path));
}

// Verifies newer state versions survive both startup and shutdown without being overwritten.
TEST_F(WindowPlacementTest, FutureVersion)
{
    FILE* file = fopen(m_Path, "wb");
    ASSERT_NE((FILE*)0, file);
    fputs("[window]\nversion = 2\n", file);
    fclose(file);
    OpenAndShow();
    Recreate();
    dmWindowState::State state;
    ASSERT_EQ(dmWindowState::LOAD_UNSUPPORTED_VERSION, dmWindowState::Load(m_Path, &state));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
