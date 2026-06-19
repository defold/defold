/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.android.games.vkquality;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

public class VKQuality {
    // Used to load the 'vkqualitytest' library on application startup.
    static {
        System.loadLibrary("vkquality");
    }

    public static final int INIT_FLAG_SKIP_STARTUP_MITIGATION = 1;
    public static final int INIT_FLAG_GLES_ONLY_STARTUP_MITIGATION_DEVICES = 2;
    public static final int INIT_FLAG_SKIP_DRIVER_FINGERPRINT_CHECK = 4;

    public static final int INIT_SUCCESS = 0;
    public static final int ERROR_INITIALIZATION_FAILURE = -1;
    public static final int ERROR_NO_VULKAN = -2;
    public static final int ERROR_INVALID_DATA_VERSION = -3;
    public static final int ERROR_INVALID_DATA_FILE = -4;
    public static final int ERROR_MISSING_DATA_FILE = -5;

    public static final int RECOMMENDATION_ERROR_NOT_INITIALIZED = -1;
    public static final int RECOMMENDATION_NOT_READY = -2;
    public static final int RECOMMENDATION_VULKAN_BECAUSE_DEVICE_MATCH = 0;
    public static final int RECOMMENDATION_VULKAN_BECAUSE_PREDICTION_MATCH = 1;
    public static final int RECOMMENDATION_VULKAN_BECAUSE_FUTURE_ANDROID = 2;
    public static final int RECOMMENDATION_GLES_BECAUSE_OLD_DEVICE = 3;
    public static final int RECOMMENDATION_GLES_BECAUSE_OLD_DRIVER = 4;
    public static final int RECOMMENDATION_GLES_BECAUSE_NO_DEVICE_MATCH = 5;
    public static final int RECOMMENDATION_GLES_BECAUSE_PREDICTION_MATCH = 6;

    private static final String DEFAULT_QUALITY_FILE = "vkqualitydata.vkq";

    public VKQuality(Context appContext)
    {
        mAppContext = appContext;
        mStartupMitigation = false;
        mMitigationRecommendation = RECOMMENDATION_GLES_BECAUSE_OLD_DRIVER;
        mFlags = 0;
    }

    public int StartVkQuality(String customDataFilename) {
        return StartVkQualityWithFlags(customDataFilename, 0);
    }

    public int StartVkQualityWithFlags(String customDataFilename, int flags)
    {
        mFlags = flags;
        String dataFilename = (customDataFilename == null || customDataFilename.isEmpty()) ?
                DEFAULT_QUALITY_FILE : customDataFilename;

        if ((mFlags & INIT_FLAG_SKIP_STARTUP_MITIGATION) == 0)
        {
            // Startup mitigation path to check against calling vkCreateInstance
            // on devices which may crash
            RunStartupMitigation();
            if (mStartupMitigation)
            {
                return INIT_SUCCESS;
            }
        }
        else
        {
            Log.d("VKQUALITY", "Skipping startup mitigation because of flag");
        }
        return startVkQualityFlags(mAppContext.getResources().getAssets(),
                mAppContext.getFilesDir().getAbsolutePath(),
                dataFilename, mFlags);
    }

    public void StopVkQuality()
    {
        if (!mStartupMitigation) {
            stopVkQuality();
        }
    }

    public int GetVkQuality() {
        if (mStartupMitigation)
        {
            return mMitigationRecommendation;
        }
        return getVkQuality();
    }

    private void RunStartupMitigation()
    {
        // Certain device/SoC combinations are experiencing crashes when attempting
        // to query the Vulkan driver version. Screen out potentially affected devices
        // running affected versions of Android and issue a default recommendation
        // based on SoC and security patch date as a proxy for driver version.
        StartupMitigation startupMitigation = new StartupMitigation();
        startupMitigation.createMitigationDeviceList();
        if (startupMitigation.isDeviceAffected() == StartupMitigation.DEVICE_AFFECTED)
        {
            mStartupMitigation = true;
            if (startupMitigation.useVulkanOnAffectedDevice() &&
                    ((mFlags & INIT_FLAG_GLES_ONLY_STARTUP_MITIGATION_DEVICES) == 0))
            {
                mMitigationRecommendation = RECOMMENDATION_VULKAN_BECAUSE_DEVICE_MATCH;
            }
        }
        Log.d("VKQUALITY", startupMitigation.getResultString());
    }

    private final Context mAppContext;

    private int mFlags;
    private boolean mStartupMitigation;
    private int mMitigationRecommendation;

    // JNI native functions
    public native int startVkQuality(AssetManager jasset_manager, String storage_path,
                                     String data_filename);

    public native int startVkQualityFlags(AssetManager jasset_manager, String storage_path,
                                     String data_filename, int flags);

    public native void stopVkQuality();

    public native int getVkQuality();
}
