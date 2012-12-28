package com.dynamo.bob.pipeline;

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
import com.dynamo.tile.proto.Tile.Playback;

public class AtlasBuilderTest {

    BufferedImage newImage(int w, int h) {
        return new BufferedImage(w, h, BufferedImage.TYPE_4BYTE_ABGR);
    }

    AnimDesc newAnim(String id, List<String> ids) {
        return new AnimDesc(id, ids, Playback.PLAYBACK_LOOP_BACKWARD, 30, false, false);
    }

    @Test
    public void testAtlas() throws Exception {
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
        BufferedImage image = pair.right;
        assertThat(image.getWidth(), is(32));
        assertThat(image.getHeight(), is(32));

        TextureSet textureSet = pair.left.setTexture("").build();

        assertThat(textureSet.getAnimationsCount(), is(4 + 2));
        // NOTE: We currently assume 6 vertices per frame
        // and vertex-format described below
        int vertex_size = 4 * 4; // x, y, z, u, v (3 * float + 2 * short)
        int vertices_per_frame = 6; // two triangles
        assertThat(textureSet.getVertices().size(), is(vertex_size * vertices_per_frame * (4 + 3 + 2)));
    }
}
