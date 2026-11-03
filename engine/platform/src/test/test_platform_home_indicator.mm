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

@interface HomeIndicatorTestViewController : ViewController
@property (nonatomic) NSUInteger gestureUpdates;
@property (nonatomic) NSUInteger indicatorUpdates;
@end

@implementation HomeIndicatorTestViewController
- (void)setNeedsUpdateOfScreenEdgesDeferringSystemGestures
{
    self.gestureUpdates += 1;
}

- (void)setNeedsUpdateOfHomeIndicatorAutoHidden
{
    self.indicatorUpdates += 1;
}
@end

class HomeIndicatorTest : public jc_test_base_class
{
protected:
    HWindow m_Window;
    ViewController* m_Controller;

    virtual void SetUp()
    {
        m_Window = WindowNew();
        WindowCreateParams params;
        WindowCreateParamsInitialize(&params);
        params.m_Title = "HomeIndicatorTest";
        params.m_Width = 1920;
        params.m_Height = 1080;
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
        ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(m_Window, &params));
        UIWindow* window = (UIWindow*)dmPlatform::GetiOSUIWindow();
        m_Controller = (ViewController*)window.rootViewController;
        ASSERT_TRUE(m_Controller != nil);
    }

    virtual void TearDown()
    {
        dmPlatform::SetiOSHomeIndicatorMode("deferred");
        WindowDelete(m_Window);
    }
};

// Verifies the controller protects all edges before project configuration is available.
TEST(dmPlatform, HomeIndicatorDefaults)
{
    ViewController* controller = [[ViewController alloc] init];
    EXPECT_EQ(UIRectEdgeAll, controller.preferredScreenEdgesDeferringSystemGestures);
    EXPECT_FALSE(controller.prefersHomeIndicatorAutoHidden);
    [controller release];
}

// Verifies changing modes invalidates both UIKit preferences, which may already have been queried at launch.
TEST(dmPlatform, HomeIndicatorPreferenceUpdates)
{
    HomeIndicatorTestViewController* controller = [[HomeIndicatorTestViewController alloc] init];
    controller.homeIndicatorMode = HOME_INDICATOR_MODE_AUTO_HIDE;
    EXPECT_GT(controller.gestureUpdates, 0u);
    EXPECT_GT(controller.indicatorUpdates, 0u);
    [controller release];
}

// Verifies project modes replace both preferences on the live controller without retaining the previous mode on restart.
TEST_F(HomeIndicatorTest, ModeChanges)
{
    dmPlatform::SetiOSHomeIndicatorMode("auto_hide");
    EXPECT_EQ(UIRectEdgeNone, m_Controller.preferredScreenEdgesDeferringSystemGestures);
    EXPECT_TRUE(m_Controller.prefersHomeIndicatorAutoHidden);

    dmPlatform::SetiOSHomeIndicatorMode("system");
    EXPECT_EQ(UIRectEdgeNone, m_Controller.preferredScreenEdgesDeferringSystemGestures);
    EXPECT_FALSE(m_Controller.prefersHomeIndicatorAutoHidden);

    dmPlatform::SetiOSHomeIndicatorMode("auto_hide");
    dmPlatform::SetiOSHomeIndicatorMode("deferred");
    EXPECT_EQ(UIRectEdgeAll, m_Controller.preferredScreenEdgesDeferringSystemGestures);
    EXPECT_FALSE(m_Controller.prefersHomeIndicatorAutoHidden);
}

// Verifies an invalid runtime override restores the legacy default instead of leaving a previous project's preference active.
TEST_F(HomeIndicatorTest, InvalidModeRestoresDefault)
{
    dmPlatform::SetiOSHomeIndicatorMode("auto_hide");
    dmPlatform::SetiOSHomeIndicatorMode("invalid");
    EXPECT_EQ(UIRectEdgeAll, m_Controller.preferredScreenEdgesDeferringSystemGestures);
    EXPECT_FALSE(m_Controller.prefersHomeIndicatorAutoHidden);
}

// Verifies render-view recreation and window reattachment preserve the controller-owned preference.
TEST_F(HomeIndicatorTest, ViewRecreationPreservesMode)
{
    dmPlatform::SetiOSHomeIndicatorMode("auto_hide");
    [m_Controller createView:YES];

    UIWindow* window = (UIWindow*)dmPlatform::GetiOSUIWindow();
    window.rootViewController = nil;
    window.rootViewController = m_Controller;
    EXPECT_EQ(UIRectEdgeNone, m_Controller.preferredScreenEdgesDeferringSystemGestures);
    EXPECT_TRUE(m_Controller.prefersHomeIndicatorAutoHidden);
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
