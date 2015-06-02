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
import java.util.ArrayList;
import java.util.List;

import org.junit.Test;

import com.dynamo.bob.textureset.TextureSetGenerator.AnimDesc;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.bob.tile.TileSetGenerator;
import com.dynamo.bob.tile.TileSetGenerator.IndexedAnimDesc;
import com.dynamo.bob.tile.TileSetGenerator.IndexedAnimIterator;
import com.dynamo.textureset.proto.TextureSetProto;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.ConvexHull;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.ByteString;

public class TileSetGeneratorTest {

    @Test
    public void testTileSet() throws Exception {
        BufferedImage image = newImage(64, 32);
        int tileWidth = 32;
        int tileHeight = 32;
        TileSet.Builder tileSet = newTileSet(tileWidth, tileHeight);
        tileSet.addAnimations(newAnim("a1", 0, 0));
        tileSet.addAnimations(newAnim("a2", 1, 1));

        TextureSetResult result = TileSetGenerator.generate(tileSet.build(), image, image, false, false);
        TextureSet textureSet = result.builder.setTexture("").build();

        int vertexSize = TextureSetProto.Constants.VERTEX_SIZE.getNumber();

        ByteString vertices = textureSet.getVertices();
        List<Integer> verticesCount = textureSet.getVertexCountList();

        ByteString outlineVertices = textureSet.getOutlineVertices();
        List<Integer> outlineVerticesCount = textureSet.getOutlineVertexCountList();

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

        final int quadCount = 2 + 2; // 2 tiles, 2 anim frames
        assertThat(vertices.size(), is(vertexSize * quadCount * 6));
        assertThat(verticesCount.size(), is(quadCount));
        assertThat(verticesCount.get(0), is(6));
        assertThat(verticesCount.get(1), is(6));
        assertThat(verticesCount.get(2), is(6));
        assertThat(verticesCount.get(3), is(6));

        ByteBuffer v = ByteBuffer.wrap(vertices.toByteArray());
        // Vertex buffers is in little endian (java big)
        v.order(ByteOrder.LITTLE_ENDIAN);
        float du = 0.0f;
        for (int i = 0; i < 2; ++i) {
            assertQuad(v, 32, 32, 0 + du, 0.5f + du, 0, 1);

            du += 0.5f;
        }

        assertThat(outlineVertices.size(), is(0));
        assertThat(outlineVerticesCount.size(), is(0));
        assertThat(textureSet.getTexCoords().size(), is(8 * 4 * quadCount));
    }

    @Test
    public void testSplit() {
        BufferedImage image = newImage(384, 384);
        int tileWidth = 128;
        int tileHeight = 128;
        int tileCount = 9;
        TileSet.Builder tileSet = newTileSet(tileWidth, tileHeight);

        TextureSetResult result = TileSetGenerator.generate(tileSet.build(), image, image, false, false);
        TextureSet textureSet = result.builder.setTexture("").build();
        BufferedImage texture = result.image;

        assertEquals(512, texture.getWidth());
        assertEquals(512, texture.getHeight());

        ByteString vertices = textureSet.getVertices();
        ByteBuffer v = ByteBuffer.wrap(vertices.toByteArray());
        // Vertex buffers is in little endian (java big)
        v.order(ByteOrder.LITTLE_ENDIAN);
        float[] us = { 0.0f, 0.25f, 0.5f, 0.75f, 1f};
        float[] vs = { 0.0f, 0.25f, 0.5f, 0.75f, 1f};
        // Verify vertices
        for (int i = 0; i < tileCount; ++i) {
            int x = i % 3;
            int y = i / 3;
            assertQuad(v, tileWidth, tileHeight, us[x], us[x + 1], vs[y], vs[y + 1]);
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
    public void testSplitStrip() {
        BufferedImage image = newImage(1, 16);
        int tileWidth = 1;
        int tileHeight = 16;
        TileSet.Builder tileSet = newTileSet(tileWidth, tileHeight);

        TextureSetResult result = TileSetGenerator.generate(tileSet.build(), image, image, false, false);
        TextureSet textureSet = result.builder.setTexture("").build();
        BufferedImage texture = result.image;

        assertEquals(1, texture.getWidth());
        assertEquals(16, texture.getHeight());

        ByteBuffer uv = ByteBuffer.wrap(textureSet.getTexCoords().toByteArray());
        // Vertex buffers is in little endian (java big)
        uv.order(ByteOrder.LITTLE_ENDIAN);
        assertQuadTexCoords(uv, 0.0f, 1.0f, 0.0f, 1.0f, false);
    }

    @Test
    public void testSplitStripExtrude() throws IOException {
        BufferedImage image = newImage(1, 16);

        int tileWidth = 1;
        int tileHeight = 16;
        TileSet.Builder tileSet = newTileSet(tileWidth, tileHeight);
        tileSet.setExtrudeBorders(1);

        TextureSetResult result = TileSetGenerator.generate(tileSet.build(), image, image, false, false);
        TextureSet textureSet = result.builder.setTexture("").build();
        BufferedImage texture = result.image;

        assertEquals(4, texture.getWidth());
        assertEquals(32, texture.getHeight());

        ByteBuffer uv = ByteBuffer.wrap(textureSet.getTexCoords().toByteArray());
        // Vertex buffers is in little endian (java big)
        uv.order(ByteOrder.LITTLE_ENDIAN);
        assertQuadTexCoords(uv, 1.0f / 4, 2.0f / 4, 1.0f / 32, 0.5f + 1.0f / 32, false);
    }

    @Test
    public void textIndexedAnimIterator() throws Exception {
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

    private static short toShortUV(float fuv) {
        int uv = (int) (fuv * 65535.0f);
        return (short) (uv & 0xffff);
    }

    private static void assertQuad(ByteBuffer b, float width, float height, float minU,
            float maxU, float minV, float maxV) {
        float halfWidth = width * 0.5f;
        float halfHeight = height * 0.5f;

        assertVertex(b, -halfWidth, -halfHeight, 0, minU, maxV);
        assertVertex(b, -halfWidth, halfHeight, 0, minU, minV);
        assertVertex(b, halfWidth, halfHeight, 0, maxU, minV);

        assertVertex(b, halfWidth, halfHeight, 0, maxU, minV);
        assertVertex(b, halfWidth, -halfHeight, 0, maxU, maxV);
        assertVertex(b, -halfWidth, -halfHeight, 0, minU, maxV);
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

    private static void assertVertex(ByteBuffer b, float x, float y, float z, float u, float v) {
        assertThat(b.getFloat(), is(x));
        assertThat(b.getFloat(), is(y));
        assertThat(b.getFloat(), is(z));
        assertThat(b.getShort(), is(toShortUV(u)));
        assertThat(b.getShort(), is(toShortUV(v)));
    }

    private static TileSet.Builder newTileSet(int tileWidth, int tileHeight) {
        return TileSet.newBuilder().setImage("/test.png").setTileWidth(tileWidth).setTileHeight(tileHeight)
                .setTileMargin(0)
                .setTileSpacing(0).setMaterialTag("foo");
    }

    private static Tile.Animation.Builder newAnim(String id, int start, int end) {
        return Tile.Animation.newBuilder().setId(id).setStartTile(start + 1).setEndTile(end + 1);
    }
}
