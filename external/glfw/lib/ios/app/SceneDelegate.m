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

#import "SceneDelegate.h"
#import "AppDelegate.h"
#import "ViewController.h"

#include "internal.h"

extern AppDelegate* g_ApplicationDelegate;
extern UIWindow* g_ApplicationWindow;
extern _GLFWwin g_Savewin;

#define MAX_SCENE_DELEGATES (32)
static id<UISceneDelegate> g_SceneDelegates[MAX_SCENE_DELEGATES];
static int g_SceneDelegateCount = 0;
static UIScene* g_ConnectedScene = nil;
static BOOL g_SceneActive = NO;

// Retain a snapshot so delegates may unregister/release themselves during delivery.
static NSArray* GetSceneDelegates(void)
{
    return [NSArray arrayWithObjects:g_SceneDelegates count:g_SceneDelegateCount];
}

static void SetSceneActive(BOOL active)
{
    BOOL changed = g_SceneActive != active;
    g_SceneActive = active;
    _glfwWin.iconified = !active;
    if (changed && _glfwWin.windowFocusCallback)
        _glfwWin.windowFocusCallback(active);
}

int _glfwPlatformIsSceneActive(void)
{
    return g_ConnectedScene != nil && g_SceneActive;
}

@implementation DefoldSceneDelegate

@synthesize window;

- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options
{
    if (![scene isKindOfClass:[UIWindowScene class]] ||
        ![session.role isEqualToString:UIWindowSceneSessionRoleApplication] ||
        g_ConnectedScene != nil)
    {
        NSLog(@"Defold supports one application window scene");
        return;
    }

    g_ConnectedScene = scene;
    SetSceneActive(NO);
    self.window = [[[UIWindow alloc] initWithWindowScene:(UIWindowScene*)scene] autorelease];
    g_ApplicationWindow = window;
    _glfwWin.window = window;

    // The app owns the controller/render view across disconnection. Do not destroy
    // the engine or its graphics contexts when UIKit disconnects the window.
    if (!g_ApplicationDelegate.viewController)
        g_ApplicationDelegate.viewController = [[[ViewController alloc] init] autorelease];
    window.rootViewController = g_ApplicationDelegate.viewController;
    _glfwWin.viewController = g_ApplicationDelegate.viewController;

    NSString* launchScreenName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"UILaunchStoryboardName"];
    if (launchScreenName)
    {
        UIStoryboard* storyboard = [UIStoryboard storyboardWithName:launchScreenName bundle:nil];
        UIView* launchScreenView = [storyboard instantiateInitialViewController].view;
        launchScreenView.frame = window.bounds;
        launchScreenView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        launchScreenView.tag = 999;
        [window addSubview:launchScreenView];
    }
    [window makeKeyAndVisible];

    for (id<UISceneDelegate> delegate in GetSceneDelegates())
    {
        if ([delegate respondsToSelector:_cmd])
            [delegate scene:scene willConnectToSession:session options:options];
    }
}

- (void)sceneDidDisconnect:(UIScene*)scene
{
    if (scene == g_ConnectedScene)
    {
        SetSceneActive(NO);
        [g_ApplicationDelegate.viewController.baseView invalidateDisplayLink];
        window.hidden = YES;
        window.rootViewController = nil;
        window.windowScene = nil;
        g_ApplicationWindow = nil;
        _glfwWin.window = nil;
        g_Savewin.window = nil;
        g_ConnectedScene = nil;
        self.window = nil;
    }
    for (id<UISceneDelegate> delegate in GetSceneDelegates())
    {
        if ([delegate respondsToSelector:_cmd])
            [delegate sceneDidDisconnect:scene];
    }
}

- (void)sceneDidBecomeActive:(UIScene*)scene
{
    if (scene == g_ConnectedScene)
        SetSceneActive(YES);
    for (id<UISceneDelegate> delegate in GetSceneDelegates())
    {
        if ([delegate respondsToSelector:_cmd])
            [delegate sceneDidBecomeActive:scene];
    }
}

