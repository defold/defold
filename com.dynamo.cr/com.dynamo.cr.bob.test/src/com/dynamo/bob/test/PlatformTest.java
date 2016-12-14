package com.dynamo.bob.test;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;

import org.junit.Test;

import com.dynamo.bob.Platform;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;

public class PlatformTest {

    @Test
    public void testPlatformMatching() {

        assertTrue(Platform.matchPlatformAgainstOS("",              PlatformProfile.OS.OS_ID_GENERIC));
        assertTrue(Platform.matchPlatformAgainstOS("x86-win32",     PlatformProfile.OS.OS_ID_WINDOWS));
        assertTrue(Platform.matchPlatformAgainstOS("x86_64-win32",  PlatformProfile.OS.OS_ID_WINDOWS));
        assertTrue(Platform.matchPlatformAgainstOS("x86-darwin",    PlatformProfile.OS.OS_ID_OSX));
        assertTrue(Platform.matchPlatformAgainstOS("x86_64-darwin", PlatformProfile.OS.OS_ID_OSX));
        assertTrue(Platform.matchPlatformAgainstOS("x86-linux",     PlatformProfile.OS.OS_ID_LINUX));
        assertTrue(Platform.matchPlatformAgainstOS("armv7-darwin",  PlatformProfile.OS.OS_ID_IOS));
        assertTrue(Platform.matchPlatformAgainstOS("arm64-darwin",  PlatformProfile.OS.OS_ID_IOS));
        assertTrue(Platform.matchPlatformAgainstOS("armv7-android", PlatformProfile.OS.OS_ID_ANDROID));
        assertTrue(Platform.matchPlatformAgainstOS("js-web",        PlatformProfile.OS.OS_ID_WEB));

        assertTrue(Platform.matchPlatformAgainstOS("x86-linux",     PlatformProfile.OS.OS_ID_GENERIC));

        assertFalse(Platform.matchPlatformAgainstOS("armv7-darwin",  PlatformProfile.OS.OS_ID_WINDOWS));
        assertFalse(Platform.matchPlatformAgainstOS("arm64-darwin",  PlatformProfile.OS.OS_ID_OSX));
        assertFalse(Platform.matchPlatformAgainstOS("armv7-android", PlatformProfile.OS.OS_ID_IOS));
        assertFalse(Platform.matchPlatformAgainstOS("js-web",        PlatformProfile.OS.OS_ID_ANDROID));
    }

}
