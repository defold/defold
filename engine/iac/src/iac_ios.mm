#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/json.h>
#include <script/script.h>
#include <dmsdk/extension/extension.h>

#include "iac.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

struct IACInvocationListener
{
    IACInvocationListener()
    {
        Clear();
    }

    void Clear()
    {
        m_L = 0;
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
    }

    lua_State* m_L;
    int        m_Callback;
    int        m_Self;
};


struct IAC
{
    IAC()
    {
        m_SavedInvocation = 0;
        Clear();
    }

    void Clear() {
        m_AppDelegate = 0;
        m_IACListener.Clear();
        m_LaunchInvocation = false;

        if (m_SavedInvocation) {
             [m_SavedInvocation release];
             m_SavedInvocation = 0;
        }
    }

    id<UIApplicationDelegate>   m_AppDelegate;
    IACInvocationListener       m_IACListener;
    NSMutableDictionary*        m_SavedInvocation;
    bool                        m_LaunchInvocation;
} g_IAC;


static void ObjCToLua(lua_State*L, id obj)
{
    if ([obj isKindOfClass:[NSString class]]) {
        const char* str = [((NSString*) obj) UTF8String];
        lua_pushstring(L, str);
    } else if ([obj isKindOfClass:[NSNumber class]]) {
        lua_pushnumber(L, [((NSNumber*) obj) doubleValue]);
    } else if ([obj isKindOfClass:[NSNull class]]) {
        lua_pushnil(L);
    } else if ([obj isKindOfClass:[NSDictionary class]]) {
        NSDictionary* dict = (NSDictionary*) obj;
        lua_createtable(L, 0, [dict count]);
        for (NSString* key in dict) {
            lua_pushstring(L, [key UTF8String]);
            id value = [dict objectForKey:key];
            ObjCToLua(L, (NSDictionary*) value);
            lua_rawset(L, -3);
        }
    } else if ([obj isKindOfClass:[NSArray class]]) {
        NSArray* a = (NSArray*) obj;
        lua_createtable(L, [a count], 0);
        for (int i = 0; i < [a count]; ++i) {
            ObjCToLua(L, [a objectAtIndex: i]);
            lua_rawseti(L, -2, i+1);
        }
    } else {
        dmLogWarning("Unsupported iac payload value '%s' (%s)", [[obj description] UTF8String], [[[obj class] description] UTF8String]);
        lua_pushnil(L);
    }
}


static void RunIACListener(NSDictionary *userdata, uint32_t type)
{
    if (g_IAC.m_IACListener.m_Callback != LUA_NOREF)
    {
        lua_State* L = g_IAC.m_IACListener.m_L;
        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, g_IAC.m_IACListener.m_Callback);

        // Setup self
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_IAC.m_IACListener.m_Self);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        ObjCToLua(L, userdata);
        lua_pushnumber(L, type);

        int ret = dmScript::PCall(L, 3, 0);
        if (ret != 0) {
            dmLogError("Error running iac callback");
        }
        assert(top == lua_gettop(L));
    }
}


@interface IACAppDelegate : NSObject <UIApplicationDelegate>

@end


@implementation IACAppDelegate

-(BOOL) application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation{

    // Handle invocations
    if(g_IAC.m_LaunchInvocation)
    {
        // If this is the launch invocation saved, skip this first call to openURL as it is the same invocation and we want to store it!
        g_IAC.m_LaunchInvocation = false;
        return YES;
    }
    if (g_IAC.m_SavedInvocation)
    {
        [g_IAC.m_SavedInvocation release];
        g_IAC.m_SavedInvocation = 0;
        if (g_IAC.m_IACListener.m_Callback == LUA_NOREF)
        {
            dmLogWarning("No iac listener set. Invocation discarded.");
        }
    }
    NSMutableDictionary* userdata = [[NSMutableDictionary alloc]initWithCapacity:2];
    [userdata setObject:[url absoluteString] forKey:@"url"];
    if( sourceApplication )
    {
        [userdata setObject:sourceApplication forKey:@"origin"];
    }
    if (g_IAC.m_IACListener.m_Callback == LUA_NOREF)
    {
        g_IAC.m_SavedInvocation = userdata;
    }
    else
    {
        RunIACListener(userdata, DM_IAC_EXTENSION_TYPE_INVOCATION);
        [userdata release];
    }
    return YES;
}


