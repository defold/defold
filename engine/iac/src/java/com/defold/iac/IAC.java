package com.defold.iac;

import android.app.Activity;
import android.util.Log;

public class IAC {

    public static final String TAG = "iac";
    private static IAC instance;
    private IIACListener listener = null;
    private String storedInvocationPayload = null;
    private String storedInvocationOrigin = null;

    public void start(Activity activity, IIACListener listener) {
        this.listener = listener;
        if(listener != null && (storedInvocationPayload != null || storedInvocationOrigin != null)) {
            listener.onInvocation(storedInvocationPayload, storedInvocationOrigin);
            storedInvocationPayload = null;
            storedInvocationOrigin = null;
        }
    }

    public void stop() {
        this.listener = null;
    }

    public boolean hasListener() {
        if (this.listener != null) {
            return true;
        }
        return false;
    }

    public static IAC getInstance() {
        if (instance == null) {
            instance = new IAC();
        }
        return instance;
    }

    void onInvocation(String payload, String origin) {
        if (listener != null) {
            this.listener.onInvocation(payload, origin);
        } else {
            storedInvocationPayload = payload;
            storedInvocationOrigin = origin;
        }
    }

}
