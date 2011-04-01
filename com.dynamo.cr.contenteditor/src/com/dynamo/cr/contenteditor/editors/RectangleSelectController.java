package com.dynamo.cr.contenteditor.editors;

import javax.media.opengl.GL;
import javax.media.opengl.glu.GLU;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.scene.graph.Node;

public class RectangleSelectController {

    int startX, startY;
    int endX, endY;
    boolean selecting;
    Node[] nodes = new Node[0];

    public boolean isSelecting() {
        return selecting;
    }

    public void mouseDown(MouseEvent e, IEditor editor) {
        nodes = new Node[0];
        startX = endX = e.x;
        startY = endY = e.y;
        selecting = true;
    }

    public void mouseUp(MouseEvent e, IEditor editor) {
        if (selecting) {
            // NOTE: We must reset selecting *before* calling setSelectedNodes
            // Property view etc might call this from the callback
            selecting = false;
            editor.setSelectedNodes(nodes);
        }
    }

    public void mouseMove(MouseEvent e, IEditor editor) {
        if (selecting) {
            endX = e.x;
            endY = e.y;

            int x = Math.min(startX, endX);
            int y = Math.min(startY, endY);
            int w = Math.abs(endX - startX);
            int h = Math.abs(endY - startY);
            nodes = editor.selectNode(x+w/2, y+h/2, w, h, true, false, false);
            editor.setSelectedNodes(nodes);
        }
    }

    public void draw(GL gl, IEditor editor) {
        int[] view_port = editor.getViewPort();
        if (selecting) {
            GLU glu = new GLU();

            gl.glMatrixMode(GL.GL_PROJECTION);
            gl.glPushMatrix();
            gl.glLoadIdentity();
            glu.gluOrtho2D(view_port[0], view_port[2], view_port[3], view_port[1]);

            gl.glMatrixMode(GL.GL_MODELVIEW);
            gl.glPushMatrix();
            gl.glLoadIdentity();

            gl.glPushAttrib(GL.GL_DEPTH_BUFFER_BIT | GL.GL_COLOR_BUFFER_BIT);
            gl.glDisable(GL.GL_DEPTH_TEST);
            gl.glEnable(GL.GL_BLEND);
            gl.glBlendFunc (GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);

            gl.glColor4d(0.6, 0.3, 0.3, 0.5);
            gl.glBegin(GL.GL_QUADS);
            gl.glVertex2d(startX, startY);
            gl.glVertex2d(endX, startY);
            gl.glVertex2d(endX, endY);
            gl.glVertex2d(startX, endY);
            gl.glEnd();

            gl.glPopAttrib();

            gl.glMatrixMode(GL.GL_PROJECTION);
            gl.glPopMatrix();

            gl.glMatrixMode(GL.GL_MODELVIEW);
            gl.glPopMatrix();
        }
    }
}
