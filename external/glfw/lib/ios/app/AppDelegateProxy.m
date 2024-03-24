// Copyright 2020-2023 The Defold Foundation
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

#import "AppDelegateProxy.h"

@class AppDelegate;

#include "internal.h"

#define MAX_APP_DELEGATES (32)
id<UIApplicationDelegate> g_AppDelegates[MAX_APP_DELEGATES];
int g_AppDelegatesCount = 0;
AppDelegate* g_ApplicationDelegate = 0;

@implementation AppDelegateProxy

- (AppDelegateProxy*)init
{
    UIApplication* app = [UIApplication sharedApplication];
    g_ApplicationDelegate = (AppDelegate*)[app.delegate retain];
    app.delegate = self;
    return self;
}

+ (BOOL) application:(UIApplication *)application willFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    BOOL handled = NO;
    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: @selector(application:willFinishLaunchingWithOptions:)]) {
            if ([g_AppDelegates[i] application:application willFinishLaunchingWithOptions:launchOptions])
                handled = YES;
        }
    }
    return handled;
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: @selector(applicationDidFinishLaunching:)]) {
            [g_AppDelegates[i] applicationDidFinishLaunching: application];
        }
    }

    BOOL handled = NO;
    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: @selector(application:didFinishLaunchingWithOptions:)]) {
            if ([g_AppDelegates[i] application:application didFinishLaunchingWithOptions:launchOptions])
                handled = YES;
        }
    }
    return handled;
}

// NOTE: Don't understand why this special case is required. "forwardInvocation" et al
// should be able to intercept all invocations but for some unknown reason not handleOpenURL
-(BOOL) application:(UIApplication *)application handleOpenURL:(NSURL *)url {
    SEL sel = @selector(application:handleOpenURL:);
    BOOL handled = NO;

    if ([g_ApplicationDelegate respondsToSelector:sel]) {
        if ([g_ApplicationDelegate application: application handleOpenURL: url])
            handled = YES;
    }

    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: sel]) {
            if ([g_AppDelegates[i] application: application handleOpenURL: url])
                handled = YES;
        }
    }
    return handled;
}

-(BOOL) application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation{
    SEL sel = @selector(application:openURL:sourceApplication:annotation:);
    BOOL handled = NO;

   for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: sel])  {
            if ([g_AppDelegates[i] application: application openURL: url sourceApplication:sourceApplication annotation:(id)annotation])
                handled = YES;
        }
    }

    // handleOpenURL is deprecated. We call it from here as if openURL is implemented, handleOpenURL won't be called.
    if ([self application: application handleOpenURL:url])
        handled = YES;

    return handled;
}

- (void)forwardInvocation:(NSInvocation *)anInvocation {
    BOOL invoked = NO;
    if ([g_ApplicationDelegate respondsToSelector: [anInvocation selector]]) {
        [anInvocation invokeWithTarget: g_ApplicationDelegate];
        invoked = YES;
    }

    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: [anInvocation selector]]) {
            [anInvocation invokeWithTarget: g_AppDelegates[i]];
            invoked = YES;
        }
    }

    if (!invoked) {
        [g_ApplicationDelegate forwardInvocation:anInvocation];
    }
}

- (BOOL)respondsToSelector:(SEL)aSelector {
    if ([g_ApplicationDelegate respondsToSelector: aSelector]) {
        return YES;
    }

    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if ([g_AppDelegates[i] respondsToSelector: aSelector]) {
            return YES;
        }
    }

    return [g_ApplicationDelegate respondsToSelector: aSelector];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    NSMethodSignature* signature = [g_ApplicationDelegate methodSignatureForSelector:aSelector];

    if (!signature)
    {
        for (int i = 0; i < g_AppDelegatesCount; ++i) {
            if ([g_AppDelegates[i] respondsToSelector: aSelector]) {
                return [g_AppDelegates[i] methodSignatureForSelector:aSelector];
            }
        }
    }
    return signature;
}

@end

GLFWAPI void glfwRegisterUIApplicationDelegate(void* delegate)
{
    NSLog(@"Added delegate %@", NSStringFromClass([(id)delegate class]));
    if (g_AppDelegatesCount >= MAX_APP_DELEGATES) {
        NSLog(@"Max UIApplicationDelegates reached (%d)", MAX_APP_DELEGATES);
    } else {
        g_AppDelegates[g_AppDelegatesCount++] = (id<UIApplicationDelegate>) delegate;
    }
}

GLFWAPI void glfwUnregisterUIApplicationDelegate(void* delegate)
{
    for (int i = 0; i < g_AppDelegatesCount; ++i)
    {
        if (g_AppDelegates[i] == delegate)
        {
            g_AppDelegates[i] = g_AppDelegates[g_AppDelegatesCount - 1];
            g_AppDelegatesCount--;
            return;
        }
    }
}
