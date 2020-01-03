package com.dynamo.bob.textureset.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertEquals;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.junit.Test;

import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimDesc;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimIterator;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.bob.textureset.TextureSetGenerator.UVTransform;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.tile.proto.Tile.Playback;

public class TextureSetGeneratorTest {

    private static final float EPSILON = 0.000001f;

    public static class MappedAnimDesc extends AnimDesc {
        public List<String> ids;

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
        List<Integer> hullSizes = Arrays.asList(6, 6, 6, 6);

        List<MappedAnimDesc> animations = new ArrayList<MappedAnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1", "2", "3")));
        animations.add(newAnim("anim2", Arrays.asList("2", "4")));

        MappedAnimIterator iterator = new MappedAnimIterator(animations, ids);

        TextureSetResult result = TextureSetGenerator.generate(images, hullSizes, ids, iterator, 0, 0, 0, true, false, null);
        TextureSet textureSet = result.builder.setTexture("").build();
        BufferedImage image = result.image;
        assertThat(image.getWidth(), is(32));
        assertThat(image.getHeight(), is(32));
        assertThat(textureSet.getAnimationsCount(), is(2));

        assertThat(getIndexCount(textureSet, "anim1", 0), is(6));
        assertThat(getIndexCount(textureSet, "anim2", 0), is(6));
    }

    //@Test
    public void testAtlasGenerator() throws Exception {
        List<BufferedImage> images = Arrays.asList(newImage(16, 16), newImage(16, 16), newImage(16, 16),
                newImage(16, 16));

        List<String> ids = Arrays.asList("1", "2", "3", "4");
        List<Integer> hullSizes = Arrays.asList(0, 0, 0, 0);

        List<MappedAnimDesc> animations = new ArrayList<MappedAnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1", "2", "3")));
        animations.add(newAnim("anim2", Arrays.asList("2", "4")));

        MappedAnimIterator iterator = new MappedAnimIterator(animations, ids);

        TextureSetResult result = TextureSetGenerator.generate(images, hullSizes, ids, iterator, 0, 0, 0, true, false, null);
        BufferedImage image = result.image;
        assertThat(image.getWidth(), is(32));
        assertThat(image.getHeight(), is(32));

        TextureSet textureSet = result.builder.setTexture("").build();

        int sizeOfFloat = 4;
        int numFrames = ids.size() + animations.get(0).ids.size() + animations.get(1).ids.size();
        assertThat(textureSet.getAnimationsCount(), is(2));
        assertThat(textureSet.getTexDims().size() / sizeOfFloat, is(numFrames * 2)); // frame count * 2 floats (x, y)
    }

    //@Test
    public void testAtlasGeneratorMaxRectMargin() throws Exception {
        // These are 11x11 and with 5x5 padding added during generation, the output should be 64x16
        List<BufferedImage> images = Arrays.asList(newImage(11, 11), newImage(11, 11), newImage(11, 11),
                newImage(11, 11));

        List<String> ids = Arrays.asList("1", "2", "3", "4");
        List<Integer> hullSizes = Arrays.asList(0, 0, 0, 0);

        List<MappedAnimDesc> animations = new ArrayList<MappedAnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1", "2", "3")));
        animations.add(newAnim("anim2", Arrays.asList("2", "4")));

        MappedAnimIterator iterator = new MappedAnimIterator(animations, ids);

        TextureSetResult result = TextureSetGenerator.generate(images, hullSizes, ids, iterator, 5, 0, 0, true, false, null);
        BufferedImage image = result.image;
        assertThat(image.getWidth(), is(32));
        assertThat(image.getHeight(), is(32));

        for (UVTransform uv : result.uvTransforms)
        {
            // Same as original
            assertThat((int)(uv.scale.x * image.getWidth()), is(11));
            assertThat((int)(-uv.scale.y * image.getHeight()), is(11));
        }
    }

    //@Test
    public void testRotatedAnimations() {
        List<BufferedImage> images = Arrays.asList(newImage(64,32), newImage(64,32), newImage(32,64), newImage(32,64));

        List<String> ids = Arrays.asList("1", "2", "3", "4");
        List<Integer> hullSizes = Arrays.asList(0, 0, 0, 0);

        List<MappedAnimDesc> animations = new ArrayList<MappedAnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1","2")));
        animations.add(newAnim("anim2", Arrays.asList("3","4")));

        MappedAnimIterator iterator = new MappedAnimIterator(animations, ids);

        TextureSetResult result = TextureSetGenerator.generate(images, hullSizes, ids, iterator, 0, 0, 0, true, false, null);

        TextureSet textureSet = result.builder.setTexture("").build();

        assertUVTransform(0.0f, 1.0f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim1", 0));
        assertUVTransform(0.0f, 0.5f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim1", 1));
        assertUVTransform(0.5f, 1.0f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim2", 0));
        assertUVTransform(0.5f, 0.5f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim2", 1));
    }

    //@Test
    public void testUvTransforms() throws Exception {
        List<BufferedImage> images = Arrays.asList(newImage(16, 16), newImage(16, 16), newImage(16, 16),
                newImage(16, 16));

        List<String> ids = Arrays.asList("1", "2", "3", "4");
        List<Integer> hullSizes = Arrays.asList(0, 0, 0, 0);

        List<MappedAnimDesc> animations = new ArrayList<MappedAnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1", "2", "3")));

        MappedAnimIterator iterator = new MappedAnimIterator(animations, ids);

        TextureSetResult result = TextureSetGenerator.generate(images, hullSizes, ids, iterator, 0, 0, 0, true, false, null);

        TextureSet textureSet = result.builder.setTexture("").build();
        assertUVTransform(0.0f, 1.0f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim1", 0));
        assertUVTransform(0.0f, 0.5f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim1", 1));
        assertUVTransform(0.5f, 1.0f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim1", 2));
    }

    private static int getFrameIndex(TextureSet textureSet, String id, int frame) {
        return textureSet.getFrameIndices(getAnim(textureSet, id).getStart() + frame);
    }
    private static int getIndexCount(TextureSet textureSet, String id, int frame) {
        return textureSet.getGeometries(getFrameIndex(textureSet, id, frame)).getIndicesCount();
    }

    private static UVTransform getUvTransforms(List<UVTransform> uvTransforms, TextureSet textureSet, String id, int frame) {
        return uvTransforms.get(getAnim(textureSet, id).getStart() + frame);
    }

    private static TextureSetAnimation getAnim(TextureSet textureSet, String id) {
        for (TextureSetAnimation a : textureSet.getAnimationsList()) {
            if (a.getId().equals(id)) {
                return a;
            }
        }
        return null;

    }

    private static void assertUVTransform(float expX, float expY, float expScaleX, float expScaleY, UVTransform actualTransform) {
        assertEquals(expX, actualTransform.translation.x, EPSILON);
        assertEquals(expY, actualTransform.translation.y, EPSILON);
        assertEquals(expScaleX, actualTransform.scale.x, EPSILON);
        assertEquals(expScaleY, actualTransform.scale.y, EPSILON);
    }
}
