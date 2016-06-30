package com.defold.push;

import java.io.PrintStream;

import org.json.JSONObject;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

public class PushDispatchActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d(Push.TAG, "PushDispatchActivity.onCreate");

        PrintStream os = null;
        try {
            Bundle extras = getIntent().getExtras();
            if (extras != null) {
                boolean fromNotification = extras.getByte("fromNotification") == 1 ? true : false;
                extras.remove("fromNotification");
                JSONObject json = Push.toJson(extras);
                os = new PrintStream(openFileOutput(
                        Push.SAVED_PUSH_MESSAGE_NAME, MODE_PRIVATE));
                os.println(fromNotification);
                os.print(json.toString());
            } else {
                Log.e(Push.TAG, "Unable to queue message. extras is null");
            }
        } catch (Throwable e) {
            Log.e(Push.TAG, "Failed to write push message to disk", e);
        } finally {
            if (os != null) {
                os.close();
            }
        }

        Intent intent = getPackageManager().getLaunchIntentForPackage(
                getPackageName());
        startActivity(intent);
        finish();
    }
}
