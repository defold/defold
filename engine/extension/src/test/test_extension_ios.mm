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

#include <jc_test/jc_test.h>
#include <dmsdk/extension/extension.hpp>
#import "SceneDelegate.h"

static int g_ReleasedSceneObservers;

@interface TestSceneObserver : NSObject <UISceneDelegate, UIApplicationDelegate>
{
@public
    int m_ActiveCount;
    int m_URLCount;
    int m_ActivityCount;
    int m_LegacyCount;
    id m_LastPayload;
    id m_RemoveObserver;
}
@end

@implementation TestSceneObserver
- (void)sceneDidBecomeActive:(UIScene*)scene
{
    ++m_ActiveCount;
    if (m_RemoveObserver)
    {
        ExtensionUnregisteriOSUISceneDelegate(m_RemoveObserver);
        [m_RemoveObserver release];
        m_RemoveObserver = nil;
    }
}
- (void)scene:(UIScene*)scene openURLContexts:(NSSet<UIOpenURLContext*>*)contexts
{
    ++m_URLCount;
    m_LastPayload = contexts;
}
- (void)scene:(UIScene*)scene continueUserActivity:(NSUserActivity*)activity
{
    ++m_ActivityCount;
    m_LastPayload = activity;
}
- (void)applicationDidBecomeActive:(UIApplication*)application
{
    ++m_LegacyCount;
}
- (BOOL)application:(UIApplication*)application openURL:(NSURL*)url options:(NSDictionary*)options
{
    ++m_LegacyCount;
    return YES;
}
- (void)dealloc
{
    ++g_ReleasedSceneObservers;
    [super dealloc];
}
@end

class iOSSceneDelegates : public jc_test_base_class
{
public:
    NSAutoreleasePool* m_Pool;
    DefoldSceneDelegate* m_SceneDelegate;
    UIScene* m_Scene;

    void SetUp()
    {
        m_Pool = [[NSAutoreleasePool alloc] init];
        m_SceneDelegate = [[DefoldSceneDelegate alloc] init];
        // Forwarding tests need an identity, not a UIKit-connected window scene.
        m_Scene = (UIScene*)[[NSObject alloc] init];
        g_ReleasedSceneObservers = 0;
    }

    void TearDown()
    {
        [m_Scene release];
        [m_SceneDelegate release];
        [m_Pool drain];
    }
};

// Both C and C++ registrations must reach the scene dispatcher; duplicate
// registration must not deliver an event twice or leave a stale observer.
TEST_F(iOSSceneDelegates, Registration)
{
    TestSceneObserver* observer = [[TestSceneObserver alloc] init];
    ExtensionRegisteriOSUISceneDelegate(observer);
    dmExtension::RegisteriOSUISceneDelegate(observer);
    [m_SceneDelegate sceneDidBecomeActive:m_Scene];
    ASSERT_EQ(1, observer->m_ActiveCount);
    dmExtension::UnregisteriOSUISceneDelegate(observer);
    [m_SceneDelegate sceneDidBecomeActive:m_Scene];
    ASSERT_EQ(1, observer->m_ActiveCount);
    [observer release];
}

// Scene URL/activity payloads reach every observer unchanged, including repeated
// delivery of the same URL set; no legacy application callbacks are synthesized.
TEST_F(iOSSceneDelegates, NativeEventsWithoutLegacyBridge)
{
    TestSceneObserver* first = [[TestSceneObserver alloc] init];
    TestSceneObserver* second = [[TestSceneObserver alloc] init];
    ExtensionRegisteriOSUIApplicationDelegate(first);
    dmExtension::RegisteriOSUISceneDelegate(first);
    ExtensionRegisteriOSUISceneDelegate(second);
    NSSet* contexts = [NSSet set];
    [m_SceneDelegate sceneDidBecomeActive:m_Scene];
    [m_SceneDelegate scene:m_Scene openURLContexts:contexts];
    [m_SceneDelegate scene:m_Scene openURLContexts:contexts];
    ASSERT_EQ(2, first->m_URLCount);
    ASSERT_EQ(2, second->m_URLCount);
    ASSERT_EQ((void*)contexts, (void*)second->m_LastPayload);
    NSUserActivity* activity = [[[NSUserActivity alloc] initWithActivityType:@"com.defold.test"] autorelease];
    [m_SceneDelegate scene:m_Scene continueUserActivity:activity];
    ASSERT_EQ(1, first->m_ActivityCount);
    ASSERT_EQ((void*)activity, (void*)first->m_LastPayload);
    ASSERT_EQ(0, first->m_LegacyCount);
    ExtensionUnregisteriOSUIApplicationDelegate(first);
    ExtensionUnregisteriOSUISceneDelegate(first);
    ExtensionUnregisteriOSUISceneDelegate(second);
    [first release];
    [second release];
}

// A callback may unregister and release another observer. The current snapshot
// keeps it alive through delivery, while the next event omits it.
TEST_F(iOSSceneDelegates, UnregisterDuringDispatch)
{
    TestSceneObserver* first = [[TestSceneObserver alloc] init];
    TestSceneObserver* second = [[TestSceneObserver alloc] init];
    first->m_RemoveObserver = second;
    ExtensionRegisteriOSUISceneDelegate(first);
    ExtensionRegisteriOSUISceneDelegate(second);
    NSAutoreleasePool* eventPool = [[NSAutoreleasePool alloc] init];
    [m_SceneDelegate sceneDidBecomeActive:m_Scene];
    ASSERT_EQ(1, second->m_ActiveCount);
    ASSERT_EQ(0, g_ReleasedSceneObservers);
    [eventPool drain];
    ASSERT_EQ(1, g_ReleasedSceneObservers);
    [m_SceneDelegate sceneDidBecomeActive:m_Scene];
    ASSERT_EQ(2, first->m_ActiveCount);
    ExtensionUnregisteriOSUISceneDelegate(first);
    [first release];
}

// Registration has a bounded capacity and does not retain observers after they
// are unregistered; overflowing it must not overwrite an existing delegate.
TEST_F(iOSSceneDelegates, Capacity)
{
    TestSceneObserver* observers[33];
    for (int i = 0; i < 33; ++i)
    {
        observers[i] = [[TestSceneObserver alloc] init];
        ExtensionRegisteriOSUISceneDelegate(observers[i]);
    }
    [m_SceneDelegate sceneDidBecomeActive:m_Scene];
    for (int i = 0; i < 33; ++i)
    {
        ASSERT_EQ(i < 32 ? 1 : 0, observers[i]->m_ActiveCount);
        ExtensionUnregisteriOSUISceneDelegate(observers[i]);
        [observers[i] release];
    }
}
