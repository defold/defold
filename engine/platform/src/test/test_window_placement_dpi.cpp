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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlib/hash.h>
#include <dlib/math.h>
#include <dlib/sys.h>

#include "../platform_window_glfw3_private.h"
#include "../window_state.h"

struct GLFWmonitor
{
    dmWindowState::Monitor m_State;
    GLFWvidmode            m_Mode;
    const char*            m_Name;
};

struct GLFWwindow
{
    dmWindowState::Rect m_Rect;
    GLFWmonitor*        m_Monitor;
};

static GLFWmonitor  g_Monitors[2];
static GLFWmonitor* g_MonitorHandles[] = { &g_Monitors[0], &g_Monitors[1] };

// Model the relevant GLFW/Win32 contract: the monitor follows the largest
// overlap, and either a move or a resize can synchronously trigger automatic
// DPI scaling. No display server or event timing is involved in these tests.
static void SetGeometry(GLFWwindow* window, dmWindowState::Rect rect)
{
    int64_t largest_overlap = 0;
    GLFWmonitor* monitor = window->m_Monitor;
    for (uint32_t i = 0; i < 2; ++i)
    {
        const dmWindowState::Rect& work = g_Monitors[i].m_State.m_WorkArea;
        int64_t width = dmMath::Max(0, dmMath::Min(rect.m_X + rect.m_Width, work.m_X + work.m_Width) - dmMath::Max(rect.m_X, work.m_X));
        int64_t height = dmMath::Max(0, dmMath::Min(rect.m_Y + rect.m_Height, work.m_Y + work.m_Height) - dmMath::Max(rect.m_Y, work.m_Y));
        if (width * height > largest_overlap)
        {
            largest_overlap = width * height;
            monitor = &g_Monitors[i];
        }
    }
    rect.m_Width = (int32_t) roundf(rect.m_Width * monitor->m_State.m_ScaleX / window->m_Monitor->m_State.m_ScaleX);
    rect.m_Height = (int32_t) roundf(rect.m_Height * monitor->m_State.m_ScaleY / window->m_Monitor->m_State.m_ScaleY);
    window->m_Rect = rect;
    window->m_Monitor = monitor;
}

int glfwGetPlatform()
{
    return GLFW_PLATFORM_WIN32;
}

GLFWmonitor* glfwGetWindowMonitor(GLFWwindow*)
{
    return 0; // A normal, windowed player.
}

int glfwGetWindowAttrib(GLFWwindow*, int)
{
    return GLFW_FALSE;
}

GLFWmonitor** glfwGetMonitors(int* count)
{
    *count = 2;
    return g_MonitorHandles;
}

GLFWmonitor* glfwGetPrimaryMonitor()
{
    return &g_Monitors[0];
}

const char* glfwGetMonitorName(GLFWmonitor* monitor)
{
    return monitor->m_Name;
}

const GLFWvidmode* glfwGetVideoMode(GLFWmonitor* monitor)
{
    return &monitor->m_Mode;
}

void glfwGetMonitorPos(GLFWmonitor* monitor, int* x, int* y)
{
    *x = monitor->m_State.m_WorkArea.m_X;
    *y = monitor->m_State.m_WorkArea.m_Y;
}

void glfwGetMonitorWorkarea(GLFWmonitor* monitor, int* x, int* y, int* width, int* height)
{
    const dmWindowState::Rect& work = monitor->m_State.m_WorkArea;
    *x = work.m_X;
    *y = work.m_Y;
    *width = work.m_Width;
    *height = work.m_Height;
}

void glfwGetMonitorContentScale(GLFWmonitor* monitor, float* x, float* y)
{
    *x = monitor->m_State.m_ScaleX;
    *y = monitor->m_State.m_ScaleY;
}

void glfwGetWindowContentScale(GLFWwindow* window, float* x, float* y)
{
    glfwGetMonitorContentScale(window->m_Monitor, x, y);
}

void glfwGetWindowPos(GLFWwindow* window, int* x, int* y)
{
    *x = window->m_Rect.m_X;
    *y = window->m_Rect.m_Y;
}

