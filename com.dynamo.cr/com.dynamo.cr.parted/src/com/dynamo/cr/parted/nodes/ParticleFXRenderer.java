package com.dynamo.cr.parted.nodes;

import java.awt.geom.Rectangle2D;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Vector4f;

import com.dynamo.cr.parted.ParticleEditorPlugin;
import com.dynamo.cr.parted.ParticleLibrary;
import com.dynamo.cr.parted.ParticleLibrary.InstanceStats;
import com.dynamo.cr.parted.ParticleLibrary.RenderInstanceCallback;
import com.dynamo.cr.parted.ParticleLibrary.Stats;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.util.Shader;
import com.dynamo.cr.sceneed.ui.util.VertexBufferObject;
import com.dynamo.cr.sceneed.ui.util.VertexFormat;
import com.dynamo.cr.sceneed.ui.util.VertexFormat.AttributeFormat;
import com.dynamo.particle.proto.Particle.BlendMode;
import com.jogamp.opengl.util.awt.TextRenderer;
import com.jogamp.opengl.util.texture.Texture;
import com.sun.jna.Pointer;

public class ParticleFXRenderer implements INodeRenderer<ParticleFXNode> {

    private static final EnumSet<Pass> passes = EnumSet.of(Pass.TRANSPARENT, Pass.OVERLAY);

    // Static since there seems to be a crash-bug in JNA when the callback is GC'd
    private static final Callback callBack = new Callback();

    private long prevTime = 0;
    private double fps;
    private int frameCounter = 0;
    private Shader shader;
    private VertexFormat vertexFormat;
    private VertexBufferObject vbo;

    private static class Callback implements RenderInstanceCallback {
        GL2 gl;
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
                t = emitter.getTextureSetNode().getTextureHandle().getTexture(gl);

                t.bind(gl);
                t.setTexParameteri(gl, GL2.GL_TEXTURE_MIN_FILTER, GL2.GL_INTERPOLATE);
                t.setTexParameteri(gl, GL2.GL_TEXTURE_MAG_FILTER, GL2.GL_INTERPOLATE);
                t.setTexParameteri(gl, GL2.GL_TEXTURE_WRAP_S, GL2.GL_CLAMP);
                t.setTexParameteri(gl, GL2.GL_TEXTURE_WRAP_T, GL2.GL_CLAMP);
                t.enable(gl);
            }
            setBlendFactors(BlendMode.valueOf(blendMode), gl);
            gl.glDrawArrays(GL.GL_TRIANGLES, vertexIndex, vertexCount);
            if (t != null) {
                t.disable(gl);
            }
        }
    }

    public ParticleFXRenderer() {
        Matrix4d viewProj = new Matrix4d();
        viewProj.setIdentity();

        this.vertexFormat = new VertexFormat(
                new AttributeFormat("position", 3, GL.GL_FLOAT, false),
                new AttributeFormat("color", 4, GL.GL_UNSIGNED_BYTE, true),
                new AttributeFormat("texcoord0", 2, GL.GL_UNSIGNED_SHORT, true));
    }

    @Override
    public void dispose(GL2 gl) {
        if (this.shader != null) {
            this.shader.dispose(gl);
        }
        if (this.vbo != null) {
            this.vbo.dispose(gl);
        }
    }

    @Override
    public void setup(RenderContext renderContext, ParticleFXNode node) {
        Pointer context = node.getContext();

        GL2 gl = renderContext.getGL();
        if (this.shader == null) {
            this.shader = new Shader(gl);
            try {
                this.shader.load(gl, ParticleEditorPlugin.getDefault().getBundle(), "/content/particlefx");
            } catch (IOException e) {
                this.shader.dispose(gl);
                this.shader = null;
                throw new IllegalStateException(e);
            }
        }
        if (this.vbo == null) {
            this.vbo = new VertexBufferObject();
        }

        if (context != null && this.shader != null && passes.contains(renderContext.getPass())) {
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

        GL2 gl = renderContext.getGL();
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

            this.shader.enable(gl);

            ByteBuffer vb = node.getVertexBuffer();
            int vbSize = node.getVertexBufferSize();
            this.vbo.bufferData(vb, vbSize);
            this.vbo.enable(gl);
            this.vertexFormat.enable(gl, this.shader);

            Matrix4d viewProj = new Matrix4d();
            viewProj.mul(renderContext.getRenderView().getProjectionTransform(), renderContext.getRenderView().getViewTransform());
            this.shader.setUniforms(gl, "view_proj", viewProj);
            this.shader.setUniforms(gl, "DIFFUSE_TEXTURE", 0);
            this.shader.setUniforms(gl, "tint", new Vector4f(1.0f, 1.0f, 1.0f, 1.0f));

            callBack.gl = gl;
            callBack.currentNode = node;
            ParticleLibrary.Particle_Render(context, new Pointer(0), callBack);

            this.vbo.disable(gl);
            this.shader.disable(gl);
            this.vertexFormat.disable(gl, this.shader);

            // Reset to default blend functions
            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        }
    }
}
