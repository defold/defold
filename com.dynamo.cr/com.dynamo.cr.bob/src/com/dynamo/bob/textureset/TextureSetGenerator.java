// Copyright 2020-2024 The Defold Foundation
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

package com.dynamo.bob.textureset;

import com.dynamo.bob.pipeline.GraphicsUtil;

import com.dynamo.bob.textureset.TextureSetLayout.Grid;
import com.dynamo.bob.textureset.TextureSetLayout.Layout;
import com.dynamo.bob.textureset.TextureSetLayout.Rect;

import com.dynamo.bob.tile.ConvexHull2D;
import com.dynamo.bob.tile.TileSetUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.gamesys.proto.TextureSetProto;
import com.dynamo.gamesys.proto.TextureSetProto.SpriteGeometry;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.gamesys.proto.Tile.Playback;
import com.dynamo.gamesys.proto.Tile.SpriteTrimmingMode;
import com.google.protobuf.ByteString;

import javax.vecmath.Point2d;
import javax.vecmath.Vector2d;

import java.awt.Color;
import java.awt.Graphics2D;
import java.awt.geom.AffineTransform;
import java.awt.image.BufferedImage;
import java.awt.image.Raster;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.stream.Collectors;

/*
// For debugging image output
import java.io.IOException;
import java.io.File;
import javax.imageio.ImageIO;
*/

public class TextureSetGenerator {

    private static class Pair<L, R> {
        public Pair(L left, R right) {
            this.left = left;
            this.right = right;
        }
        public L left;
        public R right;
    }

    public static class AnimDesc {
        private String id;
        private final int fps;
        private final boolean flipHorizontally;
        private final Playback playback;
        private final boolean flipVertically;

        public AnimDesc(String id, Playback playback, int fps, boolean flipHorizontally, boolean flipVertically) {
            this.id = id;
            this.playback = playback;
            this.fps = fps;
            this.flipHorizontally = flipHorizontally;
            this.flipVertically = flipVertically;
        }

        public String getId() {
            return id;
        }

        public Playback getPlayback() {
            return playback;
        }

        public int getFps() {
            return fps;
        }

        public boolean isFlipHorizontally() {
            return flipHorizontally;
        }

        public boolean isFlipVertically() {
            return flipVertically;
        }
    }

    public interface AnimIterator {
        public AnimDesc nextAnim();         // Return the next animation
        public Integer nextFrameIndex();    // Return the global index of the image that the frame is using
        public String getFrameId();         // Returns unique frame id for the current frame
        public void rewind();               // Start iterating from the beginning
    }

    public static class UVTransform {
        public Point2d translation = new Point2d();
        public boolean rotated;
        public Vector2d scale = new Vector2d();

        public UVTransform() {
            this.translation.set(0.0, 1.0);
            this.scale.set(1.0, -1.0);
        }

        public UVTransform(Point2d translation, Vector2d scale, boolean rotated) {
            this.rotated = rotated;
            this.translation.set(translation);
            this.scale.set(scale);
        }

        public void apply(Point2d p) {
            if (rotated) {
                double x;
                double y;
                // centre on tile
                x = p.x - 0.5;
                y = p.y - 0.5;
                // rotate
                double tmp = x;
                x = -y;
                y = tmp;
                // rescale & recentre
                p.x = (x + 0.5) * this.scale.x;
                p.y = (y + 0.5) * this.scale.y;
                p.add(this.translation);
            } else {
                p.set(p.x * this.scale.x, p.y * this.scale.y);
                p.add(this.translation);
            }
        }
    }

    public static class LayoutResult {
        public final List<Layout> layouts;
        public final int innerPadding;
        public final int extrudeBorders;

        public LayoutResult(List<Layout> layouts, int innerPadding, int extrudeBorders) {
            this.layouts = layouts;
            this.innerPadding = innerPadding;
            this.extrudeBorders = extrudeBorders;
        }

