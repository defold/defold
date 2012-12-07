package com.dynamo.cr.tileeditor.atlas;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.junit.Test;

import com.dynamo.cr.tileeditor.atlas.AtlasGenerator.AnimDesc;

public class AtlasGeneratorTest {

    BufferedImage newImage(int w, int h) {
        return new BufferedImage(w, h, BufferedImage.TYPE_4BYTE_ABGR);
    }

    @Test
    public void test1() {
        List<BufferedImage> images =
                Arrays.asList(newImage(16, 16),
                              newImage(16, 16),
                              newImage(16, 16),
                              newImage(16, 16));

        List<String> ids = Arrays.asList("1", "2", "3", "4");


        List<AnimDesc> animations = new ArrayList<AtlasGenerator.AnimDesc>();
        AtlasMap atlas = AtlasGenerator.generate(images, ids, animations, 0);
        BufferedImage image = atlas.getImage();
        assertThat(image.getWidth(), is(32));
        assertThat(image.getHeight(), is(32));
    }

}
