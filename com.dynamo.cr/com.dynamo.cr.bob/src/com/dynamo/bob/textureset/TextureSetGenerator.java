package com.dynamo.bob.textureset;

import java.awt.Color;
import java.awt.Graphics2D;
import java.awt.geom.AffineTransform;
import java.awt.image.BufferedImage;
import java.awt.image.Raster;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.lang.Float;

import javax.vecmath.Point2d;
import javax.vecmath.Vector2d;

import org.apache.commons.lang3.Pair;

import com.dynamo.bob.textureset.TextureSetLayout.Layout;
import com.dynamo.bob.textureset.TextureSetLayout.Rect;
import com.dynamo.bob.textureset.TextureSetLayout.Grid;
import com.dynamo.bob.tile.ConvexHull2D;
import com.dynamo.bob.tile.TileSetUtil;
import com.dynamo.bob.tile.TileSetUtil.ConvexHulls;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.textureset.proto.TextureSetProto;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.tile.proto.Tile.Playback;
import com.google.protobuf.ByteString;

public class TextureSetGenerator {

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

    public static class TextureSetResult {
        public TextureSet.Builder builder;
        public BufferedImage image;
        public List<UVTransform> uvTransforms;
    }


    private static void buildConvexHulls(TextureSet.Builder textureSet, List<BufferedImage> images, int margin, int innerPadding, int hullVertexCount) {

        if (hullVertexCount == 0) {
            System.out.println("Hull generation is disabled. Skipping");
            return;
        }

        int num_hulls = 0;
        int counter = 0;
        for (BufferedImage image : images) {
            int width = image.getWidth();
            int height = image.getHeight();



            Raster raster = image.getAlphaRaster();
            if (raster == null) {

        System.out.println("alphaRaster: " + (raster == null ? "null" : "not null"));

                continue;
            }

            float tileSizeXRecip = 1.0f / width;
            float tileSizeYRecip = 1.0f / height;
            ++num_hulls;

            ConvexHull2D.Point[] points = TileSetUtil.calculateConvexHulls(raster, hullVertexCount);

            TextureSetProto.SpritePolygon.Builder polygonBuilder = TextureSetProto.SpritePolygon.newBuilder();

            for (int i = 0; i < points.length; ++i) {
                float x = (points[i].getX() * tileSizeXRecip) - 0.5f;
                float y = (points[i].getY() * tileSizeYRecip) - 0.5f;

                polygonBuilder.addPoints(x);
                polygonBuilder.addPoints(y);

                polygonBuilder.addUvs(0); // need to calculate these later
                polygonBuilder.addUvs(0);
            }

            for (int v = 1; v <= points.length-2; ++v) {
                polygonBuilder.addIndices(0);
                polygonBuilder.addIndices(v);
                polygonBuilder.addIndices(v+1);
            }

            textureSet.addPolygons(polygonBuilder);
        }
    }

