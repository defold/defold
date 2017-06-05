package com.dynamo.cr.parted.nodes;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.util.EnumSet;
import java.util.List;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4f;

import com.dynamo.cr.parted.ParticleEditorPlugin;
import com.dynamo.cr.parted.ParticleLibrary;
import com.dynamo.cr.parted.ParticleLibrary.Matrix4;
import com.dynamo.cr.parted.ParticleLibrary.RenderInstanceCallback;
import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.RenderUtil;
import com.dynamo.cr.sceneed.ui.util.Shader;
import com.dynamo.cr.sceneed.ui.util.VertexBufferObject;
import com.dynamo.cr.sceneed.ui.util.VertexFormat;
import com.dynamo.cr.sceneed.ui.util.VertexFormat.AttributeFormat;
import com.dynamo.particle.proto.Particle.BlendMode;
import com.dynamo.particle.proto.Particle.EmitterKey;
import com.jogamp.opengl.util.texture.Texture;
import com.sun.jna.Pointer;

public class EmitterRenderer implements INodeRenderer<EmitterNode> {

    private static final EnumSet<Pass> passes = EnumSet.of(Pass.TRANSPARENT, Pass.OUTLINE, Pass.SELECTION);
    private float[] color = new float[] {1.0f, 1.0f, 1.0f, 1.0f};
    private FloatBuffer sphere;
    private FloatBuffer box;
    private FloatBuffer cone;
    private Shader shader;
    private VertexFormat vertexFormat;
    private VertexBufferObject vbo;

    // Static since there seems to be a crash-bug in JNA when the callback is GC'd
    private static final Callback callBack = new Callback();

    private static class UserData {
        Pointer texture;
        int blendMode;
        int vertexIndex;
        int vertexCount;
    }

    private static class Callback implements RenderInstanceCallback {
        RenderContext renderContext;
        EmitterNode currentNode;
        EmitterRenderer renderer;

        @Override
        public void invoke(Pointer userContext, Pointer material,
                Pointer texture, Matrix4 worldTransform, int blendMode, int vertexIndex, int vertexCount, Pointer constants, int constantCount) {
            UserData userData = new UserData();
            userData.texture = texture;
            userData.blendMode = blendMode;
            userData.vertexIndex = vertexIndex;
            userData.vertexCount = vertexCount;
            Vector3d translation = new Vector3d();
            worldTransform.toMatrix().get(translation);
            renderContext.add(renderer, currentNode, new Point3d(translation), userData);
        }
    }

