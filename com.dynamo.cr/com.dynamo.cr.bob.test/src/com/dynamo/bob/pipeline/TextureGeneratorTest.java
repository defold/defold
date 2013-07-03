package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;

import java.awt.image.BufferedImage;
import java.io.IOException;

import org.junit.Test;

import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureImage.Image;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;

public class TextureGeneratorTest {

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
    public void testLuminance() throws TextureGeneratorException, IOException {
        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_lum.png"));

        assertEquals(1, texture.getAlternativesCount());
        Image image = texture.getAlternatives(0);
        assertEquals(TextureFormat.TEXTURE_FORMAT_LUMINANCE, image.getFormat());
    }

    @Test
    public void testRGB() throws TextureGeneratorException, IOException {
        TextureImage texture = TextureGenerator.generate(getClass().getResourceAsStream("128_64_rgb.png"));

        assertEquals(1, texture.getAlternativesCount());
        Image image = texture.getAlternatives(0);
        assertEquals(TextureFormat.TEXTURE_FORMAT_RGB, image.getFormat());
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
        TextureImage texture = TextureGenerator.generate(srcImage);

        Image image = texture.getAlternatives(0);
        assertEquals((byte) 0, image.getData().byteAt(0));
        assertEquals((byte) 0, image.getData().byteAt(1));
        assertEquals((byte) 0, image.getData().byteAt(2));
        assertEquals((byte) 0, image.getData().byteAt(3));
    }

}
