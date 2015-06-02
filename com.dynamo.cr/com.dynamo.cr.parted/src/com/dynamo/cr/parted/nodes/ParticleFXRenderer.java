package com.dynamo.cr.parted.nodes;

import java.awt.geom.Rectangle2D;
import java.util.EnumSet;

import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.cr.parted.ParticleLibrary;
import com.dynamo.cr.parted.ParticleLibrary.InstanceStats;
import com.dynamo.cr.parted.ParticleLibrary.Stats;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.jogamp.opengl.util.awt.TextRenderer;
import com.sun.jna.Pointer;

public class ParticleFXRenderer implements INodeRenderer<ParticleFXNode> {

    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OVERLAY);

    private long prevTime = 0;
    private double fps;
    private int frameCounter = 0;

    @Override
    public void dispose(GL2 gl) {
    }

    @Override
    public void setup(RenderContext renderContext, ParticleFXNode node) {
        Pointer context = node.getContext();

        // We need to simulate the particles in setup() instead of render() so
        // that all the emitter nodes get the absolute latest simulation data
        // before rendering.
        if (renderContext.getPass().equals(Pass.TRANSPARENT)) {
            double dt = renderContext.getDt();
            node.simulate(dt);
        }

        if (context != null && passes.contains(renderContext.getPass())) {
            renderContext.add(this, node, new Point3d(), null);

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

    @Override
    public void render(RenderContext renderContext, ParticleFXNode node,
            RenderData<ParticleFXNode> renderData) {

        GL2 gl = renderContext.getGL();

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

    }
}
