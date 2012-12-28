package com.dynamo.bob.atlas;

import java.awt.Graphics;
import java.awt.image.BufferedImage;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.apache.commons.lang3.Pair;

import com.dynamo.bob.atlas.AtlasLayout.Layout;
import com.dynamo.bob.atlas.AtlasLayout.LayoutType;
import com.dynamo.bob.atlas.AtlasLayout.Rect;
import com.dynamo.textureset.proto.TextureSetProto;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.tile.proto.Tile.Playback;
import com.google.protobuf.ByteString;

public class AtlasGenerator {

    private static ByteBuffer newBuffer(int n) {
        ByteBuffer bb = ByteBuffer.allocateDirect(n);
        return bb.order(ByteOrder.LITTLE_ENDIAN);
    }

    public static class AnimDesc {
        private String id;
        private List<String> ids;
        private final int fps;
        private final boolean flipHorizontally;
        private final Playback playback;
        private final boolean flipVertically;
        public AnimDesc(String id, List<String> ids, Playback playback, int fps, boolean flipHorizontally, boolean flipVertically) {
            this.id = id;
            this.ids = ids;
            this.playback = playback;
            this.fps = fps;
            this.flipHorizontally = flipHorizontally;
            this.flipVertically = flipVertically;
        }

        public String getId() {
            return id;
        }

        public List<String> getIds() {
            return ids;
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

    private static void duplicate(ByteBuffer buffer, int index, int count) {
        for (int i = 0; i < count; ++i) {
            buffer.put(buffer.get(index + i));
        }
    }

    /**
     * Generate an atlas for individual images and animations. The basic steps of the algorithm are:
     *
     * 1. All images in (images and imageIds) are laid out in an atlas. Vertex-data and separate texture coordinates are created
     *    for every image
     * 2. Vertex data and separate texture coordinates is created for animations. Animation must only refer to images in step 1
     *
     * @param images list of images
     * @param imageIds corresponding image-id to previous list
     * @param animations list of animations
     * @param margin internal atlas margin
     * @return {@link AtlasMap}
     */
    public static Pair<TextureSet.Builder, BufferedImage> generate(List<BufferedImage> images, List<String> imageIds, List<AnimDesc> animations, int margin) {

        List<AtlasLayout.Rect> rectangles = new ArrayList<AtlasLayout.Rect>(images.size());
        int i = 0;
        for (BufferedImage image : images) {
            rectangles.add(new Rect(i++, image.getWidth(), image.getHeight()));
        }

        Layout layout = AtlasLayout.layout(LayoutType.BASIC, margin, rectangles);

        BufferedImage image = new BufferedImage(layout.getWidth(), layout.getHeight(), BufferedImage.TYPE_4BYTE_ABGR);
        Graphics g = image.getGraphics();
        float xs = 1.0f / image.getWidth();
        float ys = 1.0f / image.getHeight();

        int totalAnimationFrameCount = 0;
        for (AnimDesc anim : animations) {
            totalAnimationFrameCount += anim.getIds().size();
        }

        int vertexSize = TextureSetProto.Constants.VERTEX_SIZE.getNumber();

        ByteBuffer vertexBuffer = newBuffer(vertexSize * 6 * (layout.getRectangles().size() + totalAnimationFrameCount));
        ByteBuffer outlineVertexBuffer = newBuffer(vertexSize * 4 * (layout.getRectangles().size() + totalAnimationFrameCount));
        ByteBuffer texCoordsBuffer = newBuffer(4 * 4 * (layout.getRectangles().size() + totalAnimationFrameCount));

        TextureSet.Builder textureSet = TextureSet.newBuilder();

        final int triangleVertexCount = 6;
        final int outlineVertexCount = 4;

        Map<String, TextureSetAnimation> idToAnimation = new HashMap<String, TextureSetAnimation>();
        int tileIndex = 0;

        // First layout all images
        for (Rect r : layout.getRectangles()) {
            int index = (Integer) r.id;
            g.drawImage(images.get(index), r.x, r.y, null);
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

            putVertex(outlineVertexBuffer, -w2, -h2, 0, x0 * xs, y1 * ys);
            putVertex(outlineVertexBuffer, w2, -h2, 0, x1 * xs, y1 * ys);
            putVertex(outlineVertexBuffer, w2, h2, 0, x1 * xs, y0 * ys);
            putVertex(outlineVertexBuffer, -w2, h2, 0, x0 * xs, y0 * ys);

            texCoordsBuffer.putFloat(x0 * xs);
            texCoordsBuffer.putFloat(y0 * ys);
            texCoordsBuffer.putFloat(x1 * xs);
            texCoordsBuffer.putFloat(y1 * ys);

            textureSet.addVertexStart(tileIndex * triangleVertexCount);
            textureSet.addVertexCount(triangleVertexCount);

            textureSet.addOutlineVertexStart(tileIndex * outlineVertexCount);
            textureSet.addOutlineVertexCount(outlineVertexCount);

            TextureSetAnimation anim = TextureSetAnimation.newBuilder()
                    .setId(imageIds.get(index))
                    .setStart(tileIndex)
                    .setEnd(tileIndex)
                    .setWidth(r.width)
                    .setHeight(r.height)
                    .build();

            textureSet.addAnimations(anim);

            idToAnimation.put(imageIds.get(index), anim);
            ++tileIndex;
        }
        g.dispose();

        // Create animation data for animations using idToAnimation
        for (AnimDesc anim : animations) {
            List<String> ids = anim.getIds();
            if (ids.size() > 0) {
                int animIndex = 0;
                for (String id : ids) {
                    TextureSetAnimation imageAnim = idToAnimation.get(id);

                    duplicate(vertexBuffer, imageAnim.getStart() * vertexSize * 6 , 6 * vertexSize);
                    duplicate(outlineVertexBuffer, imageAnim.getStart() * vertexSize * 4, 4 * vertexSize);
                    duplicate(texCoordsBuffer, imageAnim.getStart() * 4 * 4, 4 * 4);

                    textureSet.addVertexStart((tileIndex + animIndex) * triangleVertexCount);
                    textureSet.addVertexCount(triangleVertexCount);
                    textureSet.addOutlineVertexStart((tileIndex + animIndex) * outlineVertexCount);
                    textureSet.addOutlineVertexCount(outlineVertexCount);
                    ++animIndex;
                }

                TextureSetAnimation imageAnim = idToAnimation.get(ids.get(0));
                TextureSetAnimation setAnim = TextureSetAnimation.newBuilder()
                        .setId(anim.getId())
                        .setStart(tileIndex)
                        .setEnd(tileIndex + ids.size() - 1)
                        .setWidth(imageAnim.getWidth())
                        .setHeight(imageAnim.getHeight())
                        .setPlayback(anim.getPlayback())
                        .setFps(anim.getFps())
                        .setFlipHorizontal(anim.isFlipHorizontally() ? 1 : 0)
                        .setFlipVertical(anim.isFlipVertically() ? 1 : 0)
                        .setIsAnimation(1)
                        .build();

                textureSet.addAnimations(setAnim);

                tileIndex += ids.size();
            }
        }

        vertexBuffer.rewind();
        outlineVertexBuffer.rewind();
        texCoordsBuffer.rewind();

        textureSet.setVertices(ByteString.copyFrom(vertexBuffer));
        textureSet.setOutlineVertices(ByteString.copyFrom(outlineVertexBuffer));
        textureSet.setTexCoords(ByteString.copyFrom(texCoordsBuffer));

        return new Pair<TextureSetProto.TextureSet.Builder, BufferedImage>(textureSet, image);
    }

}
