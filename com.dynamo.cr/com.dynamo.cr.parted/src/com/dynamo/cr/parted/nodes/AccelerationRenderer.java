package com.dynamo.cr.parted.nodes;

import java.util.EnumSet;

import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.cr.parted.manipulators.ParticleManipulatorUtil;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.ManipulatorRendererUtil;
import com.dynamo.cr.sceneed.ui.RenderUtil;

public class AccelerationRenderer implements INodeRenderer<AccelerationNode> {

    private static final float[] color = new float[] { 1, 1, 1, 1 };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.SELECTION, Pass.OVERLAY);

    @Override
    public void dispose(GL2 gl) {
    }

    @Override
    public void setup(RenderContext renderContext, AccelerationNode node) {
        if (passes.contains(renderContext.getPass())) {
            if (renderContext.getPass() != Pass.OVERLAY) {
                renderContext.add(this, node, new Point3d(), null);
            }

            if (renderContext.isSelected(node)) {
                if (renderContext.getPass() == Pass.OVERLAY && !node.getMagnitude().isAnimated()) {
                    renderContext.add(this, node, new Point3d(), node.getMagnitude().getValue());
                }
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, AccelerationNode node,
            RenderData<AccelerationNode> renderData) {

        GL2 gl = renderContext.getGL();
        double factor = ManipulatorRendererUtil.getScaleFactor(node, renderContext.getRenderView());
        float[] color = renderContext.selectColor(node, AccelerationRenderer.color);
        gl.glColor4fv(color, 0);

        if (renderData.getUserData() == null) {
            double sign = Math.signum(node.getMagnitude().getValue());
            if (sign == 0) {
                sign = 1.0;
            }

            int n = 5;
            double dy = 1.5 * (ManipulatorRendererUtil.BASE_LENGTH / factor) / n;
            double y = -dy * (n-1) / 2.0;
            for (int i = 0; i < n; ++i) {
                gl.glPushMatrix();
                gl.glRotated(sign * 90.0, 0, 0, 1);
                gl.glTranslated(0, y + dy * i, 0);
                gl.glTranslated( -Math.abs((n / 2 - i)) *  dy * 0.3, 0, 0);
                drawArrow(gl, factor);
                gl.glPopMatrix();
            }
        } else if (renderData.getUserData() instanceof Double) {
            ParticleManipulatorUtil.drawNumber(renderContext, node, (Double)renderData.getUserData());
        }

    }

    private void drawArrow(GL2 gl, double factor) {
        RenderUtil.drawArrow(gl, 0.6 * ManipulatorRendererUtil.BASE_LENGTH / factor,
                                 1.3 * ManipulatorRendererUtil.BASE_HEAD_RADIUS / factor,
                                 0.2 * ManipulatorRendererUtil.BASE_THICKNESS / factor,
                                 0.5 * ManipulatorRendererUtil.BASE_HEAD_RADIUS / factor);
    }

}