        @Override
        public String toString() {
            String s = "LayoutResult:\n";
            s += String.format("  innerPadding: %d:\n", innerPadding);
            s += String.format("  extrudeBorders: %d:\n", extrudeBorders);
            for (Layout l : layouts) {
                s += String.format("%s:\n", l.toString());
            }
            s += "\n";
            return s;
        }
    }

    public static class TextureSetResult {
        public final TextureSet.Builder builder;
        public List<BufferedImage> images;
        public final List<UVTransform> uvTransforms;
        public final LayoutResult layoutResult;

        public TextureSetResult(TextureSet.Builder builder, List<UVTransform> uvTransforms, LayoutResult layoutResult) {
            this.builder = builder;
            this.images = new ArrayList<BufferedImage>();
            this.uvTransforms = uvTransforms;
            this.layoutResult = layoutResult;
        }
    }

    private static int spriteTrimModeToInt(SpriteTrimmingMode mode) {
        switch (mode) {
            case SPRITE_TRIM_MODE_OFF:   return 0;
            case SPRITE_TRIM_MODE_4:     return 4;
            case SPRITE_TRIM_MODE_5:     return 5;
            case SPRITE_TRIM_MODE_6:     return 6;
            case SPRITE_TRIM_MODE_7:     return 7;
            case SPRITE_TRIM_MODE_8:     return 8;
        }
        return 0;
    }

    // Pass in the original image (no padding or extrude borders)
    // Used by the editor
    public static SpriteGeometry buildConvexHull(BufferedImage image, SpriteTrimmingMode trimMode) {
        SpriteGeometry.Builder geometryBuilder = TextureSetProto.SpriteGeometry.newBuilder();

        int width = image.getWidth();
        int height = image.getHeight();

        geometryBuilder.setWidth(width);
        geometryBuilder.setHeight(height);
        geometryBuilder.setTrimMode(trimMode);

        // These are set later
        geometryBuilder.setCenterX(0.0f);
        geometryBuilder.setCenterY(0.0f);
        geometryBuilder.setRotated(false);

        ConvexHull2D.PointF[] points = null;

        int hullVertexCount = spriteTrimModeToInt(trimMode);
        Raster raster = image.getAlphaRaster();
        if (raster != null && hullVertexCount != 0) {
            int dilateCount = 2; // a pixel boundary to avoid filtering issues

            points = TileSetUtil.calculateConvexHull(raster, hullVertexCount, dilateCount);

            // If the hull wasn't valid, let's calculate a tight rect (slightly inflated)
            if (points == null) {
                points = TileSetUtil.calculateRect(raster, dilateCount);
            }
        }

        // We must respect the upper limit of the vertex hull count
        if (points == null || points.length > hullVertexCount) {
            // Generates a CW rect
            points = new ConvexHull2D.PointF[4];
            points[0] = new ConvexHull2D.PointF(-0.5,-0.5);
            points[1] = new ConvexHull2D.PointF(-0.5, 0.5);
            points[2] = new ConvexHull2D.PointF( 0.5, 0.5);
            points[3] = new ConvexHull2D.PointF( 0.5,-0.5);
        }

        for (int i = 0; i < points.length; ++i) {
            geometryBuilder.addVertices((float)points[i].getX());
            geometryBuilder.addVertices((float)points[i].getY());

            geometryBuilder.addUvs(0); // need to calculate these later
            geometryBuilder.addUvs(0);
        }

        for (int v = 1; v <= points.length-2; ++v) {
            geometryBuilder.addIndices(0);
            geometryBuilder.addIndices(v);
            geometryBuilder.addIndices(v+1);
        }

        return geometryBuilder.build();
    }

