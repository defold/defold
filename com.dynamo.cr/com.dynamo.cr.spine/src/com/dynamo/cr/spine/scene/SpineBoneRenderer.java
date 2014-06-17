package com.dynamo.cr.spine.scene;

import java.nio.FloatBuffer;
import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.RenderUtil;

public class SpineBoneRenderer implements INodeRenderer<SpineBoneNode> {

    private static final float COLOR[] = new float[] { 1.0f, 1.0f, 1.0f, 1.0f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE);
    FloatBuffer sphereFB = RenderUtil.createUnitLineSphere(16);

    @Override
    public void dispose(GL2 gl) {

    }

    @Override
    public void setup(RenderContext renderContext, SpineBoneNode node) {
        if (passes.contains(renderContext.getPass()) && renderContext.isSelected(node)) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    private SpineModelNode findModel(SpineBoneNode node) {
        Node parent = node.getParent();
        while (parent != null) {
            if (parent instanceof SpineModelNode) {
                return (SpineModelNode)parent;
            }
            parent = parent.getParent();
        }
        return null;
    }

    private double getBoneScale(SpineBoneNode node) {
        SpineModelNode model = findModel(node);
        if (model != null) {
            AABB aabb = new AABB();
            model.getAABB(aabb);
            double width = aabb.max.x - aabb.min.x;
            double height = aabb.max.y - aabb.min.y;
            double dim = (width + height) * 0.5;
            return dim * 0.02;
        }
        return 1.0;
    }

    @Override
    public void render(RenderContext renderContext, SpineBoneNode node,
            RenderData<SpineBoneNode> renderData) {
        GL2 gl = renderContext.getGL();
        float[] color = renderContext.selectColor(node, COLOR);
        gl.glColor4fv(color, 0);
        Matrix4d t = new Matrix4d();
        node.getLocalTransform(t);
        t.invert();
        Vector4d p = new Vector4d();
        t.getColumn(3, p);
        gl.glBegin(GL2.GL_LINES);
        gl.glVertex3d(p.x, p.y, p.z);
        gl.glVertex3d(0.0, 0.0, 0.0);
        gl.glEnd();

        FloatBuffer v = this.sphereFB;
        v.rewind();

        double scale = getBoneScale(node);
        gl.glPushMatrix();
        gl.glScaled(scale, scale, scale);
        gl.glEnableClientState(GL2.GL_VERTEX_ARRAY);
        gl.glVertexPointer(3, GL.GL_FLOAT, 0, v);
        gl.glDrawArrays(GL.GL_LINES, 0, v.limit() / 3);
        gl.glDisableClientState(GL2.GL_VERTEX_ARRAY);
        gl.glPopMatrix();
    }

}
