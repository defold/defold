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
