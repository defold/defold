package com.dynamo.bob.pipeline;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.List;

public class Mesh {
    public final FloatBuffer positions;
    public FloatBuffer normals;
    public FloatBuffer uvs;

    private static FloatBuffer newFloatBuffer(int n) {
        ByteBuffer bb = ByteBuffer.allocateDirect(n * 4);
        return bb.order(ByteOrder.nativeOrder()).asFloatBuffer();
    }

    public Mesh(List<Float> positions, List<Float> normals, List<Float> uvs) {
        int n = positions.size() / 3;
        assert positions.size() == normals.size();
        assert uvs.size() * 2 == n;

        this.positions = newFloatBuffer(n * 3);
        this.normals = newFloatBuffer(n * 3);
        this.uvs = newFloatBuffer(n * 2);

        for (int i = 0; i < n * 3; i++) {
            this.positions.put(positions.get(i));
            this.normals.put(normals.get(i));
        }

        for (int i = 0; i < n * 2; i++) {
            this.uvs.put(uvs.get(i));
        }

        this.positions.rewind();
        this.normals.rewind();
        this.uvs.rewind();
    }

    public FloatBuffer getPositions() {
        return positions;
    }

    public FloatBuffer getNormals() {
        return normals;
    }

    public FloatBuffer getTexcoord0() {
        return uvs;
    }

}
