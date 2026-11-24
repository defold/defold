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

// Wrap only the native surface access so both native platform backends can run without an Activity.
extern "C" ANativeWindow* __wrap_dmNativeAcquireAndroidWindow(void)
{
    ++g_WindowQueries;
    return g_WindowAvailable ? (ANativeWindow*)&g_NativeWindow : 0;
}

extern "C" void __wrap_dmNativeReleaseAndroidWindow(ANativeWindow* window)
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
    ASSERT_EQ(width, dmNativeWin.width);
    ASSERT_EQ(height, dmNativeWin.height);
    ASSERT_EQ(g_WindowWidth, width);
    ASSERT_EQ(g_WindowHeight, height);
    ++g_ResizeCount;
    if (g_QueueDuringCallback)
    {
        g_QueueDuringCallback = 0;
        dmNativeAndroidPlatformSetPendingResizeBecauseOfInsets();
    }
}

class AndroidInsetsTest : public jc_test_base_class
{
protected:
    void SetUp()
    {
        memset(&dmNativeWin, 0, sizeof(dmNativeWin));
        dmNativeWin.clientAPI = NATIVE_NO_API;
        g_WindowAvailable = 1;
        g_WindowWidth = 2034;
        g_WindowHeight = 1398;
        // Drain any pending event left by the previous test before installing the callback.
        dmNativeAndroidPlatformAfterFlushEvents();
        dmNativeWin.width = g_WindowWidth;
        dmNativeWin.height = g_WindowHeight;
        dmNativeWin.windowSizeCallback = OnResize;
        g_WindowQueries = 0;
        g_ResizeCount = 0;
        g_QueueDuringCallback = 0;
    }
};

// Verifies a same-size inset change refreshes Vulkan in both backends, and idle polls never query the native window.
TEST_F(AndroidInsetsTest, InsetsChangeWithoutResize)
{
    dmNativeAndroidPlatformSetPendingResizeBecauseOfInsets();
    dmNativeAndroidPlatformSetPendingResizeBecauseOfInsets();
    ASSERT_EQ(0, g_ResizeCount);
    dmNativeAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
    ASSERT_EQ(1, g_WindowQueries);
    dmNativeAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
    ASSERT_EQ(1, g_WindowQueries);
}

// Verifies an inset notification queued during the callback survives until the next poll.
TEST_F(AndroidInsetsTest, InsetsQueuedDuringCallback)
{
    g_QueueDuringCallback = 1;
    dmNativeAndroidPlatformSetPendingResizeBecauseOfInsets();
    dmNativeAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
    dmNativeAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(2, g_ResizeCount);
    dmNativeAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(2, g_ResizeCount);
}

// Verifies both inset and resize events are retried when the native window is temporarily unavailable.
TEST_F(AndroidInsetsTest, RetryWithoutNativeWindow)
{
    g_WindowAvailable = 0;
    dmNativeAndroidPlatformSetPendingResizeBecauseOfInsets();
    dmNativeAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(0, g_ResizeCount);
    g_WindowAvailable = 1;
    dmNativeAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);

    g_WindowAvailable = 0;
    dmNativeAndroidPlatformOnResize();
    dmNativeAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
    g_WindowAvailable = 1;
    dmNativeAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(2, g_ResizeCount);
}

// Verifies new native window dimensions are visible to the resize callback.
TEST_F(AndroidInsetsTest, ResizeUpdatesDimensionsBeforeCallback)
{
    g_WindowWidth = 1398;
    g_WindowHeight = 2034;
    dmNativeAndroidPlatformOnResize();
    dmNativeAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
    ASSERT_EQ(g_WindowWidth, dmNativeWin.width);
    ASSERT_EQ(g_WindowHeight, dmNativeWin.height);
}

// Verifies recreating a Vulkan native window refreshes its insets even when its dimensions are unchanged.
TEST_F(AndroidInsetsTest, RecreatedNativeWindowRefresh)
{
    dmNativeAndroidPlatformOnInitWindow();
    dmNativeAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
    dmNativeAndroidPlatformAfterFlushEvents();
    ASSERT_EQ(1, g_ResizeCount);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
