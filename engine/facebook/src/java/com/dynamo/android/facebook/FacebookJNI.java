package com.dynamo.android.facebook;

import java.net.URLEncoder;

import java.util.Map;
import java.util.Set;
import java.util.Iterator;

import android.app.Activity;
import android.util.Log;
import android.os.Bundle;

// import com.facebook.SessionDefaultAudience;
// import com.facebook.SessionState;
// import com.facebook.widget.WebDialog;
// import com.facebook.FacebookException;
// import com.facebook.Session;

import com.facebook.login.DefaultAudience;

import org.json.JSONException;
import org.json.JSONObject;

class FacebookJNI {

    private static final String TAG = "defold.facebook";

    private native void onLogin(long userData, int state, String error);

    private native void onRequestRead(long userData, String error);

    private native void onRequestPublish(long userData, String error);

    private native void onDialogComplete(long userData, String url, String error);

    private native void onIterateMeEntry(long userData, String key, String value);

    private native void onIteratePermissionsEntry(long userData, String permission);

    private Facebook facebook;
    private Activity activity;

    public FacebookJNI(Activity activity, String appId) {
        this.facebook = new Facebook(activity, appId);
        this.activity = activity;
    }

    /*
    TODO: Implement for SDK 4
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
        return -1;
    }
    */

    private DefaultAudience convertSessionDefaultAudience(int defaultAudience) {
        // Must match iOS for now
        switch (defaultAudience) {
        case 0:
            return DefaultAudience.NONE;
        case 10:
            return DefaultAudience.ONLY_ME;
        case 20:
            return DefaultAudience.FRIENDS;
        case 30:
            return DefaultAudience.EVERYONE;
        default:
            return DefaultAudience.FRIENDS;
        }
    }

    public void login(final long userData) {
        Log.d(TAG, "login");
        this.activity.runOnUiThread( new Runnable() {

            @Override
            public void run() {
                Log.d(TAG, "java jni thread: " + Thread.currentThread().getId());
                facebook.login( new Facebook.StateCallback() {

                    @Override
                    public void onDone( final int state, final String error) {
                        onLogin(userData, state, error);
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
                facebook.requestPubPermissions(convertSessionDefaultAudience(defaultAudience), permissions.split(","), cb);
            }
        });
    }

    public void showDialog(final long userData, final String dialogType, final String paramsJson) {

        this.activity.runOnUiThread(new Runnable() {

            @Override
            public void run() {
                Facebook.DialogCallback cb = new Facebook.DialogCallback() {
                    @Override
                    public void onDone(final String result, final String error) {
                        onDialogComplete(userData, result, error);
                    }
                };

                // Parse dialog params from JSON and put into Bundle
                Bundle params = new Bundle();
                JSONObject o = null;
                try {
                    o = new JSONObject(paramsJson);
                    Iterator iter = o.keys();
                    while (iter.hasNext()) {
                        String key = (String) iter.next();
                        String value = (String) o.get(key);
                        params.putString(key, value);
                    }
                } catch (Exception e) {
                    String err = "Failed to get json value";
                    Log.wtf(TAG, err, e);
                    onDialogComplete(userData, null, err);
                    return;
                }

                facebook.showDialog(dialogType, params, cb);

            }
        });


        /*
        TODO: Implement for SDK 4

            if (values != null) {
                StringBuilder sb = new StringBuilder(1024);
                sb.append("fbconnect://success?");

                Set<String> keys = values.keySet();
                int n = keys.size();
                int i = 0;
                for (String k : keys) {
                    try {
                        String v = URLEncoder.encode(values.getString(k), "UTF-8");
                        sb.append(URLEncoder.encode(k, "UTF-8"));
                        sb.append("=");
                        sb.append(v);
                        if (i < n - 1) {
                            sb.append("&");
                        }
                        ++i;
                    } catch (java.io.UnsupportedEncodingException e) {
                        Log.e(TAG, "Failed to create url", e);
                    }
                }

                String err = null;
                if (error != null) {
                    err = error.getMessage();
                }
                onDialogComplete(userData, sb.toString(), err);
            } else {
                // Happens when user closes the dialog, i.e. cancel
                onDialogComplete(userData, null, null);
            }

        */
    }

}
