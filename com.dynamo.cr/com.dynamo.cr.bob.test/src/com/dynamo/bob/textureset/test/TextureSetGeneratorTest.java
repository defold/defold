// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.textureset.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.AtlasUtil.MappedAnimDesc;
import com.dynamo.bob.pipeline.AtlasUtil.MappedAnimIterator;
import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.bob.textureset.TextureSetGenerator.UVTransform;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.gamesys.proto.Tile.Playback;
import com.dynamo.gamesys.proto.Tile.SpriteTrimmingMode;
import com.dynamo.gamesys.proto.AtlasProto.AtlasImage;

public class TextureSetGeneratorTest {

    private static final float EPSILON = 0.000001f;

    BufferedImage newImage(int w, int h) {
        return new BufferedImage(w, h, BufferedImage.TYPE_4BYTE_ABGR);
    }

    AtlasImage newAtlasImage(float pivot_x, float pivot_y, SpriteTrimmingMode mode) {
        return AtlasImage.newBuilder()
            .setImage("")
            .setPivotX(pivot_x)
            .setPivotY(pivot_y)
            .setSpriteTrimMode(mode)
            .build();
    }

    MappedAnimDesc newAnim(String id, List<String> paths) {
        return new MappedAnimDesc(id, paths, paths, Playback.PLAYBACK_LOOP_BACKWARD, 30, false, false);
    }

