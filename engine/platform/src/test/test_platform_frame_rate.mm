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
#include <testmain/testmain.h>
#include <stdlib.h>

#include "../platform_window_ios.h"
#include "../platform_app.h"
#import "../native/ios/app/ViewController.h"

class FrameRateTest : public jc_test_base_class
{
protected:
    HWindow  m_Window;
    uint32_t m_Limit;

    virtual void SetUp()
    {
        m_Window = WindowNew();
        WindowCreateParams params;
        WindowCreateParamsInitialize(&params);
        params.m_Title = "FrameRateTest";
        params.m_Width = 1920;
        params.m_Height = 1080;
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
        ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(m_Window, &params));
        m_Limit = WindowGetStateParam(m_Window, WINDOW_STATE_REFRESH_RATE);
        ASSERT_GT(m_Limit, 0u);
    }

    virtual void TearDown()
    {
        dmPlatform::SetiOSFrameRate(false, 0);
        WindowDelete(m_Window);
    }

    CAFrameRateRange GetRange()
    {
        BaseView* view = (BaseView*)dmPlatform::GetiOSUIView();
        return view->displayLink.preferredFrameRateRange;
    }
};

static void ExpectRange(CAFrameRateRange range, float rate)
{
    EXPECT_EQ(rate, range.minimum);
    EXPECT_EQ(rate, range.maximum);
    EXPECT_EQ(rate, range.preferred);
}

// Verifies the reported refresh rate never exceeds what the screen supports.
TEST_F(FrameRateTest, RefreshRateWithinScreenMaximum)
{
    UIWindow* window = (UIWindow*)dmPlatform::GetiOSUIWindow();
    EXPECT_LE(m_Limit, (uint32_t)window.screen.maximumFramesPerSecond);
}

// Verifies the display link keeps the system default when high refresh rates are disabled, preserving existing behavior.
TEST_F(FrameRateTest, DisabledKeepsDefault)
{
    dmPlatform::SetiOSFrameRate(false, 30);
    EXPECT_TRUE(CAFrameRateRangeIsEqualToRange(CAFrameRateRangeDefault, GetRange()));
}

// Verifies a variable update frequency requests the highest rate the app may use.
TEST_F(FrameRateTest, VariableFrequencyRequestsLimit)
{
    dmPlatform::SetiOSFrameRate(true, 0);
    ExpectRange(GetRange(), (float)m_Limit);
}

// Verifies an explicit update frequency is requested from the display, and clamped to the limit.
TEST_F(FrameRateTest, ExplicitFrequency)
{
    dmPlatform::SetiOSFrameRate(true, 30);
    ExpectRange(GetRange(), 30.0f);

    dmPlatform::SetiOSFrameRate(true, m_Limit + 1);
    ExpectRange(GetRange(), (float)m_Limit);
}

// Verifies disabling high refresh rates after a reboot restores the system default.
TEST_F(FrameRateTest, DisableRestoresDefault)
{
    dmPlatform::SetiOSFrameRate(true, 30);
    dmPlatform::SetiOSFrameRate(false, 0);
    EXPECT_TRUE(CAFrameRateRangeIsEqualToRange(CAFrameRateRangeDefault, GetRange()));
}

// Verifies render-view recreation creates a display link that keeps the requested rate.
TEST_F(FrameRateTest, ViewRecreationPreservesRate)
{
    dmPlatform::SetiOSFrameRate(true, 30);

    UIWindow* window = (UIWindow*)dmPlatform::GetiOSUIWindow();
    ViewController* controller = (ViewController*)window.rootViewController;
    [controller createView:YES];

    ExpectRange(controller.baseView->displayLink.preferredFrameRateRange, 30.0f);
}

static void* RunTests(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    exit(jc_test_run_all());
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    dmPlatform::AppBootstrap(argc, argv, 0, 0, 0, RunTests, 0, 0, 0);
    return 0;
}
