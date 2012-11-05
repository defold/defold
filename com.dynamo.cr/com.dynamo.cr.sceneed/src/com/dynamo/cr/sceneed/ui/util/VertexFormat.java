package com.dynamo.cr.sceneed.ui.util;

import java.util.EnumMap;

import javax.media.opengl.GL;

import com.sun.opengl.util.BufferUtil;

public class VertexFormat {
    public enum Attribute {
        POSITION,
        COLOR,
        TEX_COORD;
    }

    public static class AttributeFormat {
        public AttributeFormat(Attribute attribute, int size, int type, boolean normalize) {
            this.attribute = attribute;
            this.size = size;
            this.type = type;
            this.normalize = normalize;
        }

        private Attribute attribute;
        private int size;
        private int type;
        private boolean normalize;
    }

    private EnumMap<Attribute, Integer> attributeOffsets = new EnumMap<Attribute, Integer>(Attribute.class);
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

    public void enable(GL gl, Shader shader) {
        for (AttributeFormat attributeFormat : this.attributeFormats) {
            int index = shader.getAttributeLocation(attributeFormat.attribute);
            gl.glEnableVertexAttribArray(index);
            gl.glVertexAttribPointer(index, attributeFormat.size, attributeFormat.type, attributeFormat.normalize, this.stride, attributeOffsets.get(attributeFormat.attribute));
        }
    }

    public void disable(GL gl, Shader shader) {
        for (AttributeFormat attributeFormat : this.attributeFormats) {
            int index = shader.getAttributeLocation(attributeFormat.attribute);
            gl.glDisableVertexAttribArray(index);
        }
    }

    public static int getTypeSize(int type) {
        switch (type) {
        case GL.GL_BYTE:
        case GL.GL_UNSIGNED_BYTE:
            return BufferUtil.SIZEOF_BYTE;
        case GL.GL_SHORT:
        case GL.GL_UNSIGNED_SHORT:
            return BufferUtil.SIZEOF_SHORT;
        case GL.GL_INT:
        case GL.GL_UNSIGNED_INT:
            return BufferUtil.SIZEOF_INT;
        case GL.GL_FLOAT:
            return BufferUtil.SIZEOF_FLOAT;
        case GL.GL_DOUBLE:
            return BufferUtil.SIZEOF_DOUBLE;
        }
        throw new IllegalArgumentException(String.format("Size of type %d is unknown", type));
    }
}
