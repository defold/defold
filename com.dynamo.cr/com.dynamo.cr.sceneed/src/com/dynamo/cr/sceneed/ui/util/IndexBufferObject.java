package com.dynamo.cr.sceneed.ui.util;

import java.nio.Buffer;

import javax.media.opengl.GL;

public class IndexBufferObject extends BufferObject {

    public IndexBufferObject() {
        super(GL.GL_ELEMENT_ARRAY_BUFFER);
    }

    public IndexBufferObject(Buffer buffer, int byteSize) {
        super(buffer, byteSize, GL.GL_ELEMENT_ARRAY_BUFFER);
    }
}
