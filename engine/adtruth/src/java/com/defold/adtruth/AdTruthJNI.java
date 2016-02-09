package com.defold.adtruth;

import android.app.Activity;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.Intent;
import android.content.pm.PackageManager;

import android.net.Uri;

import android.webkit.WebView;
import android.webkit.WebViewClient;

import android.util.Log;

public class AdTruthJNI {

    public static final String PREFERENCES_FILE = "adtruth";
    public static final String TAG = "defold.adtruth";

    private WebView view;
    private Activity activity;
    // This is a hack to counter for the case where the load() experiences an error
    // In that case, the onReceivedError will be called, and onPageFinished will be called TWICE
    // This guard variable helps to avoid propagating the onPageFinished calls in that case
    private boolean hasError;

    public AdTruthJNI(Activity activity) {
        this.view = null;
        this.activity = activity;
        this.hasError = false;
    }

    public native void onPageFinished();

    public native void onReceivedError(String errorMessage);

    public void load(final String url) {
        Log.d(TAG, "load");
        this.hasError = false;
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (AdTruthJNI.this.view == null) {
                    AdTruthJNI.this.view = new WebView(AdTruthJNI.this.activity);
                    AdTruthJNI.this.view.getSettings().setJavaScriptEnabled(true);
                    AdTruthJNI.this.view.setWebViewClient(new WebViewClient() {

                        @Override
                        public boolean shouldOverrideUrlLoading(WebView view, String url) {
                            return false;
                        }

                        @Override
                        public void onPageFinished(WebView view, String url) {
                            // NOTE! this callback will be called TWICE for errors, see comment above
                            if (!AdTruthJNI.this.hasError) {
                                Log.d(TAG, "onPageFinished: " + url);
                                AdTruthJNI.this.onPageFinished();
                            }
                        }

                        @Override
                        public void onReceivedError(WebView view, int errorCode,
                                String description, String failingUrl) {

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

                            if (!AdTruthJNI.this.hasError) {
                                AdTruthJNI.this.hasError = true;
                                Log.d(TAG, "onReceivedError: " + failingUrl);
                                AdTruthJNI.this.onReceivedError(description);
                            }
                        }
                    });
                }

                AdTruthJNI.this.view.loadUrl(url);
            }
        });
    }

    public String getReferrer() {
        SharedPreferences prefs = this.activity.getSharedPreferences(PREFERENCES_FILE, Context.MODE_PRIVATE);
        return prefs.getString("referrer", null);
    }

}
