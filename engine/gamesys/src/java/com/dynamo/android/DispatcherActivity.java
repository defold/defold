package com.dynamo.android;

import android.app.Activity;
import java.lang.Thread;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

public class DispatcherActivity extends Activity {

    private static final String TAG = "DispatcherActivity";

    public static final String EXTRA_CLASS_NAME = "class_name";

    public interface PseudoActivity {
        void onCreate(Bundle savedInstanceState, Activity parent);
        void onDestroy();

        void onActivityResult(int requestCode, int resultCode, Intent data);
    }

    private PseudoActivity activity;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.v("thread", "disp onCreate thread: " + Thread.currentThread().toString());
        try {
            Class<?> c = (Class<?>) Class.forName(this.getIntent().getStringExtra(EXTRA_CLASS_NAME));
            activity = (PseudoActivity) c.getConstructor().newInstance();
            activity.onCreate(savedInstanceState, this);
        } catch (Exception e) {
            Log.wtf(TAG, e);
        }
    }

    @Override
    protected void onRestart() {
        super.onRestart();
        Log.v(TAG, "onRestart");
    }

    @Override
    protected void onStart() {
        super.onStart();
        Log.v(TAG, "onStart");
    }

    @Override
    protected void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        Log.v(TAG, "onRestoreInstanceState");
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        Log.v(TAG, "onSaveInstanceState");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (this.activity != null) {
            this.activity.onDestroy();
        }
        Log.v(TAG, "onDestroy");
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.v(TAG, "onPause");
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.v(TAG, "onResume");
    }

    @Override
    protected void onStop() {
        super.onStop();
        Log.v(TAG, "onStop");
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (this.activity != null) {
            this.activity.onActivityResult(requestCode, resultCode, data);
        }
    }
}
