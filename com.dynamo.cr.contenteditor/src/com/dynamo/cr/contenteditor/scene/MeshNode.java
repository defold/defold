package com.dynamo.cr.contenteditor.scene;

import javax.media.opengl.GL;

import com.dynamo.cr.contenteditor.editors.Constants;
import com.dynamo.cr.contenteditor.editors.DrawContext;

public class MeshNode extends LeafNode {

    private String name;
    private Mesh mesh;

    public MeshNode(Scene scene, String name, Mesh mesh) {
        super(scene);
        this.name = name;
        this.mesh = mesh;
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public void draw(DrawContext context) {
        GL gl = context.m_GL;
        if (context.isSelected(this))
            gl.glColor3fv(Constants.SELECTED_COLOR, 0);
        else
            gl.glColor3fv(Constants.OBJECT_COLOR, 0);

        gl.glBegin(GL.GL_TRIANGLES);
        float[] p = mesh.m_Positions;
        float[] n = mesh.m_Normals;
        for (int i = 0; i < p.length; i+=3)
        {
            gl.glNormal3f(n[i], n[i+1], n[i+2]);
            gl.glVertex3f(p[i], p[i+1], p[i+2]);
        }
        gl.glEnd();
    }
}