    // From the vertices and layout, generate UV coordinates
    private static SpriteGeometry.Builder createPolygonUVs(SpriteGeometry geometry, Rect rect, float width, float height) {
        SpriteGeometry.Builder geometryBuilder = TextureSetProto.SpriteGeometry.newBuilder();
        geometryBuilder.mergeFrom(geometry);

        int originalRectWidth = (rect.rotated ? rect.height : rect.width);
        int originalRectHeight = (rect.rotated ? rect.width : rect.height);
        float centerX = (float)rect.x + rect.width/2.0f;
        float centerY = (float)rect.y + rect.height/2.0f;

        geometryBuilder.setCenterX(centerX);
        geometryBuilder.setCenterY(centerY);
        geometryBuilder.setRotated(rect.rotated);

        // if (debug) {
        //     System.out.println(String.format("createPolygonUVs  - %s", rect.id));
        //     System.out.println(String.format("  cx/cy: %f, %f  ow/oh: %d, %d  numPoints: %d", centerX, centerY, originalRectWidth, originalRectHeight, geometry.getVerticesCount() / 2));
        //     System.out.println(String.format("  %d %d", rect.width, rect.height));
        // }

        int numPoints = geometry.getVerticesCount() / 2;
        for (int i = 0; i < numPoints; ++i) {

            // the points are in object space, where origin is at the center of the sprite image
            // in units [-0.5,0.5]
            // The polygon has a CCW orientation
            float localU = geometry.getVertices(i * 2 + 0);
            float localV = geometry.getVertices(i * 2 + 1);
            float localX = localU * originalRectWidth;
            float localY = localV * originalRectHeight;

            // Now the localX/Y is in original image space [(-width/2, -height/2), (width/2, height/2)]

            localY = -localY;

            if (rect.rotated) {
                // rotate 90 degrees ccw
                // where cos(pi/2)==0 and sin(pi/2)==1
                // xp = x * cos(a) - y * sin(a) = -y
                // yp = y * cos(a) + x * sin(a) = x
                float tmp = localX;
                localX = -localY;
                localY = tmp;
            }

            // from local unit space to image space
            float worldX = centerX + localX;
            float worldY = centerY + localY;

            float u = worldX / width;
            float v = 1.0f - worldY / height;

            geometryBuilder.setUvs(i * 2 + 0, u);
            geometryBuilder.setUvs(i * 2 + 1, v);

            // if (debug) {
            //     System.out.println(String.format("  uv: %f, %f   lu/lv: %f, %f  lx/ly: %f, %f  wx/wy: %f, %f", u, v, localU, localV, localX, localY, worldX, worldY));
            // }
        }

        return geometryBuilder;
    }

    /* Calculate the layout of a set of images
     * 1. Extrude rect borders and add inner padding
     * 2. Layout rects
     * 3. Shrink rects by previous extrusion
     */
    public static LayoutResult calculateLayoutResult(List<Rect> images, int margin, int innerPadding, int extrudeBorders, boolean rotate,
                                                    boolean useTileGrid, Grid gridSize, float maxPageSizeW, float maxPageSizeH) {
        TimeProfiler.start("calculateLayoutResult");

        int totalSizeIncrease = 2 * (innerPadding + extrudeBorders);

        List<Rect> resizedImages = images.stream()
                .map(i -> new Rect(i.id, 0, i.width + totalSizeIncrease, i.height + totalSizeIncrease))
                .collect(Collectors.toList());

        // Store rectangle order as the AnimIterator interface relies on stable frame indices.
        for(int i = 0; i < resizedImages.size(); i++) {
            resizedImages.get(i).index = i;
        }

        List<Layout> layouts;

        if (useTileGrid) {
            Layout layout = TextureSetLayout.gridLayout(margin, resizedImages, gridSize);
            layouts = new ArrayList<Layout>();
            layouts.add(layout);
        } else {
            layouts = TextureSetLayout.packedLayout(margin, resizedImages, rotate, maxPageSizeW, maxPageSizeH);
        }

        // Update the page indices
        int pageIndex = 0;
        for (Layout l : layouts) {
            // Update the rectangles in place
            for (Rect r : l.getRectangles()) {
                r.page = pageIndex;
            }
            pageIndex++;
        }

        LayoutResult result = new LayoutResult(layouts, innerPadding, extrudeBorders);

        TimeProfiler.stop();
        return result;
    }