    public EmitterRenderer() {
        // Unity style visualization. For 3d an additional screen-space circle should be drawn as well
        int steps = 32;
        sphere = RenderUtil.createUnitLineSphere(steps);
        box = RenderUtil.createUnitLineBox();
        cone = RenderUtil.createUnitLineCone(steps);
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
    public void setup(RenderContext renderContext, EmitterNode node) {
        if (passes.contains(renderContext.getPass())) {
            if (renderContext.getPass().equals(Pass.TRANSPARENT)) {
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
                ParticleFXNode parent = (ParticleFXNode)node.getParent();
                if (parent.getContext() != null && this.shader != null && !ParticleLibrary.Particle_IsSleeping(parent.getContext(), parent.getInstance())) {
                    // Fetch emitter index
                    // We need to manually search for the node since the parent also has modifiers
                    int emitterIndex = -1;
                    List<Node> children = parent.getChildren();
                    int childCount = children.size();
                    int currentIndex = 0;
                    for (int i = 0; i < childCount; ++i) {
                        Node child = children.get(i);
                        if (child instanceof EmitterNode) {
                            if (child.equals(node)) {
                                emitterIndex = currentIndex;
                                break;
                            }
                            ++currentIndex;
                        }
                    }
                    callBack.renderContext = renderContext;
                    callBack.renderer = this;
                    callBack.currentNode = node;
                    ParticleLibrary.Particle_RenderEmitter(parent.getContext(), parent.getInstance(), emitterIndex, new Pointer(0), callBack);
                }
            } else {
                FloatBuffer v = null;
                switch (node.getEmitterType()) {
                case EMITTER_TYPE_CIRCLE:
                    v = sphere;
                    break;
                case EMITTER_TYPE_2DCONE:
                    v = cone;
                    break;
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
    }

    private double getScale(EmitterNode node, EmitterKey key) {
        // This logic is due the abstract spline in ValueSpread
        // ValueSpread is in the ...properties bundle as it's currently not possible
        // to extend the property system with new types.
        // Another side effect is that the "value" of ValueSpread isn't set to
        // first key-frame when the property is animated, i.e. when altering the spline.
        ValueSpread vs = node.getProperty(key.toString());
        if (vs.isAnimated()) {
            return ((HermiteSpline) vs.getCurve()).getY(0);
        } else {
            return vs.getValue();
        }
    }

    @Override
    public void render(RenderContext renderContext, EmitterNode node,
            RenderData<EmitterNode> renderData) {
        GL2 gl = renderContext.getGL();
        if (renderContext.getPass().equals(Pass.TRANSPARENT)) {
            UserData userData = (UserData)renderData.getUserData();
            Texture t = null;
            if (userData.texture != null) {
                int index = (int) Pointer.nativeValue(userData.texture);
                if (index == 0) {
                    return;
                }
                --index;
                t = node.getTextureSetNode().getTextureHandle().getTexture(gl);

                t.bind(gl);
                t.setTexParameteri(gl, GL2.GL_TEXTURE_MIN_FILTER, GL2.GL_INTERPOLATE);
                t.setTexParameteri(gl, GL2.GL_TEXTURE_MAG_FILTER, GL2.GL_INTERPOLATE);
                t.setTexParameteri(gl, GL2.GL_TEXTURE_WRAP_S, GL2.GL_CLAMP);
                t.setTexParameteri(gl, GL2.GL_TEXTURE_WRAP_T, GL2.GL_CLAMP);
                t.enable(gl);
            }

            // General particle rendering

            this.shader.enable(gl);

            ParticleFXNode parent = (ParticleFXNode)node.getParent();
            ByteBuffer vb = parent.getVertexBuffer();
            int vbSize = parent.getVertexBufferSize();
            this.vbo.bufferData(vb, vbSize);
            this.vbo.enable(gl);
            this.vertexFormat.enable(gl, this.shader);

            Matrix4d viewProj = new Matrix4d();
            viewProj.mul(renderContext.getRenderView().getProjectionTransform(), renderContext.getRenderView().getViewTransform());
            this.shader.setUniforms(gl, "view_proj", viewProj);
            this.shader.setUniforms(gl, "DIFFUSE_TEXTURE", 0);
            this.shader.setUniforms(gl, "tint", new Vector4f(1.0f, 1.0f, 1.0f, 1.0f));

            setBlendFactors(BlendMode.valueOf(userData.blendMode), gl);
            gl.glDrawArrays(GL.GL_TRIANGLES, userData.vertexIndex, userData.vertexCount);
            if (t != null) {
                t.disable(gl);
            }
            // Reset to default blend functions
            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
            this.vbo.disable(gl);
            this.shader.disable(gl);
            this.vertexFormat.disable(gl, this.shader);

        } else {
            double scaleX = 1;
            double scaleY = 1;
            double scaleZ = 1;

            switch (node.getEmitterType()) {
            case EMITTER_TYPE_CIRCLE:
            case EMITTER_TYPE_SPHERE:
                scaleX = scaleY = scaleZ = 0.5 * getScale(node, EmitterKey.EMITTER_KEY_SIZE_X);
                break;
            case EMITTER_TYPE_2DCONE:
            case EMITTER_TYPE_CONE:
                scaleX = scaleZ = 0.5 * getScale(node, EmitterKey.EMITTER_KEY_SIZE_X);
                scaleY = getScale(node, EmitterKey.EMITTER_KEY_SIZE_Y);
                break;
            case EMITTER_TYPE_BOX:
                scaleX = 0.5 * getScale(node, EmitterKey.EMITTER_KEY_SIZE_X);
                scaleY = 0.5 * getScale(node, EmitterKey.EMITTER_KEY_SIZE_Y);
                scaleZ = 0.5 * getScale(node, EmitterKey.EMITTER_KEY_SIZE_Z);
                break;
            }

            gl.glPushMatrix();
            gl.glScaled(scaleX, scaleY, scaleZ);
            gl.glColor4fv(renderContext.selectColor(node, color), 0);

            FloatBuffer v = (FloatBuffer) renderData.getUserData();
            v.rewind();

            gl.glEnableClientState(GL2.GL_VERTEX_ARRAY);
            gl.glVertexPointer(3, GL.GL_FLOAT, 0, v);
            gl.glDrawArrays(GL.GL_LINES, 0, v.limit() / 3);
            gl.glDisableClientState(GL2.GL_VERTEX_ARRAY);

            gl.glPopMatrix();
        }
    }

    private void setBlendFactors(BlendMode blendMode, GL gl) {
        switch (blendMode) {
            case BLEND_MODE_ALPHA:
                gl.glBlendFunc(GL.GL_ONE, GL.GL_ONE_MINUS_SRC_ALPHA);
            break;

            case BLEND_MODE_ADD:
            case BLEND_MODE_ADD_ALPHA:
                gl.glBlendFunc(GL.GL_ONE, GL.GL_ONE);
            break;

            case BLEND_MODE_MULT:
                gl.glBlendFunc(GL.GL_ZERO, GL.GL_SRC_COLOR);
            break;
        }
    }

}
