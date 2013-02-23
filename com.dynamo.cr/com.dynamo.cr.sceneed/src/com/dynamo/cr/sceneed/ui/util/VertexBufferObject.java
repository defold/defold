package com.dynamo.cr.sceneed.ui.util;

import java.nio.Buffer;
import java.nio.IntBuffer;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;

public class VertexBufferObject {
    IntBuffer vbo = null;
    private Buffer buffer;
    private int byteSize;

    public VertexBufferObject() {
    }

    public VertexBufferObject(Buffer buffer, int byteSize) {
        this.buffer = buffer;
        this.byteSize = byteSize;

    }

    private void init(GL gl) {
        if (vbo == null) {
            vbo = IntBuffer.allocate(1);
            gl.glGenBuffers(1, vbo);
        }
    }

    public void dispose(GL gl) {
        if (this.vbo != null) {
            gl.glDeleteBuffers(1, this.vbo);
        }
    }

    public void bufferData(Buffer buffer, int byteSize) {
        this.buffer = buffer;
        this.byteSize = byteSize;
    }

    public void enable(GL gl) {
        init(gl);
        int name = vbo.get(0);
        gl.glBindBuffer(GL.GL_ARRAY_BUFFER, name);
        if (buffer != null) {
            gl.glBufferData(GL.GL_ARRAY_BUFFER, byteSize, buffer, GL2.GL_STREAM_DRAW);
            this.buffer = null;
        }
    }

    public void disable(GL gl) {
        gl.glBindBuffer(GL.GL_ARRAY_BUFFER, 0);
    }
}
