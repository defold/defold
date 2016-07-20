package com.dynamo.android.facebook;

import java.net.URLEncoder;

import java.util.Map;
import java.util.Set;
import java.util.Iterator;
import java.util.Collection;
import java.util.Collections;
import java.util.Arrays;
import java.util.concurrent.CountDownLatch;
import java.util.LinkedHashSet;

import android.content.Intent;
import android.app.Activity;
import android.net.Uri;
import android.util.Log;
import android.os.Bundle;

import android.os.Parcelable; // debug

import com.facebook.AccessToken;
import com.facebook.FacebookSdk;
import com.facebook.appevents.AppEventsLogger;
import com.facebook.applinks.AppLinkData;
import com.facebook.login.DefaultAudience;
import com.facebook.GraphRequest;
import com.facebook.GraphResponse;

import bolts.AppLinks;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONArray;

// A helper class that initializes the facebook sdk, and also activates/deactivates the app
class FacebookAppJNI {

    private static final String TAG = "defold.facebookapp";

    private static final String KEY_AL_APPLINK_DATA_KEY = "al_applink_data";
    private static final String KEY_NAME_EXTRAS = "extras";
    private static final String KEY_NAME_REFERER_APP_LINK = "referer_app_link";

    private Activity activity;
    private String appId;

    private Map<String, String> appLinkInfo;

/*


    public static final void processIntentForAppLink(Intent intent) {
        GameLib.logInfo(tag, "processIntentForAppLink");

        Uri targetUrl = AppLinks.getTargetUrlFromInboundIntent(ActivityHelper.getInstance().getActivity().getApplicationContext(), intent);
        // in case of cancel the targetUrl isn't null, but "null"
        if (targetUrl != null && targetUrl.getScheme() != null) {
            AppLinkEventData appLinkEventData = createAppLinkEventData(targetUrl, intent.getExtras());
            FacebookSdkWrapper.GetEventQueue().add(SdkEvent.create(appLinkEventData));
        } else {
            tryFetchDeferredAppLink();
        }
    }

    private static final AppLinkEventData createAppLinkEventData(Uri targetUrl, Bundle bundle) {
        GameLib.logInfo(tag, "createAppLinkEventData " + targetUrl + " " + bundle);
        AppLinkEventData appLinkEventData = new AppLinkEventData();
        appLinkEventData.url = targetUrl.toString();
        appLinkEventData.refererUrl = "";
        appLinkEventData.refererAppName = "";

        ArrayList<KeyValuePair> keyValuePairs = new ArrayList<KeyValuePair>();
        
        for (String key : myGetQueryParameterNames(targetUrl)) {
            String value = targetUrl.getQueryParameter(key);
            KeyValuePair keyValuePair = new KeyValuePair();
            keyValuePair.key = key;
            keyValuePair.value = value;
            keyValuePairs.add(keyValuePair);
        }

        if (bundle != null) {
            Bundle appLinkData = bundle.getBundle(KEY_AL_APPLINK_DATA_KEY);
            GameLib.logInfo(tag, "Got appLinkData " + appLinkData);
            if (appLinkData != null) {
                if (appLinkData.containsKey(KEY_NAME_EXTRAS)) {
                    Bundle extras = appLinkData.getBundle(KEY_NAME_EXTRAS);
                    for (String key : extras.keySet()) {
                        KeyValuePair keyValuePair = new KeyValuePair();
                        keyValuePair.key = key;
                        keyValuePair.value = "" + extras.get(key);
                        keyValuePairs.add(keyValuePair);
                    }
                }

                if (appLinkData.containsKey(KEY_NAME_REFERER_APP_LINK)) {
                    Bundle refererBundle = appLinkData.getBundle(KEY_NAME_REFERER_APP_LINK);
                    if (refererBundle != null) {
                        String url = refererBundle.getString("url");
                        String appName = refererBundle.getString("app_name");

                        if (url != null && appName != null) {
                            appLinkEventData.refererUrl = url;
                            appLinkEventData.refererAppName = appName;
                        }
                    }
                }
            }

        }
        appLinkEventData.data = keyValuePairs.toArray(new KeyValuePair[] {});

        return appLinkEventData;
    }


    private static final void tryFetchDeferredAppLink() {
        GameLib.logInfo(tag, "tryFetchDeferredAppLink");
        final Activity activity = ActivityHelper.getInstance().getActivity();

        AppLinkData.fetchDeferredAppLinkData(activity, new AppLinkData.CompletionHandler() {
            @Override
            public void onDeferredAppLinkDataFetched(AppLinkData appLinkData) {
                GameLib.logInfo(tag, "onDeferredAppLinkDataFetched");
                if (appLinkData != null) {
                    Uri targetUrl = appLinkData.getTargetUri();
                    AppLinkEventData appLinkEventData = createAppLinkEventData(targetUrl, appLinkData.getArgumentBundle());
                    FacebookSdkWrapper.GetEventQueue().add(SdkEvent.create(appLinkEventData));
                } else {
                    GameLib.logInfo(tag, "no appLinkData available");
                }
            }
        });
    }
*/

