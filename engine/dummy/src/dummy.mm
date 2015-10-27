#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/json.h>
#include <extension/extension.h>
#include <script/script.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>


#define LIB_NAME "dummy"

struct Dummy
{
    Dummy()
    {
        Clear();
    }

    void Clear() {
        m_L = 0;
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
        m_AppDelegate = 0;
        m_ScheduledID = -1;
    }

    lua_State*           m_L;
    int                  m_Callback;
    int                  m_Self;
    id<UIApplicationDelegate> m_AppDelegate;
    NSDictionary*        m_SavedNotification;

    int m_ScheduledID;
};

Dummy g_Dummy;



@interface DummyAppDelegate : NSObject <UIApplicationDelegate>
- (BOOL)application:(UIApplication *)application
                   didFinishLaunchingWithOptions:(NSDictionary *)launchOptions;
@end

@implementation DummyAppDelegate

    - (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
        return TRUE;
    }

@end



dmExtension::Result AppInitializeDummy(dmExtension::AppParams* params)
{
    NSLog(@"Dummy AppInitialize ...");
    g_Dummy.Clear();
    g_Dummy.m_AppDelegate = [[DummyAppDelegate alloc] init];
    dmExtension::RegisterUIApplicationDelegate(g_Dummy.m_AppDelegate);
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeDummy(dmExtension::AppParams* params)
{
    NSLog(@"Dummy AppFinalize ...");

    dmExtension::UnregisterUIApplicationDelegate(g_Dummy.m_AppDelegate);
    [g_Dummy.m_AppDelegate release];
    g_Dummy.m_AppDelegate = 0;
    g_Dummy.Clear();
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeDummy(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}
dmExtension::Result FinalizeDummy(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(DummyExt, "Dummy", AppInitializeDummy, AppFinalizeDummy, InitializeDummy, 0, FinalizeDummy)
