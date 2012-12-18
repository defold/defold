package com.dynamo.cr.sceneed.ui.util;

import java.io.IOException;
import java.io.InputStream;
import java.nio.IntBuffer;
import java.util.HashMap;
import java.util.Map;

import javax.media.opengl.GL;
import javax.vecmath.Matrix4d;
import javax.vecmath.Vector4f;

import org.apache.commons.io.IOUtils;
import org.osgi.framework.Bundle;

import com.dynamo.cr.sceneed.ui.RenderUtil;

public class Shader {
    private Map<String, Integer> attribLocations;
    private Map<String, Integer> uniformLocations;
    private int vertexShader;
    private int fragmentShader;
    private int program;

    public Shader(GL gl) {
        this.vertexShader = gl.glCreateShader(GL.GL_VERTEX_SHADER);
        this.fragmentShader = gl.glCreateShader(GL.GL_FRAGMENT_SHADER);
        this.program = gl.glCreateProgram();
        this.attribLocations = new HashMap<String, Integer>();
        this.uniformLocations = new HashMap<String, Integer>();
    }

    public void dispose(GL gl) {
        if (this.vertexShader != 0) {
            gl.glDeleteShader(this.vertexShader);
        }
        if (this.fragmentShader != 0) {
            gl.glDeleteShader(this.fragmentShader);
        }
        if (this.program != 0) {
            gl.glDeleteProgram(this.program);
        }
    }

    private void loadShader(GL gl, int shader, InputStream input) throws IOException {
        String source = IOUtils.toString(input);
        input.close();
        gl.glShaderSource(shader, 1, new String[] {source}, (IntBuffer)null);
    }

    public void load(GL gl, Bundle bundle, String path) throws IOException {
        loadShader(gl, this.vertexShader, bundle.getEntry(path + ".vp").openStream());
        loadShader(gl, this.fragmentShader, bundle.getEntry(path + ".fp").openStream());
        gl.glCompileShader(this.vertexShader);
        gl.glCompileShader(this.fragmentShader);
        gl.glAttachShader(this.program, this.vertexShader);
        gl.glAttachShader(this.program, this.fragmentShader);
        gl.glLinkProgram(this.program);
    }

    public void enable(GL gl) {
        gl.glUseProgram(this.program);
    }

    public void disable(GL gl ) {
        gl.glUseProgram(0);
    }

    public void setUniforms(GL gl, String uniform, Object value) {
        int index = getUniformLocation(gl, uniform);
        if (value instanceof Matrix4d) {
            gl.glUniformMatrix4fv(index, 1, false, RenderUtil.matrixToArray((Matrix4d)value), 0);
        } else if (value instanceof Vector4f) {
            Vector4f v = (Vector4f)value;
            gl.glUniform4f(index, v.getX(), v.getY(), v.getZ(), v.getW());
        } else if (value instanceof float[]) {
            float[] v = (float[])value;
            if (v.length == 3) {
                gl.glUniform4f(index, v[0], v[1], v[2], 1);
            } else {
                gl.glUniform4f(index, v[0], v[1], v[2], v[3]);
            }
        } else if (value instanceof Integer) {
            gl.glUniform1i(index, (Integer)value);
        } else {
            throw new IllegalArgumentException(String.format("No support for uniforms of type %s.", value.getClass().getName()));
        }
    }

    public int getAttributeLocation(GL gl, String attribute) {
        Integer location = attribLocations.get(attribute);
        if (location == null) {
            location = gl.glGetAttribLocation(this.program, attribute);
            this.attribLocations.put(attribute, location);
        }
        return location;
    }

    public int getUniformLocation(GL gl, String uniform) {
        Integer location = uniformLocations.get(uniform);
        if (location == null) {
            String name = uniform.toString();
            location = gl.glGetUniformLocation(this.program, name);
            this.uniformLocations.put(uniform, location);
        }
        return location;
    }
}
