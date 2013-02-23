package com.dynamo.cr.sceneed.ui;

import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;

public class AxisManipulatorRenderer implements INodeRenderer<AxisManipulator> {

    public AxisManipulatorRenderer() {
    }

    @Override
    public void dispose(GL2 gl) { }

    @Override
    public void setup(RenderContext renderContext, AxisManipulator node) {
        Pass pass = renderContext.getPass();
        if (pass == Pass.MANIPULATOR || pass == Pass.SELECTION) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext, AxisManipulator node,
            RenderData<AxisManipulator> renderData) {
        float[] color = node.getColor();
        GL2 gl = renderContext.getGL();

        double factor = ManipulatorRendererUtil.getScaleFactor(node, renderContext.getRenderView());
        gl.glColor4fv(color, 0);
        RenderUtil.drawArrow(gl, ManipulatorRendererUtil.BASE_LENGTH / factor,
                                 3.3 * ManipulatorRendererUtil.BASE_HEAD_RADIUS / factor,
                                 ManipulatorRendererUtil.BASE_THICKNESS / factor,
                                 ManipulatorRendererUtil.BASE_HEAD_RADIUS / factor);
    }

}
