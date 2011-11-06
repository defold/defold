package com.dynamo.cr.goprot.sprite;

import javax.media.opengl.GL;
import javax.media.opengl.glu.GLU;

import com.dynamo.cr.goprot.INodeRenderer;
import com.dynamo.cr.goprot.core.Node;

public class SpriteRenderer implements INodeRenderer {

    @Override
    public void render(Node node, GL gl, GLU glu) {
        float x0 = -0.5f;
        float x1 = 0.5f;
        float y0 = -0.5f;
        float y1 = 0.5f;
        float l = 0.4f;
        gl.glColor3f(l, 0, 0);
        gl.glBegin(GL.GL_QUADS);
        gl.glVertex2f(x0, y0);
        gl.glVertex2f(x0, y1);
        gl.glVertex2f(x1, y1);
        gl.glVertex2f(x1, y0);
        gl.glEnd();
    }

}
