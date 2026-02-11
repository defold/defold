// Copyright 2020-2023 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.


package com.dynamo.android;

import android.app.Activity;
import android.app.NativeActivity;
import android.content.pm.ApplicationInfo;
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
import android.widget.TextView;
import android.widget.FrameLayout;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.view.DisplayCutout;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnectionWrapper;
import android.view.inputmethod.InputConnection;
import android.text.InputType;
import android.view.inputmethod.CompletionInfo;
import android.view.ViewGroup;
import android.view.InputDevice;

import java.util.ArrayList;

import android.content.pm.PackageInfo;

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
    private boolean mDisplayCutout = false;

    private ArrayList<Integer> mGameControllerDeviceIds = new ArrayList<Integer>();

    private void updateFullscreenMode() {
        final Window window = getWindow();
        if (mImmersiveMode) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                window.setDecorFitsSystemWindows(false);

                WindowInsetsController controller = window.getInsetsController();
                if (controller != null) {
                    controller.hide(WindowInsets.Type.systemBars());
                    controller.setSystemBarsBehavior(
                            WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
                    );
                }
            } else {
                @SuppressWarnings("deprecation")
                View decorView = window.getDecorView();
                @SuppressWarnings("deprecation")
                int flags =
                        View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

                decorView.setSystemUiVisibility(flags);
            }
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            if (mDisplayCutout) {
                WindowManager.LayoutParams layoutParams = getWindow().getAttributes();
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    // Android 11+ also has ALWAYS, and on Android 15 SHORT_EDGES is treated like ALWAYS for non-floating windows
                    layoutParams.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
                } else {
                    // Android 9-10
                    layoutParams.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
                }
                getWindow().setAttributes(layoutParams);
            }
        }
    }

    public boolean isAppInstalled(String appPackageName) {
        PackageManager packageManager = this.getPackageManager();
        try {
            PackageInfo info = packageManager.getPackageInfo(appPackageName, 0);
            return info != null;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    private static final String TAG = "DefoldActivity";

    /**
     * NOTE! This method only exists because of a known bug in the NDK where the KeyEvent characters are
     * not copied over to the corresponding native AInputEvent.
     * Therefore it is implemented in android_window.c so that the characters can be sent to glfw.
     */
    public native void FakeBackspace();
    public native void FakeEnter();
    public native void glfwInputBackButton();
    public native void glfwInputCharNative(int unicode);
    public native void glfwSetMarkedTextNative(String text);

    private boolean keyboardActive;

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

    public static native void nativeOnCreate(Activity activity);
    public static native void glfwSetPendingResizeBecauseOfInsets();

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

        // Starting API 33 old implementation of the Back button doesn't work
        // https://github.com/defold/defold/issues/6821
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                OnBackInvokedDispatcher.PRIORITY_DEFAULT, new OnBackInvokedCallback() {
                @Override
                public void onBackInvoked() {
                    if (keyboardActive) {
                        hideSoftInput();
                    }
                    glfwInputBackButton();
                }
            });
        }

        nativeOnCreate(this);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            View decorView = getWindow().getDecorView();
            decorView.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
                @Override
                public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                    glfwSetPendingResizeBecauseOfInsets();
                    return v.onApplyWindowInsets(insets);
                }
            });
            decorView.requestApplyInsets();
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            updateFullscreenMode();
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (newConfig.keyboardHidden == Configuration.KEYBOARDHIDDEN_YES) {
            updateFullscreenMode();
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

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
    }

    /** show virtual keyboard
     * Implemented here to ensure that calls from native code are delayed and run on the UI thread.
     */
    public void showSoftInput(final int keyboardType) {
        final DefoldActivity self = this;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                keyboardActive = true;
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

                    // Register an action listener to catch ENTER keys being pressed.
                    mTextEdit.setOnEditorActionListener(new TextView.OnEditorActionListener() {
                        @Override
                        public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                            if (actionId == EditorInfo.IME_ACTION_DONE) {
                                self.FakeEnter();
                            }
                            return false;
                        }
                    });

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
                keyboardActive = false;
                IBinder windowToken = getWindow().getDecorView().getWindowToken();
                imm.hideSoftInputFromWindow(windowToken, 0);
                updateFullscreenMode();
            }
        });
    }

    /**
     * Method to enable/disable immersive mode and display/hide cutout
     * Called from C (android_window.c)
     * @param immersiveMode
     * @param displayCutout
     */
    public void setFullscreenParameters(final boolean immersiveMode, final boolean displayCutout) {
        mImmersiveMode = immersiveMode;
        mDisplayCutout = displayCutout;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                updateFullscreenMode();
            }
        });
    }

    public int[] getSafeAreaInsets() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            return new int[] { 0, 0, 0, 0 };
        }

        View decor = getWindow().getDecorView();
        WindowInsets insets = decor.getRootWindowInsets();
        if (insets == null) {
            return new int[] { 0, 0, 0, 0 };
        }

        DisplayCutout cutout = insets.getDisplayCutout();
        if (cutout != null) {
            return new int[] {
                cutout.getSafeInsetLeft(),
                cutout.getSafeInsetTop(),
                cutout.getSafeInsetRight(),
                cutout.getSafeInsetBottom()
            };
        }

        return new int[] { 0, 0, 0, 0 };
    }

    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        nativeOnActivityResult(this, requestCode,resultCode, data);
    }

    public static native void nativeOnActivityResult(Activity activity, int requestCode, int resultCode, Intent data);

    /**
     * Method to get device ids for any connected gamepads, joysticks etc
     * Called from glfwAndroid.
     * @return Array of device ids
     */
    public int[] getGameControllerDeviceIds() {
        mGameControllerDeviceIds.clear();
        for (int deviceId : InputDevice.getDeviceIds()) {
            InputDevice device = InputDevice.getDevice(deviceId);
            int sources = device.getSources();
            // filter out only gamepads, joysticks and things which has a dpad
            if (((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) ||
                ((sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) ||
                ((sources & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD)) {
                    mGameControllerDeviceIds.add(deviceId);
            }
        }

        // create and copy device ids to an int[]
        int[] deviceIds = new int[mGameControllerDeviceIds.size()];
        for(int i = 0; i < mGameControllerDeviceIds.size(); i++) {
            deviceIds[i] = mGameControllerDeviceIds.get(i);
        }

        return deviceIds;
    }

    /**
     * Method to get controller name
     * Called from glfwAndroid.
     * @param deviceId
     * @return Device name
     */
    public String getGameControllerDeviceName(int deviceId) {
        String name = "Android Controller";
        InputDevice device = InputDevice.getDevice(deviceId);
        if (device != null) {
            name = device.getName();
        }
        return name;
    }

    /**
     * Method to get meta-data value by key from AndroidManifest.xml
     * @param key
     * @return String
     */
    protected String getMetadata(String key) {
        try {
            PackageManager packageManager = getPackageManager();
            ApplicationInfo appInfo = packageManager.getApplicationInfo(getPackageName(),
                    PackageManager.GET_META_DATA);
            Bundle metaData = appInfo.metaData;
            if (metaData != null) {
                Object value = metaData.get(key);
                if (value != null) {
                    return value.toString();
                }
            }
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }
        return null;
    }

    /**
     * Method checks if alpha transparency is enabled for DefoldActivity
     * @return boolean
     */
    public boolean isAlphaTransparencyEnabled() {
        String alpha = getMetadata("alpha.transparency");
        if (alpha != null) {
            return alpha.equals("true");
        }
        return false;
    }
}