    /**
     * Generate an atlas for individual images and animations. The basic steps of the algorithm are:
     * Create vertex data for each frame (image) in each animation
     */
    public static TextureSetResult calculateTextureSetResult(LayoutResult layout, List<SpriteGeometry> imageHulls, int useGeometries,
                                                             AnimIterator iterator) {

        TimeProfiler.start("calculateTextureSetResult");

        int layoutWidth = layout.layouts.get(0).getWidth();
        int layoutHeight = layout.layouts.get(0).getHeight();

        List<Rect> layoutRects = new ArrayList<Rect>();

        for (Layout l : layout.layouts) {
            layoutRects.addAll(l.getRectangles());
        }

        layoutRects.sort(Comparator.comparing(o -> o.index));

        // Contract the sizes rectangles (i.e remove the extrudeBorders from them)
        layoutRects = clipBorders(layoutRects, layout.extrudeBorders);

        Pair<TextureSet.Builder, List<UVTransform>> vertexData = buildData(layoutWidth, layoutHeight, layoutRects, iterator);

        vertexData.left.setUseGeometries(useGeometries);

        if (imageHulls != null) {
            for (Rect rect : layoutRects) {
                SpriteGeometry geometry = imageHulls.get(rect.index);
                vertexData.left.addGeometries(createPolygonUVs(geometry, rect, layoutWidth, layoutHeight));
            }
        }

        TimeProfiler.stop();
        return new TextureSetResult(vertexData.left, vertexData.right, layout);
    }

    // Deprecated
    public static TextureSetResult calculateLayout(List<Rect> images, List<SpriteGeometry> imageHulls, int useGeometries,
                                                    AnimIterator iterator, int margin, int innerPadding, int extrudeBorders,
                                                    boolean rotate, boolean useTileGrid, Grid gridSize, float maxPageSizeW, float maxPageSizeH) {

        LayoutResult layout = calculateLayoutResult(images, margin, innerPadding, extrudeBorders, rotate,
                                                    useTileGrid, gridSize, maxPageSizeW, maxPageSizeH);

        return calculateTextureSetResult(layout, imageHulls, useGeometries, iterator);
    }

    public static BufferedImage layoutImages(Layout layout, int innerPadding, int extrudeBorders, Map<String, BufferedImage> images) {
        BufferedImage packedImage = new BufferedImage(layout.getWidth(), layout.getHeight(), BufferedImage.TYPE_4BYTE_ABGR);
        Graphics2D g = packedImage.createGraphics();
        for (Rect r : layout.getRectangles()) {
            BufferedImage image = images.get(r.id);

            if (innerPadding > 0) {
                image = TextureUtil.createPaddedImage(image, innerPadding, paddingColour);
            }

            if (extrudeBorders > 0) {
                image = TextureUtil.extrudeBorders(image, extrudeBorders);
            }

            if (r.rotated) {
                image = rotateImage(image);
            }

            g.drawImage(image, r.x, r.y, null);
        }
        g.dispose();
        return packedImage;
    }

