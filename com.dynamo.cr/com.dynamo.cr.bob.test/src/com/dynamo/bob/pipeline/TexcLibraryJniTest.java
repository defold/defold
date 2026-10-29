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

package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import java.util.Arrays;

import org.junit.Test;

import com.dynamo.bob.pipeline.Texc.PixelFormat;

public class TexcLibraryJniTest {
    private static byte[] pixels() {
        byte[] data = new byte[8 * 8 * 4];
        Arrays.fill(data, (byte) 255);
        return data;
    }

    // Verify JNI input cleanup preserves Java data across successful and failed default encodes.
    @Test
    public void defaultEncodeReusesSettings() {
        Texc.DefaultEncodeSettings settings = new Texc.DefaultEncodeSettings();
        settings.path = "default-input-cleanup";
        settings.width = 8;
        settings.height = 8;
        settings.pixelFormat = PixelFormat.PF_R8G8B8A8;
        settings.data = pixels();
        byte[] expected = settings.data.clone();

        for (int i = 0; i < 4; ++i) {
            settings.outPixelFormat = PixelFormat.PF_RGBA_ASTC_8x8;
            assertNull(TexcLibraryJni.DefaultEncode(settings));
            settings.outPixelFormat = PixelFormat.PF_R8G8B8A8;
            assertArrayEquals(expected, TexcLibraryJni.DefaultEncode(settings));
            assertArrayEquals(expected, settings.data);
        }
    }

    // Verify JNI input cleanup preserves Java data across successful and failed ASTC encodes.
    @Test
    public void astcEncodeReusesSettings() {
        Texc.ASTCEncodeSettings settings = new Texc.ASTCEncodeSettings();
        settings.path = "astc-input-cleanup";
        settings.width = 8;
        settings.height = 8;
        settings.pixelFormat = PixelFormat.PF_R8G8B8A8;
        settings.outPixelFormat = PixelFormat.PF_RGBA_ASTC_8x8;
        settings.numThreads = 1;
        settings.data = pixels();
        byte[] expected = settings.data.clone();

        for (int i = 0; i < 4; ++i) {
            settings.qualityLevel = -1;
            assertNull(TexcLibraryJni.ASTCEncode(settings));
            settings.qualityLevel = 0;
            assertEquals(16, TexcLibraryJni.ASTCEncode(settings).length);
            assertArrayEquals(expected, settings.data);
        }
    }

    // Verify JNI input cleanup preserves Java data across successful and failed BasisU encodes.
    @Test
    public void basisUEncodeReusesSettings() {
        Texc.BasisUEncodeSettings settings = new Texc.BasisUEncodeSettings();
        settings.path = "basis-input-cleanup";
        settings.width = 8;
        settings.height = 8;
        settings.outPixelFormat = PixelFormat.PF_R8G8B8A8;
        settings.numThreads = 1;
        settings.data = pixels();
        byte[] expected = settings.data.clone();

        for (int i = 0; i < 4; ++i) {
            settings.pixelFormat = PixelFormat.PF_RGBA_ASTC_8x8;
            assertNull(TexcLibraryJni.BasisUEncode(settings));
            settings.pixelFormat = PixelFormat.PF_R8G8B8A8;
            assertTrue(TexcLibraryJni.BasisUEncode(settings).length > 0);
            assertArrayEquals(expected, settings.data);
        }
    }

    // Verify cleanup tolerates a missing input array when ASTC rejects settings before reading it.
    @Test
    public void astcEncodeRejectsInvalidQualityWithoutData() {
        Texc.ASTCEncodeSettings settings = new Texc.ASTCEncodeSettings();
        settings.path = "astc-no-data";
        settings.qualityLevel = -1;
        assertNull(TexcLibraryJni.ASTCEncode(settings));
    }
}
