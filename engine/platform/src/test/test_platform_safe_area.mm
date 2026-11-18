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

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

typedef void (^TransitionBlock)(id<UIViewControllerTransitionCoordinatorContext>);

@interface TestTransitionCoordinator : NSObject <UIViewControllerTransitionCoordinator>
@property (nonatomic, copy) TransitionBlock completion;
@property (nonatomic, readonly, getter=isAnimated) BOOL animated;
@property (nonatomic, readonly) UIModalPresentationStyle presentationStyle;
@property (nonatomic, readonly) BOOL initiallyInteractive;
@property (nonatomic, readonly) BOOL isInterruptible;
@property (nonatomic, readonly, getter=isInteractive) BOOL interactive;
@property (nonatomic, readonly, getter=isCancelled) BOOL cancelled;
@property (nonatomic, readonly) NSTimeInterval transitionDuration;
@property (nonatomic, readonly) CGFloat percentComplete;
@property (nonatomic, readonly) CGFloat completionVelocity;
@property (nonatomic, readonly) UIViewAnimationCurve completionCurve;
@property (nonatomic, readonly) UIView* containerView;
@property (nonatomic, readonly) CGAffineTransform targetTransform;
- (void)completeTransition;
@end

@implementation TestTransitionCoordinator
- (BOOL)animateAlongsideTransition:(TransitionBlock)animation completion:(TransitionBlock)completion
{
    self.completion = completion;
    return YES;
}

- (BOOL)animateAlongsideTransitionInView:(UIView*)view animation:(TransitionBlock)animation completion:(TransitionBlock)completion
{
    return [self animateAlongsideTransition:animation completion:completion];
}

- (UIViewController*)viewControllerForKey:(UITransitionContextViewControllerKey)key
{
    return nil;
}

- (UIView*)viewForKey:(UITransitionContextViewKey)key
{
    return nil;
}

- (void)notifyWhenInteractionEndsUsingBlock:(TransitionBlock)handler
{
}

- (void)notifyWhenInteractionChangesUsingBlock:(TransitionBlock)handler
{
}

- (void)completeTransition
{
    if (self.completion)
    {
        self.completion(self);
        self.completion = nil;
    }
}

- (void)dealloc
{
    self.completion = nil;
    [super dealloc];
}
@end

struct ResizeData
{
    uint32_t m_Count;
    uint32_t m_Width;
    uint32_t m_Height;
};

static void OnWindowResize(void* user_data, uint32_t width, uint32_t height)
{
    ResizeData* data = (ResizeData*)user_data;
    ++data->m_Count;
    data->m_Width = width;
    data->m_Height = height;
}

// Verifies iOS inset notifications reach the window callback once during polling without a size change; guards against stale GUI insets after a 180-degree rotation.
TEST(dmPlatform, SafeAreaNotificationWithoutResize)
{
    HWindow window = WindowNew();
    ResizeData data = {};
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_Title = "SafeAreaTest";
    params.m_Width = 1920;
    params.m_Height = 1080;
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
    params.m_ResizeCallback = OnWindowResize;
    params.m_ResizeCallbackUserData = &data;
    ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(window, &params));

    WindowPollEvents(window);
    data.m_Count = 0;
    uint32_t width = WindowGetWidth(window);
    uint32_t height = WindowGetHeight(window);
    UIView* view = (UIView*)dmPlatform::GetiOSUIView();
    ASSERT_TRUE(view != nil);
    ASSERT_GT(width, 0u);
    ASSERT_GT(height, 0u);
    [view safeAreaInsetsDidChange];
    [view safeAreaInsetsDidChange];
    ASSERT_EQ(0u, data.m_Count);

    WindowPollEvents(window);
    ASSERT_EQ(1u, data.m_Count);
    ASSERT_EQ(width, data.m_Width);
    ASSERT_EQ(height, data.m_Height);
    WindowPollEvents(window);
    ASSERT_EQ(1u, data.m_Count);
    WindowDelete(window);
}