    // static int debugImageCount = 0;
    /**
     * Generate an atlas for individual images and animations. The basic steps of the algorithm are:
     *
     * 1. Extrude image borders
     * 2. Layout images
     * 3. Construct texture atlas
     * 4. Shrink inner rects by previous extrusion
     * 5. Create vertex data for each frame (image) in each animation
     *
     * @param images list of images
     * @param imagePaths corresponding image-id to previous list
     * @param animations list of animations
     * @param margin internal atlas margin
     * @return {@link AtlasMap}
     */
    public static TextureSetResult generate(List<BufferedImage> images, List<SpriteTrimmingMode> imageTrimModes, List<String> paths, AnimIterator iterator,
            int margin, int innerPadding, int extrudeBorders, boolean rotate, boolean useTileGrid, Grid gridSize,
            float maxPageSizeW, float maxPageSizeH) {

        List<Rect> imageRects = rectanglesFromImages(images, paths);

        // if all sizes are 0, we still need to generate hull (or rect) data
        // since it will still be part of the new code path if there is another atlas with trimming enabled
        List<SpriteGeometry> imageHulls = new ArrayList<SpriteGeometry>();
        int useGeometries = 0;
        for (int i = 0; i < images.size(); ++i) {
            BufferedImage image = images.get(i);
            useGeometries |= imageTrimModes.get(i) != SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF ? 1 : 0;
            imageHulls.add(buildConvexHull(image, imageTrimModes.get(i)));
        }

        // The layout step will expand the rect, and possibly rotate them
        TextureSetResult result = calculateLayout(imageRects, imageHulls, useGeometries, iterator,
            margin, innerPadding, extrudeBorders, rotate, useTileGrid, gridSize, maxPageSizeW, maxPageSizeH);

        for (Layout layout : result.layoutResult.layouts) {
            List<BufferedImage> layoutImages = new ArrayList<>();
            List<Rect> layoutRects           = layout.getRectangles();

            for (Rect rect : layoutRects) {
                BufferedImage image = images.get(rect.index);

                if (innerPadding > 0) {
                    image = TextureUtil.createPaddedImage(image, innerPadding, paddingColour);
                }
                if (extrudeBorders > 0) {
                    image = TextureUtil.extrudeBorders(image, extrudeBorders);
                }
                if (rect.rotated) {
                    image = rotateImage(image);
                }

                layoutImages.add(image);
            }

            BufferedImage imgOut = composite(layoutImages, layout.getWidth(), layout.getHeight(), layoutRects);
            result.images.add(imgOut);
            /*
            // For debugging page generation
            try {
                File outputfile = new File(String.format("image%d.png", debugImageCount));
                ++debugImageCount;
                ImageIO.write(imgOut, "png", outputfile);
                System.out.println(String.format("Wrote image to %s", outputfile.getAbsolutePath()));
            } catch (IOException e) {
                System.out.println(String.format("Unable to write debug image due to:\n%s", e.getMessage()));
            }
            */
        }

        return result;
    }

    private static BufferedImage rotateImage(BufferedImage src) {
        int width = src.getWidth();
        int height = src.getHeight();

        BufferedImage rotated = new BufferedImage(height, width, BufferedImage.TYPE_INT_ARGB);

        AffineTransform tx = new AffineTransform();
        // Centre on canvas
        tx.translate(height/2.0, width/2.0);
        // Rotation about image centre
        tx.rotate(Math.PI / 2.0);
        tx.translate(-width / 2.0, -height / 2.0);

        Graphics2D g = rotated.createGraphics();
        g.drawImage(src, tx, null);
        g.dispose();

        return rotated;
    }

    private static Color paddingColour = new Color(0,0,0,0);

    private static List<BufferedImage> createInnerPadding(List<BufferedImage> images, int amount) {
        if (0 < amount) {
            images = TextureUtil.createPaddedImages(images,  amount, paddingColour);
        }
        return images;
    }

    private static List<BufferedImage> extrudeBorders(List<BufferedImage> images, int amount) {
        if (amount > 0) {
            images = TextureUtil.extrudeBorders(images, amount);
        }
        return images;
    }

    private static List<Rect> rectanglesFromImages(List<BufferedImage> images, List<String> paths) {
        List<Rect> rectangles = new ArrayList<Rect>(images.size());
        int index = 0;
        for (BufferedImage image : images) {
            String path = paths.get(index);
            rectangles.add(new Rect(path, index, image.getWidth(), image.getHeight()));
            index++;
        }
        return rectangles;
    }

