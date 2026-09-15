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

import com.defold.extension.pipeline.texture.*;
import com.defold.extension.pipeline.texture.TestTextureCompressor;
import com.dynamo.bob.fs.IResource;
import com.dynamo.graphics.proto.Graphics;
import org.junit.Before;
import org.junit.Test;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;

import static org.junit.Assert.*;

public class TextureCompressorTest extends AbstractProtoBuilderTest {

    @Before
    public void setUp() {
        addTestFiles();
    }

    private void ensureBuildProject() throws Exception {
        // We need to build some dummy data
        addImage("/test.png", 16, 16);
        StringBuilder src = new StringBuilder();
        src.append("images: {");
        src.append("  image: \"/test.png\"");
        src.append("}");
        build("/test.atlas", src.toString());
    }

    @Test
    public void testDefaultCompressorExists() {
        ITextureCompressor defaultCompressor = TextureCompression.getCompressor(TextureCompressorUncompressed.TextureCompressorName);
        assertNotNull(defaultCompressor);
        assertEquals(defaultCompressor.getName(), TextureCompressorUncompressed.TextureCompressorName);
    }

    @Test
    public void testDefaultCompressorConcurrentFirstUse() throws Exception {
        // Simulate first use in a fresh Bob process. The registry is static and may have
        // already been populated by another test.
        Field compressorsField = TextureCompression.class.getDeclaredField("compressors");
        compressorsField.setAccessible(true);
        Map<?, ?> compressors = (Map<?, ?>) compressorsField.get(null);
        compressors.remove(TextureCompressorUncompressed.TextureCompressorName);

        Field presetsField = TextureCompression.class.getDeclaredField("presets");
        presetsField.setAccessible(true);
        Map<?, ?> presets = (Map<?, ?>) presetsField.get(null);
        presets.remove(TextureCompressorUncompressed.TextureCompressorUncompressedPresetName);

        int threadCount = 32;
        ExecutorService executor = Executors.newFixedThreadPool(threadCount);
        CountDownLatch ready = new CountDownLatch(threadCount);
        CountDownLatch start = new CountDownLatch(1);
        List<Future<ITextureCompressor>> futures = new ArrayList<>();
        try {
            for (int i = 0; i < threadCount; ++i) {
                futures.add(executor.submit(() -> {
                    ready.countDown();
                    start.await();
                    return TextureCompression.getCompressor(TextureCompressorUncompressed.TextureCompressorName);
                }));
            }

            assertTrue(ready.await(10, TimeUnit.SECONDS));
            start.countDown();

            ITextureCompressor defaultCompressor = futures.get(0).get(10, TimeUnit.SECONDS);
            for (Future<ITextureCompressor> future : futures) {
                assertSame(defaultCompressor, future.get(10, TimeUnit.SECONDS));
            }
            assertNotNull(TextureCompression.getPreset(TextureCompressorUncompressed.TextureCompressorUncompressedPresetName));
        } finally {
            executor.shutdownNow();
        }
    }

    @Test
    public void testCompressorNotExists() {
        ITextureCompressor notFound = TextureCompression.getCompressor("not_installed");
        assertNull(notFound);
    }

    @Test
    public void testTextureCompressionPresets() throws Exception {
        ensureBuildProject();

        TextureCompressorPreset presetOne = TextureCompression.getPreset("PresetOne");
        assertNotNull(presetOne);

        assert(presetOne.getOptionInt("test_int") == 7);
        assert(presetOne.getOptionFloat("test_float") == 42.0);
        assert(presetOne.getOptionString("test_string").equals("test_preset_one"));

        TextureCompressorPreset presetTwo = TextureCompression.getPreset("PresetTwo");
        assert(presetTwo.getOptionInt("test_int") == 1337);
        assert(presetTwo.getOptionFloat("test_float") == 999.0);
        assert(presetTwo.getOptionString("test_string").equals("test_preset_two"));
    }

    @Test
    public void testTextureCompressionPresetsGettingUsed() throws Exception {
        TestTextureCompressor testCompressor = new TestTextureCompressor();
        TestTextureCompressor.expectedOptionOne = 1234;
        TestTextureCompressor.expectedOptionTwo = 8080.0f;
        TestTextureCompressor.expectedOptionThree = "test_default_param";

        Graphics.TextureProfile.Builder textureProfile = Graphics.TextureProfile.newBuilder();
        Graphics.PlatformProfile.Builder platformProfile = Graphics.PlatformProfile.newBuilder();
        Graphics.TextureFormatAlternative.Builder textureFormatAlt1 = Graphics.TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(Graphics.TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA);
        textureFormatAlt1.setCompressor("TestCompressor");
        textureFormatAlt1.setCompressorPreset("TEST_PRESET");

        platformProfile.setOs(Graphics.PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);
        platformProfile.setPremultiplyAlpha(false);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        Graphics.PathSettings genericPath = Graphics.PathSettings.newBuilder().setProfile("Test Profile").setPath("**").build();

        Graphics.TextureProfiles.Builder texProfilesBuilder = Graphics.TextureProfiles.newBuilder();
        texProfilesBuilder.addProfiles(textureProfile.build()).addPathSettings(genericPath);

        this.getProject().setOption("texture-compression", "true");

        this.getProject().getProjectProperties().putStringValue("graphics", "texture_profiles", "my.texture_profile");
        IResource res = this.getProject().getResource("my.texture_profilesc");
        res.output().setContent(texProfilesBuilder.build().toByteArray());
        TextureCompression.registerCompressor(testCompressor);
        ensureBuildProject();

        assert(TestTextureCompressor.didRun);
    }
}
