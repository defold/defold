package com.dynamo.cr.sceneed.ui.util;

import java.nio.Buffer;
import java.nio.IntBuffer;

import javax.media.opengl.GL;

public class VertexBufferObject {
    IntBuffer vbo = IntBuffer.allocate(1);
    GL gl;

    public VertexBufferObject(GL gl) {
        this.gl = gl;
        this.gl.glGenBuffers(1, vbo);
    }

    public void dispose() {
        if (this.vbo.limit() > 0) {
            this.gl.glDeleteBuffers(1, this.vbo);
        }
    }

    public void enable(Buffer buffer, int byteSize) {
        int name = vbo.get(0);
        this.gl.glBindBuffer(GL.GL_ARRAY_BUFFER, name);
        this.gl.glBufferData(GL.GL_ARRAY_BUFFER, byteSize, buffer, GL.GL_STREAM_DRAW);
    }

    public void disable() {
        this.gl.glBindBuffer(GL.GL_ARRAY_BUFFER, 0);
    }
}
