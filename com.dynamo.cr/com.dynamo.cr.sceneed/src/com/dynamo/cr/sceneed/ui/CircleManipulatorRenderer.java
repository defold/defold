package com.dynamo.cr.sceneed.ui;

import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;

public class CircleManipulatorRenderer implements INodeRenderer<CircleManipulator> {

    public CircleManipulatorRenderer() {
    }

    @Override
    public void dispose(GL2 gl) { }

    @Override
    public void setup(RenderContext renderContext, CircleManipulator node) {
        Pass pass = renderContext.getPass();
        if (pass == Pass.MANIPULATOR || pass == Pass.SELECTION) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext, CircleManipulator node,
            RenderData<CircleManipulator> renderData) {
        float[] color = node.getColor();
        GL2 gl = renderContext.getGL();

        double factor = ManipulatorRendererUtil.getScaleFactor(node, renderContext.getRenderView());
        double radius = ManipulatorRendererUtil.BASE_LENGTH  / factor;
        gl.glColor4fv(color, 0);
        RenderUtil.drawCircle(gl, radius);
    }

}
