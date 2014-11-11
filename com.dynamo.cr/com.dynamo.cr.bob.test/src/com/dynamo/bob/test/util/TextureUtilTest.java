package com.dynamo.bob.test.util;

import static org.junit.Assert.assertEquals;

import java.awt.image.BufferedImage;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

import javax.imageio.ImageIO;

import org.junit.Test;

import com.dynamo.bob.util.TextureUtil;

public class TextureUtilTest {

    @Test
    public void testExtrudeBorders() {
        int red = (255 << 24) | (255 << 16);
        // 1 dim with 2 border
        BufferedImage src = new BufferedImage(1, 1, BufferedImage.TYPE_INT_ARGB);
        src.setRGB(0, 0, red);
        BufferedImage tgt = TextureUtil.extrudeBorders(src, 2);
        assertEquals(5, tgt.getWidth());
        assertEquals(5, tgt.getHeight());
        for (int y = 0; y < 5; ++y) {
            for (int x = 0; x < 5; ++x) {
                assertEquals(red, tgt.getRGB(x, y));
            }
        }
        // 2 dim with 1 border
        int green = (255 << 24) | (255 << 8);
        int blue = (255 << 24) | (255);
        int black = (255 << 24);
        src = new BufferedImage(2, 2, BufferedImage.TYPE_INT_ARGB);
        src.setRGB(0, 0, red);
        src.setRGB(1, 0, green);
        src.setRGB(0, 1, blue);
        src.setRGB(1, 1, black);
        tgt = TextureUtil.extrudeBorders(src, 1);
        assertEquals(4, tgt.getWidth());
        assertEquals(4, tgt.getHeight());
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                int sx = Math.min(Math.max(x - 1, 0), 1);
                int sy = Math.min(Math.max(y - 1, 0), 1);
                assertEquals(src.getRGB(sx, sy), tgt.getRGB(x, y));
            }
        }
    }

    @Test
    public void testDepalettiseImage() throws FileNotFoundException, IOException {
        final String palettisedFile = "test/def_854_with_palette.png";
        BufferedImage palettisedImage = ImageIO.read(new FileInputStream(palettisedFile));

        assert(BufferedImage.TYPE_BYTE_INDEXED == palettisedImage.getType());

        int width = palettisedImage.getWidth();
        int height = palettisedImage.getHeight();

        BufferedImage depalettisedImage = TextureUtil.depalettiseImage(palettisedImage);

        int[] palettisedPixels = new int[width * height];
        int[] depalettisedPixels = new int[width * height];

        palettisedImage.getRGB(0, 0, width, height, palettisedPixels, 0, width);
        depalettisedImage.getRGB(0, 0, width, height, depalettisedPixels, 0, width);

        for (int i=0; i<palettisedPixels.length; ++i) {
            assertEquals(palettisedPixels[i], depalettisedPixels[i]);
        }
    }
}