void glfwSetWindowPos(GLFWwindow* window, int x, int y)
{
    dmWindowState::Rect rect = window->m_Rect;
    rect.m_X = x;
    rect.m_Y = y;
    SetGeometry(window, rect);
}

void glfwGetWindowSize(GLFWwindow* window, int* width, int* height)
{
    *width = window->m_Rect.m_Width;
    *height = window->m_Rect.m_Height;
}

void glfwSetWindowSize(GLFWwindow* window, int width, int height)
{
    dmWindowState::Rect rect = window->m_Rect;
    rect.m_Width = width;
    rect.m_Height = height;
    SetGeometry(window, rect);
}

void glfwGetWindowFrameSize(GLFWwindow* window, int* left, int* top, int* right, int* bottom)
{
    float scale = window->m_Monitor->m_State.m_ScaleX;
    *left = *right = *bottom = (int) (8 * scale);
    *top = (int) (30 * scale);
}

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

class WindowPlacementDpiTest : public jc_test_base_class
{
    protected:
    char*                m_PreviousEnvironment;
    const char*          m_Path;
    WindowCreateParams   m_Params;
    dmWindowState::State m_Saved;
    GLFWwindow           m_NativeWindow;
    dmWindow             m_Window;

    void SetUp()
    {
        const char* value = dmSys::GetEnv("DM_WINDOW_STATE");
        m_PreviousEnvironment = value ? strdup(value) : 0;
        SetStateEnvironment("test-window-placement-dpi/player-0");
        dmSys::Mkdir("test-window-placement-dpi", 0755);
        m_Path = "test-window-placement-dpi/player-0-win32.ini";
        remove(m_Path);
        remove("test-window-placement-dpi/reset-size");

        memset(g_Monitors, 0, sizeof(g_Monitors));
        for (uint32_t i = 0; i < 2; ++i)
        {
            g_Monitors[i].m_Name = i == 0 ? "Primary" : "Secondary";
            g_Monitors[i].m_Mode.width = 3840;
            g_Monitors[i].m_Mode.height = 2160;
            dmWindowState::Monitor& monitor = g_Monitors[i].m_State;
            monitor.m_NameHash = dmHashString64(g_Monitors[i].m_Name);
            monitor.m_WorkArea.m_X = i == 0 ? 0 : -3840;
            monitor.m_WorkArea.m_Width = 3840;
            monitor.m_WorkArea.m_Height = 2120;
            monitor.m_ScaleX = monitor.m_ScaleY = i == 0 ? 2.0f : 1.5f;
        }
        memset(&m_Params, 0, sizeof(m_Params));
        m_Params.m_Width = 960;
        m_Params.m_Height = 640;
        memset(&m_Saved, 0, sizeof(m_Saved));
        dmWindowState::Rect rect = { -700, 100, 600, 400 };
        m_Saved.m_Rect = rect;
        m_Saved.m_Monitor = g_Monitors[1].m_State;
        m_Saved.m_ConfigWidth = m_Params.m_Width;
        m_Saved.m_ConfigHeight = m_Params.m_Height;
        m_Saved.m_HasPosition = true;
        memset(&m_Window, 0, sizeof(m_Window));
        m_Window.m_Window = &m_NativeWindow;
    }

    void Restore()
    {
        dmPlatform::DeleteWindowPlacement(&m_Window);
        // Each launch starts at the project size on the primary monitor.
        dmWindowState::Rect rect = { 100, 100, (int32_t) (m_Params.m_Width * g_Monitors[0].m_State.m_ScaleX),
                                     (int32_t) (m_Params.m_Height * g_Monitors[0].m_State.m_ScaleY) };
        m_NativeWindow.m_Rect = rect;
        m_NativeWindow.m_Monitor = &g_Monitors[0];
        dmPlatform::InitWindowPlacement(&m_Window, m_Params);
        ASSERT_TRUE(dmPlatform::RestoreWindowPlacement(&m_Window));
        dmPlatform::WindowPlacementShown(&m_Window);
        dmPlatform::UpdateWindowPlacement(&m_Window, true);
    }