    private static BufferedImage composite(List<BufferedImage> images, int width, int height, List<Rect> rects) {
        BufferedImage image = new BufferedImage(width, height, BufferedImage.TYPE_4BYTE_ABGR);
        Graphics2D g = image.createGraphics();
        int i = 0;
        for (Rect r : rects) {
            g.drawImage(images.get(i++), r.x, r.y, null);
        }
        g.dispose();
        return image;
    }

    private static List<Rect> clipBorders(List<Rect> rects, int borderWidth) {
        List<Rect> result = new ArrayList<Rect>(rects.size());
        for (Rect rect : rects) {
            Rect r = new Rect(rect.id, rect.index, rect.width - borderWidth * 2, rect.height - borderWidth * 2);
            r.x = rect.x + borderWidth;
            r.y = rect.y + borderWidth;
            r.rotated = rect.rotated;
            r.page = rect.page;
            result.add(r);
        }
        return result;
    }

    private static UVTransform genUVTransform(Rect r, float xs, float ys) {
        return new UVTransform(new Point2d(r.x * xs, 1 - r.y * ys), new Vector2d(xs * r.width, -ys * r.height), r.rotated);
    }

    private static Pair<TextureSet.Builder, List<UVTransform>> buildData(int width, int height, List<Rect> rects, AnimIterator iterator) {
        TextureSet.Builder textureSet = TextureSet.newBuilder();
        ArrayList<UVTransform> uvTransforms = new ArrayList<>();

        textureSet.setWidth(width);
        textureSet.setHeight(height);

        int tileCount = rects.size();
        textureSet.setTileCount(tileCount);

        int quadCount = tileCount;
        while (iterator.nextAnim() != null) {
            while (iterator.nextFrameIndex() != null) {
                ++quadCount;
            }
        }
        iterator.rewind();

        final int numTexCoordsPerQuad = 8;
        ByteBuffer texCoordsBuffer = GraphicsUtil.newByteBuffer(numTexCoordsPerQuad * 4 * quadCount);
        final int numTexDimsPerQuad = 2;
        ByteBuffer texDimsBuffer = GraphicsUtil.newByteBuffer(numTexDimsPerQuad * 4 * quadCount);

        float oneOverWidth = 1.0f / width;
        float oneOverHeight = 1.0f / height;
        int quadIndex = 0;

        // Populate all single frame image animations
        for (Rect r : rects) {
            putRect(r, oneOverWidth, oneOverHeight, texCoordsBuffer, texDimsBuffer);

            uvTransforms.add(genUVTransform(r, oneOverWidth, oneOverHeight));


            textureSet.addImageNameHashes(MurmurHash.hash64(r.id));
            textureSet.addFrameIndices(quadIndex);
            textureSet.addPageIndices(r.page);
            ++quadIndex;
        }

        AnimDesc animDesc = null;
        while ((animDesc = iterator.nextAnim()) != null) {
            String animId = animDesc.getId();

            Rect ref = null;
            Integer index = null;
            int startIndex = quadIndex;
            int localIndex = 0; // 0 .. num_frames(anim)-1
            while ((index = iterator.nextFrameIndex()) != null) {

                String frameId = iterator.getFrameId(); // either "id" or "anim./id"
                long frameIdHash = MurmurHash.hash64(frameId);

                textureSet.addFrameIndices(index);
                textureSet.addImageNameHashes(frameIdHash);

                Rect r = rects.get(index);
                if (ref == null) {
                    ref = r;
                }
                putRect(r, oneOverWidth, oneOverHeight, texCoordsBuffer, texDimsBuffer);
                uvTransforms.add(genUVTransform(r, oneOverWidth, oneOverHeight));
                textureSet.addPageIndices(r.page);

                ++quadIndex;
                ++localIndex;
            }
            if (ref == null) {
                continue;
            }
            int endIndex = quadIndex;

            int animWidth;
            int animHeight;

            if (ref.rotated) {
                animWidth = ref.height;
                animHeight = ref.width;
            } else {
                animWidth = ref.width;
                animHeight = ref.height;
            }

            TextureSetAnimation anim = TextureSetAnimation.newBuilder()
                    .setId(animDesc.getId())
                    .setStart(startIndex)
                    .setEnd(endIndex)
                    .setPlayback(animDesc.getPlayback())
                    .setFps(animDesc.getFps())
                    .setFlipHorizontal(animDesc.isFlipHorizontally() ? 1 : 0)
                    .setFlipVertical(animDesc.isFlipVertically() ? 1 : 0)
                    .setWidth(animWidth)
                    .setHeight(animHeight)
                    .build();

            textureSet.addAnimations(anim);
        }

        assert(textureSet.getFrameIndicesList().size() == textureSet.getImageNameHashesList().size());

        texCoordsBuffer.rewind();
        texDimsBuffer.rewind();

        textureSet.setTexCoords(ByteString.copyFrom(texCoordsBuffer));
        textureSet.setTexDims(ByteString.copyFrom(texDimsBuffer));

        return new Pair<TextureSet.Builder, List<UVTransform>>(textureSet, uvTransforms);
    }

