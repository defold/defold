package com.dynamo.bob.atlas;

import java.awt.image.BufferedImage;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;

import com.dynamo.bob.tile.ConvexHull;
import com.dynamo.bob.tile.TileSetUtil;
import com.dynamo.bob.tile.TileSetUtil.ConvexHulls;
import com.dynamo.textureset.proto.TextureSetProto;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.Animation;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.ByteString;

public class TileSetGenerator {

    public static TextureSet.Builder generate(TileSet tileSet, BufferedImage image, BufferedImage collisionImage, boolean createOutline) {
        TileSetUtil.Metrics metrics = TileSetUtil.calculateMetrics(image, tileSet.getTileWidth(), tileSet.getTileHeight(),
                tileSet.getTileMargin(), tileSet.getTileSpacing(), collisionImage, 1.0f, 0.0f);

        if (metrics == null) {
            return null;
        }

        final int triangleVertexCount = 6;
        final int outlineVertexCount = 4;

        int vertexSize = TextureSetProto.Constants.VERTEX_SIZE.getNumber();
        ByteBuffer vertices = ByteBuffer.allocate(vertexSize * triangleVertexCount * metrics.tilesPerColumn * metrics.tilesPerRow);
        vertices.order(ByteOrder.LITTLE_ENDIAN);
        ByteBuffer outlineVertices;
        if (createOutline) {
            outlineVertices = ByteBuffer.allocate(vertexSize * outlineVertexCount * metrics.tilesPerColumn * metrics.tilesPerRow);
            outlineVertices.order(ByteOrder.LITTLE_ENDIAN);
        } else {
            outlineVertices = null;
        }
        ByteBuffer texCoords = ByteBuffer.allocate(4 * 4 * metrics.tilesPerColumn * metrics.tilesPerRow).order(ByteOrder.LITTLE_ENDIAN);

        TextureSet.Builder textureSet = TextureSet.newBuilder();
        int index = 0;

        for (int y = 0; y < metrics.tilesPerColumn; ++y) {
            for (int x = 0; x < metrics.tilesPerRow; ++x) {
                float recipImageWidth = 1.0f / metrics.tileSetWidth;
                float recipImageHeight = 1.0f / metrics.tileSetHeight;

                int tileWidth = tileSet.getTileWidth();
                int tileHeight = tileSet.getTileHeight();

                float f = 0.5f;
                float x0 = -f * tileWidth;
                float x1 = f * tileWidth;
                float y0 = -f * tileHeight;
                float y1 = f * tileHeight;

                int tileMargin = tileSet.getTileMargin();
                int tileSpacing = tileSet.getTileSpacing();
                float u0 = (x * (tileSpacing + 2*tileMargin + tileWidth) + tileMargin) * recipImageWidth;
                float u1 = u0 + tileWidth * recipImageWidth;
                float v0 = (y * (tileSpacing + 2*tileMargin + tileHeight) + tileMargin) * recipImageHeight;
                float v1 = v0 + tileHeight * recipImageHeight;

                float z = 0.0f;
                ByteBuffer v = vertices;

                /*
                 * 1   2
                 *
                 * 0   3
                 *
                 * Triangles: 0 2 1
                 *            0 3 2
                 *
                 */

                TileSetGenerator.putVertex(v, x0, y0, z, u0, v1);
                TileSetGenerator.putVertex(v, x1, y1, z, u1, v0);
                TileSetGenerator.putVertex(v, x0, y1, z, u0, v0);

                TileSetGenerator.putVertex(v, x0, y0, z, u0, v1);
                TileSetGenerator.putVertex(v, x1, y0, z, u1, v1);
                TileSetGenerator.putVertex(v, x1, y1, z, u1, v0);
                textureSet.addVertexCount(triangleVertexCount);
                textureSet.addVertexStart(index * triangleVertexCount);

                if (createOutline) {
                    ByteBuffer ov = outlineVertices;
                    TileSetGenerator.putVertex(ov, x0, y0, z, u0, v1);
                    TileSetGenerator.putVertex(ov, x1, y0, z, u1, v1);
                    TileSetGenerator.putVertex(ov, x1, y1, z, u1, v0);
                    TileSetGenerator.putVertex(ov, x0, y1, z, u0, v0);
                    textureSet.addOutlineVertexCount(outlineVertexCount);
                    textureSet.addOutlineVertexStart(index * outlineVertexCount);
                }

                texCoords.putFloat(u0);
                texCoords.putFloat(v0);
                texCoords.putFloat(u1);
                texCoords.putFloat(v1);
            }
            ++index;
        }

        List<Animation> animations = tileSet.getAnimationsList();
        for (Animation animation : animations) {
            TextureSetAnimation textureSetAnimation = TileSetGenerator.createTextureSetAnimation(tileSet, animation);
            textureSet.addAnimations(textureSetAnimation);
        }

        vertices.flip();
        textureSet.setVertices(ByteString.copyFrom(vertices));
        if (createOutline) {
            outlineVertices.flip();
            textureSet.setOutlineVertices(ByteString.copyFrom(outlineVertices));
        } else {
            textureSet.setOutlineVertices(ByteString.EMPTY);
        }

        texCoords.flip();
        textureSet.setTexCoords(ByteString.copyFrom(texCoords));

        if (collisionImage != null) {
            TileSetGenerator.buildConvexHulls(tileSet, collisionImage, textureSet);
        }

        textureSet.addAllCollisionGroups(tileSet.getCollisionGroupsList());

        return textureSet;
    }

