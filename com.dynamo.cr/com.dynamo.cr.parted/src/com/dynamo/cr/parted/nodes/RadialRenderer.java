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

public class RadialRenderer implements INodeRenderer<RadialNode> {

    private static final float[] color = new float[] { 1, 1, 1, 1 };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.SELECTION);

    @Override
    public void dispose() {
    }

    @Override
    public void setup(RenderContext renderContext, RadialNode node) {
        if (passes.contains(renderContext.getPass())) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext, RadialNode node,
            RenderData<RadialNode> renderData) {

        GL gl = renderContext.getGL();
        double factor = ManipulatorRendererUtil.getScaleFactor(node, renderContext.getRenderView());
        float[] color = renderContext.selectColor(node, RadialRenderer.color);
        gl.glColor4fv(color, 0);
        boolean positive = node.getMagnitude().getValue() > 0.0;

//        gl.glPushMatrix();
        Matrix4d w = new Matrix4d();
        node.getWorldTransform(w);
//        RenderUtil.loadMatrix(gl, w);
        double length = ManipulatorRendererUtil.BASE_LENGTH / factor;
        Matrix4d dim = new Matrix4d();
        Matrix4d scale = new Matrix4d();
        int n = 3;
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
        }
//        gl.glPopMatrix();

    }

    private void drawArrow(GL gl, double factor) {
        RenderUtil.drawArrow(gl, ManipulatorRendererUtil.BASE_LENGTH / factor,
                                 2.3 * ManipulatorRendererUtil.BASE_HEAD_RADIUS / factor,
                                 0.2 * ManipulatorRendererUtil.BASE_THICKNESS / factor,
                                 ManipulatorRendererUtil.BASE_HEAD_RADIUS / factor);
    }


}
