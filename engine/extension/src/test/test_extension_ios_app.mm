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
#include <glfw/glfw.h>
#include <glfw/glfw_native.h>
#import "AppDelegate.h"
#import "SceneDelegate.h"
#import "ViewController.h"

extern AppDelegate* g_ApplicationDelegate;
static unsigned int g_UpdateCount;
static BOOL g_TestsStarted;
static BOOL g_DismissOnUpdate;
static UISceneConnectionOptions* g_ConnectionOptions;
static unsigned int g_ExtensionConfigurations;
static unsigned int g_WillLaunchCount;
static unsigned int g_DidLaunchCount;
static unsigned int g_LegacyLaunchCount;
static BOOL g_WillLaunchHasOptions;
static BOOL g_DidLaunchHasOptions;

@interface SceneConnectionObserver : NSObject <UISceneDelegate, UIApplicationDelegate>
@end

@implementation SceneConnectionObserver
- (BOOL)application:(UIApplication*)application willFinishLaunchingWithOptions:(NSDictionary*)options
{
    ++g_WillLaunchCount;
    g_WillLaunchHasOptions = options != nil;
    return NO;
}
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)options
{
    ++g_DidLaunchCount;
    g_DidLaunchHasOptions = options != nil;
    return NO;
}
- (void)applicationDidFinishLaunching:(UIApplication*)application
{
    ++g_LegacyLaunchCount;
}
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options
{
    if (!g_ConnectionOptions)
        g_ConnectionOptions = [options retain];
}
- (UISceneConfiguration*)application:(UIApplication*)application configurationForConnectingSceneSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options
{
    ++g_ExtensionConfigurations;
    return nil;
}
@end

static BOOL WaitUntil(BOOL (^condition)(void))
{
    NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:2.0];
    while (!condition() && [deadline timeIntervalSinceNow] > 0)
        [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    return condition();
}

class iOSSceneApplication : public jc_test_base_class
{
public:
    UIWindowScene* m_Scene;
    DefoldSceneDelegate* m_Delegate;
    ViewController* m_Controller;

    void SetUp()
    {
        UIWindow* window = (UIWindow*)glfwGetiOSUIWindow();
        m_Scene = [window.windowScene retain];
        m_Delegate = [(DefoldSceneDelegate*)m_Scene.delegate retain];
        m_Controller = g_ApplicationDelegate.viewController;
    }

    void TearDown()
    {
        g_DismissOnUpdate = NO;
        if (!glfwGetiOSUIWindow())
        {
            [m_Delegate scene:m_Scene willConnectToSession:m_Scene.session options:g_ConnectionOptions];
            [m_Delegate sceneDidBecomeActive:m_Scene];
        }
        [m_Controller dismissViewControllerAnimated:NO completion:nil];
        [m_Delegate release];
        [m_Scene release];
    }
};

// Real UIKit startup must deliver each modern launch callback once, with nil
// options, and never synthesize the legacy applicationDidFinishLaunching: callback.
TEST_F(iOSSceneApplication, ApplicationLaunchWithoutLegacyBridge)
{
    ASSERT_TRUE([[UIApplication sharedApplication].delegate isKindOfClass:[AppDelegateProxy class]]);
    ASSERT_EQ(1U, g_WillLaunchCount);
    ASSERT_EQ(1U, g_DidLaunchCount);
    ASSERT_FALSE(g_WillLaunchHasOptions);
    ASSERT_FALSE(g_DidLaunchHasOptions);
    ASSERT_EQ(0U, g_LegacyLaunchCount);
}

// A manifest without UISceneConfigurations must start the engine through the
// initial application delegate, before the launch callback installs its proxy.
TEST_F(iOSSceneApplication, ProgrammaticSceneStartup)
{
    NSDictionary* manifest = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"UIApplicationSceneManifest"];
    ASSERT_NE((void*)nil, (void*)manifest);
    ASSERT_EQ((void*)nil, (void*)manifest[@"UISceneConfigurations"]);
    ASSERT_TRUE([AppDelegate instancesRespondToSelector:@selector(application:configurationForConnectingSceneSession:options:)]);
    ASSERT_TRUE([m_Scene.delegate isKindOfClass:[DefoldSceneDelegate class]]);
    ASSERT_EQ((void*)m_Controller, (void*)((UIWindow*)glfwGetiOSUIWindow()).rootViewController);
    ASSERT_GT(g_UpdateCount, 0U);
    UIApplication* application = [UIApplication sharedApplication];
    UISceneConfiguration* configuration = [application.delegate application:application configurationForConnectingSceneSession:m_Scene.session options:g_ConnectionOptions];
    ASSERT_EQ((void*)[DefoldSceneDelegate class], (void*)configuration.delegateClass);
    ASSERT_EQ(0U, g_ExtensionConfigurations);
}

