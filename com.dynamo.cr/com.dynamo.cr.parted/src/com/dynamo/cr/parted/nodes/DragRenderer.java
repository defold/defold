package com.dynamo.cr.parted.nodes;

import java.nio.FloatBuffer;
import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import com.dynamo.cr.parted.manipulators.ParticleManipulatorUtil;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.ManipulatorRendererUtil;
import com.sun.opengl.util.BufferUtil;

public class DragRenderer implements INodeRenderer<DragNode> {

    private static final float[] color = new float[] { 1, 1, 1, 1 };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.SELECTION, Pass.OVERLAY);
    private FloatBuffer vertexBuffer;

    public DragRenderer() {
        createVB();
    }

    private void createVB() {
        int segments = 64;
        int circles = 5;
        vertexBuffer = BufferUtil.newFloatBuffer(segments * 6 * circles);

        float rotation = (float) (Math.PI * 4);
        float length = 60;
        float ra = 8;
        float rb = 50;
        for (int j = 0; j < circles; ++j) {
            float x = length * j / (circles - 1.0f);

            float f = j / (circles - 1.0f);
            float r = (1 - f) * ra + f * rb;

            for (int i = 0; i < segments; ++i) {
                float f0 = ((float) i) / (segments - 1);
                float f1 = ((float) i + 1) / (segments - 1);
                float a0 = rotation * f0;
                float a1 = rotation * f1;

                vertexBuffer.put(x);
                vertexBuffer.put((float) (r * Math.cos(a0)));
                vertexBuffer.put((float) (r * Math.sin(a0)));
                vertexBuffer.put(x);
                vertexBuffer.put((float) (r * Math.cos(a1)));
                vertexBuffer.put((float) (r * Math.sin(a1)));
            }
        }
        vertexBuffer.rewind();
    }

    @Override
    public void dispose() {
    }

    @Override
    public void setup(RenderContext renderContext, DragNode node) {
        if (passes.contains(renderContext.getPass())) {
            if (renderContext.getPass() != Pass.OVERLAY) {
                renderContext.add(this, node, new Point3d(), null);
            }

            if (renderContext.isSelected(node)) {
                if (renderContext.getPass() == Pass.OVERLAY) {
                    renderContext.add(this, node, new Point3d(), node.getMagnitude());
                }
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, DragNode node,
            RenderData<DragNode> renderData) {

        GL gl = renderContext.getGL();

        if (renderData.getUserData() == null) {
            double factor = ManipulatorRendererUtil.getScaleFactor(node, renderContext.getRenderView());
            double factorRecip = 1.0 / factor;

            float[] color = renderContext.selectColor(node, DragRenderer.color);
            gl.glColor4fv(color, 0);

            gl.glPushMatrix();
            gl.glScaled(factorRecip, factorRecip, factorRecip);

            gl.glColor4fv(renderContext.selectColor(node, color), 0);

            gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
            gl.glVertexPointer(3, GL.GL_FLOAT, 0, vertexBuffer);
            gl.glDrawArrays(GL.GL_LINES, 0, vertexBuffer.limit() / 3);
            gl.glDisableClientState(GL.GL_VERTEX_ARRAY);

            gl.glPopMatrix();
        } else {
            ParticleManipulatorUtil.drawNumber(renderContext, node, node.getMagnitude());
        }
    }
}
