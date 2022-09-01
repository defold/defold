// Copyright 2020-2022 The Defold Foundation
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

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;

import org.junit.Test;

import com.dynamo.bob.Platform;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;

public class PlatformTest {

    @Test
    public void testPlatformMatching() {

        assertTrue(Platform.matchPlatformAgainstOS("",               PlatformProfile.OS.OS_ID_GENERIC));
        assertTrue(Platform.matchPlatformAgainstOS("x86-win32",      PlatformProfile.OS.OS_ID_WINDOWS));
        assertTrue(Platform.matchPlatformAgainstOS("x86_64-win32",   PlatformProfile.OS.OS_ID_WINDOWS));
        assertTrue(Platform.matchPlatformAgainstOS("x86_64-macos",   PlatformProfile.OS.OS_ID_OSX));
        assertTrue(Platform.matchPlatformAgainstOS("x86_64-linux",   PlatformProfile.OS.OS_ID_LINUX));
        assertTrue(Platform.matchPlatformAgainstOS("arm64-ios",      PlatformProfile.OS.OS_ID_IOS));
        assertTrue(Platform.matchPlatformAgainstOS("armv7-android",  PlatformProfile.OS.OS_ID_ANDROID));
        assertTrue(Platform.matchPlatformAgainstOS("js-web",         PlatformProfile.OS.OS_ID_WEB));
        assertTrue(Platform.matchPlatformAgainstOS("wasm-web",       PlatformProfile.OS.OS_ID_WEB));

        assertTrue(Platform.matchPlatformAgainstOS("x86_64-linux",   PlatformProfile.OS.OS_ID_GENERIC));

        assertFalse(Platform.matchPlatformAgainstOS("arm64-ios",     PlatformProfile.OS.OS_ID_OSX));
        assertFalse(Platform.matchPlatformAgainstOS("armv7-android", PlatformProfile.OS.OS_ID_IOS));
        assertFalse(Platform.matchPlatformAgainstOS("js-web",        PlatformProfile.OS.OS_ID_ANDROID));
    }

}
