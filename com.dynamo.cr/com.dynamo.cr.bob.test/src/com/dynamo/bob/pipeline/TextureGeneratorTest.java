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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import java.awt.image.BufferedImage;
import java.io.IOException;
import java.util.EnumSet;

import com.defold.extension.pipeline.texture.*;
import com.defold.extension.pipeline.texture.TestTextureCompressor;
import com.dynamo.graphics.proto.Graphics;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.util.TextureUtil;
import com.dynamo.bob.pipeline.Texc.FlipAxis;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.TextureFormatAlternative;
import com.dynamo.graphics.proto.Graphics.TextureFormatAlternative.CompressionLevel;
import com.dynamo.graphics.proto.Graphics.TextureImage.Image;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;
import com.dynamo.graphics.proto.Graphics.TextureProfile;

public class TextureGeneratorTest {

    //                                AABBGGRR
    private static int pixelWhite = 0xFF332211;
    private static int pixelRed   = 0xFF000011;
    private static int pixelGreen = 0xFF002200;
    private static int pixelBlue  = 0xFF330000;

    @Before
    public void setUp() throws Exception {
        TextureCompression.registerCompressor(new TextureCompressorBasisU());
        TextureCompression.registerCompressor(new TextureCompressorASTC());
    }

    // Create a 2x2 image that will be easy to verify after different flip operations.
    // +---+
    // |W|R|  W= White
    // +---+  R= Red
    // |G|B|  G= Green
    // +---+  B= Blue
    //
    static BufferedImage createFlipTestImage()
    {
        BufferedImage srcImage = new BufferedImage(2, 2, BufferedImage.TYPE_4BYTE_ABGR);
        srcImage.setRGB(0, 0, pixelWhite);
        srcImage.setRGB(1, 0, pixelRed);
        srcImage.setRGB(0, 1, pixelGreen);
        srcImage.setRGB(1, 1, pixelBlue);
        return srcImage;
    }

    // Asserts that the pixel at x and y is of a certain color.
    static void assertPixel(Image image, byte[] data, int x, int y, int color)
    {
        int width = image.getWidth();
        int offset = (width*y + x)*4;
        assertEquals((byte) (color >> 16),  data[offset+0]); // B
        assertEquals((byte) (color >> 8), data[offset+1]);  // G
        assertEquals((byte) (color >> 0), data[offset+2]);  // R
        assertEquals((byte) (color >> 24), data[offset+3]); // A
    }

    @Test
    public void testRGBA() throws TextureGeneratorException, IOException {
        int[][] mipMaps = new int[][] {
                { 128, 64 },
                { 64,32 },
                { 32,16 },
                { 16,8 },
                { 8,4 },
                { 4,2 },
                { 2,1 },
                { 1,1 } };

        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"));

        assertEquals(1, result.textureImage.getAlternativesCount());
        Image image = result.textureImage.getAlternatives(0);
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGBA, image.getFormat());
        assertEquals(mipMaps.length, image.getMipMapOffsetCount());

