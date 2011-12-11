package com.dynamo.cr.ddfeditor.scene;

import javax.media.opengl.GL;
import javax.media.opengl.glu.GLU;
import javax.vecmath.Vector3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.RenderUtil;


public class SphereCollisionShapeRenderer extends CollisionShapeRenderer implements INodeRenderer<SphereCollisionShapeNode> {

    public SphereCollisionShapeRenderer() {
    }

    @Override
    public void setup(RenderContext renderContext, SphereCollisionShapeNode node) {
        if (passes.contains(renderContext.getPass())) {
            renderContext.add(this, node, new Vector3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext,
            SphereCollisionShapeNode node,
            RenderData<SphereCollisionShapeNode> renderData) {

        GL gl = renderContext.getGL();
        GLU glu = renderContext.getGLU();
        gl.glColor4fv(renderContext.selectColor(node, COLOR), 0);
        final int slices = 16;
        final int stacks = 6;
        float sr = (float) node.getRadius();
        RenderUtil.drawSphere(gl, glu, sr, slices, stacks);
    }

}
