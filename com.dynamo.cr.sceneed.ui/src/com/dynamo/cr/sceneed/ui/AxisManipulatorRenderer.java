package com.dynamo.cr.sceneed.ui;

import javax.media.opengl.GL;
import javax.vecmath.Vector3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;

public class AxisManipulatorRenderer implements INodeRenderer<AxisManipulator> {

    public AxisManipulatorRenderer() {
    }

    @Override
    public void setup(RenderContext renderContext, AxisManipulator node) {
        Pass pass = renderContext.getPass();
        if (pass == Pass.MANIPULATOR || pass == Pass.SELECTION) {
            renderContext.add(this, node, new Vector3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext, AxisManipulator node,
            RenderData<AxisManipulator> renderData) {
        float[] color = node.getColor();
        GL gl = renderContext.getGL();
        float factor = 1;
        gl.glColor4fv(color, 0);
        RenderUtil.drawArrow(gl, 120 / factor, 20 / factor, 1 / factor, 5 / factor);
    }

}