        int offset = 0;
        int i = 0;
        for (int[] dim : mipMaps) {
            int size = dim[0] * dim[1] * 4;
            assertEquals(offset, image.getMipMapOffset(i));
            assertEquals(size, image.getMipMapSize(i));
            offset += size;
            ++i;
        }
    }

    @Test
    public void testRGB() throws TextureGeneratorException, IOException {
        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgb.png"));

        assertEquals(1, result.textureImage.getAlternativesCount());
        Image image = result.textureImage.getAlternatives(0);
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, image.getFormat());
    }

    @Test
    public void testLuminance() throws TextureGeneratorException, IOException {
        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_lum.png"));

        assertEquals(1, result.textureImage.getAlternativesCount());
        Image image = result.textureImage.getAlternatives(0);
        assertEquals(TextureFormat.TEXTURE_FORMAT_LUMINANCE, image.getFormat());
    }

    @Test
    public void testLuminanceAlpha() throws TextureGeneratorException, IOException {
        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_luma.png"));

        assertEquals(1, result.textureImage.getAlternativesCount());
        Image image = result.textureImage.getAlternatives(0);
        assertEquals(TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA, image.getFormat());
    }

    @Test
    public void testIndexed() throws TextureGeneratorException, IOException {
        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_idx.png"));

        assertEquals(1, result.textureImage.getAlternativesCount());
        Image image = result.textureImage.getAlternatives(0);
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGBA, image.getFormat());
    }

    @Test
    public void testMipMaps() throws TextureGeneratorException, IOException {
        int[][] mipMaps = new int[][] {
                { 256, 256 },
                { 128, 128 },
                { 64, 64 },
                { 32, 32 },
                { 16, 16 },
                { 8, 8 },
                { 4, 4 },
                { 2, 2 },
                { 1, 1 } };

        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("btn_next_level.png"));

        assertEquals(1, result.textureImage.getAlternativesCount());
        Image image = result.textureImage.getAlternatives(0);
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGBA, image.getFormat());
        assertEquals(mipMaps.length, image.getMipMapOffsetCount());

        int offset = 0;
        int i = 0;
        for (int[] dim : mipMaps) {
            int size = dim[0] * dim[1] * 4;
            assertEquals(offset, image.getMipMapOffset(i));
            assertEquals(size, image.getMipMapSize(i));
            offset += size;
            ++i;
        }

        int resultLength = 0;
        for (byte[] bytes : result.imageDatas) {
            resultLength += bytes.length;
        }

        assertEquals(offset, resultLength);
    }

    @Test
    public void testClosestPowerTwoScale() throws TextureGeneratorException, IOException {
        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("127_65_rgba.png"));

        Image image = result.textureImage.getAlternatives(0);
        assertEquals(128, image.getWidth());
        assertEquals(64, image.getHeight());
        assertEquals(127, image.getOriginalWidth());
        assertEquals(65, image.getOriginalHeight());
    }

    @Test
    public void testCompile16BitPerChannelTexture() throws TextureGeneratorException, IOException {
        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("16_bit_texture.png"));

        Image image = result.textureImage.getAlternatives(0);
        assertEquals(8, image.getWidth());
        assertEquals(8, image.getHeight());
        assertEquals(6, image.getOriginalWidth());
        assertEquals(8, image.getOriginalHeight());
    }

    @Test
    public void testClosestPowerTwo() throws TextureGeneratorException, IOException {
        assertEquals(128, TextureUtil.closestPOT(127));
        assertEquals(128, TextureUtil.closestPOT(129));

        assertEquals(1, TextureUtil.closestPOT(1));
        assertEquals(2, TextureUtil.closestPOT(2));
        assertEquals(4, TextureUtil.closestPOT(3));
        assertEquals(4, TextureUtil.closestPOT(5));
        assertEquals(8, TextureUtil.closestPOT(6));
        assertEquals(8, TextureUtil.closestPOT(7));
        assertEquals(8, TextureUtil.closestPOT(8));
        assertEquals(8, TextureUtil.closestPOT(9));
        assertEquals(8, TextureUtil.closestPOT(10));

    }

    @Test
    public void testPreMultipliedAlpha() throws TextureGeneratorException, IOException {
        BufferedImage srcImage = new BufferedImage(1, 1, BufferedImage.TYPE_4BYTE_ABGR);
        // full transparent white pixel
        int pixel = (0 << 24) | (255 << 16) | (255 << 8) | (255 << 0);
        srcImage.setRGB(0, 0, pixel);
        TextureGenerator.GenerateResult result = TextureGenerator.generate(srcImage, null, true);

        Image image = result.textureImage.getAlternatives(0);
        assertEquals((byte) 0, result.imageDatas.get(0)[0]);
        assertEquals((byte) 0, result.imageDatas.get(0)[1]);
        assertEquals((byte) 0, result.imageDatas.get(0)[2]);
        assertEquals((byte) 0, result.imageDatas.get(0)[3]);
    }

    @Test
    public void testTextureProfileCompressors() throws TextureGeneratorException, IOException {
        BufferedImage srcImage = new BufferedImage(1, 1, BufferedImage.TYPE_4BYTE_ABGR);
        // full transparent white pixel
        int pixel = (0 << 24) | (255 << 16) | (255 << 8) | (255 << 0);
        srcImage.setRGB(0, 0, pixel);

        // Create the texture compressor + preset
        TextureCompressorPreset presetOne = new TextureCompressorPreset("TestCompressorPresetOne", "Test Compressor One", "TestCompressor");
        assertNotNull(presetOne);

        presetOne.setOptionInt("test_int", 1337);
        presetOne.setOptionFloat("test_float", 99.0f);
        presetOne.setOptionString("test_string", "test_string");

        TestTextureCompressor testCompressor = new TestTextureCompressor();
        testCompressor.expectedOptionOne = 1337;
        testCompressor.expectedOptionTwo = 99.0f;
        testCompressor.expectedOptionThree = "test_string";
        testCompressor.expectedBytes[0] = (byte) 32;
        testCompressor.expectedBytes[1] = (byte) 64;
        testCompressor.expectedBytes[2] = (byte) 128;
        testCompressor.expectedBytes[3] = (byte) 255;

        TextureCompression.registerCompressor(testCompressor);
        TextureCompression.registerPreset(presetOne);

        // Create a texture profile with texture compression
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_RGBA);
        textureFormatAlt1.setCompressor("TestCompressor");
        textureFormatAlt1.setCompressorPreset("TestCompressorPresetOne");

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);
        platformProfile.setPremultiplyAlpha(false);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        TextureGenerator.GenerateResult result = TextureGenerator.generate(srcImage, textureProfile.build(), true);

        assertEquals((byte) 32, result.imageDatas.get(0)[0]);
        assertEquals((byte) 64,  result.imageDatas.get(0)[1]);
        assertEquals((byte) 128, result.imageDatas.get(0)[2]);
        assertEquals((byte) 255, result.imageDatas.get(0)[3]);
    }

    @Test
    public void testNoPreMultipliedAlpha() throws TextureGeneratorException, IOException {
    	BufferedImage srcImage = new BufferedImage(1, 1, BufferedImage.TYPE_4BYTE_ABGR);
        // full transparent white pixel
        int pixel = (0 << 24) | (255 << 16) | (255 << 8) | (255 << 0);
        srcImage.setRGB(0, 0, pixel);

        // Create a texture profile with texture compression
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_RGBA);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.FAST);

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);
        platformProfile.setPremultiplyAlpha(false);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        TextureGenerator.GenerateResult result = TextureGenerator.generate(srcImage, textureProfile.build(), true);

        assertEquals((byte) 255, result.imageDatas.get(0)[0]);
        assertEquals((byte) 255, result.imageDatas.get(0)[1]);
        assertEquals((byte) 255, result.imageDatas.get(0)[2]);
        assertEquals((byte) 0,   result.imageDatas.get(0)[3]);
    }

    @Test
        public void testTextureProfilesNoCompressionETC1() throws TextureGeneratorException, IOException {
        // Create a texture profile with texture compression
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_RGB_ETC1);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.NORMAL);

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        // Generate texture without compression applied
        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), false);

        assertEquals(1, result.textureImage.getAlternativesCount());
        assertEquals(128, result.textureImage.getAlternatives(0).getWidth());
        assertEquals(64, result.textureImage.getAlternatives(0).getHeight());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, result.textureImage.getAlternatives(0).getFormat());
    }

    @Test
    public void testTextureProfilesNoCompressionPVRTC() throws TextureGeneratorException, IOException {
        // Create a texture profile with texture compression
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.NORMAL);

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        // Generate texture without compression applied
        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), false);

        assertEquals(1, result.textureImage.getAlternativesCount());
        assertEquals(128, result.textureImage.getAlternatives(0).getWidth());
        assertEquals(64, result.textureImage.getAlternatives(0).getHeight());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGBA, result.textureImage.getAlternatives(0).getFormat());
    }

    @Test
    public void testNoFlip() throws TextureGeneratorException, IOException {
        TextureGenerator.GenerateResult result = TextureGenerator.generate(createFlipTestImage(), null, false, EnumSet.noneOf(FlipAxis.class));

        Image image = result.textureImage.getAlternatives(0);
        assertPixel(image, result.imageDatas.get(0), 0, 0, pixelWhite);
        assertPixel(image, result.imageDatas.get(0), 1, 0, pixelRed);
        assertPixel(image, result.imageDatas.get(0), 0, 1, pixelGreen);
        assertPixel(image, result.imageDatas.get(0), 1, 1, pixelBlue);
    }

    @Test
    public void testFlipX() throws TextureGeneratorException, IOException {
        TextureGenerator.GenerateResult result = TextureGenerator.generate(createFlipTestImage(), null, false, EnumSet.of(FlipAxis.FLIP_AXIS_X));

        Image image = result.textureImage.getAlternatives(0);
        assertPixel(image, result.imageDatas.get(0), 0, 0, pixelRed);
        assertPixel(image, result.imageDatas.get(0), 1, 0, pixelWhite);
        assertPixel(image, result.imageDatas.get(0), 0, 1, pixelBlue);
        assertPixel(image, result.imageDatas.get(0), 1, 1, pixelGreen);
    }

    @Test
    public void testFlipY() throws TextureGeneratorException, IOException {
        TextureGenerator.GenerateResult result = TextureGenerator.generate(createFlipTestImage(), null, false, EnumSet.of(FlipAxis.FLIP_AXIS_Y));

        Image image = result.textureImage.getAlternatives(0);
        assertPixel(image, result.imageDatas.get(0), 0, 0, pixelGreen);
        assertPixel(image, result.imageDatas.get(0), 1, 0, pixelBlue);
        assertPixel(image, result.imageDatas.get(0), 0, 1, pixelWhite);
        assertPixel(image, result.imageDatas.get(0), 1, 1, pixelRed);
    }

    @Test
    public void testFlipXY() throws TextureGeneratorException, IOException {
        TextureGenerator.GenerateResult result = TextureGenerator.generate(createFlipTestImage(), null, false, EnumSet.of(FlipAxis.FLIP_AXIS_X, FlipAxis.FLIP_AXIS_Y));

        Image image = result.textureImage.getAlternatives(0);
        assertPixel(image, result.imageDatas.get(0), 0, 0, pixelBlue);
        assertPixel(image, result.imageDatas.get(0), 1, 0, pixelGreen);
        assertPixel(image, result.imageDatas.get(0), 0, 1, pixelRed);
        assertPixel(image, result.imageDatas.get(0), 1, 1, pixelWhite);
    }

    @Test
    public void testTextureProfilesFormat() throws TextureGeneratorException, IOException {

        // Create a texture profile with texture compression
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_RGB_ETC1);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.NORMAL);

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), true);

        assertEquals(1, result.textureImage.getAlternativesCount());
        assertEquals(128, result.textureImage.getAlternatives(0).getWidth());
        assertEquals(64, result.textureImage.getAlternatives(0).getHeight());

        // NOTE: We've currently disabled the PVRTC/ETC compression, and use UASTC as default instead
        //assertEquals(TextureFormat.TEXTURE_FORMAT_RGB_ETC1, texture.getAlternatives(0).getFormat());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, result.textureImage.getAlternatives(0).getFormat());
    }

    @Test
    public void testTextureProfilesMaxSize() throws TextureGeneratorException, IOException {

        // Create a texture profile with a max texture size
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_RGB);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.NORMAL);

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(16);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), true);

        assertEquals(16, result.textureImage.getAlternatives(0).getWidth());
        assertEquals(8, result.textureImage.getAlternatives(0).getHeight());
    }


    @Test
    public void testTextureProfilesMultiplePlatforms() throws TextureGeneratorException, IOException {

        // Create a texture profile with multiple platforms and formats
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile1 = PlatformProfile.newBuilder();
        PlatformProfile.Builder platformProfile2 = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt2 = TextureFormatAlternative.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt3 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.FAST);
        textureFormatAlt2.setFormat(TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1);
        textureFormatAlt2.setCompressionLevel(CompressionLevel.FAST);
        textureFormatAlt3.setFormat(TextureFormat.TEXTURE_FORMAT_RGB);
        textureFormatAlt3.setCompressionLevel(CompressionLevel.NORMAL);

        platformProfile1.setOs(PlatformProfile.OS.OS_ID_IOS);
        platformProfile1.addFormats(textureFormatAlt1.build());
        platformProfile1.addFormats(textureFormatAlt2.build());
        platformProfile1.setMipmaps(false);
        platformProfile1.setMaxTextureSize(16);

        platformProfile2.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile2.addFormats(textureFormatAlt3.build());
        platformProfile2.setMipmaps(false);
        platformProfile2.setMaxTextureSize(0);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile1.build());
        textureProfile.addPlatforms(platformProfile2.build());

        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), true);

        assertEquals(3, result.textureImage.getAlternativesCount());

        // PVR will result in square textures

        // NOTE: We've currently disabled the PVRTC/ETC compression, and use UASTC as default instead

        //assertEquals(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1, texture.getAlternatives(0).getFormat());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, result.textureImage.getAlternatives(0).getFormat());
        assertEquals(16, result.textureImage.getAlternatives(0).getWidth());
        assertEquals(8, result.textureImage.getAlternatives(0).getHeight());

        //assertEquals(TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1, texture.getAlternatives(1).getFormat());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGBA, result.textureImage.getAlternatives(1).getFormat());
        assertEquals(16, result.textureImage.getAlternatives(1).getWidth());
        assertEquals(8, result.textureImage.getAlternatives(1).getHeight());


        //assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, texture.getAlternatives(2).getFormat());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, result.textureImage.getAlternatives(2).getFormat());
        assertEquals(128, result.textureImage.getAlternatives(2).getWidth());
        assertEquals(64, result.textureImage.getAlternatives(2).getHeight());
    }


    @Test
    public void testTextureProfilesPVRSquare() throws TextureGeneratorException, IOException {

        // Create a texture profile with texture compression
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.FAST);

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), true);

        // PVR will result in square textures
        assertEquals(128, result.textureImage.getAlternatives(0).getWidth());
        assertEquals(64, result.textureImage.getAlternatives(0).getHeight());

        // NOTE: We've currently disabled the PVRTC/ETC compression, and use UASTC as default instead
        //assertEquals(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1, texture.getAlternatives(0).getFormat());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, result.textureImage.getAlternatives(0).getFormat());
    }


    @Test
    public void testOptimalFormat() throws TextureGeneratorException, IOException {

        // Create a texture profile with texture compression
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.FAST);

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgb.png"), textureProfile.build(), true);

        // NOTE: We've currently disabled the PVRTC/ETC compression, and use UASTC as default instead
        // If input has less channels than target format, it should use a format in the same family with fewer channels (if available).
        //assertEquals(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1, texture.getAlternatives(0).getFormat());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, result.textureImage.getAlternatives(0).getFormat());
    }


    @Test
    public void testOptimalFormatLUM2LUM() throws TextureGeneratorException, IOException {

        // Create a texture profile with texture compression
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_RGBA);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.FAST);

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_lum.png"), textureProfile.build(), true);

        // If input has less channels than target format (and is uncompressed) pick the format with equal components to not waste memory.
        assertEquals(TextureFormat.TEXTURE_FORMAT_LUMINANCE, result.textureImage.getAlternatives(0).getFormat());
        assertEquals(128*64*1, result.imageDatas.get(0).length);

    }

    @Test
    public void testOptimalFormatRGB2LUM() throws TextureGeneratorException, IOException {

        // Create a texture profile with texture compression
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_LUMINANCE);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.FAST);

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgb.png"), textureProfile.build(), true);

        // If input has more channels than target format (and is uncompressed) discard channels.
        assertEquals(TextureFormat.TEXTURE_FORMAT_LUMINANCE, result.textureImage.getAlternatives(0).getFormat());
        assertEquals(128*64*1, result.imageDatas.get(0).length);

    }

    @Test
    public void test16BPP() throws TextureGeneratorException, IOException {
        // Create a texture profile with texture compression
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt2 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_RGB_16BPP);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.FAST);
        textureFormatAlt2.setFormat(TextureFormat.TEXTURE_FORMAT_RGBA_16BPP);
        textureFormatAlt2.setCompressionLevel(CompressionLevel.FAST);

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.addFormats(textureFormatAlt2.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), true);

        // If input has more channels than target format (and is uncompressed) discard channels.
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB_16BPP, result.textureImage.getAlternatives(0).getFormat());
        assertEquals(128*64*2, result.imageDatas.get(0).length);

        assertEquals(TextureFormat.TEXTURE_FORMAT_RGBA_16BPP, result.textureImage.getAlternatives(1).getFormat());
        assertEquals(128*64*2, result.imageDatas.get(1).length);
    }

    @Test
    public void testHavingProfileWithNoCompression() throws TextureGeneratorException, IOException {
        // Create a texture profile with texture compression
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt2 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_LUMINANCE);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.FAST);
        textureFormatAlt1.setCompressionType(Graphics.TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_UASTC);

        textureFormatAlt2.setFormat(TextureFormat.TEXTURE_FORMAT_RGB);
        textureFormatAlt2.setCompressionLevel(CompressionLevel.FAST);
        textureFormatAlt2.setCompressionType(Graphics.TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_UASTC);

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.addFormats(textureFormatAlt2.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        // Run the generator WITHOUT compression so we force a default profile
        TextureGenerator.GenerateResult result = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), false);

        assertEquals(TextureFormat.TEXTURE_FORMAT_LUMINANCE, result.textureImage.getAlternatives(0).getFormat());
        assertEquals(128*64, result.textureImage.getAlternatives(0).getDataSize());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, result.textureImage.getAlternatives(1).getFormat());
        assertEquals(128*64*3, result.textureImage.getAlternatives(1).getDataSize());
    }
}
