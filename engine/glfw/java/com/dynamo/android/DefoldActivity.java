
package com.dynamo.android;

import android.app.Activity;
import android.app.NativeActivity;
import android.content.res.Configuration;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Bundle;
import android.os.Build;
import android.os.IBinder;
import android.view.KeyEvent;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnectionWrapper;
import android.view.inputmethod.InputConnection;
import android.text.InputType;
import android.view.inputmethod.CompletionInfo;
import android.view.ViewGroup;

import com.google.android.gms.ads.identifier.AdvertisingIdClient;
import com.google.android.gms.common.*;

public class DefoldActivity extends NativeActivity {

    // Must match values from sys.h
    private enum NetworkConnectivity {
        NETWORK_DISCONNECTED       (0),
        NETWORK_CONNECTED          (1),
        NETWORK_CONNECTED_CELLULAR (2);
        private final int value;
        private NetworkConnectivity(int value) { this.value = value; };
        public int getValue() { return this.value; };
    }


    // Must match values from glfw.h
    public enum GLFWKeyboardType {
        GLFW_KEYBOARD_DEFAULT    (0),
        GLFW_KEYBOARD_NUMBER_PAD (1),
        GLFW_KEYBOARD_EMAIL      (2),
        GLFW_KEYBOARD_PASSWORD   (3);
        private final int value;
        private GLFWKeyboardType(int value) { this.value = value; };
        public int getValue() { return this.value; };
    }

    private InputMethodManager imm = null;
    private EditText mTextEdit = null;
    private boolean mUseHiddenInputField = false;

    private boolean mImmersiveMode = false;

