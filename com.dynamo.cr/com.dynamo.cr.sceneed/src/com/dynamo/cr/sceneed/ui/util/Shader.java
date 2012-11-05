package com.dynamo.cr.sceneed.ui.util;

import java.io.IOException;
import java.io.InputStream;
import java.io.StringWriter;
import java.nio.IntBuffer;
import java.util.EnumMap;
import java.util.Map;

import javax.media.opengl.GL;
import javax.vecmath.Matrix4d;
import javax.vecmath.Vector4f;

import org.apache.commons.io.IOUtils;
import org.osgi.framework.Bundle;

import com.dynamo.cr.sceneed.ui.RenderUtil;
import com.dynamo.cr.sceneed.ui.util.VertexFormat.Attribute;

public class Shader {
    public enum Uniform {
        VIEW_PROJ,
        DIFFUSE_TEXTURE,
        TINT;

        @Override
        public String toString() {
            switch (this) {
            case VIEW_PROJ:
                return "view_proj";
            case DIFFUSE_TEXTURE:
                return "DIFFUSE_TEXTURE";
            case TINT:
                return "tint";
            }
            return super.toString();
        }
    }

    private GL gl;
    private EnumMap<Attribute, Integer> attribLocations;
    private EnumMap<Uniform, Integer> uniformLocations;

    public Shader(GL gl) {
        this.gl = gl;
        this.vertexShader = gl.glCreateShader(GL.GL_VERTEX_SHADER);
        this.fragmentShader = gl.glCreateShader(GL.GL_FRAGMENT_SHADER);
        this.program = gl.glCreateProgram();
        this.attribLocations = new EnumMap<Attribute, Integer>(Attribute.class);
        this.uniformLocations = new EnumMap<Uniform, Integer>(Uniform.class);
    }

    public void dispose() {
        if (this.vertexShader != 0) {
            this.gl.glDeleteShader(this.vertexShader);
        }
        if (this.fragmentShader != 0) {
            this.gl.glDeleteShader(this.fragmentShader);
        }
        if (this.program != 0) {
            this.gl.glDeleteProgram(this.program);
        }
    }

    private void loadShader(int shader, InputStream input) throws IOException {
        StringWriter writer = new StringWriter();
        IOUtils.copy(input, writer);
        this.gl.glShaderSource(shader, 1, new String[] {writer.toString()}, (IntBuffer)null);
    }

    public void load(Bundle bundle, String path) throws IOException {
        loadShader(this.vertexShader, bundle.getEntry(path + ".vp").openStream());
        loadShader(this.fragmentShader, bundle.getEntry(path + ".fp").openStream());
        this.gl.glCompileShader(this.vertexShader);
        this.gl.glCompileShader(this.fragmentShader);
        this.gl.glAttachShader(this.program, this.vertexShader);
        this.gl.glAttachShader(this.program, this.fragmentShader);
        this.gl.glLinkProgram(this.program);
    }

    public void enable() {
        this.gl.glUseProgram(this.program);
    }

    public void disable() {
        this.gl.glUseProgram(0);
    }

    public void setUniforms(EnumMap<Shader.Uniform, Object> uniforms) {
        for (Map.Entry<Shader.Uniform, Object> entry : uniforms.entrySet()) {
            int index = getUniformLocation(entry.getKey());
            Object value = entry.getValue();
            if (value instanceof Matrix4d) {
                gl.glUniformMatrix4fv(index, 1, false, RenderUtil.matrixToArray((Matrix4d)value), 0);
            } else if (value instanceof Vector4f) {
                Vector4f v = (Vector4f)value;
                gl.glUniform4f(index, v.getX(), v.getY(), v.getZ(), v.getW());
            } else if (value instanceof Integer) {
                gl.glUniform1i(index, (Integer)value);
            } else {
                throw new IllegalArgumentException(String.format("No support for uniforms of type %s.", value.getClass().getName()));
            }
        }
    }

    int vertexShader;
    int fragmentShader;
    int program;

    public int getAttributeLocation(Attribute attribute) {
        Integer location = attribLocations.get(attribute);
        if (location == null) {
            String name = getAttributeName(attribute);
            location = this.gl.glGetAttribLocation(this.program, name);
            this.attribLocations.put(attribute, location);
        }
        return location;
    }

    public int getUniformLocation(Uniform uniform) {
        Integer location = uniformLocations.get(uniform);
        if (location == null) {
            String name = uniform.toString();
            location = this.gl.glGetUniformLocation(this.program, name);
            this.uniformLocations.put(uniform, location);
        }
        return location;
    }

    private static String getAttributeName(Attribute attribute) {
        switch (attribute) {
        case POSITION:
            return "position";
        case COLOR:
            return "color";
        case TEX_COORD:
            return "texcoord0";
        }
        return attribute.toString();
    }
}
