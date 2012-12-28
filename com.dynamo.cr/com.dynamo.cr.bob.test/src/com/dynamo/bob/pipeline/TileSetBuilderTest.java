package com.dynamo.bob.pipeline;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import java.awt.Color;
import java.awt.Graphics;
import java.awt.image.BufferedImage;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;

import org.junit.Test;

import com.dynamo.bob.atlas.TileSetGenerator;
import com.dynamo.textureset.proto.TextureSetProto;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.ConvexHull;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.ByteString;

public class TileSetBuilderTest {

    BufferedImage newImage(int w, int h) {
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

    private static void assertVertex(ByteBuffer b, float x, float y, float z, float u, float v) {
        assertThat(b.getFloat(), is(x));
        assertThat(b.getFloat(), is(y));
        assertThat(b.getFloat(), is(z));
        assertThat(b.getShort(), is(toShortUV(u)));
        assertThat(b.getShort(), is(toShortUV(v)));
    }

    @Test
    public void testTileSet() throws Exception {
        BufferedImage image = newImage(64, 32);
        int tileWidth = 32;
        int tileHeight = 32;
        TileSet tileSet = TileSet.newBuilder()
                .setImage("")
                .setTileWidth(tileWidth)
                .setTileHeight(tileHeight)
                .setTileMargin(0)
                .setTileSpacing(0)
                .setMaterialTag("foo")
                .addAnimations(newAnim("a1", 32, 32, 0, 0))
                .addAnimations(newAnim("a2", 32, 32, 1, 1))
                .build();

        TextureSet textureSet = TileSetGenerator.generate(tileSet, image, image, false)
                .setTexture("")
                .build();

        final int tileCount = 2;
        int vertexSize = TextureSetProto.Constants.VERTEX_SIZE.getNumber();

        ByteString vertices = textureSet.getVertices();
        List<Integer> verticesCount = textureSet.getVertexCountList();

        ByteString outlineVertices = textureSet.getOutlineVertices();
        List<Integer> outlineVerticesCount = textureSet.getOutlineVertexCountList();

        assertThat(textureSet.getAnimationsCount(), is(2));
        List<TextureSetAnimation> animations = textureSet.getAnimationsList();
        TextureSetAnimation anim1 = animations.get(0);
        assertThat(anim1.getId(), is("a1"));
        assertThat(anim1.getStart(), is(0));
        assertThat(anim1.getEnd(), is(0));
        assertThat(anim1.getWidth(), is(32));
        assertThat(anim1.getHeight(), is(32));

        TextureSetAnimation anim2 = animations.get(1);
        assertThat(anim2.getId(), is("a2"));
        assertThat(anim2.getStart(), is(1));
        assertThat(anim2.getEnd(), is(1));
        assertThat(anim2.getWidth(), is(32));
        assertThat(anim2.getHeight(), is(32));

        assertThat(textureSet.getConvexHullsCount(), is(2));
        ConvexHull hull1 = textureSet.getConvexHulls(0);
        assertThat(hull1.getCount(), is(4));
        ConvexHull hull2 = textureSet.getConvexHulls(1);
        assertThat(hull2.getCount(), is(4));

        assertThat(vertices.size(), is(vertexSize * tileCount * 6));
        assertThat(verticesCount.size(), is(tileCount));
        assertThat(verticesCount.get(0), is(6));
        assertThat(verticesCount.get(1), is(6));

        ByteBuffer v = ByteBuffer.wrap(vertices.toByteArray());
        // Vertex buffers is in little endian (java big)
        v.order(ByteOrder.LITTLE_ENDIAN);
        float du = 0.0f;
        for (int i = 0; i < 2; ++i) {
            assertVertex(v, -16, -16, 0, 0 + du, 1);
            assertVertex(v, 16, 16, 0, 0.5f + du, 0);
            assertVertex(v, -16, 16, 0, 0 + du, 0);

            assertVertex(v, -16, -16, 0, 0 + du, 1);
            assertVertex(v, 16, -16, 0, 0.5f + du, 1);
            assertVertex(v, 16, 16, 0, 0.5f + du, 0);
            du += 0.5f;
        }

        assertThat(outlineVertices.size(), is(0));
        assertThat(outlineVerticesCount.size(), is(0));
        assertThat(textureSet.getTexCoords().size(), is(4 * 4 * tileCount));
    }

    private Tile.Animation.Builder newAnim(String id, int width, int height, int start, int end) {
        return Tile.Animation.newBuilder()
            .setId(id)
            .setStartTile(start + 1)
            .setEndTile(end + 1);
    }
}
