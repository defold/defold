package com.dynamo.cr.tileeditor.scene;

import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.util.VertexBufferObject;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.sun.opengl.util.texture.Texture;

public class SpriteRenderer implements INodeRenderer<SpriteNode> {

    private static final float COLOR[] = new float[] { 1.0f, 1.0f, 1.0f, 1.0f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);

    @Override
    public void dispose(GL gl) { }

    @Override
    public void setup(RenderContext renderContext, SpriteNode node) {
        if (passes.contains(renderContext.getPass())) {
            TextureSetNode textureSet = node.getTextureSetNode();
            String animation = node.getDefaultAnimation();
            if (textureSet != null && textureSet.getTextureHandle().getTexture() != null && textureSet.getAnimation(animation) != null) {
                renderContext.add(this, node, new Point3d(), null);
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, SpriteNode node,
            RenderData<SpriteNode> renderData) {
        TextureSetNode textureSet = node.getTextureSetNode();
        Texture texture = textureSet.getTextureHandle().getTexture();
        String animationId = node.getDefaultAnimation();

        TextureSetAnimation animation = textureSet.getAnimation(animationId);

        VertexBufferObject vertexBuffer;
        if (renderData.getPass() == Pass.OUTLINE) {
            vertexBuffer = textureSet.getOutlineVertexBuffer();
        } else {
            vertexBuffer = textureSet.getVertexBuffer();
        }

        GL gl = renderContext.getGL();

        boolean transparent = renderData.getPass() == Pass.TRANSPARENT;
        if (transparent) {
            texture.bind();
            texture.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
            texture.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
            texture.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
            texture.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
            texture.enable();

            switch (node.getBlendMode()) {
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

        gl.glColor4fv(renderContext.selectColor(node, COLOR), 0);

        gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
        gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);
        vertexBuffer.enable(gl);

        gl.glTexCoordPointer(2, GL.GL_FLOAT, TextureSetNode.COMPONENT_COUNT * 4, 0);
        gl.glVertexPointer(3, GL.GL_FLOAT, TextureSetNode.COMPONENT_COUNT * 4, 2 * 4);

        if ( renderData.getPass() == Pass.OUTLINE) {
            gl.glDrawArrays(GL.GL_LINE_LOOP, textureSet.getOutlineVertexStart(animation), textureSet.getOutlineVertexCount(animation));
        } else {
            gl.glDrawArrays(GL.GL_TRIANGLES, textureSet.getVertexStart(animation), textureSet.getVertexCount(animation));
        }

        vertexBuffer.disable(gl);
        gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
        gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);

        if (transparent) {
            texture.disable();
            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        }
    }

}