    private static void putVertex(ByteBuffer b, float x, float y, float z, float u, float v) {
        b.putFloat(x);
        b.putFloat(y);
        b.putFloat(z);
        b.putShort(TileSetGenerator.toShortUV(u));
        b.putShort(TileSetGenerator.toShortUV(v));
    }

    private static short toShortUV(float fuv) {
        int uv = (int) (fuv * 65535.0f);
        short ret = (short) (uv & 0xffff);
        return ret;
    }

    private static void buildConvexHulls(TileSet tileSet,
            BufferedImage collisionImage, TextureSet.Builder textureSet) {
        ConvexHulls convexHulls = TileSetUtil.calculateConvexHulls(
                collisionImage.getAlphaRaster(), 16, collisionImage.getWidth(), collisionImage.getHeight(),
                tileSet.getTileWidth(), tileSet.getTileHeight(),
                tileSet.getTileMargin(), tileSet.getTileSpacing());

        for (int i = 0; i < convexHulls.hulls.length; ++i) {
            ConvexHull convexHull = convexHulls.hulls[i];
            Tile.ConvexHull.Builder hullBuilder = Tile.ConvexHull.newBuilder();
            String collisionGroup = "";
            if (i < tileSet.getConvexHullsCount()) {
                collisionGroup = tileSet.getConvexHulls(i).getCollisionGroup();
            }
            hullBuilder
                .setCollisionGroup(collisionGroup)
                .setCount(convexHull.getCount())
                .setIndex(convexHull.getIndex());


            textureSet.addConvexHulls(hullBuilder);
        }

        for (int i = 0; i < convexHulls.points.length; ++i) {
            textureSet.addConvexHullPoints(convexHulls.points[i]);
        }
    }

    private static TextureSetAnimation createTextureSetAnimation(TileSet tileSet, Animation animation) {
        String id = animation.getId();
        int w = tileSet.getTileWidth();
        int h = tileSet.getTileHeight();
        return TextureSetAnimation.newBuilder()
                .setId(id)
                .setWidth(w)
                .setHeight(h)
                .setPlayback(animation.getPlayback())
                .setStart(animation.getStartTile() - 1)
                .setEnd(animation.getEndTile() - 1)
                .setFps(animation.getFps())
                .setFlipHorizontal(animation.getFlipHorizontal())
                .setFlipVertical(animation.getFlipVertical())
                .build();
    }

}
