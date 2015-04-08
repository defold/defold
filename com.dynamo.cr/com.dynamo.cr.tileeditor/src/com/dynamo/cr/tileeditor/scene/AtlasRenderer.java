package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;
import java.util.List;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Point3d;
import javax.vecmath.Vector2f;

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
import com.jogamp.opengl.util.awt.TextRenderer;
import com.jogamp.opengl.util.texture.Texture;

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
    public void setup(RenderContext renderContext, AtlasNode node) {
        GL2 gl = renderContext.getGL();
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
        GL2 gl = renderContext.getGL();
        AtlasNode atlasNode = (AtlasNode) renderData.getUserData();
        RuntimeTextureSet runtimeTextureSet = atlasNode.getRuntimeTextureSet();

        Texture texture = node.getTextureHandle().getTexture(gl);

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

            VertexBufferObject vertexBuffer = node.getRuntimeTextureSet().getAtlasVertexBuffer();
            vertexBuffer.enable(gl);

            shader.enable(gl);
            shader.setUniforms(gl, "color", new float[] {1, 1, 1, 1});
            this.vertexFormat.enable(gl, shader);

            time += renderContext.getDt();
            AtlasAnimationNode playBackNode = node.getPlayBackNode();
            if (playBackNode != null) {
                renderTiles(runtimeTextureSet, gl, 0.1f, texture);

                // AtlasVertexBuffer contains vertices that will display the bitmaps rotated as layed out in the atlas.
                // Switch here to VertexBuffer for presenting the animation so it is rendered the way it will actually appear.
                this.vertexFormat.disable(gl, shader);
                vertexBuffer.disable(gl);
                vertexBuffer = node.getRuntimeTextureSet().getVertexBuffer();
                vertexBuffer.enable(gl);
                this.vertexFormat.enable(gl, shader);

                renderAnimation(runtimeTextureSet, playBackNode, gl, texture);
            } else {
                renderTiles(runtimeTextureSet, gl, 1, texture);
            }
            this.vertexFormat.disable(gl, shader);
            vertexBuffer.disable(gl);
            shader.disable(gl);
        }
    }

    private void renderAnimation(RuntimeTextureSet runtimeTextureSet,
            AtlasAnimationNode playBackNode, GL2 gl, Texture texture) {
        bindTexture(gl, texture);
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
        case PLAYBACK_ONCE_PINGPONG:
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

        String id = playBackNode.getId();
        TextureSetAnimation animation = runtimeTextureSet.getAnimation(id);

        if (animation != null) {
            float centerX = texture.getWidth() * 0.5f;
            float centerY = texture.getHeight() * 0.5f;
            float scaleX = playBackNode.isFlipHorizontally() ? - 1 : 1;
            float scaleY = playBackNode.isFlipVertically() ? - 1 : 1;

            shader.setUniforms(gl, "color", new float[] {1, 1, 1, 1});
            renderTile(gl, runtimeTextureSet, animation.getStart() + frame, centerX, centerY, scaleX, scaleY);
            texture.disable(gl);
        }
    }

    private void renderTiles(RuntimeTextureSet runtimeTextureSet, GL2 gl, float alpha, Texture texture) {

        int tileCount = runtimeTextureSet.getTextureSet().getTileCount();
        bindTexture(gl, texture);

        shader.setUniforms(gl, "color", new float[] { 1, 1, 1, alpha });
        for (int tile = 0; tile < tileCount; ++tile) {
            Vector2f c = runtimeTextureSet.getCenter(tile);
            renderTile(gl, runtimeTextureSet, tile, texture.getWidth() * c.x,
                    texture.getHeight() * (1.0f - c.y), 1, 1);
        }
        texture.disable(gl);

        shader.setUniforms(gl, "color", new float[] { 1, 1, 1, 0.1f * alpha });
        for (int tile = 0; tile < tileCount; ++tile) {
            Vector2f c = runtimeTextureSet.getCenter(tile);
            renderTile(gl, runtimeTextureSet, tile, texture.getWidth() * c.x,
                    texture.getHeight() * (1.0f - c.y), 1, 1);
        }
    }

    private void renderTile(GL2 gl, RuntimeTextureSet runtimeTextureSet, int tile, float offsetX, float offsetY,
            float scaleX, float scaleY) {
        gl.glTranslatef(offsetX, offsetY, 0);
        gl.glScalef(scaleX, scaleY, 1);
        gl.glDrawArrays(GL.GL_TRIANGLES, runtimeTextureSet.getTextureSet().getVertexStart(tile),
 runtimeTextureSet
                .getTextureSet().getVertexCount(tile));
        gl.glScalef(scaleX, scaleY, 1);
        gl.glTranslatef(-offsetX, -offsetY, 0);
    }

    private void bindTexture(GL2 gl, Texture texture) {
        texture.bind(gl);
        texture.setTexParameteri(gl, GL.GL_TEXTURE_MIN_FILTER, GL.GL_LINEAR_MIPMAP_LINEAR);
        texture.setTexParameteri(gl, GL.GL_TEXTURE_MAG_FILTER, GL.GL_LINEAR);
        texture.setTexParameteri(gl, GL.GL_TEXTURE_WRAP_S, GL2.GL_CLAMP);
        texture.setTexParameteri(gl, GL.GL_TEXTURE_WRAP_T, GL2.GL_CLAMP);
        texture.enable(gl);
    }
}

