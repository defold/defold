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

package com.defold.window;

import android.app.Activity;
import android.view.WindowManager;

public class WindowJNI {

    private final Activity activity;

    public WindowJNI(Activity activity) {
        this.activity = activity;
    }

    public void enableScreenDimming() {
        this.activity.runOnUiThread(() -> activity.getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON));
    }

    public void disableScreenDimming() {
        this.activity.runOnUiThread(() -> activity.getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON));
    }

    public boolean isScreenDimmingEnabled() {
        int flag = (activity.getWindow().getAttributes().flags & android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        return flag == 0;
    }

}
