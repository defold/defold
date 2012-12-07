package com.dynamo.cr.ddfeditor.scene;

import java.nio.FloatBuffer;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.RenderUtil;


public class BoxCollisionShapeRenderer extends CollisionShapeRenderer implements INodeRenderer<BoxCollisionShapeNode> {
    FloatBuffer unitBox;

    public BoxCollisionShapeRenderer() {
        unitBox = RenderUtil.createUnitBoxQuads();
    }

    @Override
    public void dispose(GL gl) { }

    @Override
    public void setup(RenderContext renderContext, BoxCollisionShapeNode node) {
        if (passes.contains(renderContext.getPass())) {
            renderContext.add(this, node, new Point3d(), unitBox);
        }
    }

    @Override
    public void render(RenderContext renderContext,
            BoxCollisionShapeNode node,
            RenderData<BoxCollisionShapeNode> renderData) {

        GL gl = renderContext.getGL();
        gl.glColor4fv(renderContext.selectColor(node, COLOR), 0);
        float w_e = (float) node.getWidth() * 0.5f;
        float h_e = (float) node.getHeight() * 0.5f;
        float d_e = (float) node.getDepth() * 0.5f;
        gl.glPushMatrix();
        gl.glScalef(w_e, h_e, d_e);

        FloatBuffer v = (FloatBuffer) renderData.getUserData();
        v.rewind();

        gl.glEnableClientState(GL.GL_VERTEX_ARRAY);

        gl.glVertexPointer(3, GL.GL_FLOAT, 0, v);

        gl.glDrawArrays(GL.GL_QUADS, 0, v.limit() / 3);

        gl.glDisableClientState(GL.GL_VERTEX_ARRAY);

        gl.glPopMatrix();
    }
}
