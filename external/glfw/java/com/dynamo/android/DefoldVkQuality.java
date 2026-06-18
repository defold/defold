// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.android;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;

public final class DefoldVkQuality {
    private static final String TAG = "DefoldVkQuality";
    private static final String VKQUALITY_CLASS_NAME = "com.google.android.games.vkquality.VKQuality";
    private static final String VKQUALITY_ENABLED_META_DATA = "com.defold.vkquality.enabled";

    private static final int VKQUALITY_RECOMMENDATION_RETRY_COUNT = 10;
    private static final int VKQUALITY_RECOMMENDATION_RETRY_INTERVAL_MS = 25;

    private static final int VKQUALITY_LOG_INFO = 1;
    private static final int VKQUALITY_LOG_WARNING = 2;

    // Mirrored by AndroidVulkanIsRecommended() in graphics_vulkan_android.cpp.
    public static final int VULKAN_RECOMMENDATION_ALLOW = 0;
    public static final int VULKAN_RECOMMENDATION_DENY = 1;

    private static final int VKQUALITY_INIT_SUCCESS = 0;
    private static final int VKQUALITY_ERROR_INITIALIZATION_FAILURE = -1;

    private static final int VKQUALITY_RECOMMENDATION_NOT_READY = -2;
    private static final int VKQUALITY_RECOMMENDATION_ERROR_NOT_INITIALIZED = -1;
    private static final int VKQUALITY_RECOMMENDATION_VULKAN_BECAUSE_DEVICE_MATCH = 0;
    private static final int VKQUALITY_RECOMMENDATION_VULKAN_BECAUSE_PREDICTION_MATCH = 1;
    private static final int VKQUALITY_RECOMMENDATION_VULKAN_BECAUSE_FUTURE_ANDROID = 2;
    private static final int VKQUALITY_RECOMMENDATION_GLES_BECAUSE_OLD_DEVICE = 3;
    private static final int VKQUALITY_RECOMMENDATION_GLES_BECAUSE_OLD_DRIVER = 4;
    private static final int VKQUALITY_RECOMMENDATION_GLES_BECAUSE_NO_DEVICE_MATCH = 5;
    private static final int VKQUALITY_RECOMMENDATION_GLES_BECAUSE_PREDICTION_MATCH = 6;

    private static boolean preflightComplete = false;
    private static Object vkQuality = null;
    private static int initResult = VKQUALITY_ERROR_INITIALIZATION_FAILURE;
    private static int recommendation = VKQUALITY_RECOMMENDATION_ERROR_NOT_INITIALIZED;
    private static int vulkanRecommendation = VULKAN_RECOMMENDATION_ALLOW;

    private DefoldVkQuality() {
    }

    static synchronized void runPreflight(Context context) {
        if (preflightComplete) {
            return;
        }

        if (!isPreflightEnabled(context)) {
            vulkanRecommendation = VULKAN_RECOMMENDATION_ALLOW;
            preflightComplete = true;
            return;
        }

        try {
            Class<?> vkQualityClass = Class.forName(VKQUALITY_CLASS_NAME);
            java.lang.reflect.Constructor<?> constructor = vkQualityClass.getConstructor(Context.class);
            Context appContext = context.getApplicationContext();
            if (appContext == null) {
                appContext = context;
            }

            Object instance = constructor.newInstance(appContext);
            java.lang.reflect.Method startMethod = vkQualityClass.getMethod("StartVkQualityWithFlags", String.class, int.class);
            initResult = ((Integer) startMethod.invoke(instance, null, 0)).intValue();
            vkQuality = instance;
            evaluateRecommendation();
        } catch (Throwable t) {
            vkQuality = null;
            initResult = VKQUALITY_ERROR_INITIALIZATION_FAILURE;
            recommendation = VKQUALITY_RECOMMENDATION_ERROR_NOT_INITIALIZED;
            setDecision(true, VKQUALITY_LOG_WARNING, "VkQuality preflight unavailable (" + t.toString() + "), allowing Vulkan support probe.", t);
        } finally {
            preflightComplete = true;
        }
    }

    public static synchronized int getVulkanRecommendation() {
        return vulkanRecommendation;
    }

    private static boolean isPreflightEnabled(Context context) {
        try {
            PackageManager packageManager = context.getPackageManager();
            ApplicationInfo appInfo = packageManager.getApplicationInfo(context.getPackageName(), PackageManager.GET_META_DATA);
            Bundle metaData = appInfo.metaData;
            if (metaData != null && metaData.containsKey(VKQUALITY_ENABLED_META_DATA)) {
                Object value = metaData.get(VKQUALITY_ENABLED_META_DATA);
                return value == null || Boolean.parseBoolean(value.toString());
            }
        } catch (PackageManager.NameNotFoundException e) {
            Log.w(TAG, "Unable to read VkQuality manifest metadata", e);
        }
        // Bob writes this metadata only when the Android bundle includes the VkQuality package.
        // If it is absent, do not probe an optional dependency that may not be bundled.
        return false;
    }

