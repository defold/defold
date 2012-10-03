package com.dynamo.cr.parted.nodes;

import java.awt.Font;
import java.nio.FloatBuffer;
import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import com.dynamo.cr.parted.ParticleLibrary;
import com.dynamo.cr.parted.ParticleLibrary.RenderInstanceCallback;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.sun.jna.Pointer;
import com.sun.opengl.util.BufferUtil;
import com.sun.opengl.util.j2d.TextRenderer;
import com.sun.opengl.util.texture.Texture;

public class ParticleFXRenderer implements INodeRenderer<ParticleFXNode> {

    private static final EnumSet<Pass> passes = EnumSet.of(Pass.TRANSPARENT, Pass.OVERLAY);
    private static final int MAX_PARTICLE_COUNT = 1024;
    private static final int MAX_EMITTER_COUNT = 128;

    private FloatBuffer vertexBuffer;
    private Pointer context;
    private Callback callBack = new Callback();
    private TextRenderer textRenderer;
    private float timeElapsed = 0;

    private class Callback implements RenderInstanceCallback {
        GL gl;
        ParticleFXNode currentNode;
        @Override
        public void invoke(Pointer userContext, Pointer material,
                Pointer texture, int vertexIndex, int vertexCount) {
            Texture t = null;
            if (texture != null) {
                int index = (int) Pointer.nativeValue(texture);
                if (index == 0) {
                    return;
                }
                --index;
                EmitterNode emitter = (EmitterNode)
                        currentNode.getChildren().get(index);
                t = emitter.getTileSetNode().getTextureHandle().getTexture();
                t.bind();
                t.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_INTERPOLATE);
                t.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_INTERPOLATE);
                t.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
                t.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
                t.enable();
            }
            gl.glDrawArrays(GL.GL_TRIANGLES, vertexIndex, vertexCount);
            if (t != null) {
                t.disable();
            }
        }
    }

    public ParticleFXRenderer() {
        // 6 vertices * 6 floats
        final int elementCount = MAX_PARTICLE_COUNT * 6 * 6;
        vertexBuffer = BufferUtil.newFloatBuffer(elementCount);
        context = ParticleLibrary.Particle_CreateContext(MAX_EMITTER_COUNT, MAX_PARTICLE_COUNT);
    }

    @Override
    public void dispose() {
        if (context != null) {
            ParticleLibrary.Particle_DestroyContext(context);
        }
        if (textRenderer != null) {
            textRenderer.dispose();
        }
    }

    @Override
    public void setup(RenderContext renderContext, ParticleFXNode node) {
        if (passes.contains(renderContext.getPass())) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext, ParticleFXNode node,
            RenderData<ParticleFXNode> renderData) {

        GL gl = renderContext.getGL();
        double dt = renderContext.getDt();
        if (dt == 0) {
            timeElapsed = 0;
        }

        if (renderData.getPass() == Pass.OVERLAY) {
            // Special case for background pass. Render simulation time feedback

            if (textRenderer == null) {
                String fontName = Font.SANS_SERIF;
                Font debugFont = new Font(fontName, Font.BOLD, 20);
                textRenderer = new TextRenderer(debugFont, true, true);
            }

            gl.glPushMatrix();
            gl.glScaled(1, -1, 1);
            textRenderer.setColor(1, 1, 1, 1);
            textRenderer.begin3DRendering();
            String text = String.format("%.1f", timeElapsed);
            textRenderer.draw3D(text, (float) 10, (float) -26, 1, 1);
            textRenderer.end3DRendering();
            gl.glPopMatrix();

        } else {
            // General particle rendering

            node.simulate(context, vertexBuffer, dt);
            timeElapsed += dt;

            // TODO proper color
            gl.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

            gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
            gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);

            // TODO custom vertex format for alpha component
            final int stride = 6 * 4;
            gl.glInterleavedArrays(GL.GL_T2F_V3F, stride, vertexBuffer);

            callBack.gl = gl;
            callBack.currentNode = node;

            // TODO: Creating a new callback here instead of the allocated one above
            // jna will crash in com.sun.jna.Native.freeNativeCallback when finalizers are run (during gc)
            // WHY!? Bug in JNA?
            ParticleLibrary.Particle_Render(context, new Pointer(0), callBack);

            gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
            gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);
        }
    }
}
