package com.defold.webview;

import android.app.Activity;

import android.content.Context;
import android.content.res.Resources;
import android.content.SharedPreferences;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Point;
import android.net.Uri;
import android.os.Build;
import android.view.Display;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import com.dynamo.android.DispatcherActivity;

import android.util.Log;

// test
import android.widget.PopupWindow;


public class WebViewJNI {

    public static final String PREFERENCES_FILE = "webview";
    public static final String TAG = "defold.webview";

    private Activity activity;

    public WebViewJNI(Activity activity) {
        this.activity = activity;
    }

    public void create(final int webview_id) {
        Log.d(TAG, "create NEW");
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(WebViewJNI.this.activity, WebViewActivity.class);
                intent.putExtra("webview_id", webview_id);
                intent.setAction("create");
                activity.startActivity(intent);
            }
        });
    }

    public void destroy(final int webview_id) {
        Log.d(TAG, "destroy NEW");
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(WebViewJNI.this.activity, WebViewActivity.class);
                intent.putExtra("webview_id", webview_id);
                intent.setAction("destroy");
                activity.startActivity(intent);
            }
        });
    }

    public void loadRaw(final String html, final int webview_id, final int request_id, final int hidden) {
        Log.d(TAG, "loadRaw NEW");
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(WebViewJNI.this.activity, WebViewActivity.class);
                intent.putExtra("html", html);
                intent.putExtra("webview_id", webview_id);
                intent.putExtra("request_id", request_id);
                intent.putExtra("hidden", hidden);
                intent.setAction("loadRaw");
                activity.startActivity(intent);
            }
        });
    }

    public void load(final String url, final int webview_id, final int request_id, final int hidden) {
        Log.d(TAG, "load NEW");
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(WebViewJNI.this.activity, WebViewActivity.class);
                intent.putExtra("url", url);
                intent.putExtra("webview_id", webview_id);
                intent.putExtra("request_id", request_id);
                intent.putExtra("hidden", hidden);
                intent.setAction("load");
                activity.startActivity(intent);
            }
        });
    }

    public void eval(final String code, final int webview_id, final int request_id) {
        Log.d(TAG, "eval NEW");
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(WebViewJNI.this.activity, WebViewActivity.class);
                intent.putExtra("code", code);
                intent.putExtra("webview_id", webview_id);
                intent.putExtra("request_id", request_id);
                intent.setAction("eval");
                activity.startActivity(intent);
            }
        });
    }

    public void setVisible(final int webview_id, final int visible) {
        Log.d(TAG, "setVisible NEW");
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(WebViewJNI.this.activity, WebViewActivity.class);
                intent.putExtra("webview_id", webview_id);
                intent.putExtra("visible", visible);
                intent.setAction("setVisible");
                activity.startActivity(intent);
            }
        });  
    }

    public int isVisible(final int webview_id) {
        //int visible = this.infos[webview_id].webview.isShown() ? 1 : 0;
        //return visible;
        return 0;
    }

}
