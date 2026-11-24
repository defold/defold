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

#include <stdint.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include "../window.hpp"
#include "../platform_app.h"

#if defined(ANDROID)
#include "../platform_window_android.h"
#endif

#if defined(DM_PLATFORM_IOS)
#include "../platform_window_ios.h"
#endif

#define APP_TITLE "PlatformTest"
#define WIDTH 8u
#define HEIGHT 4u

TEST(dmPlatform, WindowCreateParamsInitialize)
{
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    ASSERT_TRUE(params.m_FocusOnShow);
}

class dmPlatformTest : public jc_test_base_class
{
protected:
    struct ResizeData
    {
        uint32_t m_Width;
        uint32_t m_Height;
    };

    HWindow m_Window;
    ResizeData          m_ResizeData;

    static void OnWindowResize(void* user_data, uint32_t width, uint32_t height)
    {
        ResizeData* data = (ResizeData*)user_data;
        data->m_Width = width;
        data->m_Height = height;
    }

    void SetUp() override
    {
        m_Window = WindowNew();

        m_ResizeData.m_Width = 0;
        m_ResizeData.m_Height = 0;

        WindowCreateParams params;
        WindowCreateParamsInitialize(&params);
        params.m_ResizeCallback = OnWindowResize;
        params.m_ResizeCallbackUserData = &m_ResizeData;
        params.m_Title = APP_TITLE;
        params.m_Width = WIDTH;
        params.m_Height = HEIGHT;
        params.m_Fullscreen = false;
        params.m_PrintDeviceInfo = false;
        params.m_ContextAlphabits = 8;

        WindowOpen(m_Window, &params);
    }

    void TearDown() override
    {
        WindowDelete(m_Window);
    }
};

TEST_F(dmPlatformTest, DoubleOpenWindow)
{
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_Title = APP_TITLE;
    params.m_Width = WIDTH;
    params.m_Height = HEIGHT;
    params.m_Fullscreen = false;
    params.m_PrintDeviceInfo = false;
    params.m_ContextAlphabits = 8;

    ASSERT_EQ(WINDOW_RESULT_WINDOW_ALREADY_OPENED, WindowOpen(m_Window, &params));
}

TEST_F(dmPlatformTest, CloseWindow)
{
    WindowClose(m_Window);
    WindowClose(m_Window);
}

TEST_F(dmPlatformTest, CloseOpenWindow)
{
    WindowClose(m_Window);
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_Title = APP_TITLE;
    params.m_Width = WIDTH;
    params.m_Height = HEIGHT;
    params.m_Fullscreen = false;
    params.m_PrintDeviceInfo = true;
    params.m_ContextAlphabits = 8;
    dmLogSetLevel(LOG_SEVERITY_INFO);

    ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(m_Window, &params));
    dmLogSetLevel(LOG_SEVERITY_WARNING);
}

TEST_F(dmPlatformTest, TestWindowState)
{
    ASSERT_TRUE(WindowGetStateParam(m_Window, WINDOW_STATE_OPENED) ? true : false);
    WindowClose(m_Window);
    ASSERT_FALSE(WindowGetStateParam(m_Window, WINDOW_STATE_OPENED));
}

TEST_F(dmPlatformTest, TestWindowSize)
{
    uint32_t width = WIDTH * 2;
    uint32_t height = HEIGHT * 2;
    WindowSetSize(m_Window, width, height);
    ASSERT_EQ(width, WindowGetWidth(m_Window));
    ASSERT_EQ(height, WindowGetHeight(m_Window));
    ASSERT_EQ(width, m_ResizeData.m_Width);
    ASSERT_EQ(height, m_ResizeData.m_Height);
}

// Verifies unsupported backends clear both cached and native areas rather than inferring safe-area strips.
TEST_F(dmPlatformTest, ReservedAreasUnsupported)
{
    WindowReservedArea sentinel = {};
    const WindowReservedArea* areas = &sentinel;
    ASSERT_EQ(0u, WindowGetReservedAreas(m_Window, &areas));
    ASSERT_EQ((const WindowReservedArea*)0, areas);

    dmArray<WindowReservedArea> native_areas;
    native_areas.SetCapacity(1);
    native_areas.Push(sentinel);
    dmPlatform::GetReservedAreas(m_Window, native_areas);
    ASSERT_TRUE(native_areas.Empty());
}

// Verifies native rectangles are clipped, rounded outward, and converted to bottom-left window pixels.
TEST(dmPlatform, ReservedAreaCoordinates)
{
    struct Case
    {
        double m_Left, m_Top, m_Right, m_Bottom;
        WindowReservedArea m_Expected;
    } cases[] = {
        {10, 20, 30, 40, {20, 180, 40, 60}},
        {10.25, 20.25, 30.25, 40.25, {20, 179, 41, 61}},
        {-10, -20, 30, 40, {0, 180, 60, 120}},
        {90, 80, 120, 120, {180, 0, 20, 60}},
        {-10, -20, 120, 120, {0, 0, 200, 300}},
    };
    for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        WindowReservedArea area;
        ASSERT_TRUE(dmPlatform::ConvertReservedArea(cases[i].m_Left, cases[i].m_Top, cases[i].m_Right, cases[i].m_Bottom,
                                                    100, 100, 200, 300, &area));
        ASSERT_EQ(cases[i].m_Expected.m_X, area.m_X);
        ASSERT_EQ(cases[i].m_Expected.m_Y, area.m_Y);
        ASSERT_EQ(cases[i].m_Expected.m_Width, area.m_Width);
        ASSERT_EQ(cases[i].m_Expected.m_Height, area.m_Height);
    }
}

