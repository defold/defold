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

package com.defold.libs;

import com.sun.jna.Callback;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;

import java.util.Arrays;
import java.util.List;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class MouseCapture {
    private static Logger logger = LoggerFactory.getLogger(MouseCapture.class);

    static {
        try {
            ResourceUnpacker.unpackResources();
            Native.register("mouse_capture_shared");
        } catch (Exception e) {
            logger.error("Failed to extract/register mousecapture so/dll", e);
        }
    }

    public static native Pointer MouseCapture_CreateContext(int saveCursorX, int saveCursorY);

    public static native void MouseCapture_WarpCursor(int x, int y);

    public static native boolean MouseCapture_StartCapture(Pointer context);

    public static native void MouseCapture_StopCapture(Pointer context);

    public static native boolean MouseCapture_PollDelta(Pointer context, MouseDelta delta);

    public static native void MouseCapture_DestroyContext(Pointer context);

    public static class MouseDelta extends Structure {
        public MouseDelta() {}

        public double dx;
        public double dy;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("dx", "dy");
        }
    }
}
