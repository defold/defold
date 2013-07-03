package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;
import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.util.Shader;
import com.dynamo.cr.sceneed.ui.util.VertexBufferObject;
import com.dynamo.cr.sceneed.ui.util.VertexFormat;
import com.dynamo.cr.sceneed.ui.util.VertexFormat.AttributeFormat;
import com.dynamo.cr.tileeditor.Activator;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.jogamp.opengl.util.texture.Texture;

public class SpriteRenderer implements INodeRenderer<SpriteNode> {

    private static final float COLOR[] = new float[] { 1.0f, 1.0f, 1.0f, 1.0f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);
    private VertexFormat vertexFormat;
    private Shader spriteShader;
    private Shader lineShader;

    public SpriteRenderer() {
        this.vertexFormat = new VertexFormat(
                new AttributeFormat("position", 3, GL.GL_FLOAT, false),
                new AttributeFormat("texcoord0", 2, GL.GL_UNSIGNED_SHORT, true));
    }

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

    @Override
    public void setup(RenderContext renderContext, SpriteNode node) {
        GL2 gl = renderContext.getGL();
        if (this.spriteShader == null) {
            this.spriteShader = loadShader(gl, "/content/pos_uv");
        }
        if (this.lineShader == null) {
            this.lineShader = loadShader(gl, "/content/line");
        }

        if (passes.contains(renderContext.getPass())) {
            TextureSetNode textureSet = node.getTextureSetNode();

            String animation = node.getDefaultAnimation();
            if (textureSet != null && textureSet.getTextureHandle().getTexture(gl) != null) {
                RuntimeTextureSet runtimeTextureSet = node.getTextureSetNode().getRuntimeTextureSet();
                if (runtimeTextureSet != null && runtimeTextureSet.getAnimation(animation) != null) {
                    renderContext.add(this, node, new Point3d(), null);
                }
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, SpriteNode node,
            RenderData<SpriteNode> renderData) {
        GL2 gl = renderContext.getGL();
        TextureSetNode textureSet = node.getTextureSetNode();
        Texture texture = textureSet.getTextureHandle().getTexture(gl);
        String animationId = node.getDefaultAnimation();

        RuntimeTextureSet runtimeTextureSet = textureSet.getRuntimeTextureSet();
        TextureSetAnimation animation = runtimeTextureSet.getAnimation(animationId);

        VertexBufferObject vertexBuffer;
        if (renderData.getPass() == Pass.OUTLINE) {
            vertexBuffer = runtimeTextureSet.getOutlineVertexBuffer();
        } else {
            vertexBuffer = runtimeTextureSet.getVertexBuffer();
        }

        boolean transparent = renderData.getPass() == Pass.TRANSPARENT;
        if (transparent) {
            texture.bind(gl);
            texture.setTexParameteri(gl, GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
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

        shader.enable(gl);
        vertexBuffer.enable(gl);
        shader.setUniforms(gl, "color", renderContext.selectColor(node, COLOR));
        this.vertexFormat.enable(gl, shader);
        if ( renderData.getPass() == Pass.OUTLINE) {
            gl.glDrawArrays(GL.GL_LINE_LOOP, runtimeTextureSet.getOutlineVertexStart(animation, 0), runtimeTextureSet.getOutlineVertexCount(animation, 0));
        } else {
            float scaleX = animation.getFlipHorizontal() != 0 ? -1 : 1;
            float scaleY = animation.getFlipVertical() != 0 ? -1 : 1;
            gl.glScalef(scaleX, scaleY, 1);
            gl.glDrawArrays(GL.GL_TRIANGLES, runtimeTextureSet.getVertexStart(animation, 0), runtimeTextureSet.getVertexCount(animation, 0));
            gl.glScalef(scaleX, scaleY, 1);
        }
        this.vertexFormat.disable(gl, shader);
        vertexBuffer.disable(gl);
        shader.disable(gl);

        if (transparent) {
            texture.disable(gl);
            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        }
    }

}
