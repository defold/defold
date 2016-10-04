package com.dynamo.cr.guied.ui;

import java.io.IOException;
import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.cr.guied.core.ClippingNode;
import com.dynamo.cr.guied.core.ClippingNode.ClippingState;
import com.dynamo.cr.guied.core.SpineNode;
import com.dynamo.cr.guied.util.Clipping;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.util.Shader;
import com.dynamo.cr.spine.Activator;
import com.dynamo.cr.tileeditor.scene.TextureSetNode;
import com.jogamp.opengl.util.texture.Texture;

public class SpineNodeRenderer implements INodeRenderer<SpineNode> {
    
    private static final float COLOR[] = new float[] { 1.0f, 1.0f, 1.0f, 1.0f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);
    private Shader spriteShader;
    private Shader lineShader;

    @Override
    public void dispose(GL2 gl) {
    }
    
    private static Shader loadShader(GL2 gl, String path) {
        Shader shader = new Shader(gl);
        try {
            shader.load(gl, Activator.getDefault().getBundle(), path);
        } catch (IOException e) {
            shader.dispose(gl);
            throw new IllegalStateException(e);
        }
        return shader;
    }

    @Override
    public void setup(RenderContext renderContext, SpineNode node) {
        GL2 gl = renderContext.getGL();
        
        if (passes.contains(renderContext.getPass())) {
            if((renderContext.getPass() == Pass.OUTLINE) && (node.isClipping()) && (!Clipping.getShowClippingNodes())) {
                return;
            }
            ClippingState clippingState = node.getClippingState();
            if (clippingState != null) {
                RenderData<SpineNode> data = renderContext.add(this, node, new Point3d(), clippingState);
                data.setIndex(node.getClippingKey());
            }
            if (!node.isClipping() || node.getClippingVisible()) {
                ClippingState childState = null;
                ClippingNode clipper = node.getClosestParentClippingNode();
                if (clipper != null) {
                    childState = clipper.getChildClippingState();
                }
                RenderData<SpineNode> data = renderContext.add(this, node, new Point3d(), childState);
                data.setIndex(node.getRenderKey());
            }
            
            if (this.spriteShader == null) {
                this.spriteShader = loadShader(gl, "/content/pos_uv");
            }
            if (this.lineShader == null) {
                this.lineShader = loadShader(gl, "/content/line");
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, SpineNode node, RenderData<SpineNode> renderData) {
        GL2 gl = renderContext.getGL();
        TextureSetNode textureSet = node.getTextureSetNode();
        if (textureSet == null || node.getCompositeMesh() == null) {
            return;
        }
        Texture texture = textureSet.getTextureHandle().getTexture(gl);

        boolean transparent = renderData.getPass() == Pass.TRANSPARENT;
        if (transparent) {
            texture.bind(gl);
            texture.setTexParameteri(gl, GL.GL_TEXTURE_MIN_FILTER, GL.GL_LINEAR_MIPMAP_NEAREST);
            texture.setTexParameteri(gl, GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
            texture.setTexParameteri(gl, GL.GL_TEXTURE_WRAP_S, GL2.GL_CLAMP);
            texture.setTexParameteri(gl, GL.GL_TEXTURE_WRAP_T, GL2.GL_CLAMP);
            texture.enable(gl);

            switch (node.getBlendMode()) {
            case BLEND_MODE_ALPHA:
                gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BLEND_MODE_ADD:
            case BLEND_MODE_ADD_ALPHA:
                gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE);
                break;
            case BLEND_MODE_MULT:
                gl.glBlendFunc(GL.GL_ZERO, GL.GL_SRC_COLOR);
                break;
            }
        }

        Shader shader = null;

        if (transparent) {
            shader = spriteShader;
        } else {
            shader = lineShader;
        }
        float[] color = renderContext.selectColor(node, COLOR);
        if (transparent) {
            node.getCompositeMesh().draw(gl, shader, color);
        } else {
            shader.enable(gl);
            shader.setUniforms(gl, "color", color);
            gl.glBegin(GL2.GL_QUADS);
            AABB aabb = new AABB();
            node.getAABB(aabb);
            Point3d min = aabb.getMin();
            Point3d max = aabb.getMax();
            gl.glColor4fv(color, 0);
            gl.glVertex3d(min.getX(), min.getY(), min.getZ());
            gl.glVertex3d(min.getX(), max.getY(), min.getZ());
            gl.glVertex3d(max.getX(), max.getY(), min.getZ());
            gl.glVertex3d(max.getX(), min.getY(), min.getZ());
            gl.glEnd();
            shader.disable(gl);
        }
        if (transparent) {
            texture.disable(gl);
            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        }
    }

}
