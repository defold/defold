package com.dynamo.cr.ddfeditor.scene;

import java.nio.FloatBuffer;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.RenderUtil;


public class SphereCollisionShapeRenderer extends CollisionShapeRenderer implements INodeRenderer<SphereCollisionShapeNode> {
    FloatBuffer unitSphere;

    public SphereCollisionShapeRenderer() {
        this.unitSphere = RenderUtil.createUnitSphereQuads(16, 8);
    }

    @Override
    public void dispose(GL2 gl) { }

    @Override
    public void setup(RenderContext renderContext, SphereCollisionShapeNode node) {
        if (passes.contains(renderContext.getPass())) {
            renderContext.add(this, node, new Point3d(), this.unitSphere);
        }
    }

    @Override
    public void render(RenderContext renderContext,
            SphereCollisionShapeNode node,
            RenderData<SphereCollisionShapeNode> renderData) {

        GL2 gl = renderContext.getGL();
        gl.glColor4fv(renderContext.selectColor(node, COLOR), 0);
        float sr = 0.5f * (float) node.getDiameter();

        gl.glPushMatrix();
        gl.glScalef(sr, sr, sr);

        FloatBuffer v = (FloatBuffer) renderData.getUserData();
        v.rewind();

        gl.glEnableClientState(GL2.GL_VERTEX_ARRAY);

        gl.glVertexPointer(3, GL.GL_FLOAT, 0, v);

        gl.glDrawArrays(GL2.GL_QUADS, 0, v.limit() / 3);

        gl.glDisableClientState(GL2.GL_VERTEX_ARRAY);

        gl.glPopMatrix();
    }

}
