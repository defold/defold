package com.dynamo.cr.tileeditor.scene;

import java.nio.FloatBuffer;
import java.util.EnumSet;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.sun.opengl.util.texture.Texture;

public class TileGridRenderer implements INodeRenderer<TileGridNode> {

    private static final float COLOR[] = new float[] { 1.0f, 1.0f, 1.0f, 1.0f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);

    @Override
    public void dispose() { }

    @Override
    public void setup(RenderContext renderContext, TileGridNode node) {
        if (passes.contains(renderContext.getPass())) {
            TileSetNode tileSet = node.getTileSetNode();
            if (tileSet != null
                    && tileSet.getTextureHandle() != null
                    && tileSet.getTextureHandle().getTexture() != null
                    && node.getVertexData() != null
                    && node.getVertexData().limit() > 0) {
                // Calculate AABB centroid
                AABB aabb = new AABB();
                node.getAABB(aabb);
                Point3d position = new Point3d(aabb.min);
                position.add(aabb.min);
                position.scale(0.5);
                renderContext.add(this, node, position, null);
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, TileGridNode node, RenderData<TileGridNode> renderData) {
        TileSetNode tileSet = node.getTileSetNode();
        Texture texture = tileSet.getTextureHandle().getTexture();
        FloatBuffer v = node.getVertexData();

        boolean useTexture = renderData.getPass() == Pass.TRANSPARENT;
        if (useTexture) {
            texture.bind();
            texture.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
            texture.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
            texture.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
            texture.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
            texture.enable();
        }

        GL gl = renderContext.getGL();
        gl.glColor4fv(renderContext.selectColor(node, COLOR), 0);

        gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
        gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);

        gl.glInterleavedArrays(GL.GL_T2F_V3F, 0, v);

        int vertexCount = v.limit() / 5;
        gl.glDrawArrays(GL.GL_QUADS, 0, vertexCount);

        gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
        gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);

        if (useTexture) {
            texture.disable();
        }
    }

}