- (BOOL)application:(UIApplication *)application willFinishLaunchingWithOptions:(NSDictionary *)launchOptions {

    // Handle invocations launching the app.
    // willFinishLaunchingWithOptions is called prior to any scripts so we are garuanteed to have this information at any time set_listener is called!
    if (launchOptions[UIApplicationLaunchOptionsURLKey]) {
        g_IAC.m_SavedInvocation = [[NSMutableDictionary alloc]initWithCapacity:2];
        if (launchOptions[UIApplicationLaunchOptionsSourceApplicationKey]) {
            [g_IAC.m_SavedInvocation setObject:[launchOptions valueForKey:UIApplicationLaunchOptionsSourceApplicationKey] forKey:@"origin"];
        }
        if (launchOptions[UIApplicationLaunchOptionsURLKey]) {
            [g_IAC.m_SavedInvocation setObject:[[launchOptions valueForKey:UIApplicationLaunchOptionsURLKey] absoluteString] forKey:@"url"];
        }
        g_IAC.m_LaunchInvocation = true;
    }
    // Return YES prevents OpenURL from being called, we need to do this as other extensions might and therefore internally handle OpenURL also being called.
    return YES;
}

@end


int IAC_PlatformSetListener(lua_State* L)
{
    IAC* iac = &g_IAC;
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);
    if (iac->m_IACListener.m_Callback != LUA_NOREF) {
        dmScript::Unref(iac->m_IACListener.m_L, LUA_REGISTRYINDEX, iac->m_IACListener.m_Callback);
        dmScript::Unref(iac->m_IACListener.m_L, LUA_REGISTRYINDEX, iac->m_IACListener.m_Self);
    }
    iac->m_IACListener.m_L = dmScript::GetMainThread(L);
    iac->m_IACListener.m_Callback = cb;

    dmScript::GetInstance(L);
    iac->m_IACListener.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    if (g_IAC.m_SavedInvocation)
    {
        RunIACListener(g_IAC.m_SavedInvocation, DM_IAC_EXTENSION_TYPE_INVOCATION);
        [g_IAC.m_SavedInvocation release];
        g_IAC.m_SavedInvocation = 0;
    }
    return 0;
}


dmExtension::Result AppInitializeIAC(dmExtension::AppParams* params)
{
    g_IAC.Clear();
    g_IAC.m_AppDelegate = [[IACAppDelegate alloc] init];
    dmExtension::RegisteriOSUIApplicationDelegate(g_IAC.m_AppDelegate);
    return dmExtension::RESULT_OK;
}


dmExtension::Result AppFinalizeIAC(dmExtension::AppParams* params)
{
    dmExtension::UnregisteriOSUIApplicationDelegate(g_IAC.m_AppDelegate);
    [g_IAC.m_AppDelegate release];
    g_IAC.m_AppDelegate = 0;
    g_IAC.Clear();
    return dmExtension::RESULT_OK;
}


dmExtension::Result InitializeIAC(dmExtension::Params* params)
{
    return dmIAC::Initialize(params);
}


dmExtension::Result FinalizeIAC(dmExtension::Params* params)
{
    if (params->m_L == g_IAC.m_IACListener.m_L && g_IAC.m_IACListener.m_Callback != LUA_NOREF) {
        dmScript::Unref(g_IAC.m_IACListener.m_L, LUA_REGISTRYINDEX, g_IAC.m_IACListener.m_Callback);
        dmScript::Unref(g_IAC.m_IACListener.m_L, LUA_REGISTRYINDEX, g_IAC.m_IACListener.m_Self);
        g_IAC.m_IACListener.m_L = 0;
        g_IAC.m_IACListener.m_Callback = LUA_NOREF;
        g_IAC.m_IACListener.m_Self = LUA_NOREF;
    }
    return dmIAC::Finalize(params);
}

DM_DECLARE_EXTENSION(IACExt, "IAC", AppInitializeIAC, AppFinalizeIAC, InitializeIAC, 0, 0, FinalizeIAC)



