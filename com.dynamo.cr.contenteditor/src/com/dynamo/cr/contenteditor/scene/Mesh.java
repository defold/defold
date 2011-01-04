package com.dynamo.cr.contenteditor.scene;

import java.util.List;

public class Mesh
{
    public final float[] m_Positions;
    public float[] m_Normals;

    public Mesh(List<Float> positions, List<Float> normals)
    {
        assert positions.size() == normals.size();
        m_Positions = new float[positions.size()];
        m_Normals = new float[positions.size()];

        for (int i = 0; i < m_Positions.length; i++)
        {
            m_Positions[i] = positions.get(i);
            m_Normals[i] = normals.get(i);
        }
    }
}
