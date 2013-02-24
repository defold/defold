package com.dynamo.cr.ddfeditor.scene;

import javax.media.opengl.GL2;
import javax.media.opengl.glu.GLU;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.RenderUtil;


public class CapsuleCollisionShapeRenderer extends CollisionShapeRenderer implements INodeRenderer<CapsuleCollisionShapeNode> {

    public CapsuleCollisionShapeRenderer() {
    }

    @Override
    public void dispose(GL2 gl) { }

    @Override
    public void setup(RenderContext renderContext, CapsuleCollisionShapeNode node) {
        if (passes.contains(renderContext.getPass())) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext,
            CapsuleCollisionShapeNode node,
            RenderData<CapsuleCollisionShapeNode> renderData) {

        GL2 gl = renderContext.getGL();
        GLU glu = renderContext.getGLU();
        gl.glColor4fv(renderContext.selectColor(node, COLOR), 0);
        final int slices = 16;
        final int stacks = 6;
        float radius = 0.5f * (float) node.getDiameter();
        float height = (float) node.getHeight();
        RenderUtil.drawCapsule(gl, glu, radius, height, slices, stacks);
    }

}