    @Test
    public void test1() throws CompileExceptionError {
        List<BufferedImage> images =
                Arrays.asList(newImage(16, 16),
                              newImage(16, 16),
                              newImage(16, 16),
                              newImage(16, 16));

        List<String> ids = Arrays.asList("1", "2", "3", "4");
        List<AtlasImage> atlasImages = new ArrayList<>();
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_6));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_6));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_6));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_6));

        List<MappedAnimDesc> animations = new ArrayList<MappedAnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1", "2", "3")));
        animations.add(newAnim("anim2", Arrays.asList("2", "4")));

        MappedAnimIterator iterator = new MappedAnimIterator(animations, ids);

        TextureSetResult result = TextureSetGenerator.generate(images, atlasImages, ids, iterator, 0, 0, 0, true, false, null, 0, 0);
        TextureSet textureSet = result.builder.setTexture("").build();
        BufferedImage image = result.images.get(0);
        assertThat(image.getWidth(), is(32));
        assertThat(image.getHeight(), is(32));
        assertThat(textureSet.getAnimationsCount(), is(2));

        assertThat(getIndexCount(textureSet, "anim1", 0), is(6));
        assertThat(getIndexCount(textureSet, "anim2", 0), is(6));
    }

    @Test
    public void testAtlasGenerator() throws Exception {
        List<BufferedImage> images = Arrays.asList(newImage(16, 16), newImage(16, 16), newImage(16, 16),
                newImage(16, 16));

        List<String> ids = Arrays.asList("1", "2", "3", "4");

        List<AtlasImage> atlasImages = new ArrayList<>();
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));

        List<MappedAnimDesc> animations = new ArrayList<MappedAnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1", "2", "3")));
        animations.add(newAnim("anim2", Arrays.asList("2", "4")));

        MappedAnimIterator iterator = new MappedAnimIterator(animations, ids);

        TextureSetResult result = TextureSetGenerator.generate(images, atlasImages, ids, iterator, 0, 0, 0, true, false, null, 0, 0);
        BufferedImage image = result.images.get(0);
        assertThat(image.getWidth(), is(32));
        assertThat(image.getHeight(), is(32));

        TextureSet textureSet = result.builder.setTexture("").build();

        int sizeOfFloat = 4;
        int numFrames = ids.size() + animations.get(0).getIds().size() + animations.get(1).getIds().size();
        assertThat(textureSet.getAnimationsCount(), is(2));
        assertThat(textureSet.getTexDims().size() / sizeOfFloat, is(numFrames * 2)); // frame count * 2 floats (x, y)
    }

    @Test
    public void testPagedAtlasGenerator() throws Exception {
        List<BufferedImage> images = Arrays.asList(
            newImage(16, 16),
            newImage(16, 16));

        List<String> ids = Arrays.asList("1", "2");
        List<AtlasImage> atlasImages = new ArrayList<>();
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));

        List<MappedAnimDesc> animations = new ArrayList<MappedAnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1")));
        animations.add(newAnim("anim2", Arrays.asList("2")));
        animations.add(newAnim("anim3", Arrays.asList("1", "2")));

        MappedAnimIterator iterator = new MappedAnimIterator(animations, ids);

        TextureSetResult result = TextureSetGenerator.generate(images, atlasImages, ids, iterator, 0, 0, 0, true, false, null, 16, 16);
        BufferedImage image0 = result.images.get(0);
        BufferedImage image1 = result.images.get(1);
        assertThat(image0.getWidth(), is(16));
        assertThat(image0.getHeight(), is(16));
        assertTrue(image0.getHeight() == image1.getHeight());
        assertTrue(image0.getWidth() == image1.getWidth());

        TextureSet textureSet = result.builder.setTexture("").build();

        assertThat(getPageIndex(textureSet, "anim1", 0), is(0));
        assertThat(getPageIndex(textureSet, "anim2", 0), is(1));
        assertThat(getPageIndex(textureSet, "anim3", 0), is(0));
        assertThat(getPageIndex(textureSet, "anim3", 1), is(1));
    }

    @Test
    public void testAtlasGeneratorMaxRectMargin() throws Exception {
        // These are 11x11 and with 5x5 padding added during generation, the output should be 64x16
        List<BufferedImage> images = Arrays.asList(newImage(11, 11), newImage(11, 11), newImage(11, 11),
                newImage(11, 11));

        List<String> ids = Arrays.asList("1", "2", "3", "4");
        List<AtlasImage> atlasImages = new ArrayList<>();
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));

        List<MappedAnimDesc> animations = new ArrayList<MappedAnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1", "2", "3")));
        animations.add(newAnim("anim2", Arrays.asList("2", "4")));

        MappedAnimIterator iterator = new MappedAnimIterator(animations, ids);

        TextureSetResult result = TextureSetGenerator.generate(images, atlasImages, ids, iterator, 5, 0, 0, true, false, null, 0, 0);
        BufferedImage image = result.images.get(0);
        assertThat(image.getWidth(), is(32));
        assertThat(image.getHeight(), is(64));

        for (UVTransform uv : result.uvTransforms)
        {
            // Same as original
            assertThat((int)(uv.scale.x * image.getWidth()), is(11));
            assertThat((int)(-uv.scale.y * image.getHeight()), is(11));
        }
    }

    @Test
    public void testRotatedAnimations() throws CompileExceptionError {
        List<BufferedImage> images = Arrays.asList(newImage(64,32), newImage(64,32), newImage(32,64), newImage(32,64));

        List<String> ids = Arrays.asList("1", "2", "3", "4");
        List<AtlasImage> atlasImages = new ArrayList<>();
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));

        List<MappedAnimDesc> animations = new ArrayList<MappedAnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1","2")));
        animations.add(newAnim("anim2", Arrays.asList("3","4")));

        MappedAnimIterator iterator = new MappedAnimIterator(animations, ids);

        TextureSetResult result = TextureSetGenerator.generate(images, atlasImages, ids, iterator, 0, 0, 0, true, false, null, 0, 0);

        TextureSet textureSet = result.builder.setTexture("").build();

        assertUVTransform(0.0f, 1.0f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim1", 0));
        assertUVTransform(0.0f, 0.5f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim1", 1));
        assertUVTransform(0.5f, 1.0f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim2", 0));
        assertUVTransform(0.5f, 0.5f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim2", 1));
    }

    @Test
    public void testUvTransforms() throws Exception {
        List<BufferedImage> images = Arrays.asList(newImage(16, 16), newImage(16, 16), newImage(16, 16),
                newImage(16, 16));

        List<String> ids = Arrays.asList("1", "2", "3", "4");
        List<AtlasImage> atlasImages = new ArrayList<>();
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));
        atlasImages.add(newAtlasImage(0.5f, 0.5f, SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF));

        List<MappedAnimDesc> animations = new ArrayList<MappedAnimDesc>();
        animations.add(newAnim("anim1", Arrays.asList("1", "2", "3")));

        MappedAnimIterator iterator = new MappedAnimIterator(animations, ids);

        TextureSetResult result = TextureSetGenerator.generate(images, atlasImages, ids, iterator, 0, 0, 0, true, false, null, 0, 0);

        TextureSet textureSet = result.builder.setTexture("").build();
        assertUVTransform(0.0f, 1.0f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim1", 0));
        assertUVTransform(0.0f, 0.5f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim1", 1));
        assertUVTransform(0.5f, 1.0f, 0.5f, -0.5f, getUvTransforms(result.uvTransforms, textureSet, "anim1", 2));
    }

    private static int getPageIndex(TextureSet textureSet, String id, int frame) {
        return textureSet.getPageIndices(getAnim(textureSet, id).getStart() + frame);
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
