package com.dynamo.bob.textureset;

import java.awt.Color;
import java.awt.Graphics;
import java.awt.image.BufferedImage;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

import javax.vecmath.Point2d;
import javax.vecmath.Vector2d;

import org.apache.commons.lang3.Pair;

import com.dynamo.bob.textureset.TextureSetLayout.Layout;
import com.dynamo.bob.textureset.TextureSetLayout.LayoutType;
import com.dynamo.bob.textureset.TextureSetLayout.Rect;
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
        public Vector2d scale = new Vector2d();

        public UVTransform() {
            this.translation.set(0.0, 0.0);
            this.scale.set(1.0, 1.0);
        }

        public UVTransform(Point2d translation, Vector2d scale) {
            this.translation.set(translation);
            this.scale.set(scale);
        }

        public void apply(Point2d p) {
            p.set(p.x * this.scale.x, p.y * this.scale.y);
            p.add(this.translation);
        }
    }

    public static class TextureSetResult {
        public TextureSet.Builder builder;
        public BufferedImage image;
        public List<UVTransform> uvTransforms;
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
            int margin, int innerPadding, int extrudeBorders, boolean genOutlines) {

        images = createInnerPadding(images, innerPadding);
        images = extrudeBorders(images, extrudeBorders);

        Layout layout = layout(LayoutType.BASIC, margin, images);

        BufferedImage image = composite(images, layout);

        List<Rect> rects = clipBorders(layout.getRectangles(), extrudeBorders);

        Pair<TextureSet.Builder, List<UVTransform>> textureSet = genVertexData(image, rects, iterator, genOutlines);

        TextureSetResult result = new TextureSetResult();
        result.builder = textureSet.left;
        result.uvTransforms = textureSet.right;
        result.image = image;
        return result;
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

    private static Layout layout(LayoutType type, int margin, List<BufferedImage> images) {
        List<Rect> rectangles = new ArrayList<Rect>(images.size());
        int index = 0;
        for (BufferedImage image : images) {
            rectangles.add(new Rect(index++, image.getWidth(), image.getHeight()));
        }

        Layout layout = TextureSetLayout.layout(LayoutType.BASIC, margin, rectangles);
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

    private static BufferedImage composite(List<BufferedImage> images, Layout layout) {
        BufferedImage image = new BufferedImage(layout.getWidth(), layout.getHeight(), BufferedImage.TYPE_4BYTE_ABGR);
        Graphics g = image.getGraphics();
        for (Rect r : layout.getRectangles()) {
            int index = (Integer) r.id;
            g.drawImage(images.get(index), r.x, r.y, null);
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
            result.add(r);
        }
        return result;
    }

    private static UVTransform genUVTransform(Rect r, float xs, float ys) {
        return new UVTransform(new Point2d(r.x * xs, r.y * ys), new Vector2d(xs * r.width, ys * r.height));
    }

    private static Pair<TextureSet.Builder, List<UVTransform>> genVertexData(BufferedImage image, List<Rect> rects, AnimIterator iterator,
            boolean genOutlines) {
        TextureSet.Builder textureSet = TextureSet.newBuilder();
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

        final int triangleVertexCount = 6;
        final int outlineVertexCount = 4;

        ByteBuffer vertexBuffer = newBuffer(vertexSize * triangleVertexCount * quadCount);
        ByteBuffer outlineVertexBuffer = null;
        if (genOutlines) {
            outlineVertexBuffer = newBuffer(vertexSize * outlineVertexCount * quadCount);
        }
        ByteBuffer texCoordsBuffer = newBuffer(4 * 4 * quadCount);

        float xs = 1.0f / image.getWidth();
        float ys = 1.0f / image.getHeight();
        int quadIndex = 0;

        // Populate all tiles i.e. rects
        for (Rect r : rects) {
            putRect(r, xs, ys, vertexBuffer, outlineVertexBuffer, texCoordsBuffer);

            textureSet.addVertexStart(quadIndex * triangleVertexCount);
            textureSet.addVertexCount(triangleVertexCount);

            if (genOutlines) {
                textureSet.addOutlineVertexStart(quadIndex * outlineVertexCount);
                textureSet.addOutlineVertexCount(outlineVertexCount);
            }

            uvTransforms.add(genUVTransform(r, xs, ys));

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
                putRect(r, xs, ys, vertexBuffer, outlineVertexBuffer, texCoordsBuffer);

                textureSet.addVertexStart(quadIndex * triangleVertexCount);
                textureSet.addVertexCount(triangleVertexCount);

                if (genOutlines) {
                    textureSet.addOutlineVertexStart(quadIndex * outlineVertexCount);
                    textureSet.addOutlineVertexCount(outlineVertexCount);
                }

                uvTransforms.add(genUVTransform(r, xs, ys));

                ++quadIndex;
            }
            if (ref == null) {
                continue;
            }
            int endIndex = quadIndex;

            TextureSetAnimation anim = TextureSetAnimation.newBuilder().setId(animDesc.getId()).setStart(startIndex)
                    .setEnd(endIndex).setPlayback(animDesc.getPlayback()).setFps(animDesc.getFps())
                    .setFlipHorizontal(animDesc.isFlipHorizontally() ? 1 : 0)
                    .setFlipVertical(animDesc.isFlipVertically() ? 1 : 0)
.setWidth(ref.width).setHeight(ref.height)
                    .build();

            textureSet.addAnimations(anim);
        }

        vertexBuffer.rewind();
        texCoordsBuffer.rewind();

        textureSet.setVertices(ByteString.copyFrom(vertexBuffer));
        textureSet.setTexCoords(ByteString.copyFrom(texCoordsBuffer));

        if (outlineVertexBuffer != null) {
            outlineVertexBuffer.rewind();
            textureSet.setOutlineVertices(ByteString.copyFrom(outlineVertexBuffer));
        } else {
            textureSet.setOutlineVertices(ByteString.EMPTY);
        }
        return new Pair<TextureSet.Builder, List<UVTransform>>(textureSet, uvTransforms);
    }

    private static short toShortUV(float fuv) {
        int uv = (int) (fuv * 65535.0f);
        return (short) (uv & 0xffff);
    }

    private static void putVertex(ByteBuffer b, float x, float y, float z, float u, float v) {
        b.putFloat(x);
        b.putFloat(y);
        b.putFloat(z);
        b.putShort(toShortUV(u));
        b.putShort(toShortUV(v));
    }

    private static void putRect(Rect r, float xs, float ys, ByteBuffer vertexBuffer, ByteBuffer outlineVertexBuffer,
            ByteBuffer texCoordsBuffer) {
        float x0 = r.x;
        float y0 = r.y;
        float x1 = r.x + r.width;
        float y1 = r.y + r.height;
        float w2 = r.width * 0.5f;
        float h2 = r.height * 0.5f;
        putVertex(vertexBuffer, -w2, -h2, 0, x0 * xs, y1 * ys);
        putVertex(vertexBuffer, w2, -h2, 0, x1 * xs, y1 * ys);
        putVertex(vertexBuffer, w2, h2, 0, x1 * xs, y0 * ys);

        putVertex(vertexBuffer, -w2, -h2, 0, x0 * xs, y1 * ys);
        putVertex(vertexBuffer, w2, h2, 0, x1 * xs, y0 * ys);
        putVertex(vertexBuffer, -w2, h2, 0, x0 * xs, y0 * ys);

        if (outlineVertexBuffer != null) {
            putVertex(outlineVertexBuffer, -w2, -h2, 0, x0 * xs, y1 * ys);
            putVertex(outlineVertexBuffer, w2, -h2, 0, x1 * xs, y1 * ys);
            putVertex(outlineVertexBuffer, w2, h2, 0, x1 * xs, y0 * ys);
            putVertex(outlineVertexBuffer, -w2, h2, 0, x0 * xs, y0 * ys);
        }

        texCoordsBuffer.putFloat(x0 * xs);
        texCoordsBuffer.putFloat(y0 * ys);
        texCoordsBuffer.putFloat(x1 * xs);
        texCoordsBuffer.putFloat(y1 * ys);

    }
}
