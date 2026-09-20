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
#import <objc/runtime.h>
#import "AppDelegate.h"
#import "EAGLView.h"
#import "MetalView.h"
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
static unsigned int g_ReleasedObjects;

@interface ReleaseObserver : NSObject
@end

@implementation ReleaseObserver
- (void)dealloc
{
    ++g_ReleasedObjects;
    [super dealloc];
}
@end

static void SetObservedMarkedTextStyle(BaseView* view)
{
    ReleaseObserver* observer = [[ReleaseObserver alloc] init];
    NSDictionary* style = [[NSDictionary alloc] initWithObjectsAndKeys:observer, @"test", nil];
    view.markedTextStyle = style;
    [style release];
    [observer release];
}

@interface AppearanceObserver : UIViewController
{
@public
    unsigned int m_AppearanceCount;
    unsigned int m_DisappearanceCount;
}
@end

@implementation AppearanceObserver
- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    ++m_AppearanceCount;
}
- (void)viewDidDisappear:(BOOL)animated
{
    [super viewDidDisappear:animated];
    ++m_DisappearanceCount;
}
@end

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

// An extension may use tag 999 after startup. Frame updates must not mistake
// its view for the engine's launch placeholder and remove it from the window.
TEST_F(iOSSceneApplication, ExtensionViewWithLaunchTagSurvivesUpdates)
{
    UIWindow* window = (UIWindow*)glfwGetiOSUIWindow();
    UIView* container = [[[UIView alloc] initWithFrame:window.bounds] autorelease];
    UIView* extensionView = [[[UIView alloc] initWithFrame:container.bounds] autorelease];
    extensionView.tag = 999;
    [container addSubview:extensionView];
    [window addSubview:container];
    unsigned int before = g_UpdateCount;
    EXPECT_TRUE(WaitUntil(^BOOL { return g_UpdateCount > before + 2; }));
    EXPECT_EQ((void*)container, (void*)extensionView.superview);
    [container removeFromSuperview];
}

// Launch cleanup must remove the retained placeholder even without its old tag,
// while preserving an extension view with tag 999 during the same frame.
TEST_F(iOSSceneApplication, LaunchCleanupRemovesOnlyPlaceholder)
{
    UIWindow* window = (UIWindow*)glfwGetiOSUIWindow();
    UIView* extensionView = [[[UIView alloc] initWithFrame:window.bounds] autorelease];
    extensionView.tag = 999;
    [window addSubview:extensionView];
    UIView* placeholder = [[[UIView alloc] initWithFrame:window.bounds] autorelease];
    m_Delegate.launchScreenView = placeholder;
    [window addSubview:placeholder];
    unsigned int before = g_UpdateCount;
    EXPECT_TRUE(WaitUntil(^BOOL { return g_UpdateCount > before + 2; }));
    EXPECT_EQ((void*)nil, (void*)placeholder.superview);
    EXPECT_EQ((void*)nil, (void*)m_Delegate.launchScreenView);
    EXPECT_EQ((void*)window, (void*)extensionView.superview);
    [placeholder removeFromSuperview];
    m_Delegate.launchScreenView = nil;
    [extensionView removeFromSuperview];
}