// Fullscreen UIKit presentation detaches the game view. Engine callbacks must
// keep running so an extension can dismiss its native UI from an update.
TEST_F(iOSSceneApplication, UpdatesDuringFullscreenPresentation)
{
    ViewController* controller = m_Controller;
    UIViewController* modal = [[[UIViewController alloc] init] autorelease];
    modal.modalPresentationStyle = UIModalPresentationFullScreen;
    [controller presentViewController:modal animated:NO completion:nil];
    ASSERT_TRUE(WaitUntil(^BOOL { return controller.baseView.window == nil; }));
    ASSERT_EQ(UISceneActivationStateForegroundActive, m_Scene.activationState);
    ASSERT_NE((void*)nil, (void*)controller.baseView->displayLink);
    unsigned int before = g_UpdateCount;
    ASSERT_TRUE(WaitUntil(^BOOL { return g_UpdateCount > before + 2; }));
    g_DismissOnUpdate = YES;
    ASSERT_TRUE(WaitUntil(^BOOL { return controller.presentedViewController == nil && controller.baseView.window != nil; }));
}

// Replacing the render view, as reboot does, must also start updates when a
// fullscreen native controller still keeps the new view outside its window.
TEST_F(iOSSceneApplication, ReplaceViewDuringFullscreenPresentation)
{
    ViewController* controller = m_Controller;
    UIViewController* modal = [[[UIViewController alloc] init] autorelease];
    modal.modalPresentationStyle = UIModalPresentationFullScreen;
    [controller presentViewController:modal animated:NO completion:nil];
    ASSERT_TRUE(WaitUntil(^BOOL { return controller.baseView.window == nil; }));
    BaseView* previous = [controller.baseView retain];
    [controller createView:TRUE];
    EXPECT_NE((void*)previous, (void*)controller.baseView);
    EXPECT_EQ((void*)nil, (void*)previous->displayLink);
    [previous release];
    ASSERT_EQ((void*)nil, (void*)controller.baseView.window);
    ASSERT_NE((void*)nil, (void*)controller.baseView->displayLink);
    unsigned int before = g_UpdateCount;
    ASSERT_TRUE(WaitUntil(^BOOL { return g_UpdateCount > before + 2; }));
    g_DismissOnUpdate = YES;
    ASSERT_TRUE(WaitUntil(^BOOL { return controller.presentedViewController == nil && controller.baseView.window != nil; }));
}

// A real scene disconnection must invalidate the frame source and stop engine
// updates; reconnecting must resume updates using the retained view.
TEST_F(iOSSceneApplication, DisconnectStopsUpdates)
{
    BaseView* view = m_Controller.baseView;
    [m_Delegate sceneDidDisconnect:m_Scene];
    ASSERT_EQ((void*)nil, glfwGetiOSUIWindow());
    ASSERT_EQ((void*)nil, (void*)view->displayLink);
    unsigned int before = g_UpdateCount;
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
    ASSERT_EQ(before, g_UpdateCount);
    [m_Delegate scene:m_Scene willConnectToSession:m_Scene.session options:g_ConnectionOptions];
    [m_Delegate sceneDidBecomeActive:m_Scene];
    ASSERT_EQ((void*)view, (void*)m_Controller.baseView);
    ASSERT_NE((void*)nil, (void*)view->displayLink);
    ASSERT_TRUE(WaitUntil(^BOOL { return g_UpdateCount > before + 2; }));
}

@interface SceneApplicationTestRunner : NSObject
- (void)runTests:(NSTimer*)timer;
@end

@implementation SceneApplicationTestRunner
- (void)runTests:(NSTimer*)timer
{
    g_TestsStarted = YES;
    exit(jc_test_run_all());
}
@end

static void* CreateTestEngine(int argc, char** argv)
{
    // Run tests from a run-loop timer, allowing UIKit and display-link callbacks
    // to progress while each test waits for its asynchronous transitions.
    SceneApplicationTestRunner* runner = [[[SceneApplicationTestRunner alloc] init] autorelease];
    [NSTimer scheduledTimerWithTimeInterval:0.1 target:runner selector:@selector(runTests:) userInfo:nil repeats:NO];
    return &g_UpdateCount;
}

static int UpdateTestEngine(void* context)
{
    ++g_UpdateCount;
    if (g_DismissOnUpdate)
    {
        g_DismissOnUpdate = NO;
        [g_ApplicationDelegate.viewController dismissViewControllerAnimated:NO completion:nil];
    }
    return 0;
}

static void DestroyTestEngine(void* context)
{
}

static void GetTestEngineResult(void* context, int* action, int* exit_code, int* argc, char*** argv)
{
    *action = GLFW_APP_RUN_EXIT;
    *exit_code = 0;
    *argc = 0;
    *argv = 0;
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    SceneConnectionObserver* observer = [[[SceneConnectionObserver alloc] init] autorelease];
    glfwRegisterUISceneDelegate(observer);
    glfwRegisterUIApplicationDelegate(observer);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        if (!g_TestsStarted)
        {
            fprintf(stderr, "Programmatic scene startup did not start the engine\n");
            exit(1);
        }
    });
    glfwAppBootstrap(argc, argv, 0, 0, 0, CreateTestEngine, DestroyTestEngine, UpdateTestEngine, GetTestEngineResult);
    [pool drain];
    return 0;
}
