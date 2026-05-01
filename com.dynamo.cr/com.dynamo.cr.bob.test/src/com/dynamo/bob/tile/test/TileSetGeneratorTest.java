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

package com.dynamo.bob.tile.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;

import java.awt.Color;
import java.awt.Graphics;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.List;

import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimDesc;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.bob.tile.TileSetGenerator;
import com.dynamo.bob.tile.TileSetGenerator.IndexedAnimDesc;
import com.dynamo.bob.tile.TileSetGenerator.IndexedAnimIterator;
import com.dynamo.gamesys.proto.TextureSetProto.SpriteGeometry;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.gamesys.proto.Tile;
import com.dynamo.gamesys.proto.Tile.ConvexHull;
import com.dynamo.gamesys.proto.Tile.SpriteTrimmingMode;
import com.dynamo.gamesys.proto.Tile.TileSet;

public class TileSetGeneratorTest {

    // @Test
    public void testTileSet() throws Exception, CompileExceptionError {
        BufferedImage image = newImage(64, 32);
        int tileWidth = 32;
        int tileHeight = 32;
        TileSet.Builder tileSet = newTileSet(tileWidth, tileHeight);
        tileSet.addAnimations(newAnim("a1", 0, 0));
        tileSet.addAnimations(newAnim("a2", 1, 1));

        TextureSetResult result = TileSetGenerator.generate(tileSet.build(), image, image);
        TextureSet textureSet = result.builder.setTexture("").build();

        assertThat(textureSet.getAnimationsCount(), is(2));
        List<TextureSetAnimation> animations = textureSet.getAnimationsList();
        TextureSetAnimation anim1 = animations.get(0);
        assertThat(anim1.getId(), is("a1"));
        assertThat(anim1.getStart(), is(2));
        assertThat(anim1.getEnd(), is(3));
        assertThat(anim1.getWidth(), is(32));
        assertThat(anim1.getHeight(), is(32));

        TextureSetAnimation anim2 = animations.get(1);
        assertThat(anim2.getId(), is("a2"));
        assertThat(anim2.getStart(), is(3));
        assertThat(anim2.getEnd(), is(4));
        assertThat(anim2.getWidth(), is(32));
        assertThat(anim2.getHeight(), is(32));

        assertThat(textureSet.getConvexHullsCount(), is(2));
        ConvexHull hull1 = textureSet.getConvexHulls(0);
        assertThat(hull1.getCount(), is(4));
        ConvexHull hull2 = textureSet.getConvexHulls(1);
        assertThat(hull2.getCount(), is(4));

        assertThat(textureSet.getGeometriesCount(), is(2));
        assertGeometry(textureSet.getGeometries(0), 32, 32, 0.0f, 0.5f, 0.0f, 1.0f);
        assertGeometry(textureSet.getGeometries(1), 32, 32, 0.5f, 1.0f, 0.0f, 1.0f);

        final int quadCount = 2 + 2; // 2 tiles, 2 anim frames
        assertThat(textureSet.getTexCoords().size(), is(8 * 4 * quadCount));
    }

    @Test
    public void testSplit() throws CompileExceptionError {
        BufferedImage image = newImage(384, 384);
        int tileWidth = 128;
        int tileHeight = 128;
        int tileCount = 9;
        TileSet.Builder tileSet = newTileSet(tileWidth, tileHeight);

        TextureSetResult result = TileSetGenerator.generate(tileSet.build(), image, image);
        TextureSet textureSet = result.builder.setTexture("").build();
        BufferedImage texture = result.images.get(0);

        assertEquals(512, texture.getWidth());
        assertEquals(512, texture.getHeight());

        // although there's only 3x3 tiles
        // the texture is rounded up to nearest power of two
        float[] us = { 0.0f, 0.25f, 0.5f, 0.75f, 1f};
        float[] vs = { 1.0f, 0.75f, 0.5f, 0.25f, 0f};

        assertThat(textureSet.getGeometriesCount(), is(tileCount));
        for (int i = 0; i < tileCount; ++i) {
            int x = i % 3;
            int y = i / 3;
            assertGeometry(textureSet.getGeometries(i), tileWidth, tileHeight, us[x], us[x + 1], vs[y+1], vs[y]);
        }

        ByteBuffer uv = ByteBuffer.wrap(textureSet.getTexCoords().toByteArray());
        // Vertex buffers is in little endian (java big)
        uv.order(ByteOrder.LITTLE_ENDIAN);
        // Verify tex coords
        assertThat(textureSet.getTexCoords().size(), is(8 * 4 * tileCount));
        for (int i = 0; i < tileCount; ++i) {
            int x = i % 3;
            int y = i / 3;
            assertQuadTexCoords(uv, us[x], us[x + 1], vs[y], vs[y + 1], false);
        }
    }

    @Test
    public void testSplitStrip() throws CompileExceptionError {
        BufferedImage image = newImage(1, 16);
        int tileWidth = 1;
        int tileHeight = 16;
        TileSet.Builder tileSet = newTileSet(tileWidth, tileHeight);

        TextureSetResult result = TileSetGenerator.generate(tileSet.build(), image, image);
        TextureSet textureSet = result.builder.setTexture("").build();
        BufferedImage texture = result.images.get(0);

        assertEquals(1, texture.getWidth());
        assertEquals(16, texture.getHeight());

        ByteBuffer uv = ByteBuffer.wrap(textureSet.getTexCoords().toByteArray());
        // Vertex buffers is in little endian (java big)
        uv.order(ByteOrder.LITTLE_ENDIAN);
        assertQuadTexCoords(uv, 0.0f, 1.0f, 1.0f, 0.0f, false);
    }

