package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.awt.image.BufferedImage;
import java.io.IOException;
import java.util.ArrayList;
import java.util.EnumSet;

import org.junit.Test;

import com.dynamo.bob.util.TextureUtil;
import com.dynamo.bob.Platform;
import com.dynamo.bob.TexcLibrary.FlipAxis;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.TextureFormatAlternative;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureFormatAlternative.CompressionLevel;
import com.dynamo.graphics.proto.Graphics.TextureImage.CompressionType;
import com.dynamo.graphics.proto.Graphics.TextureImage.Image;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;
import com.dynamo.graphics.proto.Graphics.TextureProfile;

public class TextureGeneratorTest {

    // Create a 2x2 image where the first pixel is black, and the rest white.
    static BufferedImage createFlipTestImage()
    {
        BufferedImage srcImage = new BufferedImage(2, 2, BufferedImage.TYPE_4BYTE_ABGR);

        int whitePixel = (255 << 24) | (255 << 16) | (255 << 8) | (255 << 0);
        int blackPixel = (255 << 24) | (0 << 16) | (0 << 8) | (0 << 0);
        srcImage.setRGB(0, 0, blackPixel);
        srcImage.setRGB(1, 0, whitePixel);
        srcImage.setRGB(0, 1, whitePixel);
        srcImage.setRGB(1, 1, whitePixel);

        return srcImage;
    }

    // Asserts that the pixel at x and y is of a certain color.
    static void assertPixel(Image image, int x, int y, int color)
    {
        int width = image.getWidth();
        int offset = (width*y + x)*4;
        assertEquals((byte) (color >> 0), image.getData().byteAt(offset+0));
        assertEquals((byte) (color >> 8), image.getData().byteAt(offset+1));
        assertEquals((byte) (color >> 16), image.getData().byteAt(offset+2));
        assertEquals((byte) (color >> 24), image.getData().byteAt(offset+3));
    }

    // Assert that a pixel at x and y is black.
    static void assertBlackPixel(Image image, int x, int y)
    {
        int blackPixel = (255 << 24) | (0 << 16) | (0 << 8) | (0 << 0);
        assertPixel(image, x, y, blackPixel);
    }

