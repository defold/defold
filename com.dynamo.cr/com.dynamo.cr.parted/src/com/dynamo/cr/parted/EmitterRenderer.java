package com.dynamo.cr.parted;

import java.nio.FloatBuffer;
import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.RenderUtil;
import com.dynamo.particle.proto.Particle.EmitterKey;

public class EmitterRenderer implements INodeRenderer<EmitterNode> {

    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.SELECTION);
    private float[] color = new float[] {1.0f, 1.0f, 1.0f, 1.0f};
    private FloatBuffer sphere;
    private FloatBuffer box;
    private FloatBuffer cone;

    public EmitterRenderer() {
        // Unity style visualization. For 3d an additional screen-space circle should be drawn as well
        int steps = 32;
        sphere = RenderUtil.createUnitLineSphere(steps);
        box = RenderUtil.createUnitLineBox();
        cone = RenderUtil.createUnitLineCone(steps);
    }

    @Override
    public void dispose() {
    }

    @Override
    public void setup(RenderContext renderContext, EmitterNode node) {
        if (passes.contains(renderContext.getPass())) {
            FloatBuffer v = null;
            switch (node.getEmitterType()) {
            case EMITTER_TYPE_SPHERE:
                 v = sphere;
                 break;
            case EMITTER_TYPE_CONE:
                v = cone;
                break;
            case EMITTER_TYPE_BOX:
                v = box;
                break;
            }
            renderContext.add(this, node, new Point3d(), v);
        }
    }

    @Override
    public void render(RenderContext renderContext, EmitterNode node,
            RenderData<EmitterNode> renderData) {
        GL gl = renderContext.getGL();

        double scaleX = 1;
        double scaleY = 1;
        double scaleZ = 1;

        switch (node.getEmitterType()) {
        case EMITTER_TYPE_SPHERE:
            scaleX = scaleY = scaleZ = node.getProperty(EmitterKey.EMITTER_KEY_SIZE_X.toString()).getValue();
            break;
        case EMITTER_TYPE_CONE:
            scaleX = scaleZ = node.getProperty(EmitterKey.EMITTER_KEY_SIZE_X.toString()).getValue();
            scaleY = node.getProperty(EmitterKey.EMITTER_KEY_SIZE_Y.toString()).getValue();
            break;
        case EMITTER_TYPE_BOX:
            scaleX = node.getProperty(EmitterKey.EMITTER_KEY_SIZE_X.toString()).getValue();
            scaleY = node.getProperty(EmitterKey.EMITTER_KEY_SIZE_Y.toString()).getValue();
            scaleZ = node.getProperty(EmitterKey.EMITTER_KEY_SIZE_Z.toString()).getValue();
            break;
        }

        gl.glPushMatrix();
        gl.glScaled(scaleX, scaleY, scaleZ);
        gl.glColor4fv(renderContext.selectColor(node, color), 0);

        FloatBuffer v = (FloatBuffer) renderData.getUserData();
        v.rewind();

        gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
        gl.glVertexPointer(3, GL.GL_FLOAT, 0, v);
        gl.glDrawArrays(GL.GL_LINES, 0, v.limit() / 3);
        gl.glDisableClientState(GL.GL_VERTEX_ARRAY);

        gl.glPopMatrix();
    }

}