    // Since this function isn't available until Api Level 11
    // http://stackoverflow.com/questions/11642494/android-net-uri-getqueryparameternames-alternative
    /**
     * Returns a set of the unique names of all query parameters. Iterating
     * over the set will return the names in order of their first occurrence.
     *
     * @throws UnsupportedOperationException if this isn't a hierarchical URI
     *
     * @return a set of decoded names
     */
    private static Set<String> getQueryParameterNames(Uri uri) {
        if (uri.isOpaque()) {
            throw new UnsupportedOperationException("This isn't a hierarchical URI.");
        }

        String query = uri.getEncodedQuery();
        if (query == null) {
            return Collections.emptySet();
        }

        Set<String> names = new LinkedHashSet<String>();
        int start = 0;
        do {
            int next = query.indexOf('&', start);
            int end = (next == -1) ? query.length() : next;

            int separator = query.indexOf('=', start);
            if (separator > end || separator == -1) {
                separator = end;
            }

            String name = query.substring(start, separator);
            names.add(Uri.decode(name));

            // Move start to end of name.
            start = end + 1;
        } while (start < query.length());

        return Collections.unmodifiableSet(names);
    }

    private void getAppLinkInfo(Uri targetUrl, Bundle bundle)
    {
        Log.d(TAG, "App Link Target URL: " + targetUrl);

        // Of the form: https://fb.me/865035583600867
        
        for (String key : getQueryParameterNames(targetUrl)) {
            String value = targetUrl.getQueryParameter(key);
            Log.d(TAG, "MAWE: QUERY PARAMS: " + key + ": " + value);
        }

        if (bundle != null) {
            Bundle appLinkData = bundle.getBundle(KEY_AL_APPLINK_DATA_KEY);
            if (appLinkData != null) {
                if (appLinkData.containsKey(KEY_NAME_EXTRAS)) {
                    Bundle extras = appLinkData.getBundle(KEY_NAME_EXTRAS);
                    for (String key : extras.keySet()) {
                        String value = "" + extras.get(key);
                        Log.d(TAG, "MAWE: EXTRAS: " + key + ": " + value);
                    }
                }

                if (appLinkData.containsKey(KEY_NAME_REFERER_APP_LINK)) {
                    Bundle refererBundle = appLinkData.getBundle(KEY_NAME_REFERER_APP_LINK);
                    if (refererBundle != null) {
                        String url = refererBundle.getString("url");
                        String appName = refererBundle.getString("app_name");

                        Log.d(TAG, "MAWE: REFERRER URL: " + url);
                        Log.d(TAG, "MAWE: REFERRER APP NAME: " + appName);

                        if (url != null && appName != null) {
                            //appLinkEventData.refererUrl = url;
                            //appLinkEventData.refererAppName = appName;
                        }
                    }
                }
            }

            String nativeUrl = null;
            if (bundle.containsKey(AppLinkData.ARGUMENTS_NATIVE_URL)) {
                nativeUrl = "" + bundle.get(AppLinkData.ARGUMENTS_NATIVE_URL);

            }
            Log.d(TAG, "MAWE: NATIVE URL: " + nativeUrl);
        }

        PrintBundle(bundle, "getAppLinkInfo/bundle");
    }


    public static String intentToString(Intent intent) {
        if (intent == null) {
            return null;
        }

        return intent.toString() + " " + bundleToString(intent.getExtras());
    }

