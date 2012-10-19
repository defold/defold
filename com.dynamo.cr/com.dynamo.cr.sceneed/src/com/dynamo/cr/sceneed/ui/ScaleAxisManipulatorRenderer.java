package com.dynamo.cr.sceneed.ui;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;

public class ScaleAxisManipulatorRenderer implements INodeRenderer<ScaleAxisManipulator> {

    public ScaleAxisManipulatorRenderer() {
    }

    @Override
    public void dispose() { }

    @Override
    public void setup(RenderContext renderContext, ScaleAxisManipulator node) {
        Pass pass = renderContext.getPass();
        if (pass == Pass.MANIPULATOR || pass == Pass.SELECTION) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext, ScaleAxisManipulator node,
            RenderData<ScaleAxisManipulator> renderData) {
        float[] color = node.getColor();
        GL gl = renderContext.getGL();
        double factor = ManipulatorRendererUtil.getScaleFactor(node, renderContext.getRenderView());
        gl.glColor4fv(color, 0);
        RenderUtil.drawScaleArrow(gl, ManipulatorRendererUtil.BASE_LENGTH / factor,
                                      ManipulatorRendererUtil.BASE_THICKNESS / factor,
                                      ManipulatorRendererUtil.BASE_HEAD_RADIUS / factor);
    }

}
