package com.dynamo.bob.pipeline;

import static org.junit.Assert.*;

import java.io.IOException;

import org.junit.Test;

import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureImage.Image;

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
        assertEquals(128, TextureGenerator.closestPowerTwo(127));
        assertEquals(128, TextureGenerator.closestPowerTwo(129));

        assertEquals(1, TextureGenerator.closestPowerTwo(1));
        assertEquals(2, TextureGenerator.closestPowerTwo(2));
        assertEquals(4, TextureGenerator.closestPowerTwo(3));
        assertEquals(4, TextureGenerator.closestPowerTwo(5));
        assertEquals(8, TextureGenerator.closestPowerTwo(6));
        assertEquals(8, TextureGenerator.closestPowerTwo(7));
        assertEquals(8, TextureGenerator.closestPowerTwo(8));
        assertEquals(8, TextureGenerator.closestPowerTwo(9));
        assertEquals(8, TextureGenerator.closestPowerTwo(10));

    }

}
