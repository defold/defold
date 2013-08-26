package com.dynamo.android;

import android.app.NativeActivity;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;

public class DefoldActivity extends NativeActivity {

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            ActivityInfo ai = getPackageManager().getActivityInfo(getIntent().getComponent(),
                    PackageManager.GET_META_DATA);

            if (ai.metaData != null) {
                String ln = ai.metaData.getString(META_DATA_LIB_NAME);
                if (ln != null) {
                    System.loadLibrary(ln);
                }
            }
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException("Error getting activity info", e);
        }
    }
}
