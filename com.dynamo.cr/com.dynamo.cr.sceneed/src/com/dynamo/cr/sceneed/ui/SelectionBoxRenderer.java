package com.dynamo.cr.sceneed.ui;

import javax.media.opengl.GL;
import javax.vecmath.Point2i;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.preferences.PreferenceConstants;

public class SelectionBoxRenderer implements INodeRenderer<SelectionBoxNode> {

    public SelectionBoxRenderer() {
    }

    @Override
    public void dispose() { }

    @Override
    public void setup(RenderContext renderContext, SelectionBoxNode node) {
        if (renderContext.getPass() == Pass.OVERLAY) {
            if (node.isVisible()) {
                renderContext.add(this, node, new Point3d(), null);
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, SelectionBoxNode node,
            RenderData<SelectionBoxNode> renderData) {
        GL gl = renderContext.getGL();

        float[] color = RenderUtil.parseColor(PreferenceConstants.P_SELECTION_COLOR);

        gl.glColor3f(color[0], color[1], color[2]);
        gl.glBegin(GL.GL_LINE_LOOP);
        final float z = 0.0f;
        Point2i start = node.getStart();
        Point2i current = node.getCurrent();
        int minX = Math.min(start.x, current.x);
        int minY = Math.min(start.y, current.y);
        int maxX = Math.max(start.x, current.x);
        int maxY = Math.max(start.y, current.y);
        gl.glVertex3f(minX, minY, z);
        gl.glVertex3f(minX, maxY, z);
        gl.glVertex3f(maxX, maxY, z);
        gl.glVertex3f(maxX, minY, z);
        gl.glEnd();

        gl.glBegin(GL.GL_QUADS);
        gl.glColor4f(color[0], color[1], color[2], 0.2f);
        gl.glVertex3f(minX, minY, z);
        gl.glVertex3f(minX, maxY, z);
        gl.glVertex3f(maxX, maxY, z);
        gl.glVertex3f(maxX, minY, z);
        gl.glEnd();
    }

}
