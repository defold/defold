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
import com.jogamp.opengl.util.awt.TextRenderer;

public class GuiSceneRenderer implements INodeRenderer<GuiSceneNode> {

    private static final float COLOR[] = new float[] { 1.0f, 1.0f, 1.0f, 1.0f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.OVERLAY);
    public static boolean showLayoutInfoEnabled = false;

    public GuiSceneRenderer() {
    }

    @Override
    public void dispose(GL2 gl) { }

    public static void showLayoutInfo(boolean show) {
        showLayoutInfoEnabled = show;
    }

    public static boolean getShowLayoutInfo() {
        return showLayoutInfoEnabled;
    }

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

        if (renderData.getPass() == Pass.OUTLINE) {
            float x0 = 0;
            float x1 = node.getWidth();
            float y0 = 0;
            float y1 = node.getHeight();

            gl.glColor4fv(renderContext.selectColor(node, COLOR), 0);
            gl.glBegin(GL2.GL_QUADS);
            gl.glVertex2f(x0, y1);
            gl.glVertex2f(x1, y1);
            gl.glVertex2f(x1, y0);
            gl.glVertex2f(x0, y0);
            gl.glEnd();
        } else if (renderData.getPass() == Pass.OVERLAY) {
            if(getShowLayoutInfo()) {
                TextRenderer textRenderer = renderContext.getSmallTextRenderer();
                gl.glPushMatrix();
                gl.glScaled(1, -1, 1);
                textRenderer.setColor(1, 1, 1, 1);
                textRenderer.begin3DRendering();
                String text = String.format("Layout: %s", node.getCurrentLayout().getId());
                float x0 = 12;
                float y0 = -22;
                textRenderer.draw3D(text, x0, y0, 1, 1);
                textRenderer.end3DRendering();
                gl.glPopMatrix();
            }
        }
    }

}
