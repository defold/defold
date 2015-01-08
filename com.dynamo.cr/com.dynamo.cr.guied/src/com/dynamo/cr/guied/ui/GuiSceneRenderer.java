package com.dynamo.cr.guied.ui;

import java.util.EnumSet;

import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.guied.util.Clipping;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;

public class GuiSceneRenderer implements INodeRenderer<GuiSceneNode> {

    private static final float COLOR[] = new float[] { 1.0f, 1.0f, 1.0f, 1.0f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE);

    public GuiSceneRenderer() {
    }

    @Override
    public void dispose(GL2 gl) { }

    @Override
    public void setup(RenderContext renderContext, GuiSceneNode node) {
        if (passes.contains(renderContext.getPass())) {
            renderContext.add(this, node, new Point3d(), null);
        }
        if (renderContext.getPass().equals(Pass.BACKGROUND)) {
            Clipping.updateClippingStates(node);
            node.updateRenderOrder();
        }
    }

    @Override
    public void render(RenderContext renderContext, GuiSceneNode node, RenderData<GuiSceneNode> renderData) {

        GL2 gl = renderContext.getGL();

        float x0 = 0.0f;
        float x1 = node.getWidth();
        float y0 = 0.0f;
        float y1 = node.getHeight();
        gl.glColor4fv(renderContext.selectColor(node, COLOR), 0);
        gl.glBegin(GL2.GL_QUADS);
        gl.glVertex2f(x0, y1);
        gl.glVertex2f(x1, y1);
        gl.glVertex2f(x1, y0);
        gl.glVertex2f(x0, y0);
        gl.glEnd();
    }

}
