#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/json.h>
#include <dlib/mutex.h>
#include <script/script.h>
#include <extension/extension.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "webview_common.h"

enum CommandType
{
    CMD_EVAL_OK,
    CMD_EVAL_ERROR,
};

struct Command
{
    Command()
    {
        memset(this, 0, sizeof(*this));
    }
    CommandType m_Type;
    int         m_WebViewID;
    int         m_RequestID;
    void*       m_Data;
    const char* m_Url;
};

/*
 * NOTES:
 * webViewDidFinishLoad seems to be invoked once per iframe and hence potentially
 * mulitple times. Therefore, we keep the callback and replace it whenever a new
 * load() is invoked
 */

@interface WebViewDelegate : UIViewController <UIWebViewDelegate>
{
    @public int m_WebViewID;
    @public int m_RequestID;
    @public const char* m_Url;
}
@end

struct WebView
{
    WebView()
    {
        Clear();
    }

    void Clear() {
        for( int i = 0; i < dmWebView::MAX_NUM_WEBVIEWS; ++i )
        {
            ClearWebViewInfo(&m_Info[i]);
            m_WebViewDelegates[i] = 0;
        }
        memset(m_WebViews, 0, sizeof(m_WebViews));
    }

    dmWebView::WebViewInfo  m_Info[dmWebView::MAX_NUM_WEBVIEWS];
    UIWebView*              m_WebViews[dmWebView::MAX_NUM_WEBVIEWS];
    WebViewDelegate*        m_WebViewDelegates[dmWebView::MAX_NUM_WEBVIEWS];
    dmMutex::Mutex          m_Mutex;
    dmArray<Command>        m_CmdQueue;
};

WebView g_WebView;


@implementation WebViewDelegate

- (void)webViewDidFinishLoad:(UIWebView *)webView
{
    dmWebView::CallbackInfo cbinfo;
    cbinfo.m_Info = &g_WebView.m_Info[m_WebViewID];
    cbinfo.m_WebViewID = m_WebViewID;
    cbinfo.m_RequestID = m_RequestID;
    cbinfo.m_Url = m_Url;
    cbinfo.m_Type = dmWebView::CALLBACK_RESULT_URL_OK;
    cbinfo.m_Result = 0;
    RunCallback(&cbinfo);
}

- (void)webView:(UIWebView *)webView didFailLoadWithError:(NSError *)error
{
    dmWebView::CallbackInfo cbinfo;
    cbinfo.m_Info = &g_WebView.m_Info[m_WebViewID];
    cbinfo.m_WebViewID = m_WebViewID;
    cbinfo.m_RequestID = m_RequestID;
    cbinfo.m_Url = m_Url;
    cbinfo.m_Type = dmWebView::CALLBACK_RESULT_URL_ERROR;
    cbinfo.m_Result = [error.localizedDescription UTF8String];
    RunCallback(&cbinfo);
}

@end


static char* CopyString(NSString* s)
{
    const char* osstring = [s UTF8String];
    char* copy = strdup(osstring);
    return copy;
}

static void QueueCommand(Command* cmd)
{
    dmMutex::ScopedLock lk(g_WebView.m_Mutex);
    if (g_WebView.m_CmdQueue.Full())
    {
        g_WebView.m_CmdQueue.OffsetCapacity(8);
    }
    g_WebView.m_CmdQueue.Push(*cmd);
}


