package com.dynamo.cr.tileeditor.scene;

import java.nio.FloatBuffer;

import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.TextureHandle;
import com.dynamo.cr.sceneed.ui.util.VertexBufferObject;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;

/**
 * Abstract base-class for nodes representing a set of textures and animations.
 * The fundamental entity is "animation". Separate images are represented by single-frame animation.
 * A single vertex-buffer is created for all animations. Linear access to animation frames can be assumed.
 *
 * @author chmu
 *
 */
@SuppressWarnings("serial")
public abstract class TextureSetNode extends Node {

    /**
     * Get texture of the laid out images
     * @return {@link TextureHandle}
     */
    public abstract TextureHandle getTextureHandle();

    /**
     * Get vertex buffer object the represents all images and animations.
     * The vertex buffer should be drawn using triangles
     * @return {@link VertexBufferObject} with vertex-data
     */
    public abstract VertexBufferObject getVertexBuffer();

    /**
     * Simular to {@link TextureSetNode#getVertexBuffer()} but for
     * outline drawing (line-loop)
     *
     * @return {@link VertexBufferObject} with vertex-data
     */
    public abstract VertexBufferObject getOutlineVertexBuffer();

    /**
     * Get bounds for an image/animation
     * @param id identiifer
     * @return {@link AABB}
     */
    public abstract AABB getTextureBounds(String id);

    /**
     * Get image/animation info
     * @param id identifier
     * @return {@link TextureSetAnimation}
     */
    public abstract TextureSetAnimation getAnimation(String id);

    /**
     * Get image/animation info using a custom {@link Comparable}
     * @param id identifier
     * @return {@link TextureSetAnimation}
     */
    public abstract TextureSetAnimation getAnimation(Comparable<String> comparable);

    /**
     * Get vertex-buffer start index
     * @param anim animation to get start index for
     * @return start index
     */
    public abstract int getVertexStart(TextureSetAnimation anim);

    /**
     * Get vertex count for animation
     * @param anim animation to get count for
     * @return vertex count
     */
    public abstract int getVertexCount(TextureSetAnimation anim);

    /**
     * Get outline vertex-buffer start index
     * @param anim animation to get start index for
     * @return start index
     */
    public abstract int getOutlineVertexStart(TextureSetAnimation anim);

    /**
     * Get outline vertex count for animation
     * @param anim animation to get count for
     * @return vertex count
     */
    public abstract int getOutlineVertexCount(TextureSetAnimation anim);

    /**
     * Get texture coordinates for all images/animations.
     * @note The texture form is in (min-x, min-y) (max-x, max-y)-format,
     * i.e. four floats.
     * @return {@link FloatBuffer} with texture coordinates
     */
    public abstract FloatBuffer getTexCoords();

}

