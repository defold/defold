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

package com.dynamo.bob.textureset;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.GraphicsUtil;
import com.dynamo.bob.textureset.TextureSetLayout;
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
import com.dynamo.gamesys.proto.AtlasProto.AtlasImage;
import com.google.protobuf.ByteString;

import javax.vecmath.Point2d;
import javax.vecmath.Vector2d;

import java.awt.Color;
import java.awt.Graphics2D;
import java.awt.geom.AffineTransform;
import java.awt.image.BufferedImage;
import java.awt.image.Raster;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
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

    // Public Api: AnimIterator
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
    public static SpriteGeometry buildConvexHull(BufferedImage image, float pivotX, float pivotY, SpriteTrimmingMode trimMode) {
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

        // The runtime uses (0, 0) as the center of the image
        geometryBuilder.setPivotX(pivotX);
        geometryBuilder.setPivotY(pivotY);

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

    private static SpriteGeometry.Builder createSpriteGeometryFromRect(Rect rect) {
        SpriteGeometry.Builder builder = SpriteGeometry.newBuilder();

        boolean rotated = rect.getRotated();
        builder.setRotated(rotated);

        // may be rotated
        int imageWidth = rect.getWidth();
        int imageHeight = rect.getHeight();

        int originalImageWidth = rotated ? imageHeight : imageWidth;
        int originalImageHeight = rotated ? imageWidth : imageHeight;

        // The geometry wants the size in unrotated form
        builder.setWidth(originalImageWidth);
        builder.setHeight(originalImageHeight);

        TextureSetLayout.Point center = rect.getCenter();
        builder.setCenterX(center.x);
        builder.setCenterY(center.y);

        TextureSetLayout.Point pivot = rect.getPivot();
        {
            // Convert from texel coords to unit coords [-0.5, 0.5]
            // (it may actually extend outside of its original image space)
            float x = (pivot.x / (float)originalImageWidth) - 0.5f;
            float y = (pivot.y / (float)originalImageHeight) - 0.5f;

            builder.setPivotX(x);
            builder.setPivotY(y);
        }

        builder.addAllIndices(rect.getIndices());

        // Convert from origin at top left, to center of image
        // Also convert from image space (texels) to local UV space
        int index = 0;
        for (TextureSetLayout.Pointi vertex : rect.getVertices()) {
            float localX = vertex.x / (float)originalImageWidth - 0.5f;
            float localY = vertex.y / (float)originalImageHeight - 0.5f;
            builder.addVertices(localX);
            builder.addVertices(localY);
            index += 2;
        }

        builder.setTrimMode(SpriteTrimmingMode.SPRITE_TRIM_POLYGONS);

        return builder;
    }

    // From the vertices and layout, generate UV coordinates
    // Note: The UV calculation is mostly legacy code, and only used by the editor for rendering (in collections, animation previews)
    private static SpriteGeometry.Builder createPolygonUVs(SpriteGeometry.Builder geometryBuilder, Rect rect, float width, float height) {

        boolean rotated = rect.getRotated();
        int originalRectWidth = (rotated ? rect.getHeight() : rect.getWidth());
        int originalRectHeight = (rotated ? rect.getWidth() : rect.getHeight());


        TextureSetLayout.Point center = rect.getCenter();
        geometryBuilder.setCenterX(center.x);
        geometryBuilder.setCenterY(center.y);

        float centerX = center.x;
        float centerY = center.y;

        geometryBuilder.setRotated(rotated);

        // boolean debug = rect.getId() == "boy_slash6";
        // if (debug) {
        //     System.out.println(String.format("createPolygonUVs  - %s", rect.getId()));
        //     System.out.println(String.format("  page w/h: %f %f", width, height));
        //     System.out.println(String.format("  rect x/y/w/h: %d %d  %d %d", rect.getX(), rect.getY(), rect.getWidth(), rect.getHeight()));
        //     System.out.println(String.format("  cx/cy: %f, %f  ow/oh: %d, %d  numPoints: %d", centerX, centerY, originalRectWidth, originalRectHeight, geometryBuilder.getVerticesCount() / 2));
        // }
        int numPoints = geometryBuilder.getVerticesCount() / 2;

        geometryBuilder.clearUvs();

        for (int i = 0; i < numPoints; ++i) {

            // the points are in sprite image space, not rotated,
            // in units [-0.5,0.5]
            // where origin is at the center of the sprite image (i.e. at [0,0])
            // The polygons (see indices) has a CCW orientation
            float localU = geometryBuilder.getVertices(i * 2 + 0);
            float localV = geometryBuilder.getVertices(i * 2 + 1);
            float localX = localU * originalRectWidth;
            float localY = localV * originalRectHeight;

            // Now the localX/Y is in original image space [(-width/2, -height/2), (width/2, height/2)]

            localY = -localY;

            // A rotated image is stored with a 90 deg CW rotation in the final texture
            // so we need to convert the vertices into the uv space of that texture
            if (rotated) // rotate 90 degrees CW
            {
                float tmp = localX;
                localX = -localY;
                localY = tmp;
            }

            // from local unit space to image space
            float worldX = centerX + localX;
            float worldY = centerY + localY;

            float u = worldX / width;
            float v = 1.0f - worldY / height;

            geometryBuilder.addUvs(u);
            geometryBuilder.addUvs(v);

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
    public static LayoutResult calculateLayoutResult(List<Rect> images, int margin, int innerPadding, int extrudeBorders,
                                                    boolean rotate, boolean useTileGrid, Grid gridSize, float maxPageSizeW, float maxPageSizeH) throws CompileExceptionError {
        TimeProfiler.start("calculateLayoutResult");

        int totalSizeIncrease = 2 * (innerPadding + extrudeBorders);

        // Store rectangle order as the AnimIterator interface relies on stable frame indices.
        for(int i = 0; i < images.size(); i++) {
            images.get(i).setIndex(i);
        }

        List<Rect> resizedImages = images.stream()
                .map(i -> new Rect(i.getId(), i.getIndex(), i.getWidth() + totalSizeIncrease, i.getHeight() + totalSizeIncrease))
                .collect(Collectors.toList());

        // Store rectangle order as the AnimIterator interface relies on stable frame indices.
        for(int i = 0; i < resizedImages.size(); i++) {
            resizedImages.get(i).setIndex(i);
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
                r.setPage(pageIndex);
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

        layoutRects.sort(Comparator.comparing(o -> o.getIndex()));

        // Contract the sizes rectangles (i.e remove the extrudeBorders from them)
        layoutRects = clipBorders(layoutRects, layout.extrudeBorders);

        Pair<TextureSet.Builder, List<UVTransform>> vertexData = buildData(layoutWidth, layoutHeight, layoutRects, iterator);

        vertexData.left.setUseGeometries(useGeometries);

        if (imageHulls != null) {
            for (Rect rect : layoutRects) {
                SpriteGeometry geometry = imageHulls.get(rect.getIndex());
                SpriteGeometry.Builder geometryBuilder = TextureSetProto.SpriteGeometry.newBuilder();
                geometryBuilder.mergeFrom(geometry);

                TextureSetLayout.Point center = rect.getCenter();
                geometryBuilder.setCenterX(center.x);
                geometryBuilder.setCenterY(center.y);
                geometryBuilder.setRotated(rect.getRotated());

                vertexData.left.addGeometries(createPolygonUVs(geometryBuilder, rect, layoutWidth, layoutHeight));
            }
        }

        TimeProfiler.stop();
        return new TextureSetResult(vertexData.left, vertexData.right, layout);
    }

    // Deprecated
    public static TextureSetResult calculateLayout(List<Rect> images, List<SpriteGeometry> imageHulls, int useGeometries,
                                                    AnimIterator iterator, int margin, int innerPadding, int extrudeBorders,
                                                    boolean rotate, boolean useTileGrid, Grid gridSize, float maxPageSizeW, float maxPageSizeH) throws CompileExceptionError {

        LayoutResult layout = calculateLayoutResult(images, margin, innerPadding, extrudeBorders, rotate,
                                                    useTileGrid, gridSize, maxPageSizeW, maxPageSizeH);

        return calculateTextureSetResult(layout, imageHulls, useGeometries, iterator);
    }

    // Public api
    // Convert from image space coordinates to
    public static TextureSetResult createTextureSet(List<TextureSetLayout.Layout> layouts, AnimIterator iterator) {
        List<Rect> allRects = new ArrayList<>();

        for (Layout l : layouts) {
            List<Rect> rects = l.getRectangles();
            allRects.addAll(rects);
        }

        allRects.sort(Comparator.comparing(o -> o.getIndex()));

        // Assuming that the first texture is the largest
        int largestPageWidth = layouts.get(0).getWidth();
        int largestPageHeight = layouts.get(0).getHeight();

        Pair<TextureSet.Builder, List<UVTransform>> vertexData = buildData(largestPageWidth, largestPageHeight, allRects, iterator);
        TextureSet.Builder builder = vertexData.left;
        for (Rect rect : allRects) {
            // Since the actual textures may be of different size (in the editor), we'll use the correct page size
            Layout layout = layouts.get(rect.getPage());
            int pageWidth = layout.getWidth();
            int pageHeight = layout.getHeight();

            SpriteGeometry.Builder geometryBuilder = createSpriteGeometryFromRect(rect);
            createPolygonUVs(geometryBuilder, rect, pageWidth, pageHeight);
            builder.addGeometries(geometryBuilder);
        }
        builder.setUseGeometries(1);

        return new TextureSetResult(builder, vertexData.right, new LayoutResult(layouts, 0, 0));
    }

    public static BufferedImage layoutImages(Layout layout, int innerPadding, int extrudeBorders, Map<String, BufferedImage> images) {
        BufferedImage packedImage = new BufferedImage(layout.getWidth(), layout.getHeight(), BufferedImage.TYPE_4BYTE_ABGR);
        Graphics2D g = packedImage.createGraphics();
        for (Rect r : layout.getRectangles()) {
            BufferedImage image = images.get(r.getId());

            if (innerPadding > 0) {
                image = TextureUtil.createPaddedImage(image, innerPadding, paddingColour);
            }

            if (extrudeBorders > 0) {
                image = TextureUtil.extrudeBorders(image, extrudeBorders);
            }

            if (r.getRotated()) {
                image = rotateImage(image);
            }

            g.drawImage(image, r.getX(), r.getY(), null);
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
    public static TextureSetResult generate(List<BufferedImage> images, List<AtlasImage> atlasImages, List<String> paths, AnimIterator iterator,
            int margin, int innerPadding, int extrudeBorders, boolean rotate, boolean useTileGrid, Grid gridSize,
            float maxPageSizeW, float maxPageSizeH) throws CompileExceptionError {

        List<Rect> imageRects = rectanglesFromImages(images, paths);

        // if all sizes are 0, we still need to generate hull (or rect) data
        // since it will still be part of the new code path if there is another atlas with trimming enabled
        List<SpriteGeometry> imageHulls = new ArrayList<SpriteGeometry>();
        int useGeometries = 0;
        for (int i = 0; i < images.size(); ++i) {
            BufferedImage image = images.get(i);
            AtlasImage atlasImage = atlasImages.get(i);
            SpriteTrimmingMode trimMode = atlasImage.getSpriteTrimMode();
            useGeometries |= trimMode != SpriteTrimmingMode.SPRITE_TRIM_MODE_OFF ? 1 : 0;

            // Image has pivot (0.5, 0.5) as center of image, and +Y as down (i.e. image space)
            // Whereas SpriteGeometry has (0, 0) as center and +Y as up
            float pivotX = atlasImage.getPivotX() - 0.5f;
            float pivotY = atlasImage.getPivotY() - 0.5f;
            pivotY = -pivotY;

            imageHulls.add(buildConvexHull(image, pivotX, pivotY, trimMode));
        }

        // The layout step will expand the rect, and possibly rotate them
        TextureSetResult result = calculateLayout(imageRects, imageHulls, useGeometries, iterator,
            margin, innerPadding, extrudeBorders, rotate, useTileGrid, gridSize, maxPageSizeW, maxPageSizeH);

        for (Layout layout : result.layoutResult.layouts) {
            List<BufferedImage> layoutImages = new ArrayList<>();
            List<Rect> layoutRects           = layout.getRectangles();

            for (Rect rect : layoutRects) {
                BufferedImage image = images.get(rect.getIndex());

                if (innerPadding > 0) {
                    image = TextureUtil.createPaddedImage(image, innerPadding, paddingColour);
                }
                if (extrudeBorders > 0) {
                    image = TextureUtil.extrudeBorders(image, extrudeBorders);
                }
                if (rect.getRotated()) {
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
            g.drawImage(images.get(i++), r.getX(), r.getY(), null);
        }
        g.dispose();
        return image;
    }

    private static List<Rect> clipBorders(List<Rect> rects, int borderWidth) {
        List<Rect> result = new ArrayList<Rect>(rects.size());
        for (Rect rect : rects) {
            Rect r = new Rect(rect);
            r.addBorder(-borderWidth);
            result.add(r);
        }
        return result;
    }

    private static UVTransform genUVTransform(Rect r, float xs, float ys) {
        return new UVTransform(new Point2d(r.getX() * xs, 1 - r.getY() * ys), new Vector2d(xs * r.getWidth(), -ys * r.getHeight()), r.getRotated());
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

            textureSet.addImageNameHashes(MurmurHash.hash64(r.getId()));
            textureSet.addFrameIndices(quadIndex);
            textureSet.addPageIndices(r.getPage());
            ++quadIndex;
        }

        AnimDesc animDesc = null;
        while ((animDesc = iterator.nextAnim()) != null) {
            Rect ref = null;
            Integer index = null;
            int startIndex = quadIndex;
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
                textureSet.addPageIndices(r.getPage());

                ++quadIndex;
            }
            if (ref == null) {
                continue;
            }
            int endIndex = quadIndex;

            int animWidth;
            int animHeight;

            if (ref.getRotated()) {
                animWidth = ref.getHeight();
                animHeight = ref.getWidth();
            } else {
                animWidth = ref.getWidth();
                animHeight = ref.getHeight();
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
        float width = r.getWidth();
        float height = r.getHeight();
        if (r.getRotated()) {
            putRotatedQuad(texCoordsBuffer, r, oneOverWidth, oneOverHeight);
            putTexDim(texDimsBuffer, height, width);
        } else {
            putUnrotatedQuad(texCoordsBuffer, r, oneOverWidth, oneOverHeight);
            putTexDim(texDimsBuffer, width, height);
        }
    }

    private static void putUnrotatedQuad(ByteBuffer texCoordsBuffer, Rect r, float xs, float ys) {
        float width = r.getWidth();
        float height = r.getHeight();
        float x0 = r.getX();
        float y0 = r.getY();
        float x1 = x0 + width;
        float y1 = y0 + height;

        putTexCoord(texCoordsBuffer, x0 * xs, 1.0f - y1 * ys);
        putTexCoord(texCoordsBuffer, x0 * xs, 1.0f - y0 * ys);
        putTexCoord(texCoordsBuffer, x1 * xs, 1.0f - y0 * ys);
        putTexCoord(texCoordsBuffer, x1 * xs, 1.0f - y1 * ys);
    }

    private static void putRotatedQuad(ByteBuffer texCoordsBuffer, Rect r,float xs, float ys) {
        float width = r.getWidth();
        float height = r.getHeight();
        float x0 = r.getX();
        float y0 = r.getY();
        float x1 = x0 + width;
        float y1 = y0 + height;

        putTexCoord(texCoordsBuffer, x0 * xs, 1.0f - y0 * ys);
        putTexCoord(texCoordsBuffer, x1 * xs, 1.0f - y0 * ys);
        putTexCoord(texCoordsBuffer, x1 * xs, 1.0f - y1 * ys);
        putTexCoord(texCoordsBuffer, x0 * xs, 1.0f - y1 * ys);
    }
}
