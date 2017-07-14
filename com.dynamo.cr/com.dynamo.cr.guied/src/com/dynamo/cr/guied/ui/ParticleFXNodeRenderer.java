package com.dynamo.cr.guied.ui;

import java.nio.FloatBuffer;
import java.util.EnumSet;
import java.util.List;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Matrix4d;
import javax.vecmath.Matrix4f;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4f;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector3f;

import com.dynamo.cr.guied.core.ParticleFXNode;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.ManipulatorRendererUtil;
import com.dynamo.cr.sceneed.ui.RenderUtil;
import com.dynamo.particle.proto.Particle;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.EmitterKey;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.ModifierKey;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.jogamp.common.nio.Buffers;
import com.jogamp.opengl.util.awt.TextRenderer;

public class ParticleFXNodeRenderer implements INodeRenderer<ParticleFXNode> {

    private static boolean renderOutlines;
    private static final float[] color = new float[] { 1, 1, 0, 1 };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.SELECTION, Pass.OVERLAY);
    
    // emitter shapes
    private FloatBuffer sphere;
    private FloatBuffer box;
    private FloatBuffer cone;
    
    // modifier shapes
    private FloatBuffer circle;
    private FloatBuffer spiral;
    
    // render data
    private FloatBuffer dragVertexBuffer;
    
    public static void setRenderOutlines(boolean enabled) {
        ParticleFXNodeRenderer.renderOutlines = enabled;
    }
    
    public static boolean getRenderOutlines() {
        return ParticleFXNodeRenderer.renderOutlines;
    }
    
    public ParticleFXNodeRenderer() {
        int steps = 32;
        sphere = RenderUtil.createUnitLineSphere(steps);
        box = RenderUtil.createUnitLineBox();
        cone = RenderUtil.createUnitLineCone(steps);
        
        circle = RenderUtil.createDashedCircle(64); // radial max-distance circle
        spiral = RenderUtil.createSpiral(); // vortex shape
        createDragVB();
    }

    @Override
    public void dispose(GL2 gl) {
    }
    
    @Override
    public void setup(RenderContext renderContext, ParticleFXNode node) {
        if (passes.contains(renderContext.getPass())) {
            if (renderContext.getPass() == Pass.OVERLAY) {
                RenderData<ParticleFXNode> data = renderContext.add(this, node, new Point3d(), new String("pfx"));
                data.setIndex(node.getRenderKey());
            } else {
                RenderData<ParticleFXNode> data = renderContext.add(this, node, new Point3d(), null);
                data.setIndex(node.getRenderKey());
            }
        }
    }
    
    private void createDragVB() {
        int segments = 64;
        int circles = 5;
        dragVertexBuffer = Buffers.newDirectFloatBuffer(segments * 6 * circles);

        float rotation = (float) (Math.PI * 4);
        float length = 60;
        float ra = 8;
        float rb = 50;
        for (int j = 0; j < circles; ++j) {
            float x = length * j / (circles - 1.0f);

            float f = j / (circles - 1.0f);
            float r = (1 - f) * ra + f * rb;

            for (int i = 0; i < segments; ++i) {
                float f0 = ((float) i) / (segments - 1);
                float f1 = ((float) i + 1) / (segments - 1);
                float a0 = rotation * f0;
                float a1 = rotation * f1;

                dragVertexBuffer.put(x);
                dragVertexBuffer.put((float) (r * Math.cos(a0)));
                dragVertexBuffer.put((float) (r * Math.sin(a0)));
                dragVertexBuffer.put(x);
                dragVertexBuffer.put((float) (r * Math.cos(a1)));
                dragVertexBuffer.put((float) (r * Math.sin(a1)));
            }
        }
        dragVertexBuffer.rewind();
    }

    private void renderModifier(RenderContext renderContext, ParticleFXNode node, Vector3f emitterPos, float[] emitterMat4Floats, double factor, Vector3d invAccumScale, Particle.Modifier m) {
        float magnitude = 0.0f;
        for (Modifier.Property p : m.getPropertiesList()) {
            switch (p.getKey()) {
            case MODIFIER_KEY_MAGNITUDE:
                magnitude = p.getPointsList().get(0).getY(); //ParticleUtils.java:49
                break;
            }
        }

        // Modifier transformation matrix
        Point3 modifierPos = m.getPosition();
        Vector3f modifierPosf = new Vector3f(modifierPos.getX(), modifierPos.getY(), modifierPos.getZ());
        Quat modifierRotQ = m.getRotation();
        Quat4f modifierRotQf = new Quat4f(modifierRotQ.getX(), modifierRotQ.getY(), modifierRotQ.getZ(), modifierRotQ.getW());
        Matrix4f modifierMat4 = new Matrix4f(modifierRotQf, modifierPosf, 1.0f);
        float[] modifierMat4Floats = {modifierMat4.m00, modifierMat4.m10, modifierMat4.m20, modifierMat4.m30,
                modifierMat4.m01, modifierMat4.m11, modifierMat4.m21, modifierMat4.m31,
                modifierMat4.m02, modifierMat4.m12, modifierMat4.m22, modifierMat4.m32,
                modifierMat4.m03, modifierMat4.m13, modifierMat4.m23, modifierMat4.m33};

        // render modifiers
        if (m.getType() == Particle.ModifierType.MODIFIER_TYPE_VORTEX) {
            renderModifierVortex(renderContext, node, emitterMat4Floats, modifierMat4Floats, invAccumScale, m, magnitude, factor);
        }
        if (m.getType() == Particle.ModifierType.MODIFIER_TYPE_DRAG) {
            renderModifierDrag(renderContext, emitterPos, modifierMat4Floats, invAccumScale, factor);
        }
        if (m.getType() == Particle.ModifierType.MODIFIER_TYPE_RADIAL) {
            renderModifierRadial(renderContext, node, emitterMat4Floats, modifierMat4Floats, invAccumScale, m, magnitude, factor);
        }
        if (m.getType() == Particle.ModifierType.MODIFIER_TYPE_ACCELERATION) {
            renderModifierAcceleration(renderContext, emitterPos, modifierMat4Floats, invAccumScale, magnitude, factor);
        }
    }

    @Override
    public void render(RenderContext renderContext, ParticleFXNode node, RenderData<ParticleFXNode> renderData) {
        GL2 gl = renderContext.getGL();
        
        double factor = ManipulatorRendererUtil.getScaleFactor(node, renderContext.getRenderView());
        
        AABB aabb = new AABB();
        node.getAABB(aabb);
        
        Vector3d accumScale = node.getScale();
        Node parent = node.getParent();
        while (parent != null) {
            Vector3d parentScale = parent.getScale();
            accumScale = new Vector3d(
                    accumScale.getX()*parentScale.getX(),
                    accumScale.getY()*parentScale.getY(),
                    accumScale.getZ()*parentScale.getZ());
            parent = parent.getParent();
        }
        
        Vector3d invAccumScale = new Vector3d(1.0 / accumScale.getX(), 1.0 / accumScale.getY(), 1.0 / accumScale.getZ());

        double w = aabb.getMax().getX() - aabb.getMin().getX();
        double h = aabb.getMax().getY() - aabb.getMin().getX();
        
        double x0 = aabb.getMin().getX();
        double y0 = aabb.getMin().getY();
        double x1 = x0 + w;
        double y1 = y0 + h;
        
        if (renderData.getUserData() instanceof String) {
            if (!getRenderOutlines())
            {
                TextRenderer textRenderer = renderContext.getSmallTextRenderer();
                double[] nodeScreenPos = renderContext.getRenderView().worldToScreen(node.getTranslation());
                int[] view_port = new int[4];
                renderContext.getRenderView().getCamera().getViewport(view_port);
                
                gl.glPushMatrix();
                gl.glScaled(1, -1, 1);
                textRenderer.setColor(1,1,1,1);
                textRenderer.begin3DRendering();
                String text = String.format("pfx");
                float x_pos = (float)(nodeScreenPos[0] - w / 4.0);
                float y_pos = (float)(nodeScreenPos[1] - view_port[3] - h / 8.0);
                textRenderer.draw3D(text, x_pos, y_pos, 1, 1);
                textRenderer.end3DRendering();
                gl.glPopMatrix();
            }
            return;
        }
        
        float[] color = renderContext.selectColor(node, ParticleFXNodeRenderer.color);
        gl.glColor4fv(color, 0);
        
        if (getRenderOutlines()) {
            Object[] emitters = node.getEmitters();
            
            if (emitters == null) {
                return;
            }
            
            // Render emitters
            for (int i=0; i< emitters.length; ++i) {
                Particle.Emitter e = (Particle.Emitter) emitters[i];
                double scaleX = 1;
                double scaleY = 1;
                double scaleZ = 1;
                
                FloatBuffer v = null;
                switch (e.getType()) {
                case EMITTER_TYPE_CIRCLE:
                    v = sphere;
                    scaleX = scaleY = scaleZ = 0.5 * getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_X);
                    break;
                case EMITTER_TYPE_CONE:
                case EMITTER_TYPE_2DCONE:
                    v = cone;
                    scaleX = scaleZ = 0.5 * getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_X);
                    scaleY = getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_Y);
                    break;
                case EMITTER_TYPE_SPHERE:
                     v = sphere;
                     scaleX = scaleY = scaleZ = 0.5 * getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_X);
                     break;
                case EMITTER_TYPE_BOX:
                    v = box;
                    scaleX = 0.5 * getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_X);
                    scaleY = 0.5 * getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_Y);
                    scaleZ = 0.5 * getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_Z);
                    break;
                }
                
                v.rewind();
                
                // Emitter transformation matrix
                Vector3f emitterPos = new Vector3f(e.getPosition().getX(), e.getPosition().getY(), e.getPosition().getZ());
                Quat q = e.getRotation();
                Quat4f q4 = new Quat4f(q.getX(), q.getY(), q.getZ(), q.getW());
                Matrix4f mat4 = new Matrix4f(q4, emitterPos, 1.0f);
                float[] emitterMat4Floats = {mat4.m00, mat4.m10, mat4.m20, mat4.m30,
                        mat4.m01, mat4.m11, mat4.m21, mat4.m31,
                        mat4.m02, mat4.m12, mat4.m22, mat4.m32,
                        mat4.m03, mat4.m13, mat4.m23, mat4.m33};
    
                gl.glColor4fv(renderContext.selectColor(node, color), 0);
                gl.glPushMatrix();
                gl.glMultMatrixf(emitterMat4Floats, 0);
                gl.glScaled(scaleX, scaleY, scaleZ);
                gl.glEnableClientState(GL2.GL_VERTEX_ARRAY);
                gl.glVertexPointer(3, GL.GL_FLOAT, 0, v);
                gl.glDrawArrays(GL.GL_LINES, 0, v.limit() / 3);
                gl.glDisableClientState(GL2.GL_VERTEX_ARRAY);
                gl.glPopMatrix();
                
                List<Modifier> emitter_mods = e.getModifiersList();
                int emitter_mods_count = e.getModifiersCount();
                
                for (int j = 0; j < emitter_mods_count; ++j) {
                    renderModifier(renderContext, node, emitterPos, emitterMat4Floats, factor, invAccumScale, emitter_mods.get(j));
                }
            }

            Object[] mods = node.getModifiers();

            Vector3f nodePos = new Vector3f(0, 0, 0);
            Quat4d q = node.getRotation();
            Matrix4f mat4 = new Matrix4f(new Quat4f(q), nodePos, 1.0f);
            float[] nodeMat4Floats = {mat4.m00, mat4.m10, mat4.m20, mat4.m30,
                    mat4.m01, mat4.m11, mat4.m21, mat4.m31,
                    mat4.m02, mat4.m12, mat4.m22, mat4.m32,
                    mat4.m03, mat4.m13, mat4.m23, mat4.m33};

            for (int j = 0; j < mods.length; ++j) {
                renderModifier(renderContext, node, nodePos, nodeMat4Floats, factor, invAccumScale, (Particle.Modifier) mods[j]);
            }
        } else {
            // Render AABB
            gl.glBegin(GL2.GL_QUADS);
            {
                gl.glTexCoord2d(0, 0);
                gl.glVertex2d(x0, y0);
                
                gl.glTexCoord2d(0, 1);
                gl.glVertex2d(x0, y1);
                
                gl.glTexCoord2d(1, 1);
                gl.glVertex2d(x1, y1);
    
                gl.glTexCoord2d(1, 0);
                gl.glVertex2d(x1, y0);
            }
            gl.glEnd();
        }
    }
    
    private void drawArrowAcceleration(GL2 gl, double factor) {
        RenderUtil.drawArrow(gl, 0.6 * ManipulatorRendererUtil.BASE_LENGTH / factor,
                                 1.3 * ManipulatorRendererUtil.BASE_HEAD_RADIUS / factor,
                                 0.2 * ManipulatorRendererUtil.BASE_THICKNESS / factor,
                                 0.5 * ManipulatorRendererUtil.BASE_HEAD_RADIUS / factor);
    }
    
    private void drawArrowRadial(GL2 gl, double factor) {
        RenderUtil.drawArrow(gl, 0.4 * ManipulatorRendererUtil.BASE_LENGTH / factor,
                                 1.3 * ManipulatorRendererUtil.BASE_HEAD_RADIUS / factor,
                                 0.2 * ManipulatorRendererUtil.BASE_THICKNESS / factor,
                                 0.5 * ManipulatorRendererUtil.BASE_HEAD_RADIUS / factor);
    }
    
    private double getEmitterKeyNumVal(Particle.Emitter emitter, EmitterKey key) {
        List<Emitter.Property> properties = emitter.getPropertiesList();
        Emitter.Property p = properties.get(key.getNumber());
        double res = p.getPoints(0).getY();
        return res;
    }
    
    private double getModifierKeyNumVal(Particle.Modifier modifier, ModifierKey key) {
        List<Modifier.Property> properties = modifier.getPropertiesList();
        Modifier.Property p = properties.get(key.getNumber());
        double res = p.getPoints(0).getY();
        return res;
    }
    
    private void renderModifierVortex(RenderContext renderContext, ParticleFXNode node, float[] emitterTrans, float[] modifierTrans, Vector3d invScale, Particle.Modifier modifier, float magnitude, double factor) {
        GL2 gl = renderContext.getGL();
        // Draw radius circle if selected
        if (renderContext.isSelected(node)) {
            double r = getModifierKeyNumVal(modifier, ModifierKey.MODIFIER_KEY_MAX_DISTANCE);;
            gl.glPushMatrix();
            gl.glMultMatrixf(emitterTrans, 0);
            gl.glMultMatrixf(modifierTrans, 0);
            gl.glScaled(r, r, 1.0);
            gl.glEnableClientState(GL2.GL_VERTEX_ARRAY);
            gl.glVertexPointer(3, GL2.GL_FLOAT, 0, circle);
            gl.glDrawArrays(GL2.GL_LINES, 0, circle.limit() / 3);
            gl.glDisableClientState(GL2.GL_VERTEX_ARRAY);
            gl.glPopMatrix();
        }
        
        gl.glPushMatrix();
        gl.glScaled(invScale.x, invScale.y, invScale.z); // To make the icon fixed size in the viewport
        gl.glMultMatrixf(emitterTrans, 0);
        gl.glMultMatrixf(modifierTrans, 0);
        
        double factorRecip = 1.0 / factor;
        
        if (magnitude > 0.0) {
            gl.glScaled(-1.0, 1.0, 1.0);
        }
        gl.glScaled(50.0 * factorRecip, 50 * factorRecip, 1.0);
        gl.glEnableClientState(GL2.GL_VERTEX_ARRAY);
        gl.glVertexPointer(3, GL2.GL_FLOAT, 0, spiral);
        gl.glDrawArrays(GL2.GL_LINE_STRIP, 0, spiral.limit() / 3);
        gl.glDisableClientState(GL2.GL_VERTEX_ARRAY);
        gl.glPopMatrix();
    }
    
    private void renderModifierDrag(RenderContext renderContext, Vector3f emitterPos, float[] modifierTrans, Vector3d invScale, double factor) {
        GL2 gl = renderContext.getGL();
        gl.glPushMatrix();
        gl.glScaled(invScale.x, invScale.y, invScale.z); // To make the icon fixed size in the viewport
        gl.glTranslated(emitterPos.getX(), emitterPos.getY(), emitterPos.getZ());
        gl.glMultMatrixf(modifierTrans, 0);

        double factorRecip = 1.0 / factor;
        
        gl.glScaled(factorRecip, factorRecip, factorRecip);
        gl.glEnableClientState(GL2.GL_VERTEX_ARRAY);
        gl.glVertexPointer(3, GL.GL_FLOAT, 0, dragVertexBuffer);
        gl.glDrawArrays(GL.GL_LINES, 0, dragVertexBuffer.limit() / 3);
        gl.glDisableClientState(GL2.GL_VERTEX_ARRAY);
        gl.glPopMatrix();
    }
    
    private void renderModifierRadial(RenderContext renderContext, ParticleFXNode node, float[] emitterTrans, float[] modifierTrans, Vector3d invScale, Particle.Modifier modifier, float magnitude, double factor) {
        GL2 gl = renderContext.getGL();
        if (renderContext.isSelected(node)) {
            double r = getModifierKeyNumVal(modifier, ModifierKey.MODIFIER_KEY_MAX_DISTANCE);
            gl.glPushMatrix();
            gl.glMultMatrixf(emitterTrans, 0);
            gl.glMultMatrixf(modifierTrans, 0);
            
            gl.glScaled(r, r, 1.0);
            gl.glEnableClientState(GL2.GL_VERTEX_ARRAY);
            gl.glVertexPointer(3, GL.GL_FLOAT, 0, circle);
            gl.glDrawArrays(GL.GL_LINES, 0, circle.limit() / 3);
            gl.glDisableClientState(GL2.GL_VERTEX_ARRAY);
            gl.glPopMatrix();
        }
        
        int n = 8;
        for (int k = 0; k < n; ++k) {
            double a = 360.0 * k / (double) n;
            gl.glPushMatrix();
            gl.glScaled(invScale.x, invScale.y, invScale.z); // To make the icon fixed size in the viewport
            gl.glMultMatrixf(emitterTrans, 0);
            gl.glMultMatrixf(modifierTrans, 0);
            
            gl.glRotated(a, 0, 0, 1);
            if (magnitude < 0) {
                gl.glTranslated(30 / factor, 0, 0);
                gl.glRotated(180, 0, 0, 1);
                gl.glTranslated(-30 / factor, 0, 0);
            }

            gl.glTranslated(5.0 / factor, 0, 0);
            drawArrowRadial(gl, factor);
            gl.glPopMatrix();
        }
    }

    private void renderModifierAcceleration(RenderContext renderContext, Vector3f emitterPos, float[] modifierTrans, Vector3d invScale, float magnitude, double factor) {
        GL2 gl = renderContext.getGL();
        double sign = Math.signum(magnitude);
        if (sign == 0) {
            sign = 1.0;
        }

        int n = 5;
        double dy = 1.5 * (ManipulatorRendererUtil.BASE_LENGTH / factor) / n;
        double y = -dy * (n-1) / 2.0;
        for (int k = 0; k < n; ++k) {
            gl.glPushMatrix();
            gl.glScaled(invScale.x, invScale.y, invScale.z);
            gl.glTranslated(emitterPos.getX(), emitterPos.getY(), emitterPos.getZ());
            gl.glMultMatrixf(modifierTrans, 0);
            
            gl.glRotated(sign * 90.0, 0, 0, 1);
            gl.glTranslated(0, y + dy * k, 0);
            gl.glTranslated( -Math.abs((n / 2 - k)) *  dy * 0.3, 0, 0);
            drawArrowAcceleration(gl, factor);
            gl.glPopMatrix();
        }
    }
}