namespace dmWebView
{

int Platform_Create(lua_State* L, dmWebView::WebViewInfo* _info)
{
    // Find a free slot
    int webview_id = -1;
    for( int i = 0; i < dmWebView::MAX_NUM_WEBVIEWS; ++i )
    {
        if( g_WebView.m_Info[i].m_L == 0 )
        {
            webview_id = i;
            break;
        }
    }

    if( webview_id == -1 )
    {
        dmLogError("Max number of webviews already opened: %d", dmWebView::MAX_NUM_WEBVIEWS);
        return -1;
    }

    g_WebView.m_Info[webview_id] = *_info;

    UIScreen* screen = [UIScreen mainScreen];
    
    UIWebView* view = [[UIWebView alloc] initWithFrame:screen.bounds];
    view.suppressesIncrementalRendering = YES;
    WebViewDelegate* delegate = [WebViewDelegate alloc];
    delegate->m_WebViewID = webview_id;
    delegate->m_RequestID = 0;
    delegate->m_Url = 0;
    view.delegate = delegate;

    g_WebView.m_WebViews[webview_id] = view;
    g_WebView.m_WebViewDelegates[webview_id] = delegate;

    UIView * topView = [[[[UIApplication sharedApplication] keyWindow] subviews] lastObject];
    [topView addSubview:view];
    view.hidden = TRUE;

    return webview_id;
}

#define CHECK_WEBVIEW_AND_RETURN() if( webview_id >= dmWebView::MAX_NUM_WEBVIEWS || webview_id < 0 ) { dmLogError("%s: Invalid webview_id: %d", __FUNCTION__, webview_id); return -1; }

int Platform_Destroy(lua_State* L, int webview_id)
{
    CHECK_WEBVIEW_AND_RETURN();

    [g_WebView.m_WebViews[webview_id] removeFromSuperview];
    [g_WebView.m_WebViews[webview_id] release];
    [g_WebView.m_WebViewDelegates[webview_id] release];
    g_WebView.m_Info[webview_id].m_L = 0;
    g_WebView.m_Info[webview_id].m_Self = LUA_NOREF;
    g_WebView.m_Info[webview_id].m_Callback = LUA_NOREF;

    return 0;
}

int Platform_Open(lua_State* L, int webview_id, const char* url, dmWebView::RequestInfo* options)
{
    CHECK_WEBVIEW_AND_RETURN();
    g_WebView.m_WebViews[webview_id].hidden = options->m_Hidden;
    
    NSURL* ns_url = [NSURL URLWithString: [NSString stringWithUTF8String: url]];
    NSURLRequest *request = [NSURLRequest requestWithURL: ns_url];
    [g_WebView.m_WebViews[webview_id] loadRequest:request];

    if( g_WebView.m_WebViewDelegates[webview_id]->m_Url )
        free((void*)g_WebView.m_WebViewDelegates[webview_id]->m_Url);
    g_WebView.m_WebViewDelegates[webview_id]->m_Url = strdup(url);
    return ++g_WebView.m_WebViewDelegates[webview_id]->m_RequestID;
}

int Platform_OpenRaw(lua_State* L, int webview_id, const char* html, dmWebView::RequestInfo* options)
{
    CHECK_WEBVIEW_AND_RETURN();
    g_WebView.m_WebViews[webview_id].hidden = options->m_Hidden;

    NSString* ns_html = [NSString stringWithUTF8String: html];
    [g_WebView.m_WebViews[webview_id] loadHTMLString:ns_html baseURL:nil];

    if( g_WebView.m_WebViewDelegates[webview_id]->m_Url )
        free((void*)g_WebView.m_WebViewDelegates[webview_id]->m_Url);
    g_WebView.m_WebViewDelegates[webview_id]->m_Url = 0;
    return ++g_WebView.m_WebViewDelegates[webview_id]->m_RequestID;
}


int Platform_Eval(lua_State* L, int webview_id, const char* code)
{
    CHECK_WEBVIEW_AND_RETURN();
    NSString* res = [g_WebView.m_WebViews[webview_id] stringByEvaluatingJavaScriptFromString: [NSString stringWithUTF8String: code]];

    int request_id = ++g_WebView.m_WebViewDelegates[webview_id]->m_RequestID;

    // Delay this a bit (on the main thread), so that we can return the request_id from this function,
    // before calling the callback
    Command cmd;
    cmd.m_Type = (res != nil) ? CMD_EVAL_OK : CMD_EVAL_ERROR;
    cmd.m_WebViewID = webview_id;
    cmd.m_RequestID = request_id;
    cmd.m_Url = 0;
    cmd.m_Data = (void*) ((res != nil) ? CopyString(res) : "Error string unavailable on iOS");
    QueueCommand(&cmd);

    return request_id;
}

int Platform_SetVisible(lua_State* L, int webview_id, int visible)
{
    CHECK_WEBVIEW_AND_RETURN();
    g_WebView.m_WebViews[webview_id].hidden = (BOOL)!visible;
    return 0;
}

int Platform_IsVisible(lua_State* L, int webview_id)
{
    CHECK_WEBVIEW_AND_RETURN();
    return g_WebView.m_WebViews[webview_id].isHidden ? 0 : 1;
}

#undef CHECK_WEBVIEW_AND_RETURN

} // namespace dmWebView


