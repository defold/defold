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

package com.dynamo.bob.tile;

import java.awt.Graphics;
import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimDesc;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimIterator;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.bob.textureset.TextureSetLayout.Grid;
import com.dynamo.bob.textureset.TextureSetLayout.Rect;
import com.dynamo.bob.tile.TileSetUtil.ConvexHulls;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.gamesys.proto.Tile;
import com.dynamo.gamesys.proto.Tile.Animation;
import com.dynamo.gamesys.proto.Tile.SpriteTrimmingMode;
import com.dynamo.gamesys.proto.Tile.TileSet;
import com.dynamo.gamesys.proto.AtlasProto.AtlasImage;

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
        public String getFrameId() {
            return ""; // A tile source doesn't support playing single image animations using e.g. "tile0" etc.
        }

        @Override
        public void rewind() {
            nextAnimIndex = 0;
            nextFrameIndex = 0;
        }
    }

    public static TextureSetResult generate(TileSet tileSet, BufferedImage image, BufferedImage collisionImage) throws CompileExceptionError {
        Rect imageRect = image != null ? new Rect(null, -1, image.getWidth(), image.getHeight()) : null;
        Rect collisionRect = collisionImage != null ? new Rect(null, -1, collisionImage.getWidth(), collisionImage.getHeight()) : null;
        TileSetUtil.Metrics metrics = TileSetUtil.calculateMetrics(imageRect, tileSet.getTileWidth(),
                tileSet.getTileHeight(), tileSet.getTileMargin(), tileSet.getTileSpacing(), collisionRect, 1.0f, 0.0f);

        if (metrics == null) {
            return null;
        }

        int hullVertexSize = 6;
        SpriteTrimmingMode mode = tileSet.getSpriteTrimMode();

        List<BufferedImage> images = split(image, tileSet, metrics);
        List<AtlasImage> atlasImages = new ArrayList<>();
        List<String> names = new ArrayList<String>();
        int tileIndex = 0;
        for (BufferedImage tmp : images) {
            String name =  String.format("tile%d", tileIndex++);

            atlasImages.add(AtlasImage.newBuilder()
                            .setPivotX(0.5f)
                            .setPivotY(0.5f)
                            .setImage(name)
                            .setSpriteTrimMode(mode)
                            .build());
            names.add(name);
        }

        AnimIterator iterator = createAnimIterator(tileSet, images.size());

        // Since all the images already are positioned optimally in a grid,
        // we tell TextureSetGenerator to NOT do its own packing and use this grid directly.
        Grid grid_size = new Grid(metrics.tilesPerRow, metrics.tilesPerColumn);
        TextureSetResult result = TextureSetGenerator.generate(images, atlasImages, names, iterator, 0,
                tileSet.getInnerPadding(),
                tileSet.getExtrudeBorders(), false, true, grid_size, 0, 0);

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
            BufferedImage tgt = new BufferedImage(tileWidth, tileHeight, BufferedImage.TYPE_4BYTE_ABGR);
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