    void CheckRestarts(int width, int height)
    {
        ASSERT_TRUE(dmWindowState::Save(m_Path, m_Saved));
        for (uint32_t i = 0; i < 5; ++i)
        {
            Restore();
            ASSERT_EQ(width, m_NativeWindow.m_Rect.m_Width);
            ASSERT_EQ(height, m_NativeWindow.m_Rect.m_Height);
            dmWindowState::State saved;
            ASSERT_EQ(dmWindowState::LOAD_OK, dmWindowState::Load(m_Path, &saved));
            ASSERT_EQ(width, saved.m_Rect.m_Width);
            ASSERT_EQ(height, saved.m_Rect.m_Height);
        }
    }

    void TearDown()
    {
        dmPlatform::DeleteWindowPlacement(&m_Window);
        remove(m_Path);
        remove("test-window-placement-dpi/reset-size");
        dmSys::Rmdir("test-window-placement-dpi");
        SetStateEnvironment(m_PreviousEnvironment);
        free(m_PreviousEnvironment);
    }
};

// Verifies a resize-triggered DPI transition cannot shrink saved dimensions on every launch.
TEST_F(WindowPlacementDpiTest, SmallerWindowOnSecondaryMonitor)
{
    CheckRestarts(600, 400);
}

// Verifies clamping onto a monitor after its work area shrinks cannot rescale the restored size.
TEST_F(WindowPlacementDpiTest, DpiChangeDuringClamping)
{
    m_Saved.m_Monitor.m_WorkArea.m_Width = 7680;
    m_Saved.m_Rect.m_X = 1160;
    CheckRestarts(600, 400);
}

// Verifies a real change in the saved monitor's DPI is applied once across repeated launches.
TEST_F(WindowPlacementDpiTest, ChangedMonitorDpi)
{
    m_Saved.m_Monitor.m_ScaleX = m_Saved.m_Monitor.m_ScaleY = 1.25f;
    CheckRestarts(720, 480);
}

// Verifies larger saved windows retain their dimensions when moving to the secondary monitor.
TEST_F(WindowPlacementDpiTest, LargerWindowOnSecondaryMonitor)
{
    m_Saved.m_Rect.m_X = -2600;
    m_Saved.m_Rect.m_Width = 2400;
    m_Saved.m_Rect.m_Height = 1600;
    CheckRestarts(2400, 1600);
}

// Verifies restoring between monitors with equal DPI does not alter the saved size.
TEST_F(WindowPlacementDpiTest, EqualMonitorDpi)
{
    g_Monitors[1].m_State.m_ScaleX = g_Monitors[1].m_State.m_ScaleY = 2.0f;
    m_Saved.m_Monitor = g_Monitors[1].m_State;
    CheckRestarts(600, 400);
}

// Verifies restoring onto a higher-DPI secondary monitor cannot grow the saved size on each launch.
TEST_F(WindowPlacementDpiTest, HigherDpiSecondaryMonitor)
{
    g_Monitors[0].m_State.m_ScaleX = g_Monitors[0].m_State.m_ScaleY = 1.5f;
    g_Monitors[1].m_State.m_ScaleX = g_Monitors[1].m_State.m_ScaleY = 2.0f;
    m_Saved.m_Monitor = g_Monitors[1].m_State;
    CheckRestarts(600, 400);
}

// Verifies the primary-monitor restore path retains the saved size.
TEST_F(WindowPlacementDpiTest, PrimaryMonitor)
{
    m_Saved.m_Rect.m_X = 100;
    m_Saved.m_Monitor = g_Monitors[0].m_State;
    CheckRestarts(600, 400);
}

// Verifies a reset still restores the project dimensions at the target DPI, ignoring the saved size.
TEST_F(WindowPlacementDpiTest, ResetSize)
{
    FILE* file = fopen("test-window-placement-dpi/reset-size", "wb");
    ASSERT_NE((FILE*) 0, file);
    fputs("deadbeef-1234", file);
    fclose(file);
    CheckRestarts(1440, 960);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
