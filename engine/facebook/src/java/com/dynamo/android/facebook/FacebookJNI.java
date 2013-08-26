package com.dynamo.android.facebook;

import java.util.Map;

import android.app.Activity;
import android.util.Log;

import com.facebook.SessionDefaultAudience;
import com.facebook.SessionState;

class FacebookJNI {

    private static final String TAG = "defold.facebook";

    private native void onLogin(long userData, int state, String error);

    private native void onRequestRead(long userData, String error);

    private native void onRequestPublish(long userData, String error);

    private native void onIterateMeEntry(long userData, String key, String value);

    private native void onIteratePermissionsEntry(long userData, String permission);

    private Facebook facebook;
    private Activity activity;

    public FacebookJNI(Activity activity, String appId) {
        this.facebook = new Facebook(activity, appId);
        this.activity = activity;
    }

    private int convertSessionState(SessionState state) {
        // Must match iOS for now
        switch (state) {
        case CREATED:
            return 0;
        case CREATED_TOKEN_LOADED:
            return 1;
        case OPENING:
            return 2;
        case OPENED:
            return 1 | (1 << 9);
        case OPENED_TOKEN_UPDATED:
            return 2 | (1 << 9);
        case CLOSED_LOGIN_FAILED:
            return 1 | (1 << 8);
        case CLOSED:
            return 2 | (1 << 8);
        default:
            return -1;
        }
    }

    private SessionDefaultAudience convertSessionDefaultAudience(int defaultAudience) {
        // Must match iOS for now
        switch (defaultAudience) {
        case 0:
            return SessionDefaultAudience.NONE;
        case 10:
            return SessionDefaultAudience.ONLY_ME;
        case 20:
            return SessionDefaultAudience.FRIENDS;
        case 30:
            return SessionDefaultAudience.EVERYONE;
        default:
            return SessionDefaultAudience.FRIENDS;
        }
    }

    public void login(final long userData) {
        Log.d(TAG, "login");
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Log.v(TAG, "java jni thread: " + Thread.currentThread().getId());
              //onLogin(userData, true, null);
                facebook.login(new Facebook.StateCallback() {
                    @Override
                    public void onDone(final SessionState state, final String error) {
                        onLogin(userData, convertSessionState(state), error);
                    }
                });
            }
        });
    }

    public void logout() {
        this.facebook.logout();
    }

    public void iterateMe(final long userData) {
        Map<String, String> me = this.facebook.getMe();
        if (me != null) {
            for (Map.Entry<String, String> entry : me.entrySet()) {
                this.onIterateMeEntry(userData, entry.getKey(), entry.getValue());
            }
        }
    }

    public void iteratePermissions(final long userData) {
        for (String permission : this.facebook.getPermissions()) {
            onIteratePermissionsEntry(userData, permission);
        }
    }

    public String getAccessToken() {
        return this.facebook.getAccessToken();
    }

    public void requestReadPermissions(final long userData, final String permissions) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Facebook.Callback cb = new Facebook.Callback() {
                    @Override
                    public void onDone(final String error) {
                        onRequestRead(userData, error);
                    }
                };
                facebook.requestReadPermissions(permissions.split(","), cb);
            }
        });
    }

    public void requestPublishPermissions(final long userData, final int defaultAudience, final String permissions) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Facebook.Callback cb = new Facebook.Callback() {
                    @Override
                    public void onDone(final String error) {
                        onRequestRead(userData, error);
                    }
                };
                facebook.requestPubPermissions(convertSessionDefaultAudience(defaultAudience), permissions.split(","),
                        cb);
            }
        });
    }
}