package com.dynamo.cr.spine.scene;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;

import com.dynamo.bob.util.RigUtil.MeshAttachment;
import com.dynamo.cr.sceneed.ui.RenderUtil;
import com.dynamo.cr.sceneed.ui.util.IndexBufferObject;
import com.dynamo.cr.sceneed.ui.util.Shader;
import com.dynamo.cr.sceneed.ui.util.VertexBufferObject;
import com.dynamo.cr.sceneed.ui.util.VertexFormat;
import com.dynamo.cr.sceneed.ui.util.VertexFormat.AttributeFormat;

public class CompositeMesh {

    private static final int VERTEX_SIZE = 3 * 4 + 2 * 2;
    private VertexBufferObject vbo = new VertexBufferObject();
    private IndexBufferObject ibo = new IndexBufferObject();
    private VertexFormat vertexFormat = new VertexFormat(
            new AttributeFormat("position", 3, GL.GL_FLOAT, false),
            new AttributeFormat("texcoord0", 2, GL.GL_UNSIGNED_SHORT, true));

    private int indexCount = 0;

    public void dispose(GL2 gl) {
        this.vbo.dispose(gl);
        this.ibo.dispose(gl);
    }

    public void update(List<MeshAttachment> meshes) {
        int vertexCount = 0;
        int indexCount = 0;
        for (MeshAttachment mesh : meshes) {
            vertexCount += mesh.vertices.length / 5;
            indexCount += mesh.triangles.length;
        }

        int byteSize = vertexCount * VERTEX_SIZE;
        ByteBuffer vb = ByteBuffer.allocateDirect(byteSize).order(ByteOrder.nativeOrder());
        ByteBuffer ib = ByteBuffer.allocateDirect(indexCount * 2).order(ByteOrder.nativeOrder());
        int indexOffset = 0;

        for (MeshAttachment mesh : meshes) {
            vertexCount = mesh.vertices.length / 5;
            float[] v = mesh.vertices;
            for (int i = 0; i < vertexCount; ++i) {
                int vi = i * 5;
                vb.putFloat(v[vi+0]).putFloat(v[vi+1]).putFloat(v[vi+2])
                    .putShort(RenderUtil.toShortUV(v[vi+3])).putShort(RenderUtil.toShortUV(v[vi+4]));
            }
            for (int i = 0; i < mesh.triangles.length; ++i) {
                ib.putShort((short)(indexOffset+mesh.triangles[i]));
            }
            indexOffset += vertexCount;
        }

        vb.flip();
        this.vbo.bufferData(vb, byteSize);

        ib.flip();
        this.ibo.bufferData(ib, indexCount * 2);

        this.indexCount = indexCount;
    }

    public void draw(GL2 gl, Shader shader, float[] color) {
        if (this.indexCount > 0) {
            shader.enable(gl);
            this.vbo.enable(gl);
            this.ibo.enable(gl);
            shader.setUniforms(gl, "color", color);
            this.vertexFormat.enable(gl, shader);
            gl.glDrawElements(GL2.GL_TRIANGLES, this.indexCount, GL2.GL_UNSIGNED_SHORT, 0);
            this.vertexFormat.disable(gl, shader);
            this.ibo.disable(gl);
            this.vbo.disable(gl);
            shader.disable(gl);
        }
    }
}
