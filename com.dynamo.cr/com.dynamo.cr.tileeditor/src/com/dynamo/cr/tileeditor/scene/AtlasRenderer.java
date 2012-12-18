package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;
import java.util.List;

import javax.media.opengl.GL;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.util.Shader;
import com.dynamo.cr.sceneed.ui.util.VertexBufferObject;
import com.dynamo.cr.sceneed.ui.util.VertexFormat;
import com.dynamo.cr.sceneed.ui.util.VertexFormat.AttributeFormat;
import com.dynamo.cr.tileeditor.Activator;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.sun.opengl.util.j2d.TextRenderer;
import com.sun.opengl.util.texture.Texture;

public class AtlasRenderer implements INodeRenderer<AtlasNode> {

    private float time = 0;
    private Shader shader;
    private VertexFormat vertexFormat;

    public AtlasRenderer() {
        this.vertexFormat = new VertexFormat(
                new AttributeFormat("position", 3, GL.GL_FLOAT, false),
                new AttributeFormat("texcoord0", 2, GL.GL_UNSIGNED_SHORT, true));
    }

    @Override
    public void dispose(GL gl) {
    }

    private static Shader loadShader(GL gl, String path) {
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
    public void setup(RenderContext renderContext, AtlasNode node) {
        GL gl = renderContext.getGL();
        if (this.shader == null) {
            this.shader = loadShader(gl, "/content/pos_uv");
        }

        Pass pass = renderContext.getPass();
        if (pass == Pass.TRANSPARENT || pass == Pass.OVERLAY) {
            renderContext.add(this, node, new Point3d(), node);
        }
    }

    @Override
    public void render(RenderContext renderContext, AtlasNode node,
            RenderData<AtlasNode> renderData) {
        GL gl = renderContext.getGL();
        AtlasNode atlasNode = (AtlasNode) renderData.getUserData();
        RuntimeTextureSet runtimeTextureSet = atlasNode.getRuntimeTextureSet();

        Texture texture = node.getTextureHandle().getTexture();

        if (renderData.getPass() == Pass.OVERLAY) {
            // Special case for background pass. Render simulation time feedback
            TextRenderer textRenderer = renderContext.getSmallTextRenderer();
            gl.glPushMatrix();
            gl.glScaled(1, -1, 1);
            textRenderer.setColor(1, 1, 1, 1);
            textRenderer.begin3DRendering();
            String text = String.format("Size: %dx%d", texture.getWidth(), texture.getHeight());
            float x0 = 12;
            float y0 = -22;
            textRenderer.draw3D(text, x0, y0, 1, 1);
            textRenderer.end3DRendering();
            gl.glPopMatrix();
        } else if (renderData.getPass() == Pass.TRANSPARENT) {

            VertexBufferObject vertexBuffer = node.getRuntimeTextureSet().getVertexBuffer();
            vertexBuffer.enable(gl);

            shader.enable(gl);
            shader.setUniforms(gl, "color", new float[] {1, 1, 1, 1});
            this.vertexFormat.enable(gl, shader);

            time += renderContext.getDt();
            AtlasAnimationNode playBackNode = node.getPlayBackNode();
            if (playBackNode != null) {
                renderAnimation(runtimeTextureSet, playBackNode, gl, texture);
                renderTiles(runtimeTextureSet, gl, 0.1f, texture);
            } else {
                renderTiles(runtimeTextureSet, gl, 1, texture);
            }
            this.vertexFormat.disable(gl, shader);
            vertexBuffer.disable(gl);
            shader.disable(gl);
        }
    }

    private void renderAnimation(RuntimeTextureSet runtimeTextureSet,
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
        TextureSetAnimation tileToRender = runtimeTextureSet.getAnimation(id);

        if (tileToRender != null) {
            float centerX = texture.getWidth() * 0.5f;
            float centerY = texture.getHeight() * 0.5f;
            float scaleX = playBackNode.isFlipHorizontally() ? - 1 : 1;
            float scaleY = playBackNode.isFlipVertically() ? - 1 : 1;

            shader.setUniforms(gl, "color", new float[] {1, 1, 1, 1});
            renderTile(gl, runtimeTextureSet, tileToRender, centerX, centerY, scaleX, scaleY);
            texture.disable();
        }
    }

    private void renderTiles(RuntimeTextureSet runtimeTextureSet, GL gl, float alpha, Texture texture) {
        List<TextureSetAnimation> tiles = runtimeTextureSet.getTextureSet().getAnimationsList();
        bindTexture(texture);

        shader.setUniforms(gl, "color", new float[] {1, 1, 1, alpha});
        for (TextureSetAnimation tile : tiles) {
            if (tile.getIsAnimation() == 0) {
                renderTile(gl, runtimeTextureSet, tile,
                           texture.getWidth() * runtimeTextureSet.getCenterX(tile),
                           texture.getHeight() * runtimeTextureSet.getCenterY(tile), 1, 1);
            }
        }
        texture.disable();

        shader.setUniforms(gl, "color", new float[] {1, 1, 1, 0.1f * alpha});
        for (TextureSetAnimation tile : tiles) {
            if (tile.getIsAnimation() == 0) {
                renderTile(gl, runtimeTextureSet, tile,
                          texture.getWidth() * runtimeTextureSet.getCenterX(tile),
                          texture.getHeight() * runtimeTextureSet.getCenterY(tile), 1, 1);
            }
        }
    }

    private void renderTile(GL gl, RuntimeTextureSet runtimeTextureSet, TextureSetAnimation tile, float offsetX, float offsetY, float scaleX, float scaleY) {
        gl.glTranslatef(offsetX, offsetY, 0);
        gl.glScalef(scaleX, scaleY, 1);
        gl.glDrawArrays(GL.GL_TRIANGLES, runtimeTextureSet.getVertexStart(tile, 0), runtimeTextureSet.getVertexCount(tile, 0));
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

