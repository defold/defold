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
#include "../platform_window_glfw3_private.h"
#include "../window_state.h"

static void SetStateEnvironment(const char* value)
{
#if defined(_WIN32)
    _putenv_s("DM_WINDOW_STATE", value ? value : "");
#else
    if (value)
        setenv("DM_WINDOW_STATE", value, 1);
    else
        unsetenv("DM_WINDOW_STATE");
#endif
}

class WindowPlacementTest : public jc_test_base_class
{
protected:
    HWindow m_Window;
    char* m_PreviousEnvironment;
    char m_Path[DMPATH_MAX_PATH];
    WindowCreateParams m_Params;

    void SetUp()
    {
        const char* value = dmSys::GetEnv("DM_WINDOW_STATE");
        m_PreviousEnvironment = value ? strdup(value) : 0;
        SetStateEnvironment("test-window-placement/player-0");
        dmSys::Mkdir("test-window-placement", 0755);
        remove("test-window-placement/reset-size");
        m_Window = dmPlatform::NewWindow();
        ASSERT_NE((HWindow)0, m_Window);
        const char* backend = "cocoa";
        switch (glfwGetPlatform())
        {
            case GLFW_PLATFORM_WIN32: backend = "win32"; break;
            case GLFW_PLATFORM_X11: backend = "x11"; break;
            case GLFW_PLATFORM_WAYLAND: backend = "wayland"; break;
        }
        dmSnPrintf(m_Path, sizeof(m_Path), "test-window-placement/player-0-%s.ini", backend);
        remove(m_Path);
        WindowCreateParamsInitialize(&m_Params);
        m_Params.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN; // GLFW_NO_API, no renderer required.
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
        if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
            return;
        dmPlatform::SetWindowPosition(m_Window, x, y);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        int actual_x, actual_y;
        for (;;)
        {
            glfwPollEvents();
            glfwGetWindowPos(m_Window->m_Window, &actual_x, &actual_y);
            if ((actual_x == x && actual_y == y) || dmTime::GetMonotonicTime() >= deadline)
                break;
            glfwWaitEventsTimeout(0.01);
        }
        ASSERT_EQ(x, actual_x);
        ASSERT_EQ(y, actual_y);
    }

    void DestroyWindow()
    {
        if (m_Window)
        {
            if (m_Window->m_Window)
                dmPlatform::CloseWindow(m_Window);
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

// Verifies close captures the actual geometry and X11 restoration preserves the requested position.
TEST_F(WindowPlacementTest, RecreateRestoresActualGeometry)
{
    OpenAndShow();
    dmPlatform::SetWindowSize(m_Window, 501, 351);
    SetPosition(170, 190);
    dmWindowState::Rect expected;
    glfwGetWindowSize(m_Window->m_Window, &expected.m_Width, &expected.m_Height);
    if (glfwGetPlatform() != GLFW_PLATFORM_WAYLAND)
        glfwGetWindowPos(m_Window->m_Window, &expected.m_X, &expected.m_Y);
    Recreate();
    int width, height;
    glfwGetWindowSize(m_Window->m_Window, &width, &height);
    ASSERT_EQ(expected.m_Width, width);
    ASSERT_EQ(expected.m_Height, height);
    if (glfwGetPlatform() != GLFW_PLATFORM_WAYLAND)
    {
        int x, y;
        glfwGetWindowPos(m_Window->m_Window, &x, &y);
        ASSERT_EQ(expected.m_X, x);
        ASSERT_EQ(expected.m_Y, y);
    }
    // Repeat to detect cumulative scaling during creation.
    Recreate();
    glfwGetWindowSize(m_Window->m_Window, &width, &height);
    ASSERT_EQ(expected.m_Width, width);
    ASSERT_EQ(expected.m_Height, height);
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
    glfwGetWindowSize(m_Window->m_Window, &width, &height);
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
    glfwGetWindowSize(m_Window->m_Window, &width, &height);
    ASSERT_EQ(720, width);
    ASSERT_EQ(480, height);
}

// Verifies hidden renderer initialization cannot acknowledge a reset before the player is shown.
TEST_F(WindowPlacementTest, ResetDuringStartup)
{
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
    glfwGetWindowSize(m_Window->m_Window, &width, &height);
    ASSERT_EQ(400, width);
    ASSERT_EQ(300, height);
}

// Verifies a reset made with no player running preserves position but discards the saved size.
TEST_F(WindowPlacementTest, ResetWhileStopped)
{
    OpenAndShow();
    int default_width, default_height;
    glfwGetWindowSize(m_Window->m_Window, &default_width, &default_height);
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
    glfwGetWindowSize(m_Window->m_Window, &width, &height);
    ASSERT_EQ(default_width, width);
    ASSERT_EQ(default_height, height);
    if (saved.m_HasPosition)
    {
        int x, y;
        glfwGetWindowPos(m_Window->m_Window, &x, &y);
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
    glfwGetWindowSize(m_Window->m_Window, &width, &height);
    ASSERT_EQ(501, width);
    ASSERT_EQ(351, height);
}

// Verifies standalone launches do not initialize or write the new persistence mechanism.
TEST_F(WindowPlacementTest, Disabled)
{
    SetStateEnvironment(0);
    OpenAndShow();
    ASSERT_EQ((WindowPlacement*)0, m_Window->m_Placement);
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