    public static String bundleToString(Bundle bundle) {
        StringBuilder out = new StringBuilder("Bundle[");

        if (bundle == null) {
            out.append("null");
        } else {
            boolean first = true;
            for (String key : bundle.keySet()) {
                if (!first) {
                    out.append(", ");
                }

                out.append(key).append('=');

                Object value = bundle.get(key);

                if (value instanceof int[]) {
                    out.append(Arrays.toString((int[]) value));
                } else if (value instanceof byte[]) {
                    out.append(Arrays.toString((byte[]) value));
                } else if (value instanceof boolean[]) {
                    out.append(Arrays.toString((boolean[]) value));
                } else if (value instanceof short[]) {
                    out.append(Arrays.toString((short[]) value));
                } else if (value instanceof long[]) {
                    out.append(Arrays.toString((long[]) value));
                } else if (value instanceof float[]) {
                    out.append(Arrays.toString((float[]) value));
                } else if (value instanceof double[]) {
                    out.append(Arrays.toString((double[]) value));
                } else if (value instanceof String[]) {
                    out.append(Arrays.toString((String[]) value));
                } else if (value instanceof CharSequence[]) {
                    out.append(Arrays.toString((CharSequence[]) value));
                } else if (value instanceof Parcelable[]) {
                    out.append(Arrays.toString((Parcelable[]) value));
                } else if (value instanceof Bundle) {
                    out.append(bundleToString((Bundle) value));
                } else {
                    out.append(value);
                }

                first = false;
            }
        }

        out.append("]");
        return out.toString();
    }

    private void PrintBundle(Bundle bundle, String name)
    {
        String json = null;
        try {
            JSONObject obj = FacebookJNI.bundleToJson(bundle);
            json = obj.toString();
        } catch(JSONException e) {
            json = "Error while converting dialog result to JSON: " + e.getMessage();
        }
        Log.d(TAG, "JSON bundle: " + name);
        Log.d(TAG, json);
    }