// Disconnecting before the first frame must clear the old placeholder; cleanup
// must still remove a new placeholder after reconnecting the retained engine,
// with completed disappearance and appearance callbacks between connections.
TEST_F(iOSSceneApplication, LaunchCleanupAfterReconnect)
{
    AppearanceObserver* observer = [[[AppearanceObserver alloc] init] autorelease];
    [m_Controller addChildViewController:observer];
    [m_Controller.view addSubview:observer.view];
    [observer didMoveToParentViewController:m_Controller];
    EXPECT_TRUE(WaitUntil(^BOOL { return observer->m_AppearanceCount == 1; }));

    UIWindow* window = (UIWindow*)glfwGetiOSUIWindow();
    UIView* first = [[[UIView alloc] initWithFrame:window.bounds] autorelease];
    m_Delegate.launchScreenView = first;
    [window addSubview:first];
    [m_Delegate sceneDidDisconnect:m_Scene];
    EXPECT_EQ((void*)nil, (void*)first.superview);
    EXPECT_EQ((void*)nil, (void*)m_Delegate.launchScreenView);
    // UIKit finishes removing the old root controller on its next event-loop
    // pass. Reconnecting before that starts an overlapping appearance transition.
    EXPECT_TRUE(WaitUntil(^BOOL { return observer->m_DisappearanceCount == 1; }));
    [m_Delegate scene:m_Scene willConnectToSession:m_Scene.session options:g_ConnectionOptions];
    window = (UIWindow*)glfwGetiOSUIWindow();
    UIView* second = [[[UIView alloc] initWithFrame:window.bounds] autorelease];
    m_Delegate.launchScreenView = second;
    [window addSubview:second];
    [m_Delegate sceneDidBecomeActive:m_Scene];
    unsigned int before = g_UpdateCount;
    EXPECT_TRUE(WaitUntil(^BOOL { return g_UpdateCount > before + 2; }));
    EXPECT_EQ(2U, observer->m_AppearanceCount);
    EXPECT_EQ(1U, observer->m_DisappearanceCount);
    EXPECT_EQ((void*)nil, (void*)second.superview);
    EXPECT_EQ((void*)nil, (void*)m_Delegate.launchScreenView);
    [first removeFromSuperview];
    [second removeFromSuperview];
    m_Delegate.launchScreenView = nil;
    [observer willMoveToParentViewController:nil];
    [observer.view removeFromSuperview];
    [observer removeFromParentViewController];
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

// Releasing an OpenGL view must run BaseView's cleanup and release both contexts;
// an empty EAGLView dealloc used to leak the view and all of these resources.
TEST_F(iOSSceneApplication, OpenGLViewDeallocation)
{
    EAGLContext* previousContext = [[EAGLContext currentContext] retain];
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    EAGLView* view = [[EAGLView alloc] initWithFrame:CGRectMake(0, 0, 32, 32)];
    EAGLContext* context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    EAGLContext* auxContext = [[EAGLContext alloc] initWithAPI:context.API sharegroup:context.sharegroup];
    EXPECT_NE((void*)nil, (void*)context);
    EXPECT_NE((void*)nil, (void*)auxContext);
    view.context = context;
    view.auxContext = auxContext;

    g_ReleasedObjects = 0;
    ReleaseObserver* observer = [[ReleaseObserver alloc] init];
    objc_setAssociatedObject(view, &g_ReleasedObjects, observer, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(context, &g_ReleasedObjects, observer, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(auxContext, &g_ReleasedObjects, observer, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [observer release];
    [context release];
    [auxContext release];
    [view setCurrentContext];
    [view release];
    [pool drain];
    EXPECT_EQ(1U, g_ReleasedObjects);
    EXPECT_EQ((void*)nil, (void*)[EAGLContext currentContext]);
    [EAGLContext setCurrentContext:previousContext];
    [previousContext release];
}

// UIKit must be able to read and set a copied marked-text style while composing
// text; the previously missing accessors could raise an unrecognized selector.
TEST_F(iOSSceneApplication, MarkedTextStyleDuringComposition)
{
    BaseView* view = m_Controller.baseView;
    ASSERT_TRUE([view respondsToSelector:@selector(markedTextStyle)]);
    ASSERT_TRUE([view respondsToSelector:@selector(setMarkedTextStyle:)]);
    EXPECT_EQ((void*)nil, (void*)view.markedTextStyle);
    EXPECT_TRUE([view becomeFirstResponder]);

    NSMutableDictionary* style = [[NSMutableDictionary alloc] initWithObjectsAndKeys:@1, NSUnderlineStyleAttributeName, nil];
    view.markedTextStyle = style;
    [style setObject:@2 forKey:NSUnderlineStyleAttributeName];
    [style release];
    EXPECT_EQ(1, [view.markedTextStyle[NSUnderlineStyleAttributeName] intValue]);
    view.markedTextStyle = view.markedTextStyle;
    EXPECT_EQ(1, [view.markedTextStyle[NSUnderlineStyleAttributeName] intValue]);

    [view setMarkedText:@"かな" selectedRange:NSMakeRange(2, 0)];
    EXPECT_TRUE([[view textInRange:view.markedTextRange] isEqualToString:@"かな"]);
    [view unmarkText];
    EXPECT_TRUE([[view textInRange:view.markedTextRange] isEqualToString:@""]);
    [view resignFirstResponder];
    view.markedTextStyle = nil;
    EXPECT_EQ((void*)nil, (void*)view.markedTextStyle);
}

// Both render views must release the copied style when replaced, cleared, or
// deallocated, so repeated composition and view recreation cannot leak its values.
TEST_F(iOSSceneApplication, MarkedTextStyleLifetime)
{
    ASSERT_TRUE([BaseView instancesRespondToSelector:@selector(setMarkedTextStyle:)]);
    EAGLContext* previousContext = [[EAGLContext currentContext] retain];
    Class viewClasses[] = {[MetalView class], [EAGLView class]};
    for (unsigned int i = 0; i < 2; ++i)
    {
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
        BaseView* view = [[viewClasses[i] alloc] initWithFrame:CGRectMake(0, 0, 32, 32)];
        g_ReleasedObjects = 0;
        SetObservedMarkedTextStyle(view);
        EXPECT_EQ(0U, g_ReleasedObjects);
        SetObservedMarkedTextStyle(view);
        EXPECT_EQ(1U, g_ReleasedObjects);
        view.markedTextStyle = nil;
        EXPECT_EQ(2U, g_ReleasedObjects);
        SetObservedMarkedTextStyle(view);
        [view release];
        [pool drain];
        EXPECT_EQ(3U, g_ReleasedObjects);
    }
    [EAGLContext setCurrentContext:previousContext];
    [previousContext release];
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
    return &g_UpdateCount;
}

static int UpdateTestEngine(void* context)
{
    ++g_UpdateCount;
    if (g_UpdateCount == 1)
    {
        // Start only after the first update, outside the display-link callback,
        // so each test can keep processing UIKit and frames through its run loop.
        SceneApplicationTestRunner* runner = [[[SceneApplicationTestRunner alloc] init] autorelease];
        [NSTimer scheduledTimerWithTimeInterval:0.0 target:runner selector:@selector(runTests:) userInfo:nil repeats:NO];
    }
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
            fprintf(stderr, "Programmatic scene startup did not start engine updates\n");
            exit(1);
        }
    });
    glfwAppBootstrap(argc, argv, 0, 0, 0, CreateTestEngine, DestroyTestEngine, UpdateTestEngine, GetTestEngineResult);
    [pool drain];
    return 0;
}
