
package com.dynamo.android;

import android.app.NativeActivity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.IBinder;
import android.view.KeyEvent;
import android.view.inputmethod.InputMethodManager;
import android.util.Log;

public class DefoldActivity extends NativeActivity {

    private InputMethodManager imm = null;

    private static final String TAG = "DefoldActivity";

    /**
     * NOTE! This method only exists because of a known bug in the NDK where the KeyEvent characters are
     * not copied over to the corresponding native AInputEvent.
     * Therefore it is implemented in android_window.c so that the characters can be sent to glfw.
     */
    public native void glfwInputCharNative(int unicode);

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        imm = (InputMethodManager)DefoldActivity.this.getSystemService(Context.INPUT_METHOD_SERVICE);

        try {
            ActivityInfo ai = getPackageManager().getActivityInfo(getIntent().getComponent(),
                    PackageManager.GET_META_DATA);

            if (ai.metaData != null) {
                String ln = ai.metaData.getString(META_DATA_LIB_NAME);
                if (ln != null) {
                    System.loadLibrary(ln);
                }
            }
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException("Error getting activity info", e);
        }
    }

    /**
     * See comment above (glfwInputCharNative) why this is needed.
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_MULTIPLE && event.getKeyCode() == KeyEvent.KEYCODE_UNKNOWN) {
            String chars = event.getCharacters();
            int charCount = chars.length();
            for (int i = 0; i < charCount; ++i) {
                int c = chars.codePointAt(i);
                glfwInputCharNative(c);
            }
        }
        return super.dispatchKeyEvent(event);
    }

    /** show virtual keyboard
     * Implemented here to ensure that calls from native code are delayed and run on the UI thread.
     */
    public void showSoftInput() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                imm.showSoftInput(getWindow().getDecorView(), InputMethodManager.SHOW_FORCED);
            }
        });
    }

    /** hide virtual keyboard
     * Implemented here to ensure that calls from native code are delayed and run on the UI thread.
     */
    public void hideSoftInput() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                IBinder windowToken = getWindow().getDecorView().getWindowToken();
                imm.hideSoftInputFromWindow(windowToken, 0);
            }
        });
    }
}
