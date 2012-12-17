package com.dynamo.bob.atlas.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.junit.Test;

import com.dynamo.bob.atlas.AtlasGenerator;
import com.dynamo.bob.atlas.AtlasMap;
import com.dynamo.bob.atlas.AtlasGenerator.AnimDesc;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.tile.proto.Tile.Playback;

public class AtlasGeneratorTest {

    BufferedImage newImage(int w, int h) {
        return new BufferedImage(w, h, BufferedImage.TYPE_4BYTE_ABGR);
    }

    AnimDesc newAnim(String id, List<String> ids) {
        return new AnimDesc(id, ids, Playback.PLAYBACK_LOOP_BACKWARD, 30, false, false);
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
        animations.add(newAnim("anim1", Arrays.asList("1", "2", "3")));
        animations.add(newAnim("anim2", Arrays.asList("2", "4")));

        AtlasMap atlas = AtlasGenerator.generate(images, ids, animations, 0);
        BufferedImage image = atlas.getImage();
        assertThat(image.getWidth(), is(32));
        assertThat(image.getHeight(), is(32));
        assertThat(atlas.getAnimations().size(), is(4 + 2));

        // NOTE: We currently assume 6 vertices per frame
        assertThat(atlas.getVertexCount(getAnim(atlas, "1")), is(6));
        assertThat(atlas.getVertexCount(getAnim(atlas, "2")), is(6));
        assertThat(atlas.getVertexCount(getAnim(atlas, "3")), is(6));
        assertThat(atlas.getVertexCount(getAnim(atlas, "4")), is(6));
        assertThat(getAnim(atlas, "1").getIsAnimation(), is(0));

        assertThat(atlas.getVertexCount(getAnim(atlas, "anim1")), is(6 * 3));
        assertThat(atlas.getVertexCount(getAnim(atlas, "anim2")), is(6 * 2));
        assertThat(getAnim(atlas, "anim1").getIsAnimation(), is(1));
    }

    private TextureSetAnimation getAnim(AtlasMap atlas, String id) {
        for (TextureSetAnimation a : atlas.getAnimations()) {
            if (a.getId().equals(id)) {
                return a;
            }
        }
        return null;

    }

}
