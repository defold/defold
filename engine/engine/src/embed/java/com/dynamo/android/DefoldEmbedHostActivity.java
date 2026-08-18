// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0
//
// Optional Activity base for hosts that embed dmengine without NativeActivity.
// Implements the Activity methods glfw queries; override as needed.

package com.dynamo.android;

import android.app.Activity;
import android.os.Bundle;

/**
 * Drop-in Activity base for Defold embed hosts.
 * Methods mirror the subset of {@code DefoldActivity} that native glfw/platform call.
 */
public class DefoldEmbedHostActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    public boolean isAlphaTransparencyEnabled() {
        return false;
    }

    public int[] getSafeAreaInsets() {
        return new int[] { 0, 0, 0, 0 };
    }

    public void setFullscreenParameters(boolean immersiveMode, boolean displayCutout) {
        // Optional: hosts can apply system UI flags here.
    }

    public void setUseHiddenInputField(boolean use) {
    }

    public void showSoftInput(int type) {
    }

    public void hideSoftInput() {
    }

    public void resetSoftInput() {
    }

    public int[] getGameControllerDeviceIds() {
        return new int[0];
    }

    public String getGameControllerDeviceName(int deviceId) {
        return "";
    }

    public String getGameControllerDeviceDescriptor(int deviceId) {
        return "";
    }

    public int getGameControllerDeviceVendorId(int deviceId) {
        return 0;
    }

    public int getGameControllerDeviceProductId(int deviceId) {
        return 0;
    }
}
