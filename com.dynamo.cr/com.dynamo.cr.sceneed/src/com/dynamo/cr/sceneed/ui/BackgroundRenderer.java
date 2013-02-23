package com.dynamo.cr.sceneed.ui;

import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.preferences.PreferenceConstants;

public class BackgroundRenderer implements INodeRenderer<BackgroundNode> {

    public BackgroundRenderer() {
    }

    @Override
    public void dispose(GL2 gl) { }

    @Override
    public void setup(RenderContext renderContext, BackgroundNode node) {
        if (renderContext.getPass() == Pass.BACKGROUND) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext, BackgroundNode node,
            RenderData<BackgroundNode> renderData) {
        GL2 gl = renderContext.getGL();
        float[] topColor = RenderUtil.parseColor(PreferenceConstants.P_TOP_BKGD_COLOR);
        float[] bottomColor = RenderUtil.parseColor(PreferenceConstants.P_BOTTOM_BKGD_COLOR);

        float x0 = -1.0f;
        float x1 = 1.0f;
        float y0 = -1.0f;
        float y1 = 1.0f;
        gl.glBegin(GL2.GL_QUADS);
        gl.glColor3fv(topColor, 0);
        gl.glVertex2f(x0, y1);
        gl.glVertex2f(x1, y1);
        gl.glColor3fv(bottomColor, 0);
        gl.glVertex2f(x1, y0);
        gl.glVertex2f(x0, y0);
        gl.glEnd();
    }

}
