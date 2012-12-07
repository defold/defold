package com.dynamo.cr.tileeditor.atlas;

import java.awt.image.BufferedImage;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import com.dynamo.cr.tileeditor.scene.TextureSetAnimation;

/**
 * Atlas-map representation. The fundamental entity is "animation". Separate
 * images are represented by single-frame animation.
 * A single vertex-buffer is created for all animations. Linear access to animation frames can be assumed.
 * @author chmu
 *
 */
public class AtlasMap {

    private final BufferedImage image;
    private final List<TextureSetAnimation> tiles;
    private FloatBuffer vertexBuffer;
    private FloatBuffer outlineVertexBuffer;
    private final FloatBuffer texCoordsBuffer;

    public AtlasMap(BufferedImage image, List<TextureSetAnimation> tiles, FloatBuffer vertexBuffer, FloatBuffer outlineVertexBuffer, FloatBuffer texCoordsBuffer) {
        this.image = image;
        this.tiles = Collections.unmodifiableList(new ArrayList<TextureSetAnimation>(tiles));
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
        return tiles;
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

}
