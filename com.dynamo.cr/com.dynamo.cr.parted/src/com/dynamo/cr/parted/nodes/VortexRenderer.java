package com.dynamo.cr.parted.nodes;

import java.nio.FloatBuffer;
import java.util.EnumSet;

import javax.media.opengl.GL;
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
    private FloatBuffer spiral;
    private FloatBuffer circle;

    public VortexRenderer() {
        spiral = RenderUtil.createSpiral();
        circle = RenderUtil.createDashedCircle(64);
    }

    @Override
    public void dispose() {
    }

    @Override
    public void setup(RenderContext renderContext, VortexNode node) {
        if (passes.contains(renderContext.getPass())) {
            renderContext.add(this, node, new Point3d(), spiral);
            if (renderContext.isSelected(node)) {
                renderContext.add(this, node, new Point3d(), circle);
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, VortexNode node, RenderData<VortexNode> renderData) {

        GL gl = renderContext.getGL();
        double factor = ManipulatorRendererUtil.getScaleFactor(node, renderContext.getRenderView());
        double factorRecip = 50.0 / factor;

        float[] color = renderContext.selectColor(node, VortexRenderer.color);
        gl.glColor4fv(color, 0);
        boolean positive = node.getMagnitude() > 0.0;

        gl.glPushMatrix();
        if (renderData.getUserData() == spiral) {
            if (positive) {
                gl.glScaled(-1.0, 1.0, 1.0);
            }
            gl.glScaled(factorRecip, factorRecip, 1.0);
            gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
            gl.glVertexPointer(3, GL.GL_FLOAT, 0, spiral);
            gl.glDrawArrays(GL.GL_LINE_STRIP, 0, spiral.limit() / 3);
            gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
        } else {
            double r = node.getMaxDistance();
            gl.glScaled(r, r, 1.0);
            gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
            gl.glVertexPointer(3, GL.GL_FLOAT, 0, circle);
            gl.glDrawArrays(GL.GL_LINES, 0, circle.limit() / 3);
            gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
        }
        gl.glPopMatrix();

    }

}