    @Test
    public void testSplitStripExtrude() throws IOException, CompileExceptionError {
        BufferedImage image = newImage(1, 16);

        int tileWidth = 1;
        int tileHeight = 16;
        TileSet.Builder tileSet = newTileSet(tileWidth, tileHeight);
        tileSet.setExtrudeBorders(1);

        TextureSetResult result = TileSetGenerator.generate(tileSet.build(), image, image);
        TextureSet textureSet = result.builder.setTexture("").build();
        BufferedImage texture = result.images.get(0);

        assertEquals(4, texture.getWidth());
        assertEquals(32, texture.getHeight());

        ByteBuffer uv = ByteBuffer.wrap(textureSet.getTexCoords().toByteArray());
        // Vertex buffers is in little endian (java big)
        uv.order(ByteOrder.LITTLE_ENDIAN);
        assertQuadTexCoords(uv, 1.0f / 4, 2.0f / 4, 1.0f - 1.0f / 32, 0.5f - 1.0f / 32, false);
    }

    @Test
    public void textIndexedAnimIterator() throws Exception, CompileExceptionError {
        List<IndexedAnimDesc> anims = new ArrayList<IndexedAnimDesc>(1);
        anims.add(new IndexedAnimDesc(newAnim("test", 3, 1).build()));
        TileSetGenerator.IndexedAnimIterator iterator = new IndexedAnimIterator(anims, 4);
        AnimDesc anim = iterator.nextAnim();
        assertEquals(anims.get(0).getId(), anim.getId());
        assertEquals(3, iterator.nextFrameIndex().intValue());
        assertEquals(0, iterator.nextFrameIndex().intValue());
        assertEquals(1, iterator.nextFrameIndex().intValue());
        assertEquals(null, iterator.nextFrameIndex());
    }

    private static BufferedImage newImage(int w, int h) {
        BufferedImage b = new BufferedImage(w, h, BufferedImage.TYPE_4BYTE_ABGR);
        Graphics g = b.getGraphics();
        g.setColor(new Color(1, 1, 1, 1));
        g.fillRect(0, 0, w, h);
        g.dispose();
        return b;
    }

    // Assumes a full quad
    private static void assertGeometry(SpriteGeometry geometry,
                                        int width, int height,
                                        float minU, float maxU, float minV, float maxV) {
        List<Float> vertices = geometry.getVerticesList();
        List<Integer> indices = geometry.getIndicesList();
        List<Float> uvs = geometry.getUvsList();

        assertThat(geometry.getWidth(), is(width));
        assertThat(geometry.getHeight(), is(height));
        assertThat(vertices.size(), is(8));
        assertThat(indices.size(), is(6));
        assertThat(uvs.size(), is(8));

        // bottom right, bottom left, top left, top right
        List<Float> expectedVertices = Arrays.asList(0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f);
        List<Float> expectedUVs = Arrays.asList(maxU, minV, minU, minV, minU, maxV, maxU, maxV);
        List<Integer> expectedIndices = Arrays.asList(0, 1, 2, 0, 2, 3);

        assertThat(geometry.getVerticesList(), is(expectedVertices));
        assertThat(geometry.getUvsList(), is(expectedUVs));
        assertThat(geometry.getIndicesList(), is(expectedIndices));
    }


    private static void assertQuadTexCoords(ByteBuffer b, float minU, float maxU, float minV, float maxV, boolean rotated) {
        if (rotated) {
            assertThat(b.getFloat(), is(minU));
            assertThat(b.getFloat(), is(minV));

            assertThat(b.getFloat(), is(maxU));
            assertThat(b.getFloat(), is(minV));

            assertThat(b.getFloat(), is(maxU));
            assertThat(b.getFloat(), is(maxV));

            assertThat(b.getFloat(), is(minU));
            assertThat(b.getFloat(), is(maxV));
        } else {
            assertThat(b.getFloat(), is(minU));
            assertThat(b.getFloat(), is(maxV));

            assertThat(b.getFloat(), is(minU));
            assertThat(b.getFloat(), is(minV));

            assertThat(b.getFloat(), is(maxU));
            assertThat(b.getFloat(), is(minV));

            assertThat(b.getFloat(), is(maxU));
            assertThat(b.getFloat(), is(maxV));
        }
    }

    private static TileSet.Builder newTileSet(int tileWidth, int tileHeight) {
        return TileSet.newBuilder().setImage("/test.png").setTileWidth(tileWidth).setTileHeight(tileHeight)
                .setTileMargin(0)
                .setTileSpacing(0)
                .setMaterialTag("foo")
                .setSpriteTrimMode(SpriteTrimmingMode.SPRITE_TRIM_MODE_4);
    }

    private static Tile.Animation.Builder newAnim(String id, int start, int end) {
        return Tile.Animation.newBuilder().setId(id).setStartTile(start + 1).setEndTile(end + 1);
    }
}
