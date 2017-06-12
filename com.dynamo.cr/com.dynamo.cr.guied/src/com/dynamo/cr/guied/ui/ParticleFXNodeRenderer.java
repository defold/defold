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
import javax.vecmath.Vector3f;
import javax.vecmath.Vector4d;

import com.dynamo.cr.guied.core.ClippingNode.ClippingState;
import com.dynamo.cr.guied.core.ParticleFXNode;
import com.dynamo.cr.guied.util.Clipping;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.ManipulatorRendererUtil;
import com.dynamo.cr.sceneed.ui.RenderUtil;
import com.dynamo.gui.proto.Gui.NodeDesc.Pivot;
import com.dynamo.particle.proto.Particle;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.EmitterKey;
import com.dynamo.particle.proto.Particle.Modifier;
import com.dynamo.particle.proto.Particle.ModifierKey;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.jogamp.common.nio.Buffers;

public class ParticleFXNodeRenderer implements INodeRenderer<ParticleFXNode> {

    private static final float[] color = new float[] { 1, 1, 0, 1 };
    //private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.OVERLAY, Pass.SELECTION);
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE,Pass.SELECTION);
    
    // emitter shapes
    private FloatBuffer sphere;
    private FloatBuffer box;
    private FloatBuffer cone;
    
    // modifier shapes
    private FloatBuffer circle;
    private FloatBuffer spiral;
    
    // render data
    private FloatBuffer dragVertexBuffer;
    
    
    public ParticleFXNodeRenderer() {
        System.out.println("################## ParticleFXNodeRenderer ctor");
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
            if((renderContext.getPass() == Pass.OUTLINE) && (node.isClipping()) && (!Clipping.getShowClippingNodes())) {
                return;
            }

            /*ClippingState clippingState = node.getClippingState();
            if (clippingState != null) {
                System.out.println("--------------- NOOOOOOOOOOOO CLIPPSTATE!!!!");
                RenderData<ParticleFXNode> data = renderContext.add(this, node, new Point3d(), clippingState);
                data.setIndex(node.getClippingKey());
            }*/
            if (!node.isClipping() || node.getClippingVisible()) {
                ClippingState childState = null;
                //System.out.println("NO CLIPPSTATE!!!!");
                /*ClippingNode clipper = node.getClosestParentClippingNode();
                if (clipper != null) {
                    childState = clipper.getChildClippingState();
                }*/
                RenderData<ParticleFXNode> data = renderContext.add(this, node, new Point3d(), childState);
                data.setIndex(node.getRenderKey());
            }
        }
    }

    private double pivotOffsetX(ParticleFXNode node, double width) {
        Pivot p = node.getPivot();

        switch (p) {
        case PIVOT_CENTER:
        case PIVOT_S:
        case PIVOT_N:
            return width * 0.5;

        case PIVOT_NE:
        case PIVOT_E:
        case PIVOT_SE:
            return width;

        case PIVOT_SW:
        case PIVOT_W:
        case PIVOT_NW:
            return 0;
        }

        assert false;
        return Double.MAX_VALUE;
    }

    private double pivotOffsetY(ParticleFXNode node, double height) {
        Pivot p = node.getPivot();

        switch (p) {
        case PIVOT_CENTER:
        case PIVOT_E:
        case PIVOT_W:
            return height * 0.5;

        case PIVOT_N:
        case PIVOT_NE:
        case PIVOT_NW:
            return height;

        case PIVOT_S:
        case PIVOT_SW:
        case PIVOT_SE:
            return 0;
        }

        assert false;
        return Double.MAX_VALUE;
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
    
    private static double getScaleFactorCustom(Node node,
            IRenderView renderView) {

        Matrix4d world = new Matrix4d();
        node.getWorldTransform(world);
        Vector4d position = new Vector4d();
        world.getColumn(3, position);

        Matrix4d viewTransform = new Matrix4d();
        viewTransform = renderView.getViewTransform();
        viewTransform.invert();
        Vector4d xAxis = new Vector4d();
        viewTransform.getColumn(0, xAxis);

        Vector4d p2 = new Vector4d(position);
        p2.add(xAxis);
        double[] cp1 = renderView.worldToView(new Point3d(position.x, position.y, position.z));
        double[] cp2 = renderView.worldToView(new Point3d(p2.x, p2.y, p2.z));
        double factor = Math.abs(cp1[0] - cp2[0]);
        return factor;
    }
    
    @Override
    public void render(RenderContext renderContext, ParticleFXNode node, RenderData<ParticleFXNode> renderData) {
        GL2 gl = renderContext.getGL();
        
        //double factor = ManipulatorRendererUtil.getScaleFactor(node, renderContext.getRenderView());
        double factor = getScaleFactorCustom(node, renderContext.getRenderView());
        double factorRecip = 1.0 / factor;
        float[] color = renderContext.selectColor(node, ParticleFXNodeRenderer.color);
        gl.glColor4fv(color, 0);
        
        Object[] emitters = node.getEmitters();
        //Object[] modifiers = node.getModifiers();
        
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
            case EMITTER_TYPE_2DCONE:
                v = cone;
                scaleX = scaleZ = 0.5 * getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_X);
                scaleY = getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_Y);
                break;
            case EMITTER_TYPE_SPHERE:
                 v = sphere;
                 scaleX = scaleY = scaleZ = 0.5 * getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_X);
                 break;
            case EMITTER_TYPE_CONE:
                v = cone;
                scaleX = scaleZ = 0.5 * getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_X);
                scaleY = getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_Y);
                break;
            case EMITTER_TYPE_BOX:
                v = box;
                scaleX = 0.5 * getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_X);
                scaleY = 0.5 * getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_Y);
                scaleZ = 0.5 * getEmitterKeyNumVal(e, EmitterKey.EMITTER_KEY_SIZE_Z);
                break;
            }
            
            // Emitter transformation matrix
            Point3 emitterPos = e.getPosition();
            Quat q = e.getRotation();
            Vector3f v4 = new Vector3f(emitterPos.getX(), emitterPos.getY(), emitterPos.getZ());
            Quat4f q4 = new Quat4f(q.getX(), q.getY(), q.getZ(), q.getW());
            Matrix4f mat4 = new Matrix4f(q4, v4, 1.0f);
            float[] emitterMat4Floats = {mat4.m00, mat4.m10, mat4.m20, mat4.m30,
                    mat4.m01, mat4.m11, mat4.m21, mat4.m31,
                    mat4.m02, mat4.m12, mat4.m22, mat4.m32,
                    mat4.m03, mat4.m13, mat4.m23, mat4.m33};

            gl.glPushMatrix();
            gl.glMultMatrixf(emitterMat4Floats, 0);
            gl.glScaled(scaleX, scaleY, scaleZ);
            gl.glColor4fv(renderContext.selectColor(node, color), 0);
            
            v.rewind();
            
            gl.glEnableClientState(GL2.GL_VERTEX_ARRAY);
            gl.glVertexPointer(3, GL.GL_FLOAT, 0, v);
            gl.glDrawArrays(GL.GL_LINES, 0, v.limit() / 3);
            gl.glDisableClientState(GL2.GL_VERTEX_ARRAY);
            gl.glPopMatrix();
            
            List<Modifier> emitter_mods = e.getModifiersList();
            int emitter_mods_count = e.getModifiersCount();
            
            for (int j = 0; j < emitter_mods_count; ++j) {
                Particle.Modifier m = emitter_mods.get(j);
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
                    renderModifierVortex(renderContext, node, emitterMat4Floats, modifierMat4Floats, m, magnitude, factorRecip);
                }
                if (m.getType() == Particle.ModifierType.MODIFIER_TYPE_DRAG) {
                    renderModifierDrag(renderContext, emitterPos, modifierMat4Floats, factorRecip);
                }
                if (m.getType() == Particle.ModifierType.MODIFIER_TYPE_RADIAL) {
                    renderModifierRadial(renderContext, node, emitterMat4Floats, modifierMat4Floats, m, magnitude, factor);
                }
                if (m.getType() == Particle.ModifierType.MODIFIER_TYPE_ACCELERATION) {
                    renderModifierAcceleration(renderContext, emitterPos, modifierMat4Floats, magnitude, factor);
                }
            }
        }

        if (renderContext.getPass() == Pass.OUTLINE && node.isClipping()) {
            if (renderContext.isSelected(node)) {
                gl.glColor4fv(Clipping.OUTLINE_SELECTED_COLOR, 0);
            } else {
                gl.glColor4fv(Clipping.OUTLINE_COLOR, 0);
            }
        } else {
            gl.glColor4fv(renderContext.selectColor(node, color), 0);
        }
        
        // Update AABB
        AABB aabb = new AABB();
        node.getAABB(aabb);
        // aabb.union(x0, y0, 0.0);
        // aabb.union(x1, y1, 0.0);
        node.setAABB(aabb);

        /*double x0 = -pivotOffsetX(node, node.getSize().x * node.getScale().x);
        double y0 = -pivotOffsetY(node, node.getSize().y * node.getScale().y);
        double x1 = x0 + node.getSize().x * node.getScale().x;
        double y1 = y0 + node.getSize().y * node.getScale().y;*/
        
        double x0 = -pivotOffsetX(node, aabb.getMin().getX());
        double y0 = -pivotOffsetY(node, aabb.getMin().getY());
        double x1 = x0 + aabb.getMax().getX();
        double y1 = y0 + aabb.getMax().getY();
        
        // debug just draw a quad as a place holder
        //gl.glColor4fv(renderContext.selectColor(node, color), 0);
        /*gl.glColor4fv(ParticleFXNodeRenderer.color, 0);
        gl.glBegin(GL2.GL_QUADS);
        for (int i=0;i<3;i++)
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
        gl.glEnd();*/

        /*if (clipping && renderData.getPass() == Pass.TRANSPARENT) {
            Clipping.endClipping(gl);
        }*/
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
    
    private void renderModifierVortex(RenderContext renderContext, ParticleFXNode node, float[] emitterTrans, float[] modifierTrans, Particle.Modifier modifier, float magnitude, double factorRecip) {
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
        
        gl.glMultMatrixf(emitterTrans, 0);
        gl.glMultMatrixf(modifierTrans, 0);
        
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
    
    private void renderModifierDrag(RenderContext renderContext, Point3 emitterPos, float[] modifierTrans, double factorRecip) {
        GL2 gl = renderContext.getGL();
        gl.glPushMatrix();
        
        gl.glTranslated(emitterPos.getX(), emitterPos.getY(), emitterPos.getZ());
        gl.glMultMatrixf(modifierTrans, 0);
        
        gl.glScaled(factorRecip, factorRecip, factorRecip);
        gl.glEnableClientState(GL2.GL_VERTEX_ARRAY);
        gl.glVertexPointer(3, GL.GL_FLOAT, 0, dragVertexBuffer);
        gl.glDrawArrays(GL.GL_LINES, 0, dragVertexBuffer.limit() / 3);
        gl.glDisableClientState(GL2.GL_VERTEX_ARRAY);
        gl.glPopMatrix();
    }
    
    private void renderModifierRadial(RenderContext renderContext, ParticleFXNode node, float[] emitterTrans, float[] modifierTrans, Particle.Modifier modifier, float magnitude, double factor) {
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

    private void renderModifierAcceleration(RenderContext renderContext, Point3 emitterPos, float[] modifierTrans, float magnitude, double factor) {
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

