    private static void putTexCoord(ByteBuffer texCoordsBuffer, float u, float v) {
        if (null != texCoordsBuffer) {
            texCoordsBuffer.putFloat(u);
            texCoordsBuffer.putFloat(v);
        }
    }

    private static void putTexDim(ByteBuffer texDimsBuffer, float w, float h) {
        if (null != texDimsBuffer) {
            texDimsBuffer.putFloat(w);
            texDimsBuffer.putFloat(h);
        }
    }

    private static void putRect(Rect r, float oneOverWidth, float oneOverHeight, ByteBuffer texCoordsBuffer, ByteBuffer texDimsBuffer) {
        float x0 = r.x;
        float y0 = r.y;

        float x1 = r.x + r.width;
        float y1 = r.y + r.height;
        float w2 = r.width * 0.5f;
        float h2 = r.height * 0.5f;

        if (r.rotated) {
            putRotatedQuad(texCoordsBuffer, r, oneOverWidth, oneOverHeight);
            putTexDim(texDimsBuffer, r.height, r.width);
        } else {
            putUnrotatedQuad(texCoordsBuffer, r, oneOverWidth, oneOverHeight);
            putTexDim(texDimsBuffer, r.width, r.height);
        }
    }

    private static void putUnrotatedQuad(ByteBuffer texCoordsBuffer, Rect r, float xs, float ys) {
        float x0 = r.x;
        float y0 = r.y;

        float x1 = r.x + r.width;
        float y1 = r.y + r.height;
        float w2 = r.width * 0.5f;
        float h2 = r.height * 0.5f;

        putTexCoord(texCoordsBuffer, x0 * xs, 1.0f - y1 * ys);
        putTexCoord(texCoordsBuffer, x0 * xs, 1.0f - y0 * ys);
        putTexCoord(texCoordsBuffer, x1 * xs, 1.0f - y0 * ys);
        putTexCoord(texCoordsBuffer, x1 * xs, 1.0f - y1 * ys);
    }

    private static void putRotatedQuad(ByteBuffer texCoordsBuffer, Rect r,float xs, float ys) {
        float x0 = r.x;
        float y0 = r.y;

        float x1 = r.x + r.width;
        float y1 = r.y + r.height;
        float w2 = r.width * 0.5f;
        float h2 = r.height * 0.5f;

        putTexCoord(texCoordsBuffer, x0 * xs, 1.0f - y0 * ys);
        putTexCoord(texCoordsBuffer, x1 * xs, 1.0f - y0 * ys);
        putTexCoord(texCoordsBuffer, x1 * xs, 1.0f - y1 * ys);
        putTexCoord(texCoordsBuffer, x0 * xs, 1.0f - y1 * ys);
    }
}
