package com.defold.webview;

import android.app.Activity;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.Window;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.webkit.ConsoleMessage;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebSettings;

import android.util.Log;


public class WebViewJNI {

    public static final String PREFERENCES_FILE = "webview";
    public static final String TAG = "defold.webview";
    public static final String JS_NAMESPACE = "defold";

    private Activity activity;
    private static WebViewInfo[] infos;

    public native void onPageFinished(String url, int webview_id, int request_id);
    public native void onReceivedError(String url, int webview_id, int request_id, String errorMessage);
    public native void onEvalFinished(String result, int webview_id, int request_id);
    public native void onEvalFailed(String errorMessage, int webview_id, int request_id);

    private static class CustomWebView extends WebView {
        private WebViewInfo info;

        public CustomWebView(Activity context, WebViewInfo info) {
            super(context);
            this.info = info;
        }

        // For Android 2.3 able to show Keyboard on input / textarea focus
        @Override
        public boolean onTouchEvent(MotionEvent event) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_UP:
                if (!this.hasFocus()) {
                    this.requestFocus();

                }

                if( Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB ) { // Api level 11
                    setSystemUiVisibility(
                        View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        //| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        //| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION // hide nav bar
                        | View.SYSTEM_UI_FLAG_FULLSCREEN // hide status bar
                        | View.SYSTEM_UI_FLAG_IMMERSIVE
                    );
                    invalidate();
                }
                break;
            }
            return super.onTouchEvent(event);
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            super.onLayout(changed, left, top, right, bottom);

