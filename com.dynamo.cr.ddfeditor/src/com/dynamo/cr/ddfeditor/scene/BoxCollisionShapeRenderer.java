package com.dynamo.cr.ddfeditor.scene;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.RenderUtil;


public class BoxCollisionShapeRenderer extends CollisionShapeRenderer implements INodeRenderer<BoxCollisionShapeNode> {

    public BoxCollisionShapeRenderer() {
    }

    @Override
    public void setup(RenderContext renderContext, BoxCollisionShapeNode node) {
        if (passes.contains(renderContext.getPass())) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext,
            BoxCollisionShapeNode node,
            RenderData<BoxCollisionShapeNode> renderData) {

        GL gl = renderContext.getGL();
        gl.glColor4fv(renderContext.selectColor(node, COLOR), 0);
        float w_e = (float) node.getWidth();
        float h_e = (float) node.getHeight();
        float d_e = (float) node.getDepth();
        RenderUtil.drawCube(gl, -w_e, -h_e, -d_e, w_e, h_e, d_e);
    }
}
