package com.dynamo.cr.scene.graph;

import java.util.List;

public class Mesh
{
    public final float[] m_Positions;
    public float[] m_Normals;
    public float[] m_TexCoords;

    public Mesh(List<Float> positions, List<Float> normals, List<Float> texcoords)
    {
        assert positions.size() == normals.size();
        m_Positions = new float[positions.size()];
        m_Normals = new float[positions.size()];
        m_TexCoords = null;

        for (int i = 0; i < m_Positions.length; i++)
        {
            m_Positions[i] = positions.get(i);
            m_Normals[i] = normals.get(i);
        }

        if (texcoords != null) {
            m_TexCoords = new float[(m_Positions.length / 3) * 2];
            for (int i = 0; i < m_TexCoords.length; i++)
            {
                m_TexCoords[i] = texcoords.get(i);
            }
        }
    }
}
