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

package com.dynamo.bob.test;

import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import org.junit.Test;

import java.util.Arrays;
import java.util.List;

import com.dynamo.bob.Platform;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;

public class PlatformTest {

    static void testPlatformGet(Platform p) {
        assertSame(p, Platform.get(p.getPair()));
    }

    @Test
    public void testPlatformGetFn() {
        testPlatformGet(Platform.X86_64Win32);
        assertNull(Platform.get("win32"));
        assertNull(Platform.get("x86-win32"));
        testPlatformGet(Platform.X86_64MacOS);
        testPlatformGet(Platform.Arm64MacOS);
        testPlatformGet(Platform.Arm64IosSim);
        testPlatformGet(Platform.Arm64Ios);
        testPlatformGet(Platform.X86_64Linux);
        testPlatformGet(Platform.Arm64Linux);
        testPlatformGet(Platform.Armv7Android);
        testPlatformGet(Platform.Arm64Android);
        testPlatformGet(Platform.WasmWeb);
        testPlatformGet(Platform.WasmPthreadWeb);
    }

    @Test
    public void testPlatformOS() {
        assertSame(PlatformProfile.OS.OS_ID_WINDOWS, Platform.get("x86_64-win32").getOsID());

        assertSame(PlatformProfile.OS.OS_ID_OSX, Platform.get("x86_64-macos").getOsID());
        assertSame(PlatformProfile.OS.OS_ID_OSX, Platform.get("arm64-macos").getOsID());

        assertSame(PlatformProfile.OS.OS_ID_IOS, Platform.get("arm64-ios").getOsID());
        assertSame(PlatformProfile.OS.OS_ID_IOS, Platform.get("arm64_sim-ios").getOsID());

        assertSame(PlatformProfile.OS.OS_ID_ANDROID, Platform.get("armv7-android").getOsID());
        assertSame(PlatformProfile.OS.OS_ID_ANDROID, Platform.get("arm64-android").getOsID());

        assertSame(PlatformProfile.OS.OS_ID_WEB, Platform.get("wasm-web").getOsID());
        assertSame(PlatformProfile.OS.OS_ID_WEB, Platform.get("wasm_pthread-web").getOsID());

        assertSame(PlatformProfile.OS.OS_ID_LINUX, Platform.get("x86_64-linux").getOsID());
        assertSame(PlatformProfile.OS.OS_ID_LINUX, Platform.get("arm64-linux").getOsID());

        assertNull(Platform.get(""));
    }

    @Test
    public void testPlatformArchitectures() {
        for (Platform platform : Platform.values())
        {
            List<String> availableArchitectures = Arrays.asList(platform.getArchitectures().getArchitectures());
            if (!availableArchitectures.contains(platform.getPair()))
            {
                System.out.printf("ERROR! %s is not a supported architecture for %s platform. Available architectures: %s%n", platform.getPair(), platform.getPair(), String.join(", ", availableArchitectures));
                fail();
            }
        }
    }

    @Test
    public void testPlatformMatching() {

        assertTrue(Platform.X86_64MacOS.matchesOS(PlatformProfile.OS.OS_ID_GENERIC));
        assertTrue(Platform.Arm64IosSim.matchesOS(PlatformProfile.OS.OS_ID_GENERIC));
        assertTrue(Platform.X86_64Linux.matchesOS(PlatformProfile.OS.OS_ID_GENERIC));
        assertTrue(Platform.Arm64Android.matchesOS(PlatformProfile.OS.OS_ID_GENERIC));
        assertTrue(Platform.WasmWeb.matchesOS(PlatformProfile.OS.OS_ID_GENERIC));
        assertTrue(Platform.WasmPthreadWeb.matchesOS(PlatformProfile.OS.OS_ID_GENERIC));

        assertTrue(Platform.X86_64Win32.matchesOS(PlatformProfile.OS.OS_ID_WINDOWS));

        assertTrue(Platform.Arm64MacOS.matchesOS(PlatformProfile.OS.OS_ID_OSX));
        assertTrue(Platform.X86_64MacOS.matchesOS(PlatformProfile.OS.OS_ID_OSX));

        assertTrue(Platform.Arm64Ios.matchesOS(PlatformProfile.OS.OS_ID_IOS));
        assertTrue(Platform.Arm64IosSim.matchesOS(PlatformProfile.OS.OS_ID_IOS));

        assertTrue(Platform.Arm64Linux.matchesOS(PlatformProfile.OS.OS_ID_LINUX));
        assertTrue(Platform.X86_64Linux.matchesOS(PlatformProfile.OS.OS_ID_LINUX));

        assertTrue(Platform.Armv7Android.matchesOS(PlatformProfile.OS.OS_ID_ANDROID));
        assertTrue(Platform.Arm64Android.matchesOS(PlatformProfile.OS.OS_ID_ANDROID));

        assertTrue(Platform.WasmWeb.matchesOS(PlatformProfile.OS.OS_ID_WEB));
        assertTrue(Platform.WasmPthreadWeb.matchesOS(PlatformProfile.OS.OS_ID_WEB));
    }

}
