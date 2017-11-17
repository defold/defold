#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/json.h>
#include <script/script.h>
#include <extension/extension.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#define LIB_NAME "adtruth"

struct AdTruth;

/*
 * NOTES:
 * webViewDidFinishLoad seems to be invoked once per iframe and hence potentially
 * mulitple times. Therefore, we keep the callback and replace it whenever a new
 * load() is invoked
 */

struct AdTruth
{
    AdTruth()
    {
        Clear();
    }

    void Clear() {
        m_L = 0;
        m_Callback = LUA_NOREF;
        m_Self = LUA_NOREF;
        m_WebView = 0;
        m_WebViewDelegate = 0;
    }

    lua_State*           m_L;
    int                  m_Callback;
    int                  m_Self;
    UIWebView*           m_WebView;
    id<UIWebViewDelegate> m_WebViewDelegate;
};

AdTruth g_AdTruth;

static void PushError(lua_State*L, NSError* error)
{
    // Could be extended with error codes etc
    if (error != 0) {
        lua_newtable(L);
        lua_pushstring(L, "error");
        lua_pushstring(L, [error.localizedDescription UTF8String]);
        lua_rawset(L, -3);
    } else {
        lua_pushnil(L);
    }
}

static void RunCallback(lua_State*L, NSError* error)
{
    if (g_AdTruth.m_Callback != LUA_NOREF) {
        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, g_AdTruth.m_Callback);
        // Setup self
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_AdTruth.m_Self);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        if (!dmScript::IsInstanceValid(L))
        {
            dmLogError("Could not run adtruth callback because the instance has been deleted.");
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return;
        }

        PushError(L, error);

        int ret = dmScript::PCall(L, 2, 0);
        if (ret != 0) {
            dmLogError("Error running adtruth callback");
        }
        assert(top == lua_gettop(L));
    } else {
        dmLogError("No callback set");
    }
}

@interface AdTruthWebViewDelegate : UIViewController <UIWebViewDelegate>
{
    UIWebView *m_WebView;
}
@end


@implementation AdTruthWebViewDelegate

- (void)webViewDidFinishLoad:(UIWebView *)webView
{
    RunCallback(g_AdTruth.m_L, 0);

}

- (void)webView:(UIWebView *)webView didFailLoadWithError:(NSError *)error
{
    RunCallback(g_AdTruth.m_L, error);
}

@end

static void Init()
{
    AdTruth* at = &g_AdTruth;

    if (at->m_WebView == 0) {
        UIWebView* view = [[UIWebView alloc] initWithFrame:CGRectZero];
        view.suppressesIncrementalRendering = YES;
        AdTruthWebViewDelegate* delegate = [AdTruthWebViewDelegate alloc];
        view.delegate = delegate;
        at->m_WebView = view;
        at->m_WebViewDelegate = delegate;
    }
}

static int AdTruth_Load(lua_State* L)
{
    int top = lua_gettop(L);

    const const char* url = luaL_checkstring(L, 1);

    if (g_AdTruth.m_Callback != LUA_NOREF) {
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_AdTruth.m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, g_AdTruth.m_Self);
        g_AdTruth.m_Callback = LUA_NOREF;
        g_AdTruth.m_Self = LUA_NOREF;
        g_AdTruth.m_L = 0;
    }

    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    g_AdTruth.m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);
    dmScript::GetInstance(L);
    g_AdTruth.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
    g_AdTruth.m_L = dmScript::GetMainThread(L);

    Init();

    NSURL* ns_url = [NSURL URLWithString: [NSString stringWithUTF8String: url]];
    NSURLRequest *request = [NSURLRequest requestWithURL: ns_url];
    [g_AdTruth.m_WebView loadRequest:request];

    assert(top == lua_gettop(L));

    return 0;
}

static int AdTruth_Eval(lua_State* L)
{
    int top = lua_gettop(L);

    Init();
    const char* code = luaL_checkstring(L, 1);
    NSString* res = [g_AdTruth.m_WebView stringByEvaluatingJavaScriptFromString: [NSString stringWithUTF8String: code]];

    lua_pushstring(L, [res UTF8String]);

    assert(top + 1== lua_gettop(L));
    return 1;
}

int AdTruth_GetReferrer(lua_State* L)
{
    int top = lua_gettop(L);
    lua_pushnil(L);
    assert(top + 1 == lua_gettop(L));
    return 1;
}

static const luaL_reg AdTruth_methods[] =
{
    {"load", AdTruth_Load},
    {"eval", AdTruth_Eval},
    {"get_referrer", AdTruth_GetReferrer},
    {0, 0}
};

dmExtension::Result AppInitializeAdTruth(dmExtension::AppParams* params)
{
    g_AdTruth.Clear();
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeAdTruth(dmExtension::AppParams* params)
{
    if (g_AdTruth.m_WebView) {
        [g_AdTruth.m_WebView release];
    }
    g_AdTruth.Clear();
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeAdTruth(dmExtension::Params* params)
{
    lua_State*L = params->m_L;
    int top = lua_gettop(L);
    luaL_register(L, LIB_NAME, AdTruth_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeAdTruth(dmExtension::Params* params)
{
    if (params->m_L == g_AdTruth.m_L && g_AdTruth.m_Callback != LUA_NOREF) {
        dmScript::Unref(g_AdTruth.m_L, LUA_REGISTRYINDEX, g_AdTruth.m_Callback);
        dmScript::Unref(g_AdTruth.m_L, LUA_REGISTRYINDEX, g_AdTruth.m_Self);
        g_AdTruth.m_L = 0;
        g_AdTruth.m_Callback = LUA_NOREF;
        g_AdTruth.m_Self = LUA_NOREF;
    }

    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(AdTruthExt, "AdTruth", AppInitializeAdTruth, AppFinalizeAdTruth, InitializeAdTruth, 0, 0, FinalizeAdTruth)
