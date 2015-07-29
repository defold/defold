package com.dynamo.android.facebook;

import java.net.URLEncoder;

import java.util.Map;
import java.util.Set;
import java.util.Iterator;
import java.util.Collection;
import java.util.Arrays;

import android.app.Activity;
import android.util.Log;
import android.os.Bundle;

import com.facebook.login.DefaultAudience;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONArray;

class FacebookJNI {

    private static final String TAG = "defold.facebook";

    private native void onLogin(long userData, int state, String error);

    private native void onRequestRead(long userData, String error);

    private native void onRequestPublish(long userData, String error);

    private native void onDialogComplete(long userData, String results, String error);

    private native void onIterateMeEntry(long userData, String key, String value);

    private native void onIteratePermissionsEntry(long userData, String permission);

    // JSONObject.wrap not available in Java 7,
    // Using https://android.googlesource.com/platform/libcore/+/master/json/src/main/java/org/json/JSONObject.java
    private static Object JSONWrap(Object o) {
        if (o == null) {
            return null;
        }
        if (o instanceof JSONArray || o instanceof JSONObject) {
            return o;
        }
        if (o.equals(null)) {
            return o;
        }
        try {
            if (o instanceof Collection) {
                return new JSONArray((Collection) o);
            } else if (o.getClass().isArray()) {
                return new JSONArray(Arrays.asList((Object[])o));
            }
            if (o instanceof Map) {
                return new JSONObject((Map) o);
            }
            if (o instanceof Boolean ||
                o instanceof Byte ||
                o instanceof Character ||
                o instanceof Double ||
                o instanceof Float ||
                o instanceof Integer ||
                o instanceof Long ||
                o instanceof Short ||
                o instanceof String) {
                return o;
            }
            if (o.getClass().getPackage().getName().startsWith("java.")) {
                return o.toString();
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    // From http://stackoverflow.com/questions/21858528/convert-a-bundle-to-json
    private static JSONObject bundleToJson(final Bundle in) throws JSONException {
        JSONObject json = new JSONObject();
        if (in == null) {
            return json;
        }
        Set<String> keys = in.keySet();
        for (String key : keys) {
            json.put(key, JSONWrap(in.get(key)));
        }

        return json;
    }

    private Facebook facebook;
    private Activity activity;

    public FacebookJNI(Activity activity, String appId) {
        this.facebook = new Facebook(activity, appId);
        this.activity = activity;
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
                facebook.requestPubPermissions(defaultAudience, permissions.split(","), cb);
            }
        });
    }

    public void showDialog(final long userData, final String dialogType, final String paramsJson) {

        this.activity.runOnUiThread(new Runnable() {

            @Override
            public void run() {
                Facebook.DialogCallback cb = new Facebook.DialogCallback() {
                    @Override
                    public void onDone(final Bundle result, final String error) {

                        // serialize bundle as JSON for C/Lua side
                        String res = null;
                        String err = error;
                        try {
                            JSONObject obj = bundleToJson(result);
                            res = obj.toString();
                        } catch(JSONException e) {
                            err = "Error while converting dialog result to JSON: " + e.getMessage();
                        }

                        onDialogComplete(userData, res, err);
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

    }

}