- (void)sceneWillResignActive:(UIScene*)scene
{
    if (scene == g_ConnectedScene)
    {
        SetSceneActive(NO);
        if (_glfwWin.clientAPI == GLFW_OPENGL_API)
        {
            [g_ApplicationDelegate.viewController.baseView setCurrentContext];
            glFinish();
        }
    }
    for (id<UISceneDelegate> delegate in GetSceneDelegates())
    {
        if ([delegate respondsToSelector:_cmd])
            [delegate sceneWillResignActive:scene];
    }
}

- (void)sceneWillEnterForeground:(UIScene*)scene
{
    for (id<UISceneDelegate> delegate in GetSceneDelegates())
    {
        if ([delegate respondsToSelector:_cmd])
            [delegate sceneWillEnterForeground:scene];
    }
}

- (void)sceneDidEnterBackground:(UIScene*)scene
{
    for (id<UISceneDelegate> delegate in GetSceneDelegates())
    {
        if ([delegate respondsToSelector:_cmd])
            [delegate sceneDidEnterBackground:scene];
    }
}

- (void)scene:(UIScene*)scene openURLContexts:(NSSet<UIOpenURLContext*>*)contexts
{
    for (id<UISceneDelegate> delegate in GetSceneDelegates())
    {
        if ([delegate respondsToSelector:_cmd])
            [delegate scene:scene openURLContexts:contexts];
    }
}

- (void)scene:(UIScene*)scene continueUserActivity:(NSUserActivity*)activity
{
    for (id<UISceneDelegate> delegate in GetSceneDelegates())
    {
        if ([delegate respondsToSelector:_cmd])
            [delegate scene:scene continueUserActivity:activity];
    }
}

- (void)scene:(UIScene*)scene willContinueUserActivityWithType:(NSString*)activityType
{
    for (id<UISceneDelegate> delegate in GetSceneDelegates())
    {
        if ([delegate respondsToSelector:_cmd])
            [delegate scene:scene willContinueUserActivityWithType:activityType];
    }
}

- (void)scene:(UIScene*)scene didFailToContinueUserActivityWithType:(NSString*)activityType error:(NSError*)error
{
    for (id<UISceneDelegate> delegate in GetSceneDelegates())
    {
        if ([delegate respondsToSelector:_cmd])
            [delegate scene:scene didFailToContinueUserActivityWithType:activityType error:error];
    }
}

- (void)scene:(UIScene*)scene didUpdateUserActivity:(NSUserActivity*)activity
{
    for (id<UISceneDelegate> delegate in GetSceneDelegates())
    {
        if ([delegate respondsToSelector:_cmd])
            [delegate scene:scene didUpdateUserActivity:activity];
    }
}

- (void)dealloc
{
    [window release];
    [super dealloc];
}

@end

GLFWAPI void glfwRegisterUISceneDelegate(void* delegate)
{
    assert([NSThread isMainThread]);
    if (!delegate)
        return;
    for (int i = 0; i < g_SceneDelegateCount; ++i)
    {
        if (g_SceneDelegates[i] == delegate)
            return;
    }
    if (g_SceneDelegateCount == MAX_SCENE_DELEGATES)
        NSLog(@"Max UISceneDelegates reached (%d)", MAX_SCENE_DELEGATES);
    else
        g_SceneDelegates[g_SceneDelegateCount++] = (id<UISceneDelegate>)delegate;
}

GLFWAPI void glfwUnregisterUISceneDelegate(void* delegate)
{
    assert([NSThread isMainThread]);
    for (int i = 0; i < g_SceneDelegateCount; ++i)
    {
        if (g_SceneDelegates[i] == delegate)
        {
            --g_SceneDelegateCount;
            memmove(&g_SceneDelegates[i], &g_SceneDelegates[i + 1], (g_SceneDelegateCount - i) * sizeof(g_SceneDelegates[0]));
            g_SceneDelegates[g_SceneDelegateCount] = nil;
            return;
        }
    }
}
