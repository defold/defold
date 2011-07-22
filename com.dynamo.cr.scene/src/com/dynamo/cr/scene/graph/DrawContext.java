package com.dynamo.cr.scene.graph;

import javax.media.opengl.GL;
import javax.media.opengl.glu.GLU;


public class DrawContext
{
    public GL m_GL;
    public GLU m_GLU;
    public Node[] m_SelectedNodes;

    public DrawContext(GL gl, GLU glu, Node[] selected_nodes)
    {
        m_GL = gl;
        m_GLU = glu;
        m_SelectedNodes = selected_nodes;
    }

    public boolean isSelected(Node node) {
        for (Node n : m_SelectedNodes) {

            Node tmp = node;
            while (tmp != null) {
                if (tmp == n)
                    return true;
                else
                    tmp = tmp.getParent();
            }
        }
        return false;
    }
}