// Verifies completed transitions notify during polling even when size and insets
// stay unchanged, guarding against missing events on 180-degree landscape rotations.
TEST(dmPlatform, TransitionNotificationWithoutResizeOrInsetChange)
{
    HWindow window = WindowNew();
    ResizeData data = {};
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_Title = "TransitionTest";
    params.m_Width = 1920;
    params.m_Height = 1080;
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
    params.m_ResizeCallback = OnWindowResize;
    params.m_ResizeCallbackUserData = &data;
    ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(window, &params));

    UIWindow* native_window = (UIWindow*)dmPlatform::GetiOSUIWindow();
    ViewController* controller = (ViewController*)native_window.rootViewController;
    UIView* view = (UIView*)dmPlatform::GetiOSUIView();
    UIEdgeInsets insets = view.safeAreaInsets;
    uint32_t width = WindowGetWidth(window);
    uint32_t height = WindowGetHeight(window);
    WindowPollEvents(window);
    data.m_Count = 0;

    TestTransitionCoordinator* coordinator = [[TestTransitionCoordinator alloc] init];
    [controller viewWillTransitionToSize:controller.view.bounds.size withTransitionCoordinator:coordinator];
    WindowPollEvents(window);
    EXPECT_EQ(0u, data.m_Count);

    [coordinator completeTransition];
    EXPECT_EQ(0u, data.m_Count);
    WindowPollEvents(window);
    EXPECT_EQ(1u, data.m_Count);
    EXPECT_EQ(width, data.m_Width);
    EXPECT_EQ(height, data.m_Height);
    EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(insets, view.safeAreaInsets));
    WindowPollEvents(window);
    EXPECT_EQ(1u, data.m_Count);

    // An inset notification in the same poll must coalesce with the transition.
    [controller viewWillTransitionToSize:controller.view.bounds.size withTransitionCoordinator:coordinator];
    [coordinator completeTransition];
    [view safeAreaInsetsDidChange];
    EXPECT_EQ(1u, data.m_Count);
    WindowPollEvents(window);
    EXPECT_EQ(2u, data.m_Count);
    WindowPollEvents(window);
    EXPECT_EQ(2u, data.m_Count);

    [coordinator release];
    WindowDelete(window);
}

// Verifies the native iOS backend returns no inferred rectangles on systems predating the reserved-region API.
TEST(dmPlatform, ReservedAreasOnOlderIOS)
{
    if (@available(iOS 27.1, *)) SKIP();
    HWindow window = WindowNew();
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
    ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(window, &params));
    WindowReservedArea sentinel;
    const WindowReservedArea* areas = &sentinel;
    EXPECT_EQ(0u, WindowGetReservedAreas(window, &areas));
    EXPECT_EQ((const WindowReservedArea*)0, areas);
    WindowPollEvents(window);
    EXPECT_EQ(0u, WindowGetReservedAreas(window, &areas));
    WindowDelete(window);
}

@interface TestReservedRegion : NSObject
@property (nonatomic) CGRect frame;
@property (nonatomic, getter=isActive) BOOL active;
@end

@implementation TestReservedRegion
@end

static NSMutableArray* g_TestReservedRegions;
static uint32_t g_ReservedAreaQueries;

static NSArray* QueryTestReservedRegions(id view, SEL selector, id kind, NSUInteger options)
{
    if (@available(iOS 27.1, *))
    {
        EXPECT_TRUE([kind isEqual:[UIViewReservedRegionKind occlusionRegionKind]]);
        EXPECT_EQ((NSUInteger)UIViewReservedRegionQueryOptionsNone, options);
    }
    ++g_ReservedAreaQueries;
    return g_TestReservedRegions;
}

class ReservedAreasTest : public jc_test_base_class
{
protected:
    HWindow m_Window;
    Method m_QueryMethod;
    IMP m_OriginalQuery;
    uint32_t m_Notifications;
    uint32_t m_CallbackAreaCount;
    int32_t m_CallbackX;

    static void OnResize(void* user_data, uint32_t width, uint32_t height)
    {
        ReservedAreasTest* test = (ReservedAreasTest*)user_data;
        ++test->m_Notifications;
        const WindowReservedArea* areas;
        test->m_CallbackAreaCount = WindowGetReservedAreas(test->m_Window, &areas);
        test->m_CallbackX = test->m_CallbackAreaCount ? areas[0].m_X : -1;
    }

