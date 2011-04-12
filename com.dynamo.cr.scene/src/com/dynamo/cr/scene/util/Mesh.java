package com.dynamo.cr.scene.util;

import java.util.List;

public class Mesh {

    public final float[] positions;
    public float[] normals;
    public float[] texCoords;

    public Mesh(List<Float> positions, List<Float> normals, List<Float> texCoords)
    {
        assert positions.size() == normals.size();
        this.positions = new float[positions.size()];
        this.normals = new float[positions.size()];
        this.texCoords = null;

        for (int i = 0; i < this.positions.length; i++)
        {
            this.positions[i] = positions.get(i);
            this.normals[i] = normals.get(i);
        }

        if (texCoords != null) {
            this.texCoords = new float[(this.positions.length / 3) * 2];
            for (int i = 0; i < this.texCoords.length; i++)
            {
                this.texCoords[i] = texCoords.get(i);
            }
        }
    }
}
