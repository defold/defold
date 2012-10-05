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
import com.dynamo.particle.proto.Particle.BlendMode;
import com.sun.jna.Pointer;
import com.sun.opengl.util.j2d.TextRenderer;
import com.sun.opengl.util.texture.Texture;

public class ParticleFXRenderer implements INodeRenderer<ParticleFXNode> {

    private static final EnumSet<Pass> passes = EnumSet.of(Pass.TRANSPARENT, Pass.OVERLAY);

    private Callback callBack = new Callback();
    private TextRenderer textRenderer;
    private float timeElapsed = 0;

    private class Callback implements RenderInstanceCallback {
        GL gl;
        ParticleFXNode currentNode;

        private void setBlendFactors(BlendMode blendMode, GL gl) {
            switch (blendMode) {
                case BLEND_MODE_ALPHA:
                    gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
                break;

                case BLEND_MODE_ADD:
                    gl.glBlendFunc(GL.GL_ONE, GL.GL_ONE);
                break;

                case BLEND_MODE_ADD_ALPHA:
                    gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE);
                break;

                case BLEND_MODE_MULT:
                    gl.glBlendFunc(GL.GL_ZERO, GL.GL_SRC_COLOR);
                break;
            }
        }

        @Override
        public void invoke(Pointer userContext, Pointer material,
                Pointer texture, int blendMode, int vertexIndex, int vertexCount) {
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
            setBlendFactors(BlendMode.valueOf(blendMode), gl);
            gl.glDrawArrays(GL.GL_TRIANGLES, vertexIndex, vertexCount);
            if (t != null) {
                t.disable();
            }
        }
    }

    public ParticleFXRenderer() {
    }

    @Override
    public void dispose() {
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

            Pointer context = node.getContext();

            node.simulate(dt);
            timeElapsed += dt;

            gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);
            gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
            gl.glEnableClientState(GL.GL_COLOR_ARRAY);

            final int stride = ParticleFXNode.VERTEX_COMPONENT_COUNT * 4;
            FloatBuffer vertexBuffer = node.getVertexBuffer();
            FloatBuffer texCoords = vertexBuffer.slice();
            gl.glTexCoordPointer(2, GL.GL_FLOAT, stride, texCoords);
            vertexBuffer.position(2);
            FloatBuffer vertices = vertexBuffer.slice();
            gl.glVertexPointer(3, GL.GL_FLOAT, stride, vertices);
            vertexBuffer.position(5);
            FloatBuffer colors = vertexBuffer.slice();
            gl.glColorPointer(4, GL.GL_FLOAT, stride, colors);
            vertexBuffer.position(0);

            callBack.gl = gl;
            callBack.currentNode = node;

            // TODO: Creating a new callback here instead of the allocated one above
            // jna will crash in com.sun.jna.Native.freeNativeCallback when finalizers are run (during gc)
            // WHY!? Bug in JNA?
            ParticleLibrary.Particle_Render(context, new Pointer(0), callBack);

            gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);
            gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
            gl.glDisableClientState(GL.GL_COLOR_ARRAY);

            // Reset to default blend functions
            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        }
    }
}
