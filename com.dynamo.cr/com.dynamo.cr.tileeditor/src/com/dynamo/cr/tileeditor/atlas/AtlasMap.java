package com.dynamo.cr.tileeditor.atlas;

import java.awt.image.BufferedImage;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;

/**
 * Atlas-map representation. The fundamental entity is "animation". Separate
 * images are represented by single-frame animation.
 * A single vertex-buffer is created for all animations. Linear access to animation frames can be assumed.
 * @author chmu
 *
 */
public class AtlasMap {

    private final BufferedImage image;
    private final List<TextureSetAnimation> animations;
    private FloatBuffer vertexBuffer;
    private FloatBuffer outlineVertexBuffer;
    private final FloatBuffer texCoordsBuffer;

    public AtlasMap(BufferedImage image, List<TextureSetAnimation> animations, FloatBuffer vertexBuffer, FloatBuffer outlineVertexBuffer, FloatBuffer texCoordsBuffer) {
        this.image = image;
        this.animations = Collections.unmodifiableList(new ArrayList<TextureSetAnimation>(animations));
        this.vertexBuffer = vertexBuffer;
        this.outlineVertexBuffer = outlineVertexBuffer;
        this.texCoordsBuffer = texCoordsBuffer;
    }

    /**
     * Get image representing the atlas
     * @return {@link BufferedImage}
     */
    public BufferedImage getImage() {
        return image;
    }

    /**
     * Get all animations
     * @return
     */
    public List<TextureSetAnimation> getAnimations() {
        return animations;
    }

    /**
     * Get vertex buffer representing all frames
     * The vertex buffer should be drawn using triangles
     * @return {@link FloatBuffer} with vertex-data
     */
    public FloatBuffer getVertexBuffer() {
        return vertexBuffer;
    }

    /**
     * Get outline vertex buffer representing all frames
     * The vertex buffer should be drawn using line-loops.
     * @return {@link FloatBuffer} with outline vertex-data
     */
    public FloatBuffer getOutlineVertexBuffer() {
        return outlineVertexBuffer;
    }

    /**
     * Get texture coordinates for all frames
     * @note Texture coordinates represents min/max, i.e. bounds, of a frame using four floats.
     *       Hence, the format is not suitable for direct drawing
     * @return {@link FloatBuffer} with texture coordinates
     */
    public FloatBuffer getTexCoordsBuffer() {
        return texCoordsBuffer;
    }

    /**
     * Get vertex-buffer start index
     * @param anim animation to get start index for
     * @return start index
     */
    public int getVertexStart(TextureSetAnimation anim) {
        return anim.getStart() * 6;
    }

    /**
     * Get vertex count for animation
     * @param anim animation to get count for
     * @return vertex count
     */
    public int getVertexCount(TextureSetAnimation anim) {
        return (anim.getEnd() - anim.getStart() + 1) * 6;
    }

    /**
     * Get outline vertex-buffer start index
     * @param anim animation to get start index for
     * @return start index
     */
    public int getOutlineVertexStart(TextureSetAnimation anim) {
        return anim.getStart() * 4;
    }

    /**
     * Get outline vertex count for animation
     * @param anim animation to get count for
     * @return vertex count
     */
    public int getOutlineVertexCount(TextureSetAnimation anim) {
        return (anim.getEnd() - anim.getStart() + 1) * 4;
    }

    /**
     * Get atlas-image center x in pixels
     * @param anim animation to get center x for
     * @return center x
     */
    public float getCenterX(TextureSetAnimation anim) {
        int width = getImage().getWidth();
        FloatBuffer tc = getTexCoordsBuffer();
        int start = anim.getStart() * 4;
        float c = width * (tc.get(start) + tc.get(start + 2)) * 0.5f;
        return c;
    }

    /**
     * Get atlas-image center y in pixels
     * @param anim animation to get center y for
     * @return center y
     */
    public float getCenterY(TextureSetAnimation anim) {
        int height = getImage().getHeight();
        FloatBuffer tc = getTexCoordsBuffer();
        int start = anim.getStart() * 4;
        float c = height * (tc.get(start + 1) + tc.get(start + 3)) * 0.5f;
        return c;
    }

}
