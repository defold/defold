package com.dynamo.cr.sceneed.ui;

import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;

public class ScreenPlaneManipulatorRenderer implements INodeRenderer<ScreenPlaneManipulator> {

    public ScreenPlaneManipulatorRenderer() {
    }

    @Override
    public void dispose(GL2 gl) { }

    @Override
    public void setup(RenderContext renderContext, ScreenPlaneManipulator node) {
        Pass pass = renderContext.getPass();
        if (pass == Pass.MANIPULATOR || pass == Pass.SELECTION) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext, ScreenPlaneManipulator node,
            RenderData<ScreenPlaneManipulator> renderData) {
        float[] color = node.getColor();
        GL2 gl = renderContext.getGL();

        double factor = ManipulatorRendererUtil.getScaleFactor(node, renderContext.getRenderView());
        double dim = ManipulatorRendererUtil.BASE_DIM / factor;

        // TODO Fix screen space rendering, necessary for 3D cameras
        // https://defold.fogbugz.com/default.asp?2011

        gl.glColor4fv(color, 0);
        if (renderContext.getPass() == Pass.MANIPULATOR) {
            RenderUtil.drawSquare(gl, dim, dim);
        } else if (renderContext.getPass() == Pass.SELECTION) {
            RenderUtil.drawFilledSquare(gl, dim, dim);
        }
    }

}
