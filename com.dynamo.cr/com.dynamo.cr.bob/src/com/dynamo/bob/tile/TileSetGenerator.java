package com.dynamo.bob.tile;

import java.awt.Graphics;
import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimDesc;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimIterator;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.bob.textureset.TextureSetLayout.Grid;
import com.dynamo.bob.textureset.TextureSetLayout.Rect;
import com.dynamo.bob.tile.TileSetUtil.ConvexHulls;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.textureset.proto.TextureSetProto.SpriteGeometry;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.Animation;
import com.dynamo.tile.proto.Tile.SpriteTrimmingMode;
import com.dynamo.tile.proto.Tile.TileSet;

public class TileSetGenerator {

    public static class IndexedAnimDesc extends AnimDesc {
        int start;
        int end;

        public IndexedAnimDesc(Animation animation) {
            super(animation.getId(), animation.getPlayback(), animation.getFps(), animation.getFlipHorizontal() != 0,
                    animation.getFlipVertical() != 0);
            this.start = animation.getStartTile() - 1;
            this.end = animation.getEndTile();
        }

        public int getStart() {
            return this.start;
        }

        public int getEnd() {
            return this.end;
        }
    }

    public static class IndexedAnimIterator implements AnimIterator {
        final List<IndexedAnimDesc> anims;
        final int tileCount;
        int nextAnimIndex;
        int nextFrameIndex;

        public IndexedAnimIterator(List<IndexedAnimDesc> anims, int tileCount) {
            this.anims = anims;
            this.tileCount = tileCount;
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
            IndexedAnimDesc anim = anims.get(nextAnimIndex - 1);
            int start = anim.getStart();
            int end = anim.getEnd();
            if (end <= start) {
                end += tileCount;
            }
            int index = start + nextFrameIndex;
            if (index < end) {
                ++nextFrameIndex;
                if (index >= tileCount)
                    return index - tileCount;
                else
                    return index;
            }
            return null;
        }

        @Override
        public void rewind() {
            nextAnimIndex = 0;
            nextFrameIndex = 0;
        }
    }

    public static TextureSetResult generate(TileSet tileSet, BufferedImage image, BufferedImage collisionImage) {
        Rect imageRect = image != null ? new Rect(null, -1, image.getWidth(), image.getHeight()) : null;
        Rect collisionRect = collisionImage != null ? new Rect(null, -1, collisionImage.getWidth(), collisionImage.getHeight()) : null;
        TileSetUtil.Metrics metrics = TileSetUtil.calculateMetrics(imageRect, tileSet.getTileWidth(),
                tileSet.getTileHeight(), tileSet.getTileMargin(), tileSet.getTileSpacing(), collisionRect, 1.0f, 0.0f);

        if (metrics == null) {
            return null;
        }

        int hullVertexSize = 6;
        SpriteTrimmingMode mode = tileSet.getSpriteTrimMode();
        switch (mode) {
            case SPRITE_TRIM_MODE_OFF:   hullVertexSize = 0; break;
            case SPRITE_TRIM_MODE_4:     hullVertexSize = 4; break;
            case SPRITE_TRIM_MODE_5:     hullVertexSize = 5; break;
            case SPRITE_TRIM_MODE_6:     hullVertexSize = 6; break;
            case SPRITE_TRIM_MODE_7:     hullVertexSize = 7; break;
            case SPRITE_TRIM_MODE_8:     hullVertexSize = 8; break;
        }

        List<BufferedImage> images = split(image, tileSet, metrics);
        List<Integer> imageHullSizes = new ArrayList<Integer>();
        List<String> names = new ArrayList<String>();
        int tileIndex = 0;
        for (BufferedImage tmp : images) {
            imageHullSizes.add(hullVertexSize);
            names.add(String.format("tile%d", tileIndex++));
        }

        AnimIterator iterator = createAnimIterator(tileSet, images.size());

        // Since all the images already are positioned optimally in a grid,
        // we tell TextureSetGenerator to NOT do its own packing and use this grid directly.
        Grid grid_size = new Grid(metrics.tilesPerRow, metrics.tilesPerColumn);
        TextureSetResult result = TextureSetGenerator.generate(images, imageHullSizes, names, iterator, 0,
                tileSet.getInnerPadding(),
                tileSet.getExtrudeBorders(), false, true, grid_size );

        TextureSet.Builder builder = result.builder;

        builder.setTileWidth(tileSet.getTileWidth()).setTileHeight(tileSet.getTileHeight());

        // These hulls are for collision detection
        buildCollisionConvexHulls(tileSet, collisionImage, builder);

        return result;
    }