dmExtension::Result AppInitializeWebView(dmExtension::AppParams* params)
{
    g_WebView.Clear();    
    g_WebView.m_Mutex = dmMutex::New();
    g_WebView.m_CmdQueue.SetCapacity(8);

    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeWebView(dmExtension::AppParams* params)
{
    dmMutex::Delete(g_WebView.m_Mutex);
    g_WebView.Clear();
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeWebView(dmExtension::Params* params)
{
    dmWebView::LuaInit(params->m_L);
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeWebView(dmExtension::Params* params)
{
    for( int i = 0; i < dmWebView::MAX_NUM_WEBVIEWS; ++i )
    {
        dmWebView::WebViewInfo& info = g_WebView.m_Info[i];
        if (params->m_L == info.m_L && info.m_Callback != LUA_NOREF)
        {
            ClearWebViewInfo(&info);
        }

        if (g_WebView.m_WebViews[i])
        {
            [g_WebView.m_WebViews[i] release];

            if( g_WebView.m_WebViewDelegates[i]->m_Url )
                free((void*)g_WebView.m_WebViewDelegates[i]->m_Url);

            [g_WebView.m_WebViewDelegates[i] release];
        }
    }

    g_WebView.Clear();
    return dmExtension::RESULT_OK;
}


dmExtension::Result UpdateWebView(dmExtension::Params* params)
{
    if (g_WebView.m_CmdQueue.Empty())
        return dmExtension::RESULT_OK; // avoid a lock (~300us on iPhone 4s)

    dmMutex::ScopedLock lk(g_WebView.m_Mutex);
    for (uint32_t i=0; i != g_WebView.m_CmdQueue.Size(); ++i)
    {
        const Command& cmd = g_WebView.m_CmdQueue[i];

        dmWebView::CallbackInfo cbinfo;
        switch (cmd.m_Type)
        {
        case CMD_EVAL_OK:
            cbinfo.m_Info = &g_WebView.m_Info[cmd.m_WebViewID];
            cbinfo.m_WebViewID = cmd.m_WebViewID;
            cbinfo.m_RequestID = cmd.m_RequestID;
            cbinfo.m_Url = 0;
            cbinfo.m_Type = dmWebView::CALLBACK_RESULT_EVAL_OK;
            cbinfo.m_Result = (const char*)cmd.m_Data;
            RunCallback(&cbinfo);
            break;

        case CMD_EVAL_ERROR:
            cbinfo.m_Info = &g_WebView.m_Info[cmd.m_WebViewID];
            cbinfo.m_WebViewID = cmd.m_WebViewID;
            cbinfo.m_RequestID = cmd.m_RequestID;
            cbinfo.m_Url = 0;
            cbinfo.m_Type = dmWebView::CALLBACK_RESULT_EVAL_ERROR;
            cbinfo.m_Result = (const char*)cmd.m_Data;
            RunCallback(&cbinfo);
            break;

        default:
            assert(false);
        }
        if (cmd.m_Url) {
            free((void*)cmd.m_Url);
        }
        if (cmd.m_Data) {
            free(cmd.m_Data);
        }
    }
    g_WebView.m_CmdQueue.SetSize(0);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(WebViewExt, "WebView", AppInitializeWebView, AppFinalizeWebView, InitializeWebView, UpdateWebView, 0, FinalizeWebView)
