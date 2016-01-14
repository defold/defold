
package com.dynamo.android;

import android.app.NativeActivity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Bundle;
import android.os.IBinder;
import android.view.KeyEvent;
import android.view.inputmethod.InputMethodManager;

import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.EditText;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.PopupWindow;
import android.text.TextWatcher;
import android.text.Editable;
import android.widget.FrameLayout;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnectionWrapper;
import android.view.inputmethod.InputConnection;
import android.text.InputType;
import android.view.inputmethod.CompletionInfo;
import java.lang.CharSequence;

import android.util.Log;
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

    private InputMethodManager imm = null;
    private TextWatcher mTextWatcher = null;
    private EditText mTextEdit = null;

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
        private String m_ComposingText = "";

        // String markedText;
        public DefoldInputWrapper(DefoldActivity ctx, InputConnection target, boolean mutable) {
            super(target, mutable);
            _ctx = ctx;
            m_ComposingText = "";
        }

        @Override
        public CharSequence getTextBeforeCursor(int n, int flags)
        {
            // Hacky...
            return " ";
        }

        @Override
        public boolean setComposingText(CharSequence text, int newCursorPosition)
        {
            m_ComposingText = text.toString();
            Log.d(TAG, "setComposingText '" + m_ComposingText.toString() + "'");
            _ctx.sendMarkedText(m_ComposingText.toString());

            return super.setComposingText(text, newCursorPosition);
        }

        @Override
        public boolean commitCompletion (CompletionInfo text)
        {
            Log.d(TAG, "commitCompletion: " + text.toString());
            return super.commitCompletion(text);
        }

        @Override
        public boolean finishComposingText ()
        {
            Log.d(TAG, "finishComposingText: '" + m_ComposingText + "'");
            _ctx.sendInputText(new String(m_ComposingText));
            _ctx.sendMarkedText("");

            // If we are finished composing, clear the composing text
            if (m_ComposingText.length() > 0) {
                this.setComposingText("", 0);
            }
            m_ComposingText = "";

            return super.finishComposingText();
        }

        @Override
        public boolean commitText(CharSequence text, int newCursorPosition)
        {
            Log.d(TAG, "commitText '" + text.toString() + "'");
            _ctx.sendInputText(text.toString());

            // When committing new text, we assume the composing text should be cleared.
            m_ComposingText = "";
            _ctx.clearComposingText();

            return super.commitText(text, newCursorPosition);
        }

        @Override
        public boolean sendKeyEvent(KeyEvent event) {
            return super.sendKeyEvent(event);
        }


        @Override
        public boolean deleteSurroundingText(int beforeLength, int afterLength) {
            if (beforeLength == 1 && afterLength == 0) {
                Log.d(TAG, "DELETE!");

                _ctx.clearText();
                _ctx.clearComposingText();
                _ctx.FakeBackspace();
            }

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

        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
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

        // TODO: Remove text watcher before commit, not used...
        mTextWatcher = new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                System.out.println("SVEN: time to send: '" + s.toString() + "'");
                System.out.println("SVEN: changed: '" + s.toString().substring(start, start + count) + "'");
            }

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
                System.out.println("SVEN: beforeTextChanged");
            }

            @Override
            public void afterTextChanged(Editable s) {
                System.out.println("SVEN: afterTextChanged");
            }
        };

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
        System.out.println("SVEN: dispatchKeyEvent triggered.");
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
        final DefoldActivity self = this;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {

                if (mTextEdit != null)
                    mTextEdit.setVisibility(View.GONE);
                mTextEdit = new EditText(self) {
                    @Override
                    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {

                        // We set manual input connection to catch all keyboard interaction
                        InputConnection inpc = super.onCreateInputConnection(outAttrs);
                        outAttrs.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_NORMAL | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
                        return new DefoldInputWrapper(self, inpc, true);

                    }

                    @Override
                    public boolean onCheckIsTextEditor() {
                        return true;
                    }
                };


                // Disable the fullscreen keyboard mode (present in landscape)
                // If we don't do this, we get a large grey inpt box above the keyboard
                // blocking the game view.
                mTextEdit.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI);

                FrameLayout.LayoutParams mEditTextLayoutParams = new FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT);
                mEditTextLayoutParams.gravity = Gravity.TOP;
                mEditTextLayoutParams.setMargins(0, 0, 0, 0);
                mTextEdit.setLayoutParams(mEditTextLayoutParams);
                mTextEdit.setVisibility(View.VISIBLE);
                mTextEdit.addTextChangedListener(self.mTextWatcher); // TODO remove this?

                // mTextEdit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS & (~InputType.TYPE_TEXT_FLAG_AUTO_CORRECT));
                // mTextEdit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_FILTER | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS & (~InputType.TYPE_TEXT_FLAG_AUTO_CORRECT));
                mTextEdit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_NORMAL | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
                // mTextEdit.setRawInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_NORMAL);
                // self.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_NORMAL | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD);

                // Start on an empty slate each time we bring up the keyboard
                mTextEdit.setText("");

                getWindow().addContentView(mTextEdit, mEditTextLayoutParams);

                mTextEdit.bringToFront();
                mTextEdit.setSelection(0);
                mTextEdit.requestFocus();

                imm.showSoftInput(mTextEdit, InputMethodManager.SHOW_FORCED);
            }
        });
    }

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

    public void resetSoftInput() {
        final DefoldActivity self = this;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Log.d(TAG, "resetSoftInput");
                if (mTextEdit != null) {
                    Log.d(TAG, "mTextEdit exist, clearing text");
                    mTextEdit.setText("");
                    mTextEdit.clearComposingText();
                }
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
