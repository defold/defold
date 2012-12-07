package com.dynamo.cr.ddfeditor.scene;

import java.nio.FloatBuffer;
import java.util.EnumSet;
import java.util.List;

import javax.media.opengl.GL;
import javax.media.opengl.glu.GLU;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.RenderUtil;
import com.dynamo.physics.proto.Physics.ConvexShape;

public class CollisionObjectRenderer implements INodeRenderer<CollisionObjectNode> {
    private final FloatBuffer unitSphere;

    public CollisionObjectRenderer() {
        unitSphere = RenderUtil.createUnitSphereQuads(16, 8);
    }

    @Override
    public void dispose(GL gl) { }

    private final static float COLOR[] = new float[] { 255.0f / 255.0f, 247.0f / 255.0f, 73.0f/255.0f, 0.4f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);

    @Override
    public void setup(RenderContext renderContext, CollisionObjectNode node) {
        // NOTE: passes.contains(.) is just an example on how it could work
        // Currently we issue only outline, transparent and selection pass
        // Time will tell if the opaque pass will be used etc
        // TODO physical tile grids are currently not rendered, as the last term in the if below takes care of
        if (passes.contains(renderContext.getPass()) && node.getCollisionShapeNode() != null && node.getCollisionShapeNode() instanceof ConvexShapeNode) {
            renderContext.add(this, node, new Point3d(), unitSphere);
        }
    }

    @Override
    public void render(RenderContext renderContext, CollisionObjectNode node, RenderData<CollisionObjectNode> renderData) {
        GL gl = renderContext.getGL();
        GLU glu = renderContext.getGLU();

        ConvexShapeNode shapeNode = (ConvexShapeNode) node.getCollisionShapeNode();
        ConvexShape shape = shapeNode.getConvexShape();
        gl.glColor4fv(renderContext.selectColor(node, COLOR), 0);
        final int slices = 16;
        final int stacks = 6;

        switch (shape.getShapeType()) {
        case TYPE_BOX:
            float w_e = shape.getData(0);
            float h_e = shape.getData(1);
            float d_e = shape.getData(2);
            RenderUtil.drawCube(gl, -w_e, -h_e, -d_e, w_e, h_e, d_e);
            break;
        case TYPE_CAPSULE:
            float r = shape.getData(0);
            float h = shape.getData(1);
            RenderUtil.drawCapsule(gl, glu, r, h, slices, stacks);
            break;
        case TYPE_HULL:
            // NOTE: At least something but not beautiful
            List<Float> data = shape.getDataList();
            gl.glEnable(GL.GL_POINT_SMOOTH);
            gl.glBegin(GL.GL_POINTS);
            gl.glPointSize(10.0f);
            int n = (data.size() / 3) * 3;
            for (int i = 0; i < n; i += 3) {
                gl.glVertex3f(data.get(i), data.get(i+1), data.get(i+2));
            }
            gl.glEnd();
            break;
        case TYPE_SPHERE:
            float sr = shape.getData(0);
            gl.glPushMatrix();
            gl.glScalef(sr, sr, sr);

            FloatBuffer v = (FloatBuffer) renderData.getUserData();
            v.rewind();

            gl.glEnableClientState(GL.GL_VERTEX_ARRAY);

            gl.glVertexPointer(3, GL.GL_FLOAT, 0, v);

            gl.glDrawArrays(GL.GL_QUADS, 0, v.limit() / 3);

            gl.glDisableClientState(GL.GL_VERTEX_ARRAY);

            gl.glPopMatrix();
            break;
        }
    }
}
