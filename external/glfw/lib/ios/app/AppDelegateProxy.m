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

// Extensions compiled against older SDKs may still implement the legacy URL
// selectors. Forward those dynamically, as we do other optional delegate methods.
static BOOL InvokeLegacyOpenURL(id delegate, SEL selector, id* arguments, NSUInteger count)
{
    if (![delegate respondsToSelector:selector])
        return NO;

    NSInvocation* invocation = [NSInvocation invocationWithMethodSignature:[delegate methodSignatureForSelector:selector]];
    [invocation setSelector:selector];
    for (NSUInteger i = 0; i < count; ++i)
        [invocation setArgument:&arguments[i] atIndex:i + 2];
    [invocation invokeWithTarget:delegate];
    BOOL handled = NO;
    [invocation getReturnValue:&handled];
    return handled;
}

static BOOL OpenURL(id<UIApplicationDelegate> delegate, UIApplication* application, NSURL* url,
                    NSDictionary<UIApplicationOpenURLOptionsKey, id>* options)
{
    if ([delegate respondsToSelector:@selector(application:openURL:options:)])
        return [delegate application:application openURL:url options:options];

    id arguments[] = { application, url, options[UIApplicationOpenURLOptionsSourceApplicationKey],
                       options[UIApplicationOpenURLOptionsAnnotationKey] };
    BOOL handled = InvokeLegacyOpenURL(delegate, @selector(application:openURL:sourceApplication:annotation:), arguments, 4);
    if (InvokeLegacyOpenURL(delegate, @selector(application:handleOpenURL:), arguments, 2))
        handled = YES;
    return handled;
}

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

- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url options:(NSDictionary<UIApplicationOpenURLOptionsKey, id> *)options {
    // Every delegate must see the URL, even after another delegate handles it.
    // Generic forwarding would only retain the last delegate's return value.
    BOOL handled = OpenURL((id<UIApplicationDelegate>)g_ApplicationDelegate, application, url, options);
    for (int i = 0; i < g_AppDelegatesCount; ++i) {
        if (OpenURL(g_AppDelegates[i], application, url, options))
            handled = YES;
    }
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
    if ([super respondsToSelector:aSelector]) {
        return YES;
    }
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
    NSMethodSignature* signature = [super methodSignatureForSelector:aSelector];
    if (!signature)
        signature = [g_ApplicationDelegate methodSignatureForSelector:aSelector];

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