    private static int calcTileStart(TileSet tileSet, int size, int tileIndex) {
        int offset = tileSet.getTileMargin();
        int actualTileSize = size + tileSet.getTileSpacing() + 2 * tileSet.getTileMargin();
        return offset + actualTileSize * tileIndex;
    }

    private static List<BufferedImage> split(BufferedImage image, TileSet tileSet, TileSetUtil.Metrics metrics) {
        int count = metrics.tilesPerRow * metrics.tilesPerColumn;
        int tileWidth = tileSet.getTileWidth();
        int tileHeight = tileSet.getTileHeight();
        List<BufferedImage> result = new ArrayList<BufferedImage>(count);
        for (int i = 0; i < count; ++i) {
            int type = TextureUtil.getImageType(image);
            BufferedImage tgt = new BufferedImage(tileWidth, tileHeight, type);
            Graphics g = tgt.getGraphics();
            int tileX = i % metrics.tilesPerRow;
            int tileY = i / metrics.tilesPerRow;
            int sx = calcTileStart(tileSet, tileWidth, tileX);
            int sy = calcTileStart(tileSet, tileHeight, tileY);
            g.drawImage(image, 0, 0, tileWidth, tileHeight, sx, sy, sx + tileWidth, sy + tileHeight, null);
            g.dispose();
            result.add(tgt);
        }
        return result;
    }

    private static AnimIterator createAnimIterator(TileSet tileSet, int tileCount) {
        List<Animation> animations = tileSet.getAnimationsList();
        List<IndexedAnimDesc> anims = new ArrayList<IndexedAnimDesc>(animations.size());
        for (Animation animation : animations) {
            anims.add(new IndexedAnimDesc(animation));
        }
        IndexedAnimIterator iterator = new IndexedAnimIterator(anims, tileCount);
        return iterator;
    }

    private static void buildCollisionConvexHulls(TileSet tileSet, BufferedImage image, TextureSet.Builder textureSet) {
        if (image != null) {
            ConvexHulls convexHulls = TileSetUtil.calculateConvexHulls(image.getAlphaRaster(), 16,
                    image.getWidth(), image.getHeight(), tileSet.getTileWidth(),
                    tileSet.getTileHeight(), tileSet.getTileMargin(), tileSet.getTileSpacing());

            for (int i = 0; i < convexHulls.hulls.length; ++i) {
                ConvexHull convexHull = convexHulls.hulls[i];
                Tile.ConvexHull.Builder hullBuilder = Tile.ConvexHull.newBuilder();
                String collisionGroup = "";
                if (i < tileSet.getConvexHullsCount()) {
                    collisionGroup = tileSet.getConvexHulls(i).getCollisionGroup();
                }
                hullBuilder.setCollisionGroup(collisionGroup).setCount(convexHull.getCount())
                        .setIndex(convexHull.getIndex());

                textureSet.addConvexHulls(hullBuilder);
            }

            for (int i = 0; i < convexHulls.points.length; ++i) {
                textureSet.addCollisionHullPoints(convexHulls.points[i]);
            }
        }
        textureSet.addAllCollisionGroups(tileSet.getCollisionGroupsList());
    }
}
