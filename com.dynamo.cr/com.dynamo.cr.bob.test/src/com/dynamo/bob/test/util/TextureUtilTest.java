package com.dynamo.bob.test.util;

import static org.junit.Assert.assertEquals;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

import javax.imageio.ImageIO;

import org.junit.Test;

import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.PathSettings;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.dynamo.graphics.proto.Graphics.TextureProfiles;

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

    @Test
    public void testCreatePaddedImage() throws FileNotFoundException, IOException {
        final String sourceFile = "test/apply_inner_padding.png";
        final int paddingAmount = 2;
        final Color paddingColour = new Color(0, 0, 0, 0);
        BufferedImage image = ImageIO.read(new FileInputStream(sourceFile));

        int srcWidth = image.getWidth();
        int srcHeight = image.getHeight();

        BufferedImage paddedImage = TextureUtil.createPaddedImage(image, paddingAmount, paddingColour);

        int width = paddedImage.getWidth();
        int height = paddedImage.getHeight();

        assertEquals(width, srcWidth + 2 * paddingAmount);
        assertEquals(height, srcHeight + 2 * paddingAmount);

        int paddingRGBA = paddingColour.getRGB();
        for (int y=0; y<height; ++y) {
            for (int x=0; x<width; ++x) {
                boolean isPadding = false;
                int outputColour = paddedImage.getRGB(x, y);

                int sx = x;
                int sy = y;
                if (x < paddingAmount || (x >= srcWidth + paddingAmount)) {
                    isPadding = true;
                } else {
                    sx = x - paddingAmount;
                }
                if (y < paddingAmount || (y >= srcHeight + paddingAmount)) {
                    isPadding = true;
                } else {
                    sy = y - paddingAmount;
                }

                if (isPadding) {
                    assertEquals(paddingRGBA, outputColour);
                } else {
                    int comparisonColour = image.getRGB(sx, sy);
                    boolean comparisonTransparent = 0 == ((comparisonColour>>24)&0xff);
                    if (comparisonTransparent) {
                        assertEquals(outputColour, paddingRGBA);
                    } else {
                        assertEquals(outputColour, comparisonColour);
                    }
                }
            }
        }
    }

    @Test
    public void testTextureProfilePaths() throws FileNotFoundException, IOException {

        // Create a texture profile with a max texture size
        PathSettings p1 = PathSettings.newBuilder().setProfile("match1").setPath("/**/test.png").build();
        PathSettings p2 = PathSettings.newBuilder().setProfile("match2").setPath("/**/*.jpg").build();
        PathSettings p3 = PathSettings.newBuilder().setProfile("match3").setPath("/**/subdir/**/*.txt").build();
        PathSettings p4 = PathSettings.newBuilder().setProfile("match4").setPath("**").build();
        TextureProfiles texProfiles = TextureProfiles.newBuilder()
                .addPathSettings(p1)
                .addPathSettings(p2)
                .addPathSettings(p3)
                .addPathSettings(p4)
                .addProfiles(TextureProfile.newBuilder().setName("match1").build())
                .addProfiles(TextureProfile.newBuilder().setName("match2").build())
                .addProfiles(TextureProfile.newBuilder().setName("match3").build())
                .addProfiles(TextureProfile.newBuilder().setName("match4").build())
                .build();

        assertEquals("match1", TextureUtil.getTextureProfileByPath(texProfiles, "test.png" ).getName());
        assertEquals("match1", TextureUtil.getTextureProfileByPath(texProfiles, "a/test.png" ).getName());
        assertEquals("match1", TextureUtil.getTextureProfileByPath(texProfiles, "a/b/test.png" ).getName());

        assertEquals("match2", TextureUtil.getTextureProfileByPath(texProfiles, "test.jpg" ).getName());
        assertEquals("match2", TextureUtil.getTextureProfileByPath(texProfiles, "a/test.jpg" ).getName());
        assertEquals("match2", TextureUtil.getTextureProfileByPath(texProfiles, "a/b/c.jpg" ).getName());

        assertEquals("match3", TextureUtil.getTextureProfileByPath(texProfiles, "subdir/a.txt" ).getName());
        assertEquals("match3", TextureUtil.getTextureProfileByPath(texProfiles, "a/subdir/b.txt" ).getName());
        assertEquals("match3", TextureUtil.getTextureProfileByPath(texProfiles, "a/subdir/b/c.txt" ).getName());

        assertEquals("match4", TextureUtil.getTextureProfileByPath(texProfiles, "a.bin" ).getName());
        assertEquals("match4", TextureUtil.getTextureProfileByPath(texProfiles, "a/a.bin" ).getName());
        assertEquals("match4", TextureUtil.getTextureProfileByPath(texProfiles, "a/b/c.bin" ).getName());
    }

}
