package com.defold.webview;

import android.app.Activity;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.Point;
import android.graphics.drawable.ColorDrawable;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.view.KeyEvent;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.view.Display;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnectionWrapper;
import android.view.inputmethod.InputConnection;
import android.text.InputType;
import android.view.inputmethod.CompletionInfo;
import android.view.ViewGroup;

import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import com.dynamo.android.DispatcherActivity.PseudoActivity;



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



public class WebViewActivity extends Activity {

    public static final String TAG = "defold.webview.activity";
    public static final String JS_NAMESPACE = "defold";
    public static final int MAX_NUM_WEBVIEWS = 1;

    // This is a hack to counter for the case where the load() experiences an error
    // In that case, the onReceivedError will be called, and onPageFinished will be called TWICE
    // This guard variable helps to avoid propagating the onPageFinished calls in that case
    private boolean hasError;


    public native void onPageFinished(String url, int webview_id, int request_id);
    public native void onReceivedError(String url, int webview_id, int request_id, String errorMessage);
    public native void onEvalFinished(String result, int webview_id, int request_id);

    private class WebViewInfo
    {
        PopupWindow             popup;
        CustomWebView           webview;
        CustomWebViewClient     webviewClient;
    };

    private static WebViewInfo[] infos;

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
        private String PACKAGE_NAME;

        // This is a hack to counter for the case where the load() experiences an error
        // In that case, the onReceivedError will be called, and onPageFinished will be called TWICE
        // This guard variable helps to avoid propagating the onPageFinished calls in that case
        private boolean hasError;

        public CustomWebViewClient(Activity activity, int webview_id) {
            super();
            this.activity = activity;
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
                WebViewActivity wva = (WebViewActivity)activity;
                wva.onPageFinished(url, webviewID, requestID);
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
                WebViewActivity wva = (WebViewActivity)activity;
                wva.onReceivedError(failingUrl, webviewID, requestID, description);
            }
        }

        @JavascriptInterface
        public void returnResultToJava(String result) {
            WebViewActivity wva = (WebViewActivity)activity;
            wva.onEvalFinished(result, webviewID, requestID);
        }
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

        //info.webview.setVisibility(View.GONE);
        info.webview.setVisibility(View.VISIBLE);

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

        info.webviewClient = new CustomWebViewClient(activity, webview_id);
        info.webview.setWebViewClient(info.webviewClient);

        WebSettings webSettings = info.webview.getSettings();
        webSettings.setJavaScriptEnabled(true);

        info.webview.addJavascriptInterface(info.webviewClient, JS_NAMESPACE);  

        info.popup.showAtLocation(mainLayout, Gravity.BOTTOM, 0, 0);
        info.popup.update();

        info.webview.loadUrl("http://www.w3schools.com/tags/tryit.asp?filename=tryhtml_textarea");

        return info;
    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.d(TAG, "onCreate");

        this.infos = new WebViewInfo[MAX_NUM_WEBVIEWS]; // Matches the constant in webview_common.h
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy");
    }


    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.d(TAG, "onActivityResult");
        super.onActivityResult(requestCode, resultCode, data);
        Bundle bundle = null;
        if (data != null) {
            String action = data.getAction();
            int webview_id = (int)data.getLongExtra("webview_id", -1);

            if( action == "load" ) {
                String url = data.getStringExtra("url");
                int request_id = (int)data.getLongExtra("request_id", -1);
                int hidden = (int)data.getLongExtra("hidden", -1);

                load(url, webview_id, request_id, hidden);
            }
            else if(action == "loadRaw") {
                String html = data.getStringExtra("html");
                int request_id = (int)data.getLongExtra("request_id", -1);
                int hidden = (int)data.getLongExtra("hidden", -1);
                loadRaw(html, webview_id, request_id, hidden);
            }
            else if(action == "eval") {
                String code = data.getStringExtra("code");
                int request_id = (int)data.getLongExtra("request_id", -1);
                eval(code, webview_id, request_id);
            }
            else if(action == "setVisible") {
                int visible = (int)data.getLongExtra("visible", -1);
                setVisible(webview_id, visible);
            }
            else if(action == "create") {
                create(webview_id);
            }
            else if(action == "destroy") {
                destroy(webview_id);
            }

        }

        /*
        // Send message if generated above
        if (bundle != null) {
            Message msg = new Message();
            msg.setData(bundle);
            try {
                messenger.send(msg);
            } catch (RemoteException e) {
                Log.wtf(IapGooglePlay.TAG, "Unable to send message", e);
            }
        }*/

        //this.finish();
    }

    public void create(final int webview_id) {
        final Activity activity = this;
        Log.d(TAG, "create NEW");
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebViewActivity.this.infos[webview_id] = createView(activity, webview_id);
            }
        });
    }

    public void destroy(final int webview_id) {
        Log.d(TAG, "destroy NEW");
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if( WebViewActivity.this.infos[webview_id] != null )                    
                {
                    WebViewActivity.this.infos[webview_id].popup.dismiss();
                    WebViewActivity.this.infos[webview_id].webview = null;
                    WebViewActivity.this.infos[webview_id].popup = null;
                    WebViewActivity.this.infos[webview_id] = null;
                }
            }
        });
    }

    public void loadRaw(final String html, final int webview_id, final int request_id, final int hidden) {
        Log.d(TAG, "loadRaw NEW");
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebViewActivity.this.infos[webview_id].webviewClient.reset(request_id);
                WebViewActivity.this.infos[webview_id].webview.setVisibility((hidden != 0) ? View.GONE : View.VISIBLE);
                WebViewActivity.this.infos[webview_id].webview.loadDataWithBaseURL("file:///android_res/", html, "text/html", "utf-8", null);
            }
        });
    }

    public void load(final String url, final int webview_id, final int request_id, final int hidden) {
        Log.d(TAG, "load NEW");
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebViewActivity.this.infos[webview_id].webviewClient.reset(request_id);
                WebViewActivity.this.infos[webview_id].webview.setVisibility((hidden != 0) ? View.GONE : View.VISIBLE);
                WebViewActivity.this.infos[webview_id].webview.loadUrl(url);
            }
        });
    }

    public void eval(final String code, final int webview_id, final int request_id) {
        Log.d(TAG, "eval NEW");
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebViewActivity.this.infos[webview_id].webviewClient.reset(request_id);
                String javascript = String.format("javascript:%s.returnResultToJava(eval(\"%s\"))", JS_NAMESPACE, code);
                WebViewActivity.this.infos[webview_id].webview.loadUrl(javascript);
            }
        });
    }

    public void setVisible(final int webview_id, final int visible) {
        Log.d(TAG, "setVisible NEW");
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebViewActivity.this.infos[webview_id].webview.setVisibility((visible != 0) ? View.VISIBLE : View.GONE);
            }
        });  
    }

    public int isVisible(final int webview_id) {
        int visible = this.infos[webview_id].webview.isShown() ? 1 : 0;
        return visible;
    }


}
