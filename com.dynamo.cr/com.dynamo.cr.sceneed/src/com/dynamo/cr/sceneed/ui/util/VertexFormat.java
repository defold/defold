package com.dynamo.cr.sceneed.ui.util;

import java.util.HashMap;
import java.util.Map;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;

import com.jogamp.common.nio.Buffers;

public class VertexFormat {
    public static class AttributeFormat {
        public AttributeFormat(String attribute, int size, int type, boolean normalize) {
            this.attribute = attribute;
            this.size = size;
            this.type = type;
            this.normalize = normalize;
        }

        private String attribute;
        private int size;
        private int type;
        private boolean normalize;
    }

    private Map<String, Integer> attributeOffsets = new HashMap<String, Integer>();
    private AttributeFormat[] attributeFormats;
    private int stride;

    public VertexFormat(AttributeFormat... attributeFormats) {
        this.attributeFormats = attributeFormats;
        int offset = 0;
        for (AttributeFormat attributeFormat : attributeFormats) {
            attributeOffsets.put(attributeFormat.attribute, offset);
            offset += attributeFormat.size * getTypeSize(attributeFormat.type);
        }
        this.stride = offset;
    }

    public void enable(GL2 gl, Shader shader) {
        for (AttributeFormat attributeFormat : this.attributeFormats) {
            int index = shader.getAttributeLocation(gl, attributeFormat.attribute);
            if (index != -1) {
                // We skip attributes not found. Certain vertex attribute
                // might not be used in shader, e.g. texcoords in lines
                gl.glEnableVertexAttribArray(index);
                gl.glVertexAttribPointer(index, attributeFormat.size, attributeFormat.type, attributeFormat.normalize, this.stride, attributeOffsets.get(attributeFormat.attribute));
            }
        }
    }

    public void disable(GL2 gl, Shader shader) {
        for (AttributeFormat attributeFormat : this.attributeFormats) {
            int index = shader.getAttributeLocation(gl, attributeFormat.attribute);
            if (index != -1) {
                gl.glDisableVertexAttribArray(index);
            }
        }
    }

    public static int getTypeSize(int type) {
        switch (type) {
        case GL.GL_BYTE:
        case GL.GL_UNSIGNED_BYTE:
            return Buffers.SIZEOF_BYTE;
        case GL.GL_SHORT:
        case GL.GL_UNSIGNED_SHORT:
            return Buffers.SIZEOF_SHORT;
        case GL2.GL_INT:
        case GL.GL_UNSIGNED_INT:
            return Buffers.SIZEOF_INT;
        case GL.GL_FLOAT:
            return Buffers.SIZEOF_FLOAT;
        case GL2.GL_DOUBLE:
            return Buffers.SIZEOF_DOUBLE;
        }
        throw new IllegalArgumentException(String.format("Size of type %d is unknown", type));
    }
}
