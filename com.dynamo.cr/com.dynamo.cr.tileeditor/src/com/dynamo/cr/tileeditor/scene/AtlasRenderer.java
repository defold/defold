package com.dynamo.cr.tileeditor.scene;

import java.util.List;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.util.VertexBufferObject;
import com.dynamo.cr.tileeditor.atlas.AtlasMap;
import com.sun.opengl.util.j2d.TextRenderer;
import com.sun.opengl.util.texture.Texture;

public class AtlasRenderer implements INodeRenderer<AtlasNode> {

    private float time = 0;

    @Override
    public void dispose(GL gl) {
    }

    @Override
    public void setup(RenderContext renderContext, AtlasNode node) {
        Pass pass = renderContext.getPass();
        if (pass == Pass.TRANSPARENT || pass == Pass.OVERLAY) {
            AtlasMap atlasMap = node.getAtlasMap();
            if (atlasMap != null) {
                renderContext.add(this, node, new Point3d(), atlasMap);
            }
        }
    }

    @Override
    public void render(RenderContext renderContext, AtlasNode node,
            RenderData<AtlasNode> renderData) {
        GL gl = renderContext.getGL();
        AtlasMap atlasMap = (AtlasMap) renderData.getUserData();

        Texture texture = node.getTextureHandle().getTexture();

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
        } else if (renderData.getPass() == Pass.TRANSPARENT) {

            gl.glEnableClientState(GL.GL_VERTEX_ARRAY);
            gl.glEnableClientState(GL.GL_TEXTURE_COORD_ARRAY);

            VertexBufferObject vertexBuffer = node.getVertexBuffer();
            vertexBuffer.enable(gl);

            gl.glTexCoordPointer(2, GL.GL_FLOAT, TextureSetAnimation.COMPONENT_COUNT * 4, 0);
            gl.glVertexPointer(3, GL.GL_FLOAT, TextureSetAnimation.COMPONENT_COUNT * 4, 2 * 4);

            time += renderContext.getDt();
            AtlasAnimationNode playBackNode = node.getPlayBackNode();
            if (playBackNode != null) {
                renderAnimation(atlasMap, playBackNode, gl, texture);
                renderTiles(atlasMap, gl, 0.1f, texture);
            } else {
                renderTiles(atlasMap, gl, 1, texture);
            }
            gl.glDisableClientState(GL.GL_VERTEX_ARRAY);
            gl.glDisableClientState(GL.GL_TEXTURE_COORD_ARRAY);
        }
    }

    private void renderAnimation(AtlasMap atlasMap,
            AtlasAnimationNode playBackNode, GL gl, Texture texture) {
        bindTexture(texture);
        List<Node> children = playBackNode.getChildren();
        int nFrames = children.size();
        if (nFrames == 0 || playBackNode.getFps() <= 0) {
            return;
        }

        float T = 1.0f / playBackNode.getFps();
        int frameTime = (int) (time / T + 0.5);
        int frame = 0;

        switch (playBackNode.getPlayback()) {
        case PLAYBACK_LOOP_FORWARD:
        case PLAYBACK_ONCE_FORWARD:
            frame = frameTime % nFrames;
            break;
        case PLAYBACK_LOOP_BACKWARD:
        case PLAYBACK_ONCE_BACKWARD:
            frame = (nFrames-1) - (frameTime % nFrames);
            break;
        case PLAYBACK_LOOP_PINGPONG:
            // Unwrap to nFrames + (nFrames - 2)
            frame = frameTime % (nFrames * 2 - 2);
            if (frame >= nFrames) {
                // Map backwards
                frame = (nFrames - 1) - (frame - nFrames) - 1;
            }
        case PLAYBACK_NONE:
            break;
        }

        AtlasImageNode imageNode = (AtlasImageNode) children.get(frame);
        String id = imageNode.getId();
        List<TextureSetAnimation> tiles = atlasMap.getAnimations();
        TextureSetAnimation tileToRender = null;
        for (TextureSetAnimation tile : tiles) {
            if (tile.getId().equals(id)) {
                tileToRender = tile;
                break;
            }
        }

        if (tileToRender != null) {
            float centerX = atlasMap.getImage().getWidth() * 0.5f;
            float centerY = atlasMap.getImage().getHeight() * 0.5f;
            float scaleX = playBackNode.isFlipHorizontally() ? - 1 : 1;
            float scaleY = playBackNode.isFlipVertically() ? - 1 : 1;

            gl.glColor4f(1, 1, 1, 1);
            renderTile(gl, tileToRender, centerX, centerY, scaleX, scaleY);
            texture.disable();
        }
    }

    private void renderTiles(AtlasMap atlasMap, GL gl, float alpha, Texture texture) {
        List<TextureSetAnimation> tiles = atlasMap.getAnimations();
        bindTexture(texture);

        gl.glColor4f(1, 1, 1, 1 * alpha);
        for (TextureSetAnimation tile : tiles) {
            if (!tile.isAnimation()) {
                renderTile(gl, tile, tile.getCenterX(), tile.getCenterY(), 1, 1);
            }
        }
        texture.disable();

        gl.glColor4f(1, 1, 1, 0.1f * alpha);
        for (TextureSetAnimation tile : tiles) {
            if (!tile.isAnimation()) {
                renderTile(gl, tile, tile.getCenterX(), tile.getCenterY(), 1, 1);
            }
        }
    }

    private void renderTile(GL gl, TextureSetAnimation tile, float offsetX, float offsetY, float scaleX, float scaleY) {
        gl.glTranslatef(offsetX, offsetY, 0);
        gl.glScalef(scaleX, scaleY, 1);
        gl.glDrawArrays(GL.GL_TRIANGLES, tile.getVertexStart(), tile.getVertexCount());
        gl.glScalef(scaleX, scaleY, 1);
        gl.glTranslatef(-offsetX, -offsetY, 0);
    }

    private void bindTexture(Texture texture) {
        texture.bind();
        texture.setTexParameteri(GL.GL_TEXTURE_MIN_FILTER, GL.GL_LINEAR_MIPMAP_LINEAR);
        texture.setTexParameteri(GL.GL_TEXTURE_MAG_FILTER, GL.GL_LINEAR);
        texture.setTexParameteri(GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
        texture.setTexParameteri(GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
        texture.enable();
    }
}

