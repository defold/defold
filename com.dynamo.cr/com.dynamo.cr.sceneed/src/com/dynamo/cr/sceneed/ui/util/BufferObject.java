package com.dynamo.cr.sceneed.ui.util;

import java.nio.Buffer;
import java.nio.IntBuffer;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;

public class BufferObject {
    IntBuffer bo = null;
    private Buffer buffer;
    private int byteSize;
    private int bufferType;

    public BufferObject(int bufferType) {
        this.bufferType = bufferType;
    }

    public BufferObject(Buffer buffer, int byteSize, int bufferType) {
        this.buffer = buffer;
        this.byteSize = byteSize;
        this.bufferType = bufferType;
    }

    private void init(GL gl) {
        if (bo == null) {
            bo = IntBuffer.allocate(1);
            gl.glGenBuffers(1, bo);
        }
    }

    public void dispose(GL gl) {
        if (this.bo != null) {
            gl.glDeleteBuffers(1, this.bo);
        }
    }

    public void bufferData(Buffer buffer, int byteSize) {
        this.buffer = buffer;
        this.byteSize = byteSize;
    }

    public void enable(GL gl) {
        init(gl);
        int name = bo.get(0);
        gl.glBindBuffer(this.bufferType, name);
        if (buffer != null) {
            gl.glBufferData(this.bufferType, byteSize, buffer, GL2.GL_STREAM_DRAW);
            this.buffer = null;
        }
    }

    public void disable(GL gl) {
        gl.glBindBuffer(this.bufferType, 0);
    }
}