    /**
     * Update immersive sticky mode based on current setting. This will only
     * be done if Android OS version is equal to or above KitKat
     * https://developer.android.com/training/system-ui/immersive.html
     */
    private void updateImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            if (mImmersiveMode) {
                getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
            }
        }
    }

    private static boolean activityVisible;
    public static boolean isActivityVisible() {
        return activityVisible;
    }

    private static final String TAG = "DefoldActivity";

    /**
     * NOTE! This method only exists because of a known bug in the NDK where the KeyEvent characters are
     * not copied over to the corresponding native AInputEvent.
     * Therefore it is implemented in android_window.c so that the characters can be sent to glfw.
     */
    public native void FakeBackspace();
    public native void glfwInputCharNative(int unicode);
    public native void glfwSetMarkedTextNative(String text);

    private class DefoldInputWrapper extends InputConnectionWrapper {
        private DefoldActivity _ctx;
        private String mComposingText = "";

        public DefoldInputWrapper(DefoldActivity ctx, InputConnection target, boolean mutable) {
            super(target, mutable);
            _ctx = ctx;
            mComposingText = "";
        }

        @Override
        public CharSequence getTextBeforeCursor(int n, int flags)
        {
            // Hacky Android: Need to trick the input system into thinking
            // there is at least one char to delete, otherwise we wont
            // be getting any backspace events if the EditText is empty.
            return " ";
        }

        @Override
        public boolean setComposingText(CharSequence text, int newCursorPosition)
        {
            mComposingText = text.toString();
            _ctx.sendMarkedText(mComposingText);

            return super.setComposingText(text, newCursorPosition);
        }

        @Override
        public boolean commitCompletion (CompletionInfo text)
        {
            return super.commitCompletion(text);
        }

        @Override
        public boolean finishComposingText ()
        {
            _ctx.sendInputText(new String(mComposingText));
            _ctx.sendMarkedText("");

            // If we are finished composing, clear the composing text
            if (mComposingText.length() > 0) {
                this.setComposingText("", 0);
            }
            mComposingText = "";

            return super.finishComposingText();
        }

        @Override
        public boolean commitText(CharSequence text, int newCursorPosition)
        {
            _ctx.sendInputText(text.toString());

            // When committing new text, we assume the composing text should be cleared.
            mComposingText = "";
            _ctx.clearComposingText();

            return super.commitText(text, newCursorPosition);
        }

        @Override
        public boolean sendKeyEvent(KeyEvent event) {
            return super.sendKeyEvent(event);
        }


        @Override
        public boolean deleteSurroundingText(int beforeLength, int afterLength) {
            _ctx.clearText();
            _ctx.clearComposingText();
            _ctx.FakeBackspace();

            return super.deleteSurroundingText(beforeLength, afterLength);
        }

        }

    public synchronized void sendInputText(final String text) {
        int charCount = text.length();
        for (int i = 0; i < charCount; ++i) {
            int c = text.codePointAt(i);
            glfwInputCharNative(c);
        }

        sendMarkedText("");
    }

    public synchronized void sendMarkedText(final String text) {
        glfwSetMarkedTextNative(text);
    }

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final DefoldActivity self = this;

        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
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

        new Thread(new Runnable() {
            public void run() {
                try {
                    AdvertisingIdClient.Info info = AdvertisingIdClient.getAdvertisingIdInfo(self);
                    self.setAdInfo(info.getId(), info.isLimitAdTrackingEnabled());
                } catch (java.io.IOException e) {
                    self.setAdInfo("", false);
                } catch (GooglePlayServicesNotAvailableException e) {
                    self.setAdInfo("", false);
                } catch (GooglePlayServicesRepairableException e) {
                    self.setAdInfo("", false);
                }
            }
        }).start();
    }

    @Override
    protected void onPause() {
        super.onPause();
        activityVisible = false;
    }

    @Override
    protected void onResume() {
        super.onResume();
        activityVisible = true;
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            updateImmersiveMode();
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (newConfig.keyboardHidden == Configuration.KEYBOARDHIDDEN_YES) {
            updateImmersiveMode();
        }
    }

    private Object m_AdGuard = new Object();
    private boolean m_GotAdInfo = false;
    private String m_AdId;
    private boolean m_LimitAdTracking;


    public void setAdInfo(String adId, boolean limitAdTracking)
    {
        synchronized(m_AdGuard)
        {
            m_AdId = adId;
            m_LimitAdTracking = limitAdTracking;
            m_GotAdInfo = true;
        }
    }

    /**
     * Return null for does not exist yet, blank string if completed but unable to get id.
     */
    public String getAdId()
    {
        synchronized(m_AdGuard)
        {
            return m_AdId;
        }
    }

    public boolean getLimitAdTracking()
    {
        synchronized(m_AdGuard)
        {
            return m_LimitAdTracking;
        }
    }

    public boolean isStartupDone()
    {
        synchronized(m_AdGuard)
        {
            return m_GotAdInfo;
        }
    }

    public int getConnectivity() {

        ConnectivityManager connectivity_manager = (ConnectivityManager)DefoldActivity.this.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo active_network = connectivity_manager.getActiveNetworkInfo();

        if (active_network != null) {
            if (active_network.isConnected()) {
                if (ConnectivityManager.TYPE_MOBILE == active_network.getType()) {
                    return NetworkConnectivity.NETWORK_CONNECTED_CELLULAR.getValue();
                }
                return NetworkConnectivity.NETWORK_CONNECTED.getValue();
            }
        }

        return NetworkConnectivity.NETWORK_DISCONNECTED.getValue();
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
    public void showSoftInput(final int keyboardType) {
        final DefoldActivity self = this;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {

                if (mTextEdit != null) {
                    mTextEdit.setVisibility(View.GONE);
                    ViewGroup vg = (ViewGroup)(mTextEdit.getParent());
                    vg.removeView(mTextEdit);
                    mTextEdit = null;
                }

                if (mUseHiddenInputField) {

                    // Convert GLFW input enum into EditText enum
                    final int input_type;
                    if (keyboardType == GLFWKeyboardType.GLFW_KEYBOARD_PASSWORD.getValue()) {
                        input_type = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
                    } else if (keyboardType == GLFWKeyboardType.GLFW_KEYBOARD_EMAIL.getValue()) {
                        input_type = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
                    } else if (keyboardType == GLFWKeyboardType.GLFW_KEYBOARD_NUMBER_PAD.getValue()) {
                        input_type = InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_VARIATION_NORMAL;
                    } else {
                        input_type = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_NORMAL | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
                    }

                    mTextEdit = new EditText(self) {
                        @Override
                        public InputConnection onCreateInputConnection(EditorInfo outAttrs) {

                            // We set manual input connection to catch all keyboard interaction
                            InputConnection inpc = super.onCreateInputConnection(outAttrs);
                            outAttrs.inputType = input_type;
                            return new DefoldInputWrapper(self, inpc, true);

                        }

                        @Override
                        public boolean onCheckIsTextEditor() {
                            return true;
                        }
                    };


                    // Disable the fullscreen keyboard mode (present in landscape)
                    // If we don't do this, we get a large grey input box above the keyboard
                    // blocking the game view.
                    mTextEdit.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI);

                    FrameLayout.LayoutParams mEditTextLayoutParams = new FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT);
                    mEditTextLayoutParams.gravity = Gravity.TOP;
                    mEditTextLayoutParams.setMargins(0, 0, 0, 0);
                    mTextEdit.setLayoutParams(mEditTextLayoutParams);
                    mTextEdit.setVisibility(View.VISIBLE);
                    mTextEdit.setInputType(input_type);

                    // Start on an empty slate each time we bring up the keyboard
                    mTextEdit.setText("");

                    getWindow().addContentView(mTextEdit, mEditTextLayoutParams);

                    mTextEdit.bringToFront();
                    mTextEdit.setSelection(0);
                    mTextEdit.requestFocus();

                    imm.showSoftInput(mTextEdit, InputMethodManager.SHOW_FORCED);
                } else {

                    // "Old style" key event inputs
                    imm.showSoftInput(getWindow().getDecorView(), InputMethodManager.SHOW_FORCED);

                }
            }
        });
    }

    /**
     * Clears the internal text field if hidden input method is used.
     */
    public void clearText() {
        final DefoldActivity self = this;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mTextEdit != null) {
                    mTextEdit.setText("");
                }
            }
        });
    }

    /**
     * Clears the composing part of the text field if hidden input method is used.
     */
    public void clearComposingText() {
        final DefoldActivity self = this;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mTextEdit != null) {
                    mTextEdit.clearComposingText();
                }
            }
        });
    }

    /**
     * Clears both internal text and composing part of text field if hidden input method is used.
     */
    public void resetSoftInput() {
        final DefoldActivity self = this;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mTextEdit != null) {
                    mTextEdit.setText("");
                    mTextEdit.clearComposingText();
                }
            }
        });
    }

    /**
     * Method to switch between "old" (key event) or "new" (hidden text input) method.
     * Called from C (android_window.c).
     */
    public void setUseHiddenInputField(boolean useHiddenInputField) {
        mUseHiddenInputField = useHiddenInputField;
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
                updateImmersiveMode();
            }
        });
    }

    /**
     * Method to enable/disable immersive mode
     * Called from C (android_window.c)
     * @param immersiveMode
     */
    public void setImmersiveMode(final boolean immersiveMode) {
        mImmersiveMode = immersiveMode;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                updateImmersiveMode();
            }
        });
    }

    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        nativeOnActivityResult(this, requestCode,resultCode, data);
    }
    
    public static native void nativeOnActivityResult(Activity activity, int requestCode, int resultCode, Intent data);
}
