package com.dynamo.cr.tileeditor.scene;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import javax.media.opengl.GL;
import javax.vecmath.Vector2f;

import com.dynamo.bob.textureset.TextureSetGenerator.UVTransform;
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
    private VertexBufferObject atlasVertexBuffer = new VertexBufferObject();
    private VertexBufferObject outlineVertexBuffer = new VertexBufferObject();
    private ByteBuffer texCoordsBuffer = ByteBuffer.allocateDirect(0);
    private List<UVTransform> uvTransforms;

    public RuntimeTextureSet() {
    }

    public RuntimeTextureSet(TextureSet textureSet) {
        update(textureSet, Collections.<UVTransform>emptyList());
    }

    public void dispose(GL gl) {
        vertexBuffer.dispose(gl);
        atlasVertexBuffer.dispose(gl);
        outlineVertexBuffer.dispose(gl);
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
    public void update(TextureSet textureSet, List<UVTransform> uvTransforms) {
        this.textureSet = textureSet;
        updateBuffer(vertexBuffer, textureSet.getVertices());
        updateBuffer(atlasVertexBuffer, textureSet.getAtlasVertices());
        updateBuffer(outlineVertexBuffer, textureSet.getOutlineVertices());

        texCoordsBuffer = ByteBuffer.allocateDirect(textureSet.getTexCoords().size());
        texCoordsBuffer.put(textureSet.getTexCoords().asReadOnlyByteBuffer());
        texCoordsBuffer.flip();

        this.uvTransforms = new ArrayList<UVTransform>(uvTransforms);
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
     *  Gets the vertex buffer object that represents images in the space of the atlas
     *  i.e. if an object has been packed with a rotation in the atlas, then we will see
     *  that rotation when rendering from this buffer.
     *  @return {@link VertexBufferObject} with vertex-data
     */
    public VertexBufferObject getAtlasVertexBuffer() {
        return atlasVertexBuffer;
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
     * @note The texture form describes quad UVs, allowing for rotations on the atlas.
     * i.e. four pairs of floats.
     * @return {@link ByteBuffer} with texture coordinates
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
        if (getTextureSet() != null) {
            List<TextureSetAnimation> animations = getTextureSet().getAnimationsList();
            for (TextureSetAnimation a : animations) {
                if (comparable.compareTo(a.getId()) == 0) {
                    return a;
                }
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
     * Get tile-image center x,y in uv-space
     * @param tile
     *             tile to get center x,y for
     * @return center x,y
     */
    public Vector2f getCenter(int tile) {
        Vector2f center = new Vector2f();
        center.add(getMin(tile), getMax(tile));
        center.scale(0.5f);
        return center;
    }

    /**
     * Get tile-image minimum x,y in uv-space
     * @param tile
     *             tile to get minimum x,y for
     * @return min x,y
     */
    public Vector2f getMin(int tile) {
        final int floatsPerFrame = 8;
        FloatBuffer src = this.texCoordsBuffer.order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer();
        float minU = Float.MAX_VALUE;
        float minV = Float.MAX_VALUE;
        int offset = floatsPerFrame * tile;
        for (int j=0;j<floatsPerFrame;j+=2) {
            minU = Math.min(minU, src.get(offset + j));
            minV = Math.min(minV, src.get(offset + j + 1));
        }
        return new Vector2f(minU, minV);
    }

    /**
     * Get tile-image maximum x,y in uv-space
     * @param tile
     *             tile to get maximum x,y for
     * @return max x,y
     */
    public Vector2f getMax(int tile) {
        final int floatsPerFrame = 8;
        FloatBuffer src = this.texCoordsBuffer.order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer();
        float maxU = -Float.MAX_VALUE;
        float maxV = -Float.MAX_VALUE;
        int offset = floatsPerFrame * tile;
        for (int j=0;j<floatsPerFrame;j+=2) {
            maxU = Math.max(maxU,  src.get(offset + j));
            maxV = Math.max(maxV, src.get(offset + j + 1));
        }
        return new Vector2f(maxU, maxV);
    }

    /**
     * Get bounds for an image/animation
     *
     * @param id
     *            identifier
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

    /**
     * Get the transform from the UV space of the original tile to the atlas UV space.
     *
     * @param anim Animation from which to fetch frame UV transform
     * @param frame which frame of the animation to use
     * @return transform from the original UV space to the modified one
     */
    public UVTransform getUvTransform(TextureSetAnimation anim, int frame) {
        return this.uvTransforms.get(anim.getStart() + frame);
    }
}