    // From the vertices and layout, generate UV coordinates
    private static void createPolygonUVs(TextureSet.Builder textureSet, List<Rect> rects, float width, float height) {
        System.out.println(String.format("num polygons: %d", textureSet.getPolygonsCount()));
        System.out.println(String.format("num rects: %d", rects.size()));

        int index = 0;
        for (TextureSetProto.SpritePolygon polygon : textureSet.getPolygonsList()) {

            TextureSetProto.SpritePolygon.Builder polygonBuilder = TextureSetProto.SpritePolygon.newBuilder();
            polygonBuilder.mergeFrom(polygon);

            int numPoints = polygon.getPointsCount() / 2;
            for (int i = 0; i < numPoints; ++i) {

                Rect rect = rects.get(index);

                float centerX = (float)rect.x + rect.width/2.0f;
                float centerY = (float)rect.y + rect.height/2.0f;

                // the points are in object space, where origin is at the center of the sprite image
                // in units
                float localX = polygon.getPoints(i * 2 + 0) * rect.width;
                float localY = polygon.getPoints(i * 2 + 1) * rect.height;

                // flip the y
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

                polygonBuilder.setUvs(i * 2 + 0, u);
                polygonBuilder.setUvs(i * 2 + 1, v);
            }

            textureSet.setPolygons(index, polygonBuilder);
            ++index;
        }
    }

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
    public static TextureSetResult generate(List<BufferedImage> images, AnimIterator iterator,
            int margin, int innerPadding, int extrudeBorders, boolean rotate, boolean useTileGrid, Grid gridSize) {

        TextureSet.Builder builder = TextureSet.newBuilder();

        int targetHullVertexCount = 4;
        buildConvexHulls(builder, images, margin, innerPadding, targetHullVertexCount);

        images = createInnerPadding(images, innerPadding);
        images = extrudeBorders(images, extrudeBorders);

        Layout layout;
        if (useTileGrid) {
            layout = TextureSetLayout.gridLayout(margin, rectanglesFromImages(images), gridSize);
        } else {
            layout = packedImageLayout(margin, images, rotate);
        }

        rotateImages(layout, images);

        BufferedImage image = composite(images, layout);

        List<Rect> rects = clipBorders(layout.getRectangles(), extrudeBorders);

        createPolygonUVs(builder, rects, image.getWidth(), image.getHeight());

        Pair<TextureSet.Builder, List<UVTransform>> textureSet = genVertexData(builder, image, rects, iterator);

        TextureSetResult result = new TextureSetResult();
        result.builder = textureSet.left;
        result.uvTransforms = textureSet.right;
        result.image = image;
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

    private static void rotateImages(Layout layout, List<BufferedImage> images) {
        List<Rect> rects = layout.getRectangles();
        for (Rect r: rects) {
            if (r.rotated) {
                Integer id = (Integer)r.id;
                BufferedImage src = images.get(id);
                BufferedImage rotated = rotateImage(src);
                images.set(id, rotated);
            }
        }
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

    private static Layout packedImageLayout(int margin, List<BufferedImage> images, boolean rotate ) {
        Layout layout = TextureSetLayout.packedLayout(margin, rectanglesFromImages(images), rotate);
        Collections.sort(layout.getRectangles(), new Comparator<Rect>() {
            @Override
            public int compare(Rect r1, Rect r2) {
                int i1 = (Integer) r1.id;
                int i2 = (Integer) r2.id;
                return i1 - i2;
            }
        });
        return layout;
    }

    private static List<Rect> rectanglesFromImages( List<BufferedImage> images ) {
        List<Rect> rectangles = new ArrayList<Rect>(images.size());
        int index = 0;
        for (BufferedImage image : images) {
            rectangles.add(new Rect(index++, image.getWidth(), image.getHeight()));
        }

        return rectangles;
    }

    private static BufferedImage composite(List<BufferedImage> images, Layout layout) {
        BufferedImage image = new BufferedImage(layout.getWidth(), layout.getHeight(), BufferedImage.TYPE_4BYTE_ABGR);
        Graphics2D g = image.createGraphics();
        for (Rect r : layout.getRectangles()) {
            g.drawImage(images.get((Integer)r.id), r.x, r.y, null);
        }
        g.dispose();
        return image;
    }

    private static List<Rect> clipBorders(List<Rect> rects, int borderWidth) {
        List<Rect> result = new ArrayList<Rect>(rects.size());
        for (Rect rect : rects) {
            Rect r = new Rect(rect.id, rect.width - borderWidth * 2, rect.height - borderWidth * 2);
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

    private static Pair<TextureSet.Builder, List<UVTransform>> genVertexData(TextureSet.Builder textureSet, BufferedImage image, List<Rect> rects, AnimIterator iterator) {
        List<UVTransform> uvTransforms = new ArrayList<UVTransform>();

        int tileCount = rects.size();
        textureSet.setTileCount(tileCount);

        int quadCount = tileCount;
        while (iterator.nextAnim() != null) {
            while (iterator.nextFrameIndex() != null) {
                ++quadCount;
            }
        }
        iterator.rewind();

        int vertexSize = TextureSetProto.Constants.VERTEX_SIZE.getNumber();

        final int numTexCoordsPerQuad = 8;
        ByteBuffer texCoordsBuffer = newBuffer(numTexCoordsPerQuad * 4 * quadCount);
        final int numTexDimsPerQuad = 2;
        ByteBuffer texDimsBuffer = newBuffer(numTexDimsPerQuad * 4 * quadCount);

        float oneOverWidth = 1.0f / image.getWidth();
        float oneOverHeight = 1.0f / image.getHeight();
        int quadIndex = 0;

        // Populate all tiles i.e. rects
        for (Rect r : rects) {
            putRect(r, oneOverWidth, oneOverHeight, texCoordsBuffer, texDimsBuffer);

            uvTransforms.add(genUVTransform(r, oneOverWidth, oneOverHeight));

            ++quadIndex;
        }

        AnimDesc animDesc = null;
        while ((animDesc = iterator.nextAnim()) != null) {
            Rect ref = null;
            Integer index = null;
            int startIndex = quadIndex;
            while ((index = iterator.nextFrameIndex()) != null) {
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

    private static short toShortUV(float fuv) {
        int uv = (int) (fuv * 65535.0f);
        return (short) (uv & 0xffff);
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
