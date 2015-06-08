package com.defold.push;

import java.io.PrintStream;

// import org.json.JSONObject;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

public class LocalPushDispatchActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        PrintStream os = null;
        try {
            Bundle extras = getIntent().getExtras();
            if (extras != null) {

                if (Push.getInstance().hasListener()) {
                    Push.getInstance().onLocalPush(extras.getString("payload"), extras.getInt("uid"));
                } else {

                    // need to save this to disk until a listener is set
                    os = new PrintStream(openFileOutput(
                            Push.SAVED_LOCAL_MESSAGE_NAME, MODE_PRIVATE));
                    os.println(extras.getInt("uid"));
                    os.print(extras.getString("payload"));

                    Intent intent = getPackageManager().getLaunchIntentForPackage(
                            getPackageName());
                    intent.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
                    startActivity(intent);
                }


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

        finish();
    }
}
