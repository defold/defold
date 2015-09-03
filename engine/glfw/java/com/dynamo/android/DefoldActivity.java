
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

        final DefoldActivity self = this;
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
