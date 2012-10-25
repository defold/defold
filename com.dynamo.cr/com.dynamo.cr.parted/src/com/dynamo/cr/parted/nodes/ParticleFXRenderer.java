package com.dynamo.cr.parted.nodes;

import java.awt.geom.Rectangle2D;
import java.nio.FloatBuffer;
import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import com.dynamo.cr.parted.ParticleLibrary;
import com.dynamo.cr.parted.ParticleLibrary.InstanceStats;
import com.dynamo.cr.parted.ParticleLibrary.RenderInstanceCallback;
import com.dynamo.cr.parted.ParticleLibrary.Stats;
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
    // Static since there seems to be a crash-bug in JNA when the callback is GC'd
    private static final Callback callBack = new Callback();

    private long prevTime = 0;
    private double fps;
    private int frameCounter = 0;

    private static class Callback implements RenderInstanceCallback {
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
                Pointer texture, int blendMode, int vertexIndex, int vertexCount, Pointer constants, int constantCount) {
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
    }

    @Override
    public void setup(RenderContext renderContext, ParticleFXNode node) {
        Pointer context = node.getContext();

        if (context != null && passes.contains(renderContext.getPass())) {
            renderContext.add(this, node, new Point3d(), null);

            if (renderContext.getPass() == Pass.TRANSPARENT) {
                if (renderContext.getDt() > 0) {
                    int averageCount = 10;
                    if (frameCounter % averageCount == 0) {
                        long now = System.currentTimeMillis();
                        double diff = (now - prevTime) / 1000.0;
                        diff /= averageCount;
                        prevTime = now;

                        if (diff > 0) {
                            fps = Math.round(1.0 / diff);
                        }
                    }
                } else {
                    fps = 0;
                }
                ++frameCounter;
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, ParticleFXNode node,
            RenderData<ParticleFXNode> renderData) {

        GL gl = renderContext.getGL();
        double dt = renderContext.getDt();

        if (renderData.getPass() == Pass.OVERLAY) {
            // Special case for background pass. Render simulation time feedback

            TextRenderer textRenderer = renderContext.getSmallTextRenderer();

            gl.glPushMatrix();
            gl.glScaled(1, -1, 1);
            textRenderer.setColor(1, 1, 1, 1);
            textRenderer.begin3DRendering();

            Pointer context = node.getContext();
            Pointer instance = node.getInstance();

            Stats stats = new Stats();
            ParticleLibrary.Particle_GetStats(context, stats);
            InstanceStats instanceStats = new InstanceStats();
            ParticleLibrary.Particle_GetInstanceStats(context, instance, instanceStats);

            String text1 = String.format("Time: %.1f", instanceStats.time);
            String text2 = "Particles:";
            String text3 = String.format("%d/%d", stats.particles, stats.maxParticles);
            String text4 = String.format("FPS: %.0f", fps);
            Rectangle2D bounds = textRenderer.getBounds(text2);

            float x0 = 12;
            float y0 = -22;
            float dy = (float) bounds.getHeight();
            textRenderer.draw3D(text1, x0, y0, 1, 1);
            textRenderer.draw3D(text2, x0, y0 - dy * 2, 1, 1);
            if (stats.particles >= stats.maxParticles) {
                textRenderer.setColor(1, 0, 0, 1);
            }
            textRenderer.draw3D(text3, (x0 + 4 + (float) bounds.getWidth()), y0 - 2 * dy, 1, 1);
            textRenderer.draw3D(text4, x0, y0 - dy * 4, 1, 1);

            textRenderer.end3DRendering();
            gl.glPopMatrix();

        } else {
            // General particle rendering

            Pointer context = node.getContext();

            node.simulate(dt);

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

            ParticleLibrary.Particle_Render(context, new Pointer(0), callBack);

            gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);
            gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
            gl.glDisableClientState(GL.GL_COLOR_ARRAY);

            // Reset to default blend functions
            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        }
    }
}
