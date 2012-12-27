package com.dynamo.cr.tileeditor.scene;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.List;

import javax.media.opengl.GL;

import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.ui.util.VertexBufferObject;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.google.protobuf.ByteString;

/**
 * Runtime representation of {@link TextureSet} with additional helper methods and vertex-buffers
 * @author chmu
 *
 */
public class RuntimeTextureSet {

    private TextureSet textureSet;
    private VertexBufferObject vertexBuffer = new VertexBufferObject();
    private VertexBufferObject outlineVertexBuffer = new VertexBufferObject();
    private ByteBuffer texCoordsBuffer = ByteBuffer.allocateDirect(0);

    public RuntimeTextureSet() {
    }

    public RuntimeTextureSet(TextureSet textureSet) {
        update(textureSet);
    }

    public void dispose(GL gl) {
        vertexBuffer.dispose(gl);
        vertexBuffer.dispose(gl);
    }

    private static void updateBuffer(VertexBufferObject vb, ByteString bs) {
        ByteBuffer bb = ByteBuffer.allocateDirect(bs.size());
        bb.put(bs.asReadOnlyByteBuffer());
        bb.flip();
        vb.bufferData(bb, bs.size());
    }

    /**
     * Update with new {@link TextureSet}
     * @param textureSet texture set to update with
     */
    public void update(TextureSet textureSet) {
        this.textureSet = textureSet;
        updateBuffer(vertexBuffer, textureSet.getVertices());
        updateBuffer(outlineVertexBuffer, textureSet.getOutlineVertices());

        texCoordsBuffer = ByteBuffer.allocateDirect(textureSet.getTexCoords().size());
        texCoordsBuffer.put(textureSet.getTexCoords().asReadOnlyByteBuffer());
        texCoordsBuffer.flip();
    }

    /**
     * Get vertex buffer object the represents all images and animations.
     * The vertex buffer should be drawn using triangles
     * @return {@link VertexBufferObject} with vertex-data
     */
    public VertexBufferObject getVertexBuffer() {
        return vertexBuffer;
    }

    /**
     * Similar to {@link TextureSetNode#getVertexBuffer()} but for
     * outline drawing (line-loop)
     *
     * @return {@link VertexBufferObject} with vertex-data
     */
    public VertexBufferObject getOutlineVertexBuffer() {
        return outlineVertexBuffer;
    }

    /**
     * Get texture coordinates for all images/animations.
     * @note The texture form is in (min-x, min-y) (max-x, max-y)-format,
     * i.e. four floats.
     * @return {@link FloatBuffer} with texture coordinates
     */
    public ByteBuffer getTexCoords() {
        return texCoordsBuffer;
    }

    /**
     * Get texure-set
     * @return {@link TextureSet}
     */
    public TextureSet getTextureSet() {
        return textureSet;
    }

    /**
     * Get image/animation info
     * @param id identifier
     * @return {@link TextureSetAnimation}
     */
    public TextureSetAnimation getAnimation(String id) {
        TextureSet textureSet = getTextureSet();
        if (textureSet != null) {
            List<TextureSetAnimation> animations = getTextureSet().getAnimationsList();
            for (TextureSetAnimation a : animations) {
                if (a.getId().equals(id)) {
                    return a;
                }
            }
        }
        return null;
    }

    /**
     * Get image/animation info using a custom {@link Comparable}
     * @param id identifier
     * @return {@link TextureSetAnimation}
     */
    public TextureSetAnimation getAnimation(Comparable<String> comparable) {
        List<TextureSetAnimation> animations = getTextureSet().getAnimationsList();
        for (TextureSetAnimation a : animations) {
            if (comparable.compareTo(a.getId()) == 0) {
                return a;
            }
        }
        return null;
    }

    /**
     * Get vertex-buffer start index
     * @param anim animation to get start index for
     * @param frame frame index
     * @return start index
     */
    public int getVertexStart(TextureSetAnimation anim, int frame) {
        return this.textureSet.getVertexStart(anim.getStart() + frame);
    }

    /**
     * Get vertex count for animation
     * @param anim animation to get count for
     * @param frame frame index
     * @return vertex count
     */
    public int getVertexCount(TextureSetAnimation anim, int frame) {
        return this.textureSet.getVertexCount(anim.getStart() + frame);
    }

    /**
     * Get outline vertex-buffer start index
     * @param anim animation to get start index for
     * @param frame frame index
     * @return start index
     */
    public int getOutlineVertexStart(TextureSetAnimation anim, int frame) {
        return this.textureSet.getOutlineVertexStart(anim.getStart() + frame);
    }

    /**
     * Get outline vertex count for animation
     * @param anim animation to get count for
     * @param frame frame index
     * @return vertex count
     */
    public int getOutlineVertexCount(TextureSetAnimation anim, int frame) {
        return this.textureSet.getOutlineVertexCount(anim.getStart() + frame);
    }

    /**
     * Get atlas-image center x in uv-space
     * @param anim animation to get center x for
     * @return center x
     */
    public float getCenterX(TextureSetAnimation anim) {
        // NOTE: The texcoords-buffer is in little endian
        FloatBuffer tc = getTexCoords().asReadOnlyBuffer().order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer();
        int start = anim.getStart() * 4;
        float c = (tc.get(start) + tc.get(start + 2)) * 0.5f;
        return c;
    }

    /**
     * Get atlas-image center y in uv-space
     * @param anim animation to get center y for
     * @return center y
     */
    public float getCenterY(TextureSetAnimation anim) {
        // NOTE: The texcoords-buffer is in little endian
        FloatBuffer tc = getTexCoords().asReadOnlyBuffer().order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer();
        int start = anim.getStart() * 4;
        float c = (tc.get(start + 1) + tc.get(start + 3)) * 0.5f;
        return c;
    }

    /**
     * Get bounds for an image/animation
     * @param id identifier
     * @return {@link AABB}
     */
    public AABB getTextureBounds(String id) {
        AABB aabb = new AABB();
        TextureSetAnimation anim = getAnimation(id);
        if (anim != null) {
            float w2 = anim.getWidth() * 0.5f;
            float h2 = anim.getHeight() * 0.5f;
            aabb.union(-w2, -h2, 0);
            aabb.union(w2, h2, 0);
        }

        return aabb;
    }


}