    void SetUp()
    {
        m_Window = 0;
        m_QueryMethod = 0;
        if (@available(iOS 27.1, *))
        {
            m_Window = WindowNew();
            WindowCreateParams params;
            WindowCreateParamsInitialize(&params);
            params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
            ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(m_Window, &params));
            WindowPollEvents(m_Window);
            g_TestReservedRegions = [[NSMutableArray alloc] init];
            m_QueryMethod = class_getInstanceMethod([UIView class], @selector(reservedRegionsOfKind:options:));
            m_OriginalQuery = method_setImplementation(m_QueryMethod, (IMP)QueryTestReservedRegions);
            g_ReservedAreaQueries = 0;
            m_Notifications = 0;
        }
    }

    void TearDown()
    {
        if (m_QueryMethod)
        {
            method_setImplementation(m_QueryMethod, m_OriginalQuery);
            [g_TestReservedRegions release];
            g_TestReservedRegions = nil;
        }
        if (m_Window)
            WindowDelete(m_Window);
    }

    void AddRegion(CGRect frame, BOOL active)
    {
        TestReservedRegion* region = [[TestReservedRegion alloc] init];
        region.frame = frame;
        region.active = active;
        [g_TestReservedRegions addObject:region];
        [region release];
    }

    void Listen()
    {
        // Reopen to install the callback through the public window interface.
        WindowClose(m_Window);
        WindowCreateParams params;
        WindowCreateParamsInitialize(&params);
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
        params.m_ResizeCallback = OnResize;
        params.m_ResizeCallbackUserData = this;
        ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(m_Window, &params));
        WindowPollEvents(m_Window);
        const WindowReservedArea* areas;
        WindowGetReservedAreas(m_Window, &areas);
        m_Notifications = 0;
    }
};

// Verifies UIKit filtering, clipping, and dynamic capacity without treating insets or inactive regions as occlusions.
TEST_F(ReservedAreasTest, ActiveRectangles)
{
    if (!m_Window) SKIP();
    UIView* view = (UIView*)dmPlatform::GetiOSUIView();
    const CGRect bounds = view.bounds;
    AddRegion(CGRectMake(-10, -10, bounds.size.width + 20, bounds.size.height + 20), YES);
    AddRegion(CGRectMake(10, 10, 10, 10), NO);
    AddRegion(CGRectMake(-20, -20, 10, 10), YES);
    AddRegion(CGRectMake(10, 10, 0, 10), YES);
    for (int i = 0; i < 20; ++i)
        AddRegion(CGRectMake(10 + i * 2, 10, 1, 1), YES);

    const WindowReservedArea* areas;
    ASSERT_EQ(21u, WindowGetReservedAreas(m_Window, &areas));
    ASSERT_EQ(0, areas[0].m_X);
    ASSERT_EQ(0, areas[0].m_Y);
    ASSERT_EQ(WindowGetWidth(m_Window), areas[0].m_Width);
    ASSERT_EQ(WindowGetHeight(m_Window), areas[0].m_Height);
    ASSERT_EQ(1u, g_ReservedAreaQueries);
}

// Verifies changed, reordered, and removed occlusions notify only when needed, with the new snapshot visible inside the callback.
TEST_F(ReservedAreasTest, ChangesWithoutResize)
{
    if (!m_Window) SKIP();
    Listen();
    AddRegion(CGRectMake(10, 10, 10, 10), YES);
    AddRegion(CGRectMake(30, 10, 10, 10), YES);
    WindowPollEvents(m_Window);
    m_Notifications = 0;

    [g_TestReservedRegions exchangeObjectAtIndex:0 withObjectAtIndex:1];
    WindowPollEvents(m_Window);
    ASSERT_EQ(0u, m_Notifications);

    [g_TestReservedRegions removeAllObjects];
    AddRegion(CGRectMake(0, 10, 10, 10), YES);
    WindowPollEvents(m_Window);
    ASSERT_EQ(1u, m_Notifications);
    ASSERT_EQ(1u, m_CallbackAreaCount);
    ASSERT_EQ(0, m_CallbackX);
    WindowPollEvents(m_Window);
    ASSERT_EQ(1u, m_Notifications);

    // An inset event already refreshes the snapshot before invoking the listener.
    [g_TestReservedRegions removeAllObjects];
    [(UIView*)dmPlatform::GetiOSUIView() safeAreaInsetsDidChange];
    WindowPollEvents(m_Window);
    ASSERT_EQ(2u, m_Notifications);
    ASSERT_EQ(0u, m_CallbackAreaCount);
    WindowPollEvents(m_Window);
    ASSERT_EQ(2u, m_Notifications);
}

// Verifies applications that never query reserved areas incur no polling, and closing a window clears observation state.
TEST_F(ReservedAreasTest, ObservationLifetime)
{
    if (!m_Window) SKIP();
    WindowPollEvents(m_Window);
    ASSERT_EQ(0u, g_ReservedAreaQueries);
    const WindowReservedArea* areas;
    ASSERT_EQ(0u, WindowGetReservedAreas(m_Window, &areas));
    ASSERT_EQ(1u, g_ReservedAreaQueries);
    WindowPollEvents(m_Window);
    ASSERT_EQ(2u, g_ReservedAreaQueries);
    WindowClose(m_Window);
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
    ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(m_Window, &params));
    WindowPollEvents(m_Window);
    ASSERT_EQ(2u, g_ReservedAreaQueries);
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