    // Assert that a pixel at x and y is white.
    static void assertWhitePixel(Image image, int x, int y)
    {
        int whitePixel = (255 << 24) | (255 << 16) | (255 << 8) | (255 << 0);
        assertPixel(image, x, y, whitePixel);
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

        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"));

        assertEquals(1, texture.getAlternativesCount());
        Image image = texture.getAlternatives(0);
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
        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgb.png"));

        assertEquals(1, texture.getAlternativesCount());
        Image image = texture.getAlternatives(0);
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, image.getFormat());
    }

    @Test
    public void testLuminance() throws TextureGeneratorException, IOException {
        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_lum.png"));

        assertEquals(1, texture.getAlternativesCount());
        Image image = texture.getAlternatives(0);
        assertEquals(TextureFormat.TEXTURE_FORMAT_LUMINANCE, image.getFormat());
    }

    @Test
    public void testLuminanceAlpha() throws TextureGeneratorException, IOException {
        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_luma.png"));

        assertEquals(1, texture.getAlternativesCount());
        Image image = texture.getAlternatives(0);
        assertEquals(TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA, image.getFormat());
    }

    @Test
    public void testIndexed() throws TextureGeneratorException, IOException {
        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_idx.png"));

        assertEquals(1, texture.getAlternativesCount());
        Image image = texture.getAlternatives(0);
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

        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("btn_next_level.png"));

        assertEquals(1, texture.getAlternativesCount());
        Image image = texture.getAlternatives(0);
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
        assertEquals(offset, image.getData().size());
    }

    @Test
    public void testClosestPowerTwoScale() throws TextureGeneratorException, IOException {
        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("127_65_rgba.png"));

        Image image = texture.getAlternatives(0);
        assertEquals(128, image.getWidth());
        assertEquals(64, image.getHeight());
        assertEquals(127, image.getOriginalWidth());
        assertEquals(65, image.getOriginalHeight());
    }

    @Test
    public void testCompile16BitPerChannelTexture() throws TextureGeneratorException, IOException {
        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("16_bit_texture.png"));

        Image image = texture.getAlternatives(0);
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
        TextureImage texture = TextureGenerator.generate(srcImage, null, true);

        Image image = texture.getAlternatives(0);
        assertEquals((byte) 0, image.getData().byteAt(0));
        assertEquals((byte) 0, image.getData().byteAt(1));
        assertEquals((byte) 0, image.getData().byteAt(2));
        assertEquals((byte) 0, image.getData().byteAt(3));
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

        TextureImage texture = TextureGenerator.generate(srcImage, textureProfile.build(), true);

        Image image = texture.getAlternatives(0);
        assertEquals((byte) 255,   image.getData().byteAt(0));
        assertEquals((byte) 255, image.getData().byteAt(1));
        assertEquals((byte) 255, image.getData().byteAt(2));
        assertEquals((byte) 0, image.getData().byteAt(3));
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

        // Generate texture withput compression applied
        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), false);

        assertEquals(1, texture.getAlternativesCount());
        assertEquals(128, texture.getAlternatives(0).getWidth());
        assertEquals(64, texture.getAlternatives(0).getHeight());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, texture.getAlternatives(0).getFormat());
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

        // Generate texture withput compression applied
        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), false);

        assertEquals(1, texture.getAlternativesCount());
        assertEquals(128, texture.getAlternatives(0).getWidth());
        assertEquals(64, texture.getAlternatives(0).getHeight());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGBA, texture.getAlternatives(0).getFormat());
    }

    @Test
    public void testNoFlip() throws TextureGeneratorException, IOException {
        TextureImage texture = TextureGenerator.generate(createFlipTestImage(), null, false, EnumSet.noneOf(FlipAxis.class));

        Image image = texture.getAlternatives(0);
        assertBlackPixel(image, 0, 0);
        assertWhitePixel(image, 1, 0);
        assertWhitePixel(image, 0, 1);
        assertWhitePixel(image, 1, 1);
    }

    @Test
    public void testFlipX() throws TextureGeneratorException, IOException {
        TextureImage texture = TextureGenerator.generate(createFlipTestImage(), null, false, EnumSet.of(FlipAxis.FLIP_AXIS_X));

        Image image = texture.getAlternatives(0);
        assertWhitePixel(image, 0, 0);
        assertBlackPixel(image, 1, 0);
        assertWhitePixel(image, 0, 1);
        assertWhitePixel(image, 1, 1);
    }

    @Test
    public void testFlipY() throws TextureGeneratorException, IOException {
        TextureImage texture = TextureGenerator.generate(createFlipTestImage(), null, false, EnumSet.of(FlipAxis.FLIP_AXIS_Y));

        Image image = texture.getAlternatives(0);
        assertWhitePixel(image, 0, 0);
        assertWhitePixel(image, 1, 0);
        assertBlackPixel(image, 0, 1);
        assertWhitePixel(image, 1, 1);
    }

    @Test
    public void testFlipXY() throws TextureGeneratorException, IOException {
        TextureImage texture = TextureGenerator.generate(createFlipTestImage(), null, false, EnumSet.of(FlipAxis.FLIP_AXIS_X, FlipAxis.FLIP_AXIS_Y));

        Image image = texture.getAlternatives(0);
        assertWhitePixel(image, 0, 0);
        assertWhitePixel(image, 1, 0);
        assertWhitePixel(image, 0, 1);
        assertBlackPixel(image, 1, 1);
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

        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), true);

        assertEquals(1, texture.getAlternativesCount());
        assertEquals(128, texture.getAlternatives(0).getWidth());
        assertEquals(64, texture.getAlternatives(0).getHeight());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB_ETC1, texture.getAlternatives(0).getFormat());

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

        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), true);

        assertEquals(16, texture.getAlternatives(0).getWidth());
        assertEquals(8, texture.getAlternatives(0).getHeight());

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

        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), true);

        assertEquals(3, texture.getAlternativesCount());

        // PVR will result in square textures
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1, texture.getAlternatives(0).getFormat());
        assertEquals(16, texture.getAlternatives(0).getWidth());
        assertEquals(16, texture.getAlternatives(0).getHeight());

        assertEquals(TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1, texture.getAlternatives(1).getFormat());
        assertEquals(16, texture.getAlternatives(1).getWidth());
        assertEquals(16, texture.getAlternatives(1).getHeight());


        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, texture.getAlternatives(2).getFormat());
        assertEquals(128, texture.getAlternatives(2).getWidth());
        assertEquals(64, texture.getAlternatives(2).getHeight());

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

        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), true);

        // PVR will result in square textures
        assertEquals(128, texture.getAlternatives(0).getWidth());
        assertEquals(128, texture.getAlternatives(0).getHeight());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1, texture.getAlternatives(0).getFormat());

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

        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgb.png"), textureProfile.build(), true);

        // If input has less channels than target format, it should use a format in the same family with fewer channels (if available).
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1, texture.getAlternatives(0).getFormat());

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

        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_lum.png"), textureProfile.build(), true);

        // If input has less channels than target format (and is uncompressed) pick the format with equal components to not waste memory.
        assertEquals(TextureFormat.TEXTURE_FORMAT_LUMINANCE, texture.getAlternatives(0).getFormat());
        assertEquals(128*64*1, texture.getAlternatives(0).getData().toByteArray().length);

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

        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgb.png"), textureProfile.build(), true);

        // If input has more channels than target format (and is uncompressed) discard channels.
        assertEquals(TextureFormat.TEXTURE_FORMAT_LUMINANCE, texture.getAlternatives(0).getFormat());
        assertEquals(128*64*1, texture.getAlternatives(0).getData().toByteArray().length);

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

        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), true);

        // If input has more channels than target format (and is uncompressed) discard channels.
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB_16BPP, texture.getAlternatives(0).getFormat());
        assertEquals(128*64*2, texture.getAlternatives(0).getData().toByteArray().length);
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGBA_16BPP, texture.getAlternatives(1).getFormat());
        assertEquals(128*64*2, texture.getAlternatives(1).getData().toByteArray().length);
    }


    @Test
    public void testWEBP() throws TextureGeneratorException, IOException {

        // Create texture profiles with texture compression for webp lossy and lossless
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        textureProfile.setName("Test Profile");

        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);

        ArrayList<TextureFormat> formats = new ArrayList<TextureFormat>();
        formats.add(TextureFormat.TEXTURE_FORMAT_LUMINANCE);
        formats.add(TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA);
        formats.add(TextureFormat.TEXTURE_FORMAT_RGB);
        formats.add(TextureFormat.TEXTURE_FORMAT_RGB_16BPP);
        formats.add(TextureFormat.TEXTURE_FORMAT_RGB_ETC1);
        formats.add(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1);
        formats.add(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1);
        formats.add(TextureFormat.TEXTURE_FORMAT_RGBA);
        formats.add(TextureFormat.TEXTURE_FORMAT_RGBA_16BPP);
        formats.add(TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1);
        formats.add(TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1);
        for(TextureFormat format : formats)
        {
            TextureFormatAlternative.Builder textureFormatAlt = TextureFormatAlternative.newBuilder();
            textureFormatAlt.setFormat(format);
            textureFormatAlt.setCompressionLevel(CompressionLevel.FAST);
            textureFormatAlt.setCompressionType(CompressionType.COMPRESSION_TYPE_WEBP);
            platformProfile.addFormats(textureFormatAlt.build());
        }

        ArrayList<TextureFormat> formats_lossy = new ArrayList<TextureFormat>();
        formats_lossy.add(TextureFormat.TEXTURE_FORMAT_LUMINANCE);
        formats_lossy.add(TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA);
        formats_lossy.add(TextureFormat.TEXTURE_FORMAT_RGB);
        formats_lossy.add(TextureFormat.TEXTURE_FORMAT_RGB_16BPP);
        formats_lossy.add(TextureFormat.TEXTURE_FORMAT_RGBA);
        formats_lossy.add(TextureFormat.TEXTURE_FORMAT_RGBA_16BPP);
        for(TextureFormat format : formats_lossy)
        {
            TextureFormatAlternative.Builder textureFormatAlt = TextureFormatAlternative.newBuilder();
            textureFormatAlt.setFormat(format);
            textureFormatAlt.setCompressionLevel(CompressionLevel.FAST);
            textureFormatAlt.setCompressionType(CompressionType.COMPRESSION_TYPE_WEBP_LOSSY);
            platformProfile.addFormats(textureFormatAlt.build());
        }

        textureProfile.addPlatforms(platformProfile.build());
        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgba.png"), textureProfile.build(), true);

        int formatIndex = 0;
        for(TextureFormat format : formats)
        {
            assertEquals(format, texture.getAlternatives(formatIndex).getFormat());
            assertTrue(texture.getAlternatives(formatIndex).getMipMapSizeCompressed(0) > 0);
            assertTrue(texture.getAlternatives(formatIndex).getMipMapSizeCompressed(0) < texture.getAlternatives(formatIndex).getMipMapSize(0));
            formatIndex++;
        }
        for(TextureFormat format : formats_lossy)
        {
            assertEquals(format, texture.getAlternatives(formatIndex).getFormat());
            assertTrue(texture.getAlternatives(formatIndex).getMipMapSizeCompressed(0) > 0);
            assertTrue(texture.getAlternatives(formatIndex).getMipMapSizeCompressed(0) < texture.getAlternatives(formatIndex).getMipMapSize(0));
            formatIndex++;
        }
    }

    @Test
    public void testWEBPMipMapped() throws TextureGeneratorException, IOException {

        // Create texture profiles with texture compression for webp lossy, lossless and with mipmaps
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt2 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_RGBA);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.FAST);
        textureFormatAlt1.setCompressionType(CompressionType.COMPRESSION_TYPE_WEBP);
        textureFormatAlt2.setFormat(TextureFormat.TEXTURE_FORMAT_RGBA);
        textureFormatAlt2.setCompressionLevel(CompressionLevel.FAST);
        textureFormatAlt2.setCompressionType(CompressionType.COMPRESSION_TYPE_WEBP_LOSSY);

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.addFormats(textureFormatAlt2.build());
        platformProfile.setMipmaps(true);
        platformProfile.setMaxTextureSize(0);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());

        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("btn_next_level.png"), textureProfile.build(), true);
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGBA, texture.getAlternatives(0).getFormat());
        assertTrue(texture.getAlternatives(0).getMipMapSizeCompressed(0) > texture.getAlternatives(0).getMipMapSizeCompressed(1));
        assertTrue(texture.getAlternatives(0).getMipMapSizeCompressed(1) > texture.getAlternatives(0).getMipMapSizeCompressed(2));
        assertEquals(0, texture.getAlternatives(0).getMipMapSizeCompressed(texture.getAlternatives(0).getMipMapSizeCount()-1)); // ensure small mipmaps aren't compressed
        assertTrue(texture.getAlternatives(1).getMipMapSizeCompressed(0) > texture.getAlternatives(1).getMipMapSizeCompressed(1));
        assertTrue(texture.getAlternatives(1).getMipMapSizeCompressed(1) > texture.getAlternatives(1).getMipMapSizeCompressed(2));
        assertEquals(0, texture.getAlternatives(1).getMipMapSizeCompressed(texture.getAlternatives(1).getMipMapSizeCount()-1));
    }

    /*
     JIRA issue: DEF-994
    @Test
    public void testDXTCompress() throws TextureGeneratorException, IOException {

        // Create a texture profile with texture compression
        TextureProfile.Builder textureProfile = TextureProfile.newBuilder();
        PlatformProfile.Builder platformProfile = PlatformProfile.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt1 = TextureFormatAlternative.newBuilder();
        TextureFormatAlternative.Builder textureFormatAlt2 = TextureFormatAlternative.newBuilder();

        textureFormatAlt1.setFormat(TextureFormat.TEXTURE_FORMAT_RGB_DXT1);
        textureFormatAlt1.setCompressionLevel(CompressionLevel.FAST);
        textureFormatAlt2.setFormat(TextureFormat.TEXTURE_FORMAT_RGB);
        textureFormatAlt2.setCompressionLevel(CompressionLevel.FAST);

        platformProfile.setOs(PlatformProfile.OS.OS_ID_GENERIC);
        platformProfile.addFormats(textureFormatAlt1.build());
        platformProfile.addFormats(textureFormatAlt2.build());
        platformProfile.setMipmaps(false);
        platformProfile.setMaxTextureSize(0);

        textureProfile.setName("Test Profile");
        textureProfile.addPlatforms(platformProfile.build());


        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgb.png"), textureProfile.build());

        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB_DXT1, texture.getAlternatives(0).getFormat());
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, texture.getAlternatives(1).getFormat());
        assertEquals(2, texture.getAlternativesCount());

    }
    */

}
