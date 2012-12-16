package com.dynamo.bob.atlas;

import java.awt.Graphics;
import java.awt.image.BufferedImage;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.dynamo.bob.atlas.AtlasLayout.Layout;
import com.dynamo.bob.atlas.AtlasLayout.LayoutType;
import com.dynamo.bob.atlas.AtlasLayout.Rect;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.dynamo.tile.proto.Tile.Playback;

public class AtlasGenerator {

    public static int COMPONENT_COUNT = 5;

    private static FloatBuffer newFloatBuffer(int n) {
        ByteBuffer bb = ByteBuffer.allocateDirect(n * 4);
        return bb.order(ByteOrder.nativeOrder()).asFloatBuffer();
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

    /**
     * Generate an atlas-map for individual images and animations. The basic steps of the algorithm are:
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
    public static AtlasMap generate(List<BufferedImage> images, List<String> imageIds, List<AnimDesc> animations, int margin) {
        List<AtlasLayout.Rect> rectangles = new ArrayList<AtlasLayout.Rect>(images.size());
        int i = 0;
        for (BufferedImage image : images) {
            rectangles.add(new Rect(i++, image.getWidth(), image.getHeight()));
        }

        Layout layout = AtlasLayout.layout(LayoutType.BASIC, margin, rectangles);

        List<TextureSetAnimation> tiles = new ArrayList<TextureSetAnimation>(layout.getRectangles().size());

        BufferedImage image = new BufferedImage(layout.getWidth(), layout.getHeight(), BufferedImage.TYPE_4BYTE_ABGR);
        Graphics g = image.getGraphics();
        float xs = 1.0f / image.getWidth();
        float ys = 1.0f / image.getHeight();

        int totalAnimationFrameCount = 0;
        for (AnimDesc anim : animations) {
            totalAnimationFrameCount += anim.getIds().size();
        }

        final int components = COMPONENT_COUNT;
        FloatBuffer vertexBuffer = newFloatBuffer(components * 6 * (layout.getRectangles().size() + totalAnimationFrameCount));
        FloatBuffer outlineVertexBuffer = newFloatBuffer(components * 4 * (layout.getRectangles().size() + totalAnimationFrameCount));
        FloatBuffer texCoordsBuffer = newFloatBuffer(4 * (layout.getRectangles().size() + totalAnimationFrameCount));

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
            float[] vertices = new float[] {
                    x0 * xs, y1 * ys, -w2, -h2, 0,
                    x1 * xs, y1 * ys, w2, -h2, 0,
                    x1 * xs, y0 * ys, w2, h2, 0,

                    x0 * xs, y1 * ys, -w2, -h2, 0,
                    x1 * xs, y0 * ys, w2, h2, 0,
                    x0 * xs, y0 * ys, -w2, h2, 0,
            };

            float[] outlineVertices = new float[] {
                    x0 * xs, y1 * ys, -w2, -h2, 0,
                    x1 * xs, y1 * ys, w2, -h2, 0,
                    x1 * xs, y0 * ys, w2, h2, 0,
                    x0 * xs, y0 * ys, -w2, h2, 0,
            };

            float[] texCoords = new float[] {
                    x0 * xs, y0 * ys,
                    x1 * xs, y1 * ys,
            };

            vertexBuffer.put(vertices);
            outlineVertexBuffer.put(outlineVertices);
            texCoordsBuffer.put(texCoords);

            TextureSetAnimation anim = TextureSetAnimation.newBuilder()
                    .setId(imageIds.get(index))
                    .setStart(tileIndex)
                    .setEnd(tileIndex)
                    .setWidth(r.width)
                    .setHeight(r.height)
                    .build();

            tiles.add(anim);
            idToAnimation.put(imageIds.get(index), anim);
            ++tileIndex;
        }
        g.dispose();

        // Create animation data for animations using idToAnimation
        for (AnimDesc anim : animations) {
            List<String> ids = anim.getIds();
            if (ids.size() > 0) {
                for (String id : ids) {
                    TextureSetAnimation imageAnim = idToAnimation.get(id);

                    duplicate(vertexBuffer, imageAnim.getStart() * components * 6 , 6 * components);
                    duplicate(outlineVertexBuffer, imageAnim.getStart() * components * 4, 4 * components);
                    duplicate(texCoordsBuffer, imageAnim.getStart() * 4, 4);
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

                tiles.add(setAnim);

                tileIndex += ids.size();
            }
        }

        vertexBuffer.rewind();
        outlineVertexBuffer.rewind();
        texCoordsBuffer.rewind();

        AtlasMap atlasMap = new AtlasMap(image, tiles, vertexBuffer, outlineVertexBuffer, texCoordsBuffer);
        return atlasMap;
    }

    private static void duplicate(FloatBuffer buffer, int index, int count) {
        for (int i = 0; i < count; ++i) {
            buffer.put(buffer.get(index + i));
        }
    }

}
