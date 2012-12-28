package com.dynamo.bob.atlas.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.apache.commons.lang3.Pair;
import org.junit.Test;

import com.dynamo.bob.atlas.AtlasGenerator;
import com.dynamo.bob.atlas.AtlasGenerator.AnimDesc;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet.Builder;
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

        Pair<Builder, BufferedImage> pair = AtlasGenerator.generate(images, ids, animations, 0);
        TextureSet textureSet = pair.left.setTexture("").build();
        BufferedImage image = pair.right;
        assertThat(image.getWidth(), is(32));
        assertThat(image.getHeight(), is(32));
        assertThat(textureSet.getAnimationsCount(), is(4 + 2));


        // NOTE: We currently assume 6 vertices per frame
        assertThat(getVertexCount(textureSet, "1", 0), is(6));
        assertThat(getVertexCount(textureSet, "2", 0), is(6));
        assertThat(getVertexCount(textureSet, "3", 0), is(6));
        assertThat(getVertexCount(textureSet, "4", 0), is(6));
        assertThat(getAnim(textureSet, "1").getIsAnimation(), is(0));

        assertThat(getVertexCount(textureSet, "anim1", 0), is(6));
        assertThat(getVertexCount(textureSet, "anim2", 0), is(6));

        assertThat(getAnim(textureSet, "anim1").getIsAnimation(), is(1));
    }

    private int getVertexCount(TextureSet textureSet, String id, int frame) {
        return textureSet.getVertexCount(getAnim(textureSet, id).getStart() + frame);
    }

    private TextureSetAnimation getAnim(TextureSet textureSet, String id) {
        for (TextureSetAnimation a : textureSet.getAnimationsList()) {
            if (a.getId().equals(id)) {
                return a;
            }
        }
        return null;

    }

}
