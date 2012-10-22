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
}
