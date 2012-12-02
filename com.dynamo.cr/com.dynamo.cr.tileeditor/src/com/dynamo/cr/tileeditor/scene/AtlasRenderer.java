package com.dynamo.cr.tileeditor.scene;

import java.awt.image.BufferedImage;
import java.util.List;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.tileeditor.atlas.AtlasMap;
import com.dynamo.cr.tileeditor.atlas.AtlasMap.Tile;
import com.sun.opengl.util.j2d.TextRenderer;
import com.sun.opengl.util.texture.Texture;
import com.sun.opengl.util.texture.TextureIO;

public class AtlasRenderer implements INodeRenderer<AtlasNode> {

    private Texture texture;
    private int version = -1;

    @Override
    public void dispose() {
        if (texture != null) {
            texture.dispose();
        }
    }

    @Override
    public void setup(RenderContext renderContext, AtlasNode node) {
        Pass pass = renderContext.getPass();
        if (pass == Pass.TRANSPARENT || pass == Pass.OVERLAY) {
            if (texture == null) {
                texture = TextureIO.newTexture(new BufferedImage(1, 1, BufferedImage.TYPE_4BYTE_ABGR), false);
            }

            AtlasMap atlasMap = node.getAtlasMap();
            if (atlasMap != null) {
                if (node.getVersion() != version) {
                    version = node.getVersion();
                    texture.updateImage(TextureIO.newTextureData(atlasMap.getImage(), true));
                }
                renderContext.add(this, node, new Point3d(), atlasMap);
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, AtlasNode node,
            RenderData<AtlasNode> renderData) {
        AtlasMap atlasMap = (AtlasMap) renderData.getUserData();

        GL gl = renderContext.getGL();

        if (renderData.getPass() == Pass.OVERLAY) {
            // Special case for background pass. Render simulation time feedback
            TextRenderer textRenderer = renderContext.getSmallTextRenderer();
            gl.glPushMatrix();
            gl.glScaled(1, -1, 1);
            textRenderer.setColor(1, 1, 1, 1);
            textRenderer.begin3DRendering();
            String text = String.format("Size: %dx%d", atlasMap.getImage().getWidth(), atlasMap.getImage().getHeight());
            float x0 = 12;
            float y0 = -22;
            textRenderer.draw3D(text, x0, y0, 1, 1);
            textRenderer.end3DRendering();
            gl.glPopMatrix();

        } else if (renderData.getPass() == Pass.TRANSPARENT){
            List<Tile> tiles = atlasMap.getTiles();

            texture.bind();
            texture.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_LINEAR_MIPMAP_LINEAR);
            texture.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_LINEAR);
            texture.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
            texture.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
            texture.enable();

            gl.glBegin(GL.GL_TRIANGLES);
            gl.glColor4f(1, 1, 1, 1);
            for (Tile tile : tiles) {
                float[] vertices = tile.getVertices();

                int index = 0;
                for (int i = 0; i < vertices.length / 5; ++i) {
                    gl.glTexCoord2f(vertices[index++], vertices[index++]);
                    gl.glVertex3f(vertices[index++], vertices[index++], vertices[index++]);
                }
            }
            gl.glEnd();
            texture.disable();

            gl.glBegin(GL.GL_TRIANGLES);
            gl.glColor4f(1, 1, 1, 0.1f);
            for (Tile tile : tiles) {
                float[] vertices = tile.getVertices();

                int index = 0;
                for (int i = 0; i < vertices.length / 5; ++i) {
                    gl.glTexCoord2f(vertices[index++], vertices[index++]);
                    gl.glVertex3f(vertices[index++], vertices[index++], vertices[index++]);
                }
            }
            gl.glEnd();
        }
    }
}

