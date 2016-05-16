package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;
import java.util.EnumSet;

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
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.jogamp.opengl.util.texture.Texture;

public class AtlasImageRenderer implements INodeRenderer<AtlasImageNode> {

    private static final float COLOR[] = new float[] { 1.0f, 1.0f, 1.0f, 1.0f };
    private static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.SELECTION);
    private Shader shader;
    private VertexFormat vertexFormat;

    public AtlasImageRenderer() {
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

    private AtlasNode getAtlasNode(AtlasImageNode node) {
        Node parent = node.getParent();
        while(parent != null) {
            if(parent instanceof AtlasNode) {
                return (AtlasNode) parent;
            }
            parent = parent.getParent();
        }
        return null;
    }

    @Override
    public void setup(RenderContext renderContext, AtlasImageNode node) {
        GL2 gl = renderContext.getGL();
        if (this.shader == null) {
            this.shader = loadShader(gl, "/content/line");

        }

        Pass pass = renderContext.getPass();
        if (passes.contains(pass)) {
            renderContext.add(this, node, new Point3d(), node);
        }
    }

    @Override
    public void render(RenderContext renderContext, AtlasImageNode node,
            RenderData<AtlasImageNode> renderData) {

        boolean selected = renderContext.isSelected(node);
        if(!selected && (renderData.getPass() == Pass.OUTLINE)) {
            return;
        }
        AtlasNode atlasNode = getAtlasNode(node);
        if(atlasNode == null || atlasNode.getPlayBackNode() != null) {
            return;
        }

        GL2 gl = renderContext.getGL();
        Texture texture = atlasNode.getTextureHandle().getTexture(gl);
        RuntimeTextureSet runtimeTextureSet = atlasNode.getRuntimeTextureSet();

        VertexBufferObject vertexBuffer = runtimeTextureSet.getAtlasVertexBuffer();
        vertexBuffer.enable(gl);

        shader.enable(gl);
        shader.setUniforms(gl, "color", renderContext.selectColor(node, COLOR));
        this.vertexFormat.enable(gl, shader);

        int tileIndex = node.getIndex();
        Vector2f c = runtimeTextureSet.getCenter(tileIndex);
        float offsetX = texture.getWidth() * c.x;
        float offsetY = texture.getHeight() * (1.0f - c.y);
        TextureSet textureSet = runtimeTextureSet.getTextureSet();
        gl.glTranslatef(offsetX, offsetY, 0);
        gl.glDrawArrays(renderData.getPass() == Pass.OUTLINE ? GL.GL_LINE_LOOP : GL.GL_TRIANGLES, textureSet.getVertexStart(tileIndex), textureSet.getVertexCount(tileIndex));
        gl.glDrawArrays(GL.GL_LINE_LOOP, textureSet.getVertexStart(tileIndex), textureSet.getVertexCount(tileIndex));
        gl.glTranslatef(-offsetX, -offsetY, 0);

        this.vertexFormat.disable(gl, shader);
        vertexBuffer.disable(gl);
        shader.disable(gl);
    }

}
