package com.dynamo.cr.tileeditor.scene;

import java.nio.FloatBuffer;
import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.sun.opengl.util.texture.Texture;

public class SpriteRenderer implements INodeRenderer<SpriteNode> {

    private static final float COLOR[] = new float[] { 1.0f, 1.0f, 1.0f, 1.0f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);

    @Override
    public void dispose() { }

    @Override
    public void setup(RenderContext renderContext, SpriteNode node) {
        if (passes.contains(renderContext.getPass())) {
            TileSetNode tileSet = node.getTileSetNode();
            if (tileSet != null && tileSet.getTextureHandle().getTexture() != null && node.getVertexData() != null) {
                renderContext.add(this, node, new Point3d(), null);
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, SpriteNode node,
            RenderData<SpriteNode> renderData) {
        TileSetNode tileSet = node.getTileSetNode();
        Texture texture = tileSet.getTextureHandle().getTexture();
        FloatBuffer v = node.getVertexData();

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

        gl.glInterleavedArrays(GL.GL_T2F_V3F, 0, v);

        gl.glDrawArrays(GL.GL_QUADS, 0, 4);

        gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
        gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);

        if (transparent) {
            texture.disable();

            gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        }
    }

}
