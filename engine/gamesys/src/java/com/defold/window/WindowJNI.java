package com.defold.window;

import android.app.Activity;
import android.view.WindowManager;

public class WindowJNI {

    private final Activity activity;

    public WindowJNI(Activity activity) {
        this.activity = activity;
    }

    public void setSleepMode(boolean normal_sleep) {
        if (normal_sleep) {
            this.activity.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    activity.getWindow().clearFlags(
                        android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                }
            });
        } else {
            this.activity.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    activity.getWindow().addFlags(
                        android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                }
            });
        }
    }

    public boolean getSleepMode() {
        return ((this.activity.getWindow().getAttributes().flags
            & android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) != 0);
    }

}