    private static void evaluateRecommendation() {
        // VkQuality uses upstream integer result values:
        // initResult is 0 on success and negative on initialization failure.
        // recommendation is -2 while pending, -1 if uninitialized, 0..2 for Vulkan, and 3..6 for GLES.
        if (initResult != VKQUALITY_INIT_SUCCESS) {
            setDecision(true, VKQUALITY_LOG_WARNING, "VkQuality initialization failed (" + initResult + "), allowing Vulkan support probe.");
            return;
        }

        if (!updateRecommendation()) {
            return;
        }

        if (recommendation == VKQUALITY_RECOMMENDATION_NOT_READY) {
            for (int i = 0; i < VKQUALITY_RECOMMENDATION_RETRY_COUNT; ++i) {
                try {
                    Thread.sleep(VKQUALITY_RECOMMENDATION_RETRY_INTERVAL_MS);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    break;
                }

                if (!updateRecommendation() || recommendation != VKQUALITY_RECOMMENDATION_NOT_READY) {
                    break;
                }
            }
        }

        if (recommendation >= VKQUALITY_RECOMMENDATION_VULKAN_BECAUSE_DEVICE_MATCH &&
                recommendation <= VKQUALITY_RECOMMENDATION_VULKAN_BECAUSE_FUTURE_ANDROID) {
            setDecision(true, VKQUALITY_LOG_INFO, "VkQuality recommends Vulkan (" + recommendationToString(recommendation) + ").");
            return;
        }

        if (recommendation >= VKQUALITY_RECOMMENDATION_GLES_BECAUSE_OLD_DEVICE &&
                recommendation <= VKQUALITY_RECOMMENDATION_GLES_BECAUSE_PREDICTION_MATCH) {
            setDecision(false, VKQUALITY_LOG_INFO, "VkQuality recommends OpenGL ES (" + recommendationToString(recommendation) + "), disabling Vulkan adapter.");
            return;
        }

        if (recommendation == VKQUALITY_RECOMMENDATION_NOT_READY) {
            setDecision(false, VKQUALITY_LOG_WARNING, "VkQuality recommendation was not ready after waiting, disabling Vulkan adapter.");
            return;
        }

        setDecision(true, VKQUALITY_LOG_WARNING, "VkQuality recommendation is " + recommendationToString(recommendation) + " (" + recommendation + "), allowing Vulkan support probe.");
    }

    private static boolean updateRecommendation() {
        if (vkQuality == null || initResult != VKQUALITY_INIT_SUCCESS) {
            return false;
        }

        try {
            java.lang.reflect.Method getMethod = vkQuality.getClass().getMethod("GetVkQuality");
            recommendation = ((Integer) getMethod.invoke(vkQuality)).intValue();
            return true;
        } catch (Throwable t) {
            setDecision(true, VKQUALITY_LOG_WARNING, "VkQuality recommendation unavailable (" + t.toString() + "), allowing Vulkan support probe.", t);
            return false;
        }
    }

    private static void setDecision(boolean recommended, int level, String message) {
        setDecision(recommended, level, message, null);
    }

    private static void setDecision(boolean recommended, int level, String message, Throwable throwable) {
        vulkanRecommendation = recommended ? VULKAN_RECOMMENDATION_ALLOW : VULKAN_RECOMMENDATION_DENY;

        if (level == VKQUALITY_LOG_WARNING) {
            if (throwable != null) {
                Log.w(TAG, message, throwable);
            } else {
                Log.w(TAG, message);
            }
        } else {
            Log.i(TAG, message);
        }
    }

    private static String recommendationToString(int value) {
        switch (value) {
            case VKQUALITY_RECOMMENDATION_NOT_READY: return "not ready";
            case VKQUALITY_RECOMMENDATION_ERROR_NOT_INITIALIZED: return "not initialized";
            case VKQUALITY_RECOMMENDATION_VULKAN_BECAUSE_DEVICE_MATCH: return "Vulkan because device match";
            case VKQUALITY_RECOMMENDATION_VULKAN_BECAUSE_PREDICTION_MATCH: return "Vulkan because prediction match";
            case VKQUALITY_RECOMMENDATION_VULKAN_BECAUSE_FUTURE_ANDROID: return "Vulkan because future Android";
            case VKQUALITY_RECOMMENDATION_GLES_BECAUSE_OLD_DEVICE: return "GLES because old device";
            case VKQUALITY_RECOMMENDATION_GLES_BECAUSE_OLD_DRIVER: return "GLES because old driver";
            case VKQUALITY_RECOMMENDATION_GLES_BECAUSE_NO_DEVICE_MATCH: return "GLES because no device match";
            case VKQUALITY_RECOMMENDATION_GLES_BECAUSE_PREDICTION_MATCH: return "GLES because prediction match";
            default: return "unknown";
        }
    }
}
