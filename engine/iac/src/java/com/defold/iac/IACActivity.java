package com.defold.iac;

import android.net.Uri;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.net.ParseException;

public class IACActivity extends Activity {

    // helper function to extract referrer (origin) if possible
    private Uri getReferrerInternal() {
        Intent intent = this.getIntent();
        Uri referrerUri = intent.getParcelableExtra(Intent.EXTRA_REFERRER);
        if (referrerUri != null) {
            return referrerUri;
        }
        String referrer = intent.getStringExtra("android.intent.extra.REFERRER_NAME");
        if (referrer != null) {
            // try parsing the referrer URL; if it's invalid, return null
            try {
                return Uri.parse(referrer);
            } catch (ParseException e) {
                return null;
            }
        }
        return null;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Intent intent = getIntent();
        String action = intent.getAction();

        // handle invocation
        if (Intent.ACTION_VIEW.equals(action)) {
            Uri data = intent.getData();
            if(data != null) {
                Uri referrerUri = getReferrerInternal();
                String origin;
                if(referrerUri == null) {
                    origin = "";
                } else {
                    origin = referrerUri.toString();
                }
                IAC.getInstance().onInvocation(data.toString(), origin);
                intent = getPackageManager().getLaunchIntentForPackage(getPackageName());
                intent.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
                startActivity(intent);
            }
        }

        finish();
    }


}
