package com.dynamo.cr.spine.scene;

import java.io.IOException;
import java.util.EnumSet;
import java.util.List;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.bob.util.SpineSceneUtil;
import com.dynamo.bob.util.RigUtil.MeshAttachment;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.util.Shader;
import com.dynamo.cr.spine.Activator;
import com.dynamo.cr.tileeditor.scene.RuntimeTextureSet;
import com.dynamo.cr.tileeditor.scene.TextureSetNode;
import com.jogamp.opengl.util.texture.Texture;

public class SpineModelRenderer implements INodeRenderer<SpineModelNode> {

    private static final float COLOR[] = new float[] { 1.0f, 1.0f, 1.0f, 1.0f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);
    private Shader spriteShader;
    private Shader lineShader;

    @Override
    public void dispose(GL2 gl) {
        if (this.spriteShader != null) {
            this.spriteShader.dispose(gl);
        }
        if (this.lineShader != null) {
            this.lineShader.dispose(gl);
        }
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

    private boolean shouldRender(GL2 gl, SpineModelNode node) {
        SpineSceneUtil scene = node.getScene();
        if (scene == null) {
            return false;
        }
        TextureSetNode textureSet = node.getTextureSetNode();
        if (textureSet == null || textureSet.getTextureHandle().getTexture(gl) == null) {
            return false;
        }
        RuntimeTextureSet runtimeTextureSet = textureSet.getRuntimeTextureSet();
        if (runtimeTextureSet == null) {
            return false;
        }
        List<MeshAttachment> meshes = scene.getDefaultAttachments();
        String skin = node.getSkin();
        if (!skin.isEmpty() && scene.skins.containsKey(skin)) {
            meshes = scene.getAttachmentsForSkin(node.getSkin());
        }
        for (MeshAttachment mesh : meshes) {
            if (runtimeTextureSet.getAnimation(mesh.path) == null) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void setup(RenderContext renderContext, SpineModelNode node) {
        GL2 gl = renderContext.getGL();
        if (this.spriteShader == null) {
            this.spriteShader = loadShader(gl, "/content/pos_uv");
        }
        if (this.lineShader == null) {
            this.lineShader = loadShader(gl, "/content/line");
        }

        if (passes.contains(renderContext.getPass())) {
            if (shouldRender(gl, node)) {
                renderContext.add(this, node, new Point3d(), null);
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, SpineModelNode node,
            RenderData<SpineModelNode> renderData) {
        GL2 gl = renderContext.getGL();
        TextureSetNode textureSet = node.getTextureSetNode();
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
                gl.glBlendFunc(GL.GL_ONE, GL.GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BLEND_MODE_ADD:
                gl.glBlendFunc(GL.GL_ONE, GL.GL_ONE);
                break;
            case BLEND_MODE_MULT:
                gl.glBlendFunc(GL.GL_DST_COLOR, GL.GL_ONE_MINUS_SRC_ALPHA);
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