// Verifies floating-point roundoff cannot extend a clipped rectangle beyond the window.
TEST(dmPlatform, ReservedAreaRoundedWindowBounds)
{
    WindowReservedArea area;
    ASSERT_TRUE(dmPlatform::ConvertReservedArea(0, 0, 123.4, 123.4, 123.4, 123.4, 31, 31, &area));
    ASSERT_EQ(0, area.m_X);
    ASSERT_EQ(0, area.m_Y);
    ASSERT_EQ(31u, area.m_Width);
    ASSERT_EQ(31u, area.m_Height);
}

// Verifies empty, offscreen, and unavailable geometry cannot create edge occlusions or divide by zero.
TEST(dmPlatform, ReservedAreaEmptyBounds)
{
    WindowReservedArea area;
    ASSERT_FALSE(dmPlatform::ConvertReservedArea(-10, 0, 0, 10, 100, 100, 200, 300, &area));
    ASSERT_FALSE(dmPlatform::ConvertReservedArea(100, 0, 110, 10, 100, 100, 200, 300, &area));
    ASSERT_FALSE(dmPlatform::ConvertReservedArea(0, 100, 10, 110, 100, 100, 200, 300, &area));
    ASSERT_FALSE(dmPlatform::ConvertReservedArea(10, 20, 10, 30, 100, 100, 200, 300, &area));
    ASSERT_FALSE(dmPlatform::ConvertReservedArea(0, 0, 10, 10, 0, 100, 200, 300, &area));
    ASSERT_FALSE(dmPlatform::ConvertReservedArea(0, 0, 10, 10, 100, 0, 200, 300, &area));
    ASSERT_FALSE(dmPlatform::ConvertReservedArea(0, 0, 10, 10, 100, 100, 0, 300, &area));
    ASSERT_FALSE(dmPlatform::ConvertReservedArea(0, 0, 10, 10, 100, 100, 200, 0, &area));
}

#if defined(ANDROID)

namespace
{
    template <typename T>
    static void ReferenceAndroidSymbol(T symbol)
    {
        volatile T referenced_symbol = symbol;
        (void) referenced_symbol;
    }
}

// Verifies the null backend provides the Android interface without linking the real window backend.
TEST(AndroidPlatform, LinkPlatformWindowAndroidSymbols)
{
    ReferenceAndroidSymbol(&dmPlatform::AndroidVerifySurface);
    ReferenceAndroidSymbol(&dmPlatform::AndroidBeginFrame);
    ReferenceAndroidSymbol(&dmPlatform::GetAndroidEGLContext);
    ReferenceAndroidSymbol(&dmPlatform::GetAndroidEGLSurface);
    ReferenceAndroidSymbol(&dmPlatform::GetAndroidJavaVM);
    ReferenceAndroidSymbol(&dmPlatform::GetAndroidActivity);
    ReferenceAndroidSymbol(&dmPlatform::GetAndroidApp);
    ReferenceAndroidSymbol(&dmPlatform::AcquireAndroidWindow);
    ReferenceAndroidSymbol(&dmPlatform::WaitForAndroidWindow);
    ReferenceAndroidSymbol(&dmPlatform::IsAndroidWindowCurrent);
    ReferenceAndroidSymbol(&dmPlatform::ReleaseAndroidWindow);
    ReferenceAndroidSymbol(&dmPlatform::GetSafeAreaAndroid);
    ReferenceAndroidSymbol(&dmPlatform::SetAndroidInputMethod);
    ReferenceAndroidSymbol(&dmPlatform::SetAndroidFullscreenParameters);
    ReferenceAndroidSymbol(&dmPlatform::SetAndroidJoystickLayoutLegacy);
    ReferenceAndroidSymbol(&dmPlatform::InitializeAndroidJoystickAxes);
    SKIP();
}
#endif

#if defined(DM_PLATFORM_IOS)
// Verifies home indicator configuration links and runs with platform_null, guarding against headless iOS link failures.
TEST(iOSPlatform, NullPlatformHomeIndicatorMode)
{
    dmPlatform::SetiOSHomeIndicatorMode("deferred");
    dmPlatform::SetiOSHomeIndicatorMode("system");
    dmPlatform::SetiOSHomeIndicatorMode("auto_hide");
}

// Verifies platform_null provides the iOS delegate registration functions used by
// dmExtension::RegisteriOSUI*Delegate, guarding against headless iOS link failures.
TEST(iOSPlatform, NullPlatformDelegateRegistration)
{
    dmPlatform::RegisteriOSApplicationDelegate(0);
    dmPlatform::UnregisteriOSApplicationDelegate(0);
    dmPlatform::RegisteriOSSceneDelegate(0);
    dmPlatform::UnregisteriOSSceneDelegate(0);
}
#endif

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
