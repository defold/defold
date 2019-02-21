package com.defold.push;

import com.google.firebase.iid.FirebaseInstanceIdService;
import com.google.firebase.iid.FirebaseInstanceId;

import android.util.Log;


public class MyInstanceIDListenerService extends FirebaseInstanceIdService {

    public static final String TAG = "push-firebase";

    // MyInstanceIDListenerService() {
    //     Log.d(TAG, "MyInstanceIDListenerService()");
    // }

    @Override
    public void onTokenRefresh() {
        // Get updated InstanceID token.
        String refreshedToken = FirebaseInstanceId.getInstance().getToken();
        Log.d(TAG, "Refreshed token: " + refreshedToken);
        // TODO: Implement this method to send any registration to your app's servers.
        // sendRegistrationToServer(refreshedToken);
    }

}
