package com.defold.adtruth;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;

/*
 * Testing installation referrer
 * From an "adb shell":
 * am broadcast -a com.android.vending.INSTALL_REFERRER -n com.defold.dmengine/com.defold.adtruth.InstallReceiver --es "referrer" "AABqDnQAaDs"
 */

public class InstallReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        Bundle extras = intent.getExtras();
        if (extras != null) {
            String referrer = extras.getString("referrer");
            if (referrer != null) {
                Log.i(AdTruthJNI.TAG, "referrer: " + referrer);
                storeReferrer(context, referrer);
            }
        }
    }

    private void storeReferrer(Context context, String referrer) {
        SharedPreferences prefs = context.getSharedPreferences(AdTruthJNI.PREFERENCES_FILE, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putString("referrer", referrer);
        editor.commit();
    }
}
