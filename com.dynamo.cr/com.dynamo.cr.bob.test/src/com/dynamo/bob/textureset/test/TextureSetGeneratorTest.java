package com.dynamo.bob.textureset.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.apache.commons.lang3.Pair;
import org.junit.Test;

import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimDesc;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimIterator;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet.Builder;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.tile.proto.Tile.Playback;

public class TextureSetGeneratorTest {

    public static class MappedAnimDesc extends AnimDesc {
        List<String> ids;

        public MappedAnimDesc(String id, List<String> ids, Playback playback, int fps, boolean flipHorizontal,
                boolean flipVertical) {
            super(id, playback, fps, flipHorizontal, flipVertical);
            this.ids = ids;
        }

        public MappedAnimDesc(String id, List<String> ids) {
            super(id, Playback.PLAYBACK_NONE, 0, false, false);
            this.ids = ids;
        }

        public List<String> getIds() {
            return this.ids;
        }
    }

    public static class MappedAnimIterator implements AnimIterator {
        final List<MappedAnimDesc> anims;
        final List<String> imageIds;
        int nextAnimIndex;
        int nextFrameIndex;

        public MappedAnimIterator(List<MappedAnimDesc> anims, List<String> imageIds) {
            this.anims = anims;
            this.imageIds = imageIds;
        }

        @Override
        public AnimDesc nextAnim() {
            if (nextAnimIndex < anims.size()) {
                nextFrameIndex = 0;
                return anims.get(nextAnimIndex++);
            }
            return null;
        }

        @Override
        public Integer nextFrameIndex() {
            MappedAnimDesc anim = anims.get(nextAnimIndex - 1);
            if (nextFrameIndex < anim.getIds().size()) {
                return imageIds.indexOf(anim.getIds().get(nextFrameIndex++));
            }
            return null;
        }

        @Override
        public void rewind() {
            nextAnimIndex = 0;
            nextFrameIndex = 0;
        }
    }

    BufferedImage newImage(int w, int h) {
        return new BufferedImage(w, h, BufferedImage.TYPE_4BYTE_ABGR);
    }

    MappedAnimDesc newAnim(String id, List<String> ids) {
        return new MappedAnimDesc(id, ids, Playback.PLAYBACK_LOOP_BACKWARD, 30, false, false);
    }

    @Test
    public void test1() {
        List<BufferedImage> images =
                Arrays.asList(newImage(16, 16),
                              newImage(16, 16),
                              newImage(16, 16),
                              newImage(16, 16));

        List<String> ids = Arrays.asList("1", "2", "3", "4");

        List<MappedAnimDesc> animations = new ArrayList<MappedAnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1", "2", "3")));
        animations.add(newAnim("anim2", Arrays.asList("2", "4")));

        MappedAnimIterator iterator = new MappedAnimIterator(animations, ids);

        Pair<Builder, BufferedImage> pair = TextureSetGenerator.generate(images, iterator, 0, 0, false);
        TextureSet textureSet = pair.left.setTexture("").build();
        BufferedImage image = pair.right;
        assertThat(image.getWidth(), is(32));
        assertThat(image.getHeight(), is(32));
        assertThat(textureSet.getAnimationsCount(), is(2));

        assertThat(getVertexCount(textureSet, "anim1", 0), is(6));
        assertThat(getVertexCount(textureSet, "anim2", 0), is(6));
    }

    @Test
    public void testAtlasGenerator() throws Exception {
        List<BufferedImage> images = Arrays.asList(newImage(16, 16), newImage(16, 16), newImage(16, 16),
                newImage(16, 16));

        List<String> ids = Arrays.asList("1", "2", "3", "4");

        List<MappedAnimDesc> animations = new ArrayList<MappedAnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1", "2", "3")));
        animations.add(newAnim("anim2", Arrays.asList("2", "4")));

        MappedAnimIterator iterator = new MappedAnimIterator(animations, ids);

        Pair<Builder, BufferedImage> pair = TextureSetGenerator.generate(images, iterator, 0, 0, false);
        BufferedImage image = pair.right;
        assertThat(image.getWidth(), is(32));
        assertThat(image.getHeight(), is(32));

        TextureSet textureSet = pair.left.setTexture("").build();

        assertThat(textureSet.getAnimationsCount(), is(2));
        // NOTE: We currently assume 6 vertices per frame
        // and vertex-format described below
        int vertex_size = 4 * 4; // x, y, z, u, v (3 * float + 2 * short)
        int vertices_per_frame = 6; // two triangles
        assertThat(textureSet.getVertices().size(), is(vertex_size * vertices_per_frame * (4 + 3 + 2)));
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