    public FacebookAppJNI(Activity activity, String appId) {
        this.activity = activity;
        this.appId = appId;

        final CountDownLatch latch = new CountDownLatch(1);

        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                String s = String.format("sdkInitialize: activity %s   appid: %s", FacebookAppJNI.this.activity, FacebookAppJNI.this.appId);
                Log.d(TAG, s);
                
                Log.d(TAG, "sdkInitialize");
                
                FacebookSdk.sdkInitialize( FacebookAppJNI.this.activity );
                Log.d(TAG, "setApplicationId");
                FacebookSdk.setApplicationId( FacebookAppJNI.this.appId );
                
Log.d(TAG, "MAWE: FB SDK INITIALIZED  (deferred uses appid now)");

                Log.d(TAG, "FacebookAppJNI.this.activity.getIntent()");
                
                Intent intent = FacebookAppJNI.this.activity.getIntent();

                String intentstring = intentToString(intent);


                Log.d(TAG, "AppLinks.getTargetUrlFromInboundIntent()");
                Log.d(TAG, "MAWE: intent: " + intentstring);

                Uri targetUrl = AppLinks.getTargetUrlFromInboundIntent(FacebookAppJNI.this.activity, intent);
                if (targetUrl != null) {
                    Log.d(TAG, "MAWE: App Link was found");
                    String action = intent.getAction();
                    //Uri data = intent.getData();
                    //Log.d(TAG, "MAWE: getData(): " + data.toString());
                    Log.d(TAG, "MAWE: getAction(): " + action);
                    getAppLinkInfo(targetUrl, intent.getExtras());
                } else {
                    AppLinkData.fetchDeferredAppLinkData( FacebookAppJNI.this.activity, FacebookAppJNI.this.appId,
                        new AppLinkData.CompletionHandler() {
                            @Override
                            public void onDeferredAppLinkDataFetched(AppLinkData appLinkData) {
                                //process applink data
                                if( appLinkData != null ) {
                                    Log.d(TAG, "MAWE: Deferred App Link was found");
                                    //Uri data = FacebookAppJNI.this.activity.getIntent().getData();
                                    //Log.d(TAG, "MAWE: getData(): " + data.toString());
                                    
                                    getAppLinkInfo(appLinkData.getTargetUri(), appLinkData.getArgumentBundle());


                                    PrintBundle(appLinkData.getArgumentBundle(), "getArgumentBundle");
                                    PrintBundle(appLinkData.getRefererData(), "getRefererData");


                                    String ref = appLinkData.getRef();
                                    Log.d(TAG, "MAWE: getRef(): " + (ref != null ? ref : "null"));

                                    Bundle referrerBundle = appLinkData.getRefererData();
                                    if (referrerBundle != null) {
                                        String referrerUrl = referrerBundle.getString("fb_ref");
                                        Log.d(TAG, "MAWE: getRefererData fb_ref: " + referrerUrl);
                                    }
                                    else
                                    {
                                        Log.d(TAG, "MAWE: getRefererData fb_ref: null");
                                    }

                                    // http://stackoverflow.com/questions/20404170/will-the-user-receive-facebook-request-data-if-the-application-not-installed

                                    {
                                        Bundle refererBundle = appLinkData.getRefererData();

                                        if (refererBundle != null ) {
                                            String json = null;
                                            try {
                                                JSONObject obj = FacebookJNI.bundleToJson(refererBundle);
                                                json = obj.toString();
                                            } catch(JSONException e) {
                                                json = "Error while converting dialog result to JSON: " + e.getMessage();
                                            }
                                            Log.d(TAG, "JSON refererBundle:");
                                            Log.d(TAG, json);
                                        } else {
                                            Log.d(TAG, "No refererBundle found!");
                                        }
                                    }

                                    {
                                        Bundle appLinkData2 = appLinkData.getArgumentBundle().getBundle(KEY_AL_APPLINK_DATA_KEY);
                                        if( appLinkData2 != null )
                                        {
                                            Bundle refererBundle = appLinkData2.getBundle(KEY_NAME_REFERER_APP_LINK);
                                            if (refererBundle != null) {
                                                String url = refererBundle.getString("url");
                                                String appName = refererBundle.getString("app_name");
                                                Log.d(TAG, "MAWE: referrer url: " + url);
                                                Log.d(TAG, "MAWE: referrer appName: " + appName);
                                            } else {
                                                Log.d(TAG, "MAWE: appLinkData.getBundle(KEY_NAME_REFERER_APP_LINK): null");
                                            }
                                        } else {
                                            Log.d(TAG, "MAWE: appLinkData.getArgumentBundle().getBundle(KEY_AL_APPLINK_DATA_KEY): null");
                                        }
                                    }

                                } else {
                                    Log.d(TAG, "No App Link was found");

                                    Log.d(TAG, "Activity Name: " + FacebookAppJNI.this.activity.getClass().toString());
                                    PrintBundle(FacebookAppJNI.this.activity.getIntent().getExtras(), "main intent");
                                }
                            }
                        });
                }

                latch.countDown();
            }
        });

        try {
            latch.await();
        } catch (InterruptedException ex) {
        }
    }

    public void activate() {
        String s = String.format("activateApp: activity %s   appid: %s", this.activity, this.appId);
        Log.d(TAG, s);
        AppEventsLogger.activateApp(this.activity, this.appId);
    }

    public void deactivate() {
        String s = String.format("deactivateApp: activity %s   appid: %s", this.activity, this.appId);
        Log.d(TAG, s);
        AppEventsLogger.deactivateApp(this.activity, this.appId);
    }
}

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
    public static JSONObject bundleToJson(final Bundle in) throws JSONException {
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
    private String appId;

    public FacebookJNI(Activity activity, String appId) {
        this.activity = activity;
        this.facebook = new Facebook(this.activity, appId);
        this.appId = appId;
    }

    public void login(final long userData) {
        Log.d(TAG, "login");
        final String appId = this.appId;
        final Activity activity = this.activity;
        this.activity.runOnUiThread( new Runnable() {

            @Override
            public void run() {
                Log.d(TAG, "java jni thread: " + Thread.currentThread().getId());
                facebook.login( new Facebook.StateCallback() {
                    @Override
                    public void onDone( final int state, final String error) {
                        onLogin(userData, state, error);

                        GraphRequest request = GraphRequest.newGraphPathRequest(
                              AccessToken.getCurrentAccessToken(),
                              "/me/apprequests",
                              new GraphRequest.Callback() {
                                @Override
                                public void onCompleted(GraphResponse response) {
                                    // process the info received
                                    if( response != null )
                                    {
                                        JSONObject obj = response.getJSONObject();
                                        if( obj != null ) {
                                            String json = obj.toString();
                                            Log.d(TAG, "JSON GraphResponse:");
                                            Log.d(TAG, json);
                                        }

                                        if (response.getError() != null) {
                                            Log.d(TAG, "MAWE: Graph Response error: " + response.getError().getErrorMessage() );
                                        }
                                    }
                                }
                        });

                        request.executeAsync();
                    }
                });
            }

        });
    }

    public void logout() {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                FacebookJNI.this.facebook.logout();
            }
        });
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
                        String key = iter.next().toString();
                        String value = o.get(key).toString();
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

    public void postEvent(String event, double valueToSum, String[] keys, String[] values) {
        facebook.postEvent(event, valueToSum, keys, values);
    }

    public void enableEventUsage() {
        facebook.disableEventUsage();
    }

    public void disableEventUsage() {
        facebook.enableEventUsage();
    }
}
