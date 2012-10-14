package com.dynamo.cr.parted.nodes;

import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.ManipulatorRendererUtil;
import com.dynamo.cr.sceneed.ui.RenderUtil;

public class VortexRenderer implements INodeRenderer<VortexNode> {

    private static final float[] color = new float[] { 1, 1, 1, 1 };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.SELECTION);

    @Override
    public void dispose() {
    }

    @Override
    public void setup(RenderContext renderContext, VortexNode node) {
        if (passes.contains(renderContext.getPass())) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext, VortexNode node,
            RenderData<VortexNode> renderData) {

        GL gl = renderContext.getGL();
        double factor = ManipulatorRendererUtil.getScaleFactor(node, renderContext.getRenderView());
        float[] color = renderContext.selectColor(node, VortexRenderer.color);
        gl.glColor4fv(color, 0);
        boolean positive = node.getMagnitude().getValue() > 0.0;

        double length = ManipulatorRendererUtil.BASE_LENGTH / factor;
        Matrix4d scale = new Matrix4d();

        for (int i = 0; i < 4; ++i) {
            gl.glPushMatrix();
            gl.glRotated(90.0 * i + 45.0, 0.0, 1.0, 0.0);
            gl.glTranslated(0.0, 0.0, length);
            if (!positive) {
                gl.glScaled(-1.0, 1.0, 1.0);
            }
            gl.glTranslated(-0.5 * length, 0.0, 0.0);
            drawArrow(gl, factor);
            gl.glPopMatrix();
        }

/*        int n = 4;
        for (int i = 0; i < n; ++i) {
            dim.setIdentity();
            dim.setElement(0, 0, 0.0);
            dim.setElement(i, i, 0.0);
            dim.setElement(i, 0, 1.0);
            dim.setElement(0, i, 1.0);
            gl.glPushMatrix();
            RenderUtil.multMatrix(gl, dim);
            scale.setIdentity();
            scale.setElement(0, 0, -1.0);
            gl.glPushMatrix();
            double gap = 0.2;
            if (positive) {
                gl.glTranslated(length * (1.0 + gap), 0, 0);
                RenderUtil.multMatrix(gl, scale);
            } else {
                gl.glTranslated(length * gap, 0, 0);
            }
            drawArrow(gl, factor);
            gl.glPopMatrix();
            RenderUtil.multMatrix(gl, scale);
            gl.glPushMatrix();
            if (positive) {
                gl.glTranslated(length * (1.0 + gap), 0, 0);
                RenderUtil.multMatrix(gl, scale);
            } else {
                gl.glTranslated(length * gap, 0, 0);
            }
            drawArrow(gl, factor);
            gl.glPopMatrix();
            gl.glPopMatrix();
        }*/

    }

    private void drawArrow(GL gl, double factor) {
        RenderUtil.drawArrow(gl, ManipulatorRendererUtil.BASE_LENGTH / factor,
                                 2.3 * ManipulatorRendererUtil.BASE_HEAD_RADIUS / factor,
                                 0.2 * ManipulatorRendererUtil.BASE_THICKNESS / factor,
                                 ManipulatorRendererUtil.BASE_HEAD_RADIUS / factor);
    }


}