            if( Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB ) { // Api level 11
                setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION // hide nav bar
                    | View.SYSTEM_UI_FLAG_FULLSCREEN // hide status bar
                    | View.SYSTEM_UI_FLAG_IMMERSIVE
                );
                invalidate();
            }
        }
    }

    private static class CustomWebViewClient extends WebViewClient {
        public Activity activity;
        public int webviewID;
        public int requestID;
        private WebViewJNI webviewJNI;
        private String PACKAGE_NAME;

        // This is a hack to counter for the case where the load() experiences an error
        // In that case, the onReceivedError will be called, and onPageFinished will be called TWICE
        // This guard variable helps to avoid propagating the onPageFinished calls in that case
        private boolean hasError;

        public CustomWebViewClient(Activity activity, WebViewJNI webviewJNI, int webview_id) {
            super();
            this.activity = activity;
            this.webviewJNI = webviewJNI;
            this.webviewID = webview_id;
            PACKAGE_NAME = activity.getApplicationContext().getPackageName();
            reset(-1);
        }

        public void reset(int request_id)
        {
            this.requestID = request_id;
            this.hasError = false;
        }

        @Override
        public boolean shouldOverrideUrlLoading(WebView view, String url) {
            if( url.startsWith(PACKAGE_NAME) )
            {
                // Try to find an app that can open the url scheme,
                // otherwise continue as usual error.
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setData(Uri.parse(url));
                PackageManager packageManager = activity.getPackageManager();
                if (intent.resolveActivity(packageManager) != null) {
                    activity.startActivity(intent);
                    return true;
                }
            }
            return false;
        }

        @Override
        public void onPageFinished(WebView view, String url) {
            // NOTE! this callback will be called TWICE for errors, see comment above
            if (!this.hasError) {
                webviewJNI.onPageFinished(url, webviewID, requestID);
            }
        }

        @Override
        public void onReceivedError(WebView view, int errorCode, String description, String failingUrl) {
            if (errorCode == WebViewClient.ERROR_UNSUPPORTED_SCHEME) {
                // Try to find an app that can open the url scheme,
                // otherwise continue as usual error.
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setData(Uri.parse(failingUrl));
                PackageManager packageManager = activity.getPackageManager();
                if (intent.resolveActivity(packageManager) != null) {
                    activity.startActivity(intent);
                    return;
                }
            }

            if (!this.hasError) {
                this.hasError = true;
                webviewJNI.onReceivedError(failingUrl, webviewID, requestID, description);
            }
        }

        @JavascriptInterface
        public void returnResultToJava(String result) {
            webviewJNI.onEvalFinished(result, webviewID, requestID);
        }
    }

    private static class CustomWebChromeClient extends WebChromeClient {
        private WebViewJNI webviewJNI;
        private int webviewID;
        private int requestID;

        public CustomWebChromeClient(WebViewJNI webviewJNI, int webview_id) {
            this.webviewJNI = webviewJNI;
            this.webviewID = webview_id;
            reset(-1);
        }

        public void reset(int request_id)
        {
            this.requestID = request_id;
        }

        @Override
        public boolean onConsoleMessage(ConsoleMessage msg) {
            if( msg.messageLevel() == ConsoleMessage.MessageLevel.ERROR )
            {
                webviewJNI.onEvalFailed(String.format("js:%d: %s", msg.lineNumber(), msg.message()), webviewID, requestID);
                return true;
            }
            return false;
        }
    }

    private class WebViewInfo
    {
        CustomWebView               webview;
        CustomWebViewClient         webviewClient;
        CustomWebChromeClient       webviewChromeClient;
        LinearLayout                layout;
        WindowManager.LayoutParams  windowParams;
        int                         first;
        int                         webviewID;
    };

    public WebViewJNI(Activity activity, int maxnumviews) {
        this.activity = activity;
        this.infos = new WebViewInfo[maxnumviews];
    }

    private WebViewInfo createView(Activity activity, int webview_id)
    {
        WebViewInfo info = new WebViewInfo();
        info.webviewID = webview_id;
        info.webview = new CustomWebView(activity, info);
        info.webview.setVisibility(View.GONE);

        if( Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB ) { // Api level 11
            info.webview.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                //| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                //| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION // hide nav bar
                | View.SYSTEM_UI_FLAG_FULLSCREEN // hide status bar
                | View.SYSTEM_UI_FLAG_IMMERSIVE
                );
        }

        MarginLayoutParams params = new MarginLayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT);
        params.setMargins(0, 0, 0, 0);

        LinearLayout layout = new LinearLayout(activity);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.addView(info.webview, params);

        //Fixes 2.3 bug where the keyboard is not being shown when the user focus on an input/textarea.
        info.webview.setFocusable(true);
        info.webview.setFocusableInTouchMode(true);
        info.webview.setClickable(true);
        info.webview.requestFocus(View.FOCUS_DOWN);

        info.webview.setHapticFeedbackEnabled(true);

        info.webview.requestFocusFromTouch();

        info.webviewChromeClient = new CustomWebChromeClient(WebViewJNI.this, webview_id);
        info.webview.setWebChromeClient(info.webviewChromeClient);

        info.webviewClient = new CustomWebViewClient(activity, WebViewJNI.this, webview_id);
        info.webview.setWebViewClient(info.webviewClient);

        WebSettings webSettings = info.webview.getSettings();
        webSettings.setJavaScriptEnabled(true);

        info.webview.addJavascriptInterface(info.webviewClient, JS_NAMESPACE);

        info.layout = layout;
        info.first = 1;

        info.windowParams = new WindowManager.LayoutParams();
        info.windowParams.gravity = Gravity.TOP | Gravity.LEFT;
        info.windowParams.x = WindowManager.LayoutParams.MATCH_PARENT;
        info.windowParams.y = WindowManager.LayoutParams.MATCH_PARENT;
        info.windowParams.width = WindowManager.LayoutParams.MATCH_PARENT;
        info.windowParams.height = WindowManager.LayoutParams.MATCH_PARENT;
        info.windowParams.flags = WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;

        info.layout.setLayoutParams(info.windowParams);
        return info;
    }

    private void setVisibleInternal(WebViewInfo info, int visible)
    {
        info.webview.setVisibility((visible != 0) ? View.VISIBLE : View.GONE);
        if( visible != 0 && info.first == 1 )
        {
            info.first = 0;
            WindowManager wm = activity.getWindowManager();
            wm.addView(info.layout, info.windowParams);
        }
    }

    private void setPositionInternal(WebViewInfo info, int x, int y, int width, int height)
    {
        info.windowParams.x = x;
        info.windowParams.y = y;
        info.windowParams.width = width >= 0 ? width : WindowManager.LayoutParams.MATCH_PARENT;
        info.windowParams.height = height >= 0 ? height : WindowManager.LayoutParams.MATCH_PARENT;

        if (info.webview.getVisibility() == View.VISIBLE ) {
            WindowManager wm = activity.getWindowManager();
            wm.updateViewLayout(info.layout, info.windowParams);
        }
    }

    public void create(final int webview_id) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebViewJNI.this.infos[webview_id] = createView(WebViewJNI.this.activity, webview_id);
            }
        });
    }

    public void destroy(final int webview_id) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if( WebViewJNI.this.infos[webview_id] != null )
                {
                    if( WebViewJNI.this.infos[webview_id].layout != null )
                    {
                        WindowManager wm = activity.getWindowManager();
                        wm.removeView(WebViewJNI.this.infos[webview_id].layout);
                    }
                    WebViewJNI.this.infos[webview_id].layout = null;
                    WebViewJNI.this.infos[webview_id].webview = null;
                    WebViewJNI.this.infos[webview_id] = null;
                }
            }
        });
    }

    public void loadRaw(final String html, final int webview_id, final int request_id, final int hidden) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebViewJNI.this.infos[webview_id].webviewClient.reset(request_id);
                WebViewJNI.this.infos[webview_id].webviewChromeClient.reset(request_id);
                WebViewJNI.this.infos[webview_id].webview.loadDataWithBaseURL("file:///android_res/", html, "text/html", "utf-8", null);
                setVisibleInternal(WebViewJNI.this.infos[webview_id], hidden != 0 ? 0 : 1);
            }
        });
    }

    public void load(final String url, final int webview_id, final int request_id, final int hidden) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebViewJNI.this.infos[webview_id].webviewClient.reset(request_id);
                WebViewJNI.this.infos[webview_id].webviewChromeClient.reset(request_id);
                WebViewJNI.this.infos[webview_id].webview.loadUrl(url);
                setVisibleInternal(WebViewJNI.this.infos[webview_id], hidden != 0 ? 0 : 1);
            }
        });
    }

    public void eval(final String code, final int webview_id, final int request_id) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebViewJNI.this.infos[webview_id].webviewClient.reset(request_id);
                WebViewJNI.this.infos[webview_id].webviewChromeClient.reset(request_id);
                String javascript = String.format("javascript:%s.returnResultToJava(eval(\"%s\"))", JS_NAMESPACE, code);
                WebViewJNI.this.infos[webview_id].webview.loadUrl(javascript);
            }
        });
    }

    public void setVisible(final int webview_id, final int visible) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                setVisibleInternal(WebViewJNI.this.infos[webview_id], visible);
            }
        });
    }

    public int isVisible(final int webview_id) {
        int visible = this.infos[webview_id].webview.isShown() ? 1 : 0;
        return visible;
    }

    public void setPosition(final int webview_id, final int x, final int y, final int width, final int height) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                setPositionInternal(WebViewJNI.this.infos[webview_id], x, y, width, height);
            }
        });
    }
}
