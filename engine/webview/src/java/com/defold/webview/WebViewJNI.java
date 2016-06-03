package com.defold.webview;

import android.app.Activity;

import android.content.Context;
import android.content.res.Resources;
import android.content.SharedPreferences;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.drawable.ColorDrawable;
import android.graphics.Point;
import android.net.Uri;
import android.os.Build;
import android.view.Display;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
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

    private static class CustomWebView extends WebView {
        public CustomWebView(Context context) {
            super(context);
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
                break;
            }
            return super.onTouchEvent(event);
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

    private class WebViewInfo
    {
        PopupWindow             popup;
        CustomWebView           webview;
        CustomWebViewClient     webviewClient;
    };

    public WebViewJNI(Activity activity, int maxnumviews) {
        this.activity = activity;
        this.infos = new WebViewInfo[maxnumviews];
    }

    private WebViewInfo createView(Activity activity, int webview_id)
    {
        WebViewInfo info = new WebViewInfo();
        info.webview = new CustomWebView(activity);
        info.popup = new PopupWindow(activity);
        info.popup.setWidth(WindowManager.LayoutParams.FILL_PARENT);
        info.popup.setHeight(WindowManager.LayoutParams.FILL_PARENT);
        info.popup.setWindowLayoutMode(LayoutParams.FILL_PARENT, LayoutParams.FILL_PARENT);
        info.popup.setClippingEnabled(false);
        info.popup.setAnimationStyle(0);
        info.popup.setBackgroundDrawable(new ColorDrawable(android.graphics.Color.TRANSPARENT));
        info.popup.setFocusable(true); // allows for keyboard to show

        info.webview.setVisibility(View.GONE);

        MarginLayoutParams params = new MarginLayoutParams(LinearLayout.LayoutParams.FILL_PARENT, LinearLayout.LayoutParams.FILL_PARENT);
        params.setMargins(0, 0, 0, 0);

        LinearLayout mainLayout = new LinearLayout(activity);
        LinearLayout layout = new LinearLayout(activity);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.addView(info.webview, params);

        info.popup.setContentView(layout);
        activity.setContentView(mainLayout, params);
        
        //Fixes 2.3 bug where the keyboard is not being shown when the user focus on an input/textarea.
        info.webview.setFocusable(true);
        info.webview.setFocusableInTouchMode(true);
        info.webview.setClickable(true);
        info.webview.requestFocus(View.FOCUS_DOWN);

        info.webview.setHapticFeedbackEnabled(true);

        info.webview.requestFocusFromTouch();

        info.webview.setWebChromeClient(new WebChromeClient());

        info.webviewClient = new CustomWebViewClient(activity, WebViewJNI.this, webview_id);
        info.webview.setWebViewClient(info.webviewClient);

        WebSettings webSettings = info.webview.getSettings();
        webSettings.setJavaScriptEnabled(true);

        info.webview.addJavascriptInterface(info.webviewClient, JS_NAMESPACE);  

        info.popup.showAtLocation(mainLayout, Gravity.BOTTOM, 0, 0);
        info.popup.update();

        return info;
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
                    WebViewJNI.this.infos[webview_id].popup.dismiss();
                    WebViewJNI.this.infos[webview_id].webview = null;
                    WebViewJNI.this.infos[webview_id].popup = null;
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
                WebViewJNI.this.infos[webview_id].webview.setVisibility((hidden != 0) ? View.GONE : View.VISIBLE);
                WebViewJNI.this.infos[webview_id].webview.loadDataWithBaseURL("file:///android_res/", html, "text/html", "utf-8", null);
            }
        });
    }

    public void load(final String url, final int webview_id, final int request_id, final int hidden) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebViewJNI.this.infos[webview_id].webviewClient.reset(request_id);
                WebViewJNI.this.infos[webview_id].webview.setVisibility((hidden != 0) ? View.GONE : View.VISIBLE);
                WebViewJNI.this.infos[webview_id].webview.loadUrl(url);
            }
        });
    }

    public void eval(final String code, final int webview_id, final int request_id) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebViewJNI.this.infos[webview_id].webviewClient.reset(request_id);
                String javascript = String.format("javascript:%s.returnResultToJava(eval(\"%s\"))", JS_NAMESPACE, code);
                WebViewJNI.this.infos[webview_id].webview.loadUrl(javascript);
            }
        });
    }

    public void setVisible(final int webview_id, final int visible) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebViewJNI.this.infos[webview_id].webview.setVisibility((visible != 0) ? View.VISIBLE : View.GONE);
            }
        });  
    }

    public int isVisible(final int webview_id) {
        int visible = this.infos[webview_id].webview.isShown() ? 1 : 0;
        return visible;
    }
}
