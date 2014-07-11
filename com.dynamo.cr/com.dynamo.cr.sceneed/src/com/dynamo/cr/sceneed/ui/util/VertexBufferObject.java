package com.dynamo.cr.sceneed.ui.util;

import java.nio.Buffer;

import javax.media.opengl.GL;

public class VertexBufferObject extends BufferObject {

    public VertexBufferObject() {
        super(GL.GL_ARRAY_BUFFER);
    }

    public VertexBufferObject(Buffer buffer, int byteSize) {
        super(buffer, byteSize, GL.GL_ARRAY_BUFFER);
    }
}
