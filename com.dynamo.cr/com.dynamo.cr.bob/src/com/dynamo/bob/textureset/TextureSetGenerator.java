package com.dynamo.bob.textureset;

import com.dynamo.bob.textureset.TextureSetLayout.Grid;
import com.dynamo.bob.textureset.TextureSetLayout.Layout;
import com.dynamo.bob.textureset.TextureSetLayout.Rect;

// The code below must remain identical to the implementation in the editor!
// ./editor/src/java/com/defold/editor/pipeline/TextureSetGenerator.java

import com.dynamo.bob.tile.ConvexHull2D;
import com.dynamo.bob.tile.TileSetUtil;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.textureset.proto.TextureSetProto;
import com.dynamo.textureset.proto.TextureSetProto.SpriteGeometry;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.tile.proto.Tile.Playback;
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

// For debugging image output
// import java.io.IOException;
// import java.io.File;
// import javax.imageio.ImageIO;

public class TextureSetGenerator {

    private static class Pair<L, R> {
        public Pair(L left, R right) {
            this.left = left;
            this.right = right;
        }
        public L left;
        public R right;
    }

    private static ByteBuffer newBuffer(int n) {
        ByteBuffer bb = ByteBuffer.allocateDirect(n);
        return bb.order(ByteOrder.LITTLE_ENDIAN);
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
        public AnimDesc nextAnim();
        public Integer nextFrameIndex();
        public void rewind();
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
        public final Layout layout;
        public final int innerPadding;
        public final int extrudeBorders;

        public LayoutResult(Layout layout, int innerPadding, int extrudeBorders) {
            this.layout = layout;
            this.innerPadding = innerPadding;
            this.extrudeBorders = extrudeBorders;
        }
    }

    public static class TextureSetResult {
        public final TextureSet.Builder builder;
        public BufferedImage image;
        public final List<UVTransform> uvTransforms;
        public final LayoutResult layoutResult;

        public TextureSetResult(TextureSet.Builder builder, List<UVTransform> uvTransforms, LayoutResult layoutResult) {
            this.builder = builder;
            this.image = null;
            this.uvTransforms = uvTransforms;
            this.layoutResult = layoutResult;
        }
    }

