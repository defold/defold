package com.dynamo.cr.parted.manipulators;

import javax.media.opengl.GL2;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.jogamp.opengl.util.awt.TextRenderer;

public class ParticleManipulatorUtil {

    public static void drawNumber(RenderContext renderContext, Node node, double value) {
        TextRenderer textRenderer = renderContext.getSmallTextRenderer();
        GL2 gl = renderContext.getGL();
        gl.glPushMatrix();
        gl.glScaled(1, -1, 1);
        textRenderer.begin3DRendering();
        textRenderer.setColor(1, 1, 1, 1);
        String text = String.format("%.1f", value);
        Matrix4d transform = new Matrix4d();
        node.getWorldTransform(transform);
        Point3d pos = renderContext.getRenderView().getCamera().project(transform.getM03(), transform.getM13(), transform.getM23());
        textRenderer.draw3D(text, (float) pos.x + 12, (float) -pos.y, 1, 1);
        textRenderer.flush();
        textRenderer.end3DRendering();
        gl.glPopMatrix();

    }

}
