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

#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

extern "C"
{
#include <android_window_backend.h>
}

static int g_NativeWindow;
static int g_WindowAvailable;
static int g_WindowWidth;
static int g_WindowHeight;
static int g_WindowQueries;
static int g_ResizeCount;
static int g_QueueDuringCallback;

// Wrap only the native surface access so both real GLFW backends can run without an Activity.
extern "C" ANativeWindow* __wrap_glfwAcquireAndroidWindow(void)
{
    ++g_WindowQueries;
    return g_WindowAvailable ? (ANativeWindow*)&g_NativeWindow : 0;
}

extern "C" void __wrap_glfwReleaseAndroidWindow(ANativeWindow* window)
{
    (void)window;
}

extern "C" int32_t __wrap_ANativeWindow_getWidth(ANativeWindow* window)
{
    (void)window;
    return g_WindowWidth;
}

extern "C" int32_t __wrap_ANativeWindow_getHeight(ANativeWindow* window)
{
    (void)window;
    return g_WindowHeight;
}

static void OnResize(int width, int height)
{
    ASSERT_EQ(width, _glfwWin.width);
    ASSERT_EQ(height, _glfwWin.height);
    ASSERT_EQ(g_WindowWidth, width);
    ASSERT_EQ(g_WindowHeight, height);
    ++g_ResizeCount;
    if (g_QueueDuringCallback)
    {
        g_QueueDuringCallback = 0;
        _glfwAndroidPlatformSetPendingResizeBecauseOfInsets();
    }
}

class AndroidInsetsTest : public jc_test_base_class
{
protected:
    void SetUp()
    {
        memset(&_glfwWin, 0, sizeof(_glfwWin));
        _glfwWin.clientAPI = GLFW_NO_API;
        g_WindowAvailable = 1;
        g_WindowWidth = 2034;
        g_WindowHeight = 1398;
        // Drain any pending event left by the previous test before installing the callback.
        _glfwAndroidPlatformAfterFlushEvents();
        _glfwWin.width = g_WindowWidth;
        _glfwWin.height = g_WindowHeight;
        _glfwWin.windowSizeCallback = OnResize;
        g_WindowQueries = 0;
        g_ResizeCount = 0;
        g_QueueDuringCallback = 0;
    }
};

// Verifies a same-size inset change refreshes Vulkan in both backends, and idle polls never query the native window.
TEST_F(AndroidInsetsTest, InsetsChangeWithoutResize)
{
    _glfwAndroidPlatformSetPendingResizeBecauseOfInsets();
    _glfwAndroidPlatformSetPendingResizeBecauseOfInsets();
    ASSERT_EQ(0, g_ResizeCount);
    _glfwAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
    ASSERT_EQ(1, g_WindowQueries);
    _glfwAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
    ASSERT_EQ(1, g_WindowQueries);
}

// Verifies an inset notification queued during the callback survives until the next poll.
TEST_F(AndroidInsetsTest, InsetsQueuedDuringCallback)
{
    g_QueueDuringCallback = 1;
    _glfwAndroidPlatformSetPendingResizeBecauseOfInsets();
    _glfwAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
    _glfwAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(2, g_ResizeCount);
    _glfwAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(2, g_ResizeCount);
}

// Verifies both inset and resize events are retried when the native window is temporarily unavailable.
TEST_F(AndroidInsetsTest, RetryWithoutNativeWindow)
{
    g_WindowAvailable = 0;
    _glfwAndroidPlatformSetPendingResizeBecauseOfInsets();
    _glfwAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(0, g_ResizeCount);
    g_WindowAvailable = 1;
    _glfwAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);

    g_WindowAvailable = 0;
    _glfwAndroidPlatformOnResize();
    _glfwAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
    g_WindowAvailable = 1;
    _glfwAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(2, g_ResizeCount);
}

// Verifies new native window dimensions are visible to the resize callback.
TEST_F(AndroidInsetsTest, ResizeUpdatesDimensionsBeforeCallback)
{
    g_WindowWidth = 1398;
    g_WindowHeight = 2034;
    _glfwAndroidPlatformOnResize();
    _glfwAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
    ASSERT_EQ(g_WindowWidth, _glfwWin.width);
    ASSERT_EQ(g_WindowHeight, _glfwWin.height);
}

// Verifies recreating a Vulkan native window refreshes its insets even when its dimensions are unchanged.
TEST_F(AndroidInsetsTest, RecreatedNativeWindowRefresh)
{
    _glfwAndroidPlatformOnInitWindow();
    _glfwAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
    _glfwAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