    // Pass in the original image (no padding or extrude borders)
    public static SpriteGeometry buildConvexHull(BufferedImage image, int hullVertexCount) {
        SpriteGeometry.Builder geometryBuilder = TextureSetProto.SpriteGeometry.newBuilder();
        int width = image.getWidth();
        int height = image.getHeight();

        geometryBuilder.setWidth(width);
        geometryBuilder.setHeight(height);

        float tileSizeXRecip = 1.0f / width;
        float tileSizeYRecip = 1.0f / height;
        float halfSizeX = width / 2.0f;
        float halfSizeY = height / 2.0f;

        ConvexHull2D.PointF[] points = null;

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
    private static SpriteGeometry.Builder createPolygonUVs(SpriteGeometry geometry, Rect rect, float width, float height, int extrudeBorders) {
        SpriteGeometry.Builder geometryBuilder = TextureSetProto.SpriteGeometry.newBuilder();
        geometryBuilder.mergeFrom(geometry);

        int originalRectWidth = (rect.rotated ? rect.height : rect.width) - 2*extrudeBorders;
        int originalRectHeight = (rect.rotated ? rect.width : rect.height) - 2*extrudeBorders;
        float centerX = (float)rect.x + rect.width/2.0f;
        float centerY = (float)rect.y + rect.height/2.0f;

        // if (debug) {
             // System.out.println(String.format("createPolygonUVs  - %s", rect.id));
             // System.out.println(String.format("  cx/cy: %f, %f  ow/oh: %d, %d  numPoints: %d", centerX, centerY, originalRectWidth, originalRectHeight, geometry.getVerticesCount() / 2));
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
    /**
     * Generate an atlas for individual images and animations. The basic steps of the algorithm are:
     *
     * 1. Extrude image borders
     * 2. Layout images
     * 3. Shrink inner rects by previous extrusion
     * 4. Create vertex data for each frame (image) in each animation
     */
    public static TextureSetResult calculateLayout(List<Rect> images, List<SpriteGeometry> imageHulls, int use_geometries,
                                                AnimIterator iterator,
                                               int margin, int innerPadding, int extrudeBorders,
                                               boolean rotate, boolean useTileGrid, Grid gridSize) {

        int totalSizeIncrease = 2 * (innerPadding + extrudeBorders);

        // Store rectangle order as the AnimIterator interface relies on stable frame indices.
        for(int i = 0; i < images.size(); i++) {
            images.get(i).index = i;
        }

        List<Rect> resizedImages = images.stream()
                .map(i -> new Rect(i.id, i.index, i.width + totalSizeIncrease, i.height + totalSizeIncrease))
                .collect(Collectors.toList());

        Layout layout;
        if (useTileGrid) {
            layout = TextureSetLayout.gridLayout(margin, resizedImages, gridSize);
        } else {
            layout = TextureSetLayout.packedLayout(margin, resizedImages, rotate);
        }

        layout.getRectangles().sort(Comparator.comparing(o -> o.index));

        // Contract the sizes rectangles (i.e remove the extrudeBorders from them)
        List<Rect> rects = clipBorders(layout.getRectangles(), extrudeBorders);

        Pair<TextureSet.Builder, List<UVTransform>> vertexData = genVertexData(layout.getWidth(), layout.getHeight(), rects, iterator);

        vertexData.left.setUseGeometries(use_geometries);

        if (imageHulls != null) {
            for (Rect rect : layout.getRectangles()) {
                SpriteGeometry geometry = imageHulls.get(rect.index);
                vertexData.left.addGeometries(createPolygonUVs(geometry, rect, layout.getWidth(), layout.getHeight(), extrudeBorders));
            }
        }

        return new TextureSetResult(vertexData.left, vertexData.right, new LayoutResult(layout, innerPadding, extrudeBorders));
    }

    public static BufferedImage layoutImages(LayoutResult layoutResult, Map<String, BufferedImage> images) {
        Layout layout = layoutResult.layout;

        BufferedImage packedImage = new BufferedImage(layout.getWidth(), layout.getHeight(), BufferedImage.TYPE_4BYTE_ABGR);
        Graphics2D g = packedImage.createGraphics();
        for (Rect r : layout.getRectangles()) {
            BufferedImage image = images.get(r.id);

            if (layoutResult.innerPadding > 0) {
                image = TextureUtil.createPaddedImage(image, layoutResult.innerPadding, paddingColour);
            }

            if (layoutResult.extrudeBorders > 0) {
                image = TextureUtil.extrudeBorders(image, layoutResult.extrudeBorders);
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
    public static TextureSetResult generate(List<BufferedImage> images, List<Integer> imageHullSizes, List<String> paths, AnimIterator iterator,
            int margin, int innerPadding, int extrudeBorders, boolean rotate, boolean useTileGrid, Grid gridSize) {

        List<Rect> imageRects = rectanglesFromImages(images, paths);

        // if all sizes are 0, we still need to generate hull (or rect) data
        // since it will still be part of the new code path if there is another atlas with trimming enabled
        List<SpriteGeometry> imageHulls = new ArrayList<SpriteGeometry>();
        int use_geometries = 0;
        for (int i = 0; i < images.size(); ++i) {
            BufferedImage image = images.get(i);
            use_geometries |= imageHullSizes.get(i) > 0 ? 1 : 0;
            imageHulls.add(buildConvexHull(image, imageHullSizes.get(i)));
        }

        // The layout step will expand the rect, and possibly rotate them
        TextureSetResult result = calculateLayout(imageRects, imageHulls, use_geometries, iterator,
                                                        margin, innerPadding, extrudeBorders, rotate, useTileGrid, gridSize);

        for (int i = 0; i < images.size(); ++i) {
            BufferedImage image = images.get(i);
            Rect rect = result.layoutResult.layout.getRectangles().get(i);

            if (innerPadding > 0) {
                image = TextureUtil.createPaddedImage(image, innerPadding, paddingColour);
            }
            if (extrudeBorders > 0) {
                image = TextureUtil.extrudeBorders(image, extrudeBorders);
            }
            if (rect.rotated) {
                image = rotateImage(image);
            }
            images.set(i, image);
        }

        result.image = composite(images, result.layoutResult.layout);

        // try {
        //     File outputfile = new File(String.format("image%d.png", debugImageCount));
        //     ++debugImageCount;
        //     ImageIO.write(result.image, "png", outputfile);
        //     System.out.println(String.format("Wrote image to %s", outputfile.getAbsolutePath()));
        // } catch (IOException e) {
        // }

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

    private static BufferedImage composite(List<BufferedImage> images, Layout layout) {
        BufferedImage image = new BufferedImage(layout.getWidth(), layout.getHeight(), BufferedImage.TYPE_4BYTE_ABGR);
        Graphics2D g = image.createGraphics();
        int i = 0;
        for (Rect r : layout.getRectangles()) {
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
            result.add(r);
        }
        return result;
    }

    private static UVTransform genUVTransform(Rect r, float xs, float ys) {
        return new UVTransform(new Point2d(r.x * xs, 1 - r.y * ys), new Vector2d(xs * r.width, -ys * r.height), r.rotated);
    }

    private static Pair<TextureSet.Builder, List<UVTransform>> genVertexData(int width, int height, List<Rect> rects, AnimIterator iterator) {
        TextureSet.Builder textureSet = TextureSet.newBuilder();
        ArrayList<UVTransform> uvTransforms = new ArrayList<>();

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
        ByteBuffer texCoordsBuffer = newBuffer(numTexCoordsPerQuad * 4 * quadCount);
        final int numTexDimsPerQuad = 2;
        ByteBuffer texDimsBuffer = newBuffer(numTexDimsPerQuad * 4 * quadCount);

        float oneOverWidth = 1.0f / width;
        float oneOverHeight = 1.0f / height;
        int quadIndex = 0;

        // Populate all tiles i.e. rects
        for (Rect r : rects) {
            putRect(r, oneOverWidth, oneOverHeight, texCoordsBuffer, texDimsBuffer);

            uvTransforms.add(genUVTransform(r, oneOverWidth, oneOverHeight));

            textureSet.addFrameIndices(quadIndex);
            ++quadIndex;
        }

        AnimDesc animDesc = null;
        while ((animDesc = iterator.nextAnim()) != null) {
            Rect ref = null;
            Integer index = null;
            int startIndex = quadIndex;
            while ((index = iterator.nextFrameIndex()) != null) {
                textureSet.addFrameIndices(index);

                Rect r = rects.get(index);
                if (ref == null) {
                    ref = r;
                }
                putRect(r, oneOverWidth, oneOverHeight, texCoordsBuffer, texDimsBuffer);

                uvTransforms.add(genUVTransform(r, oneOverWidth, oneOverHeight));

                ++quadIndex;
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

            TextureSetAnimation anim = TextureSetAnimation.newBuilder().setId(animDesc.getId()).setStart(startIndex)
                    .setEnd(endIndex).setPlayback(animDesc.getPlayback()).setFps(animDesc.getFps())
                    .setFlipHorizontal(animDesc.isFlipHorizontally() ? 1 : 0)
                    .setFlipVertical(animDesc.isFlipVertically() ? 1 : 0)
                    .setWidth(animWidth).setHeight(animHeight).build();

            textureSet.addAnimations(anim);
        }

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
