package com.dynamo.cr.scene.graph;

import javax.media.opengl.GL;

import com.dynamo.cr.scene.resource.SpriteResource;
import com.dynamo.cr.scene.resource.TextureResource;
import com.dynamo.cr.scene.util.Constants;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.sun.opengl.util.texture.Texture;

public class SpriteNode extends ComponentNode {

    private SpriteResource spriteResource;
    private TextureResource textureResource;

    public SpriteNode(String resource, Scene scene, SpriteResource spriteResource, TextureResource textureResource) {
        super(resource, scene);
        this.spriteResource = spriteResource;
        this.textureResource = textureResource;
    }

    @Override
    public void draw(DrawContext context) {

        SpriteDesc spriteDesc = spriteResource.getSpriteDesc();

        float[] exts = new float[2];
        exts[0] = 0.5f * spriteDesc.getWidth();
        exts[1] = 0.5f * spriteDesc.getHeight();

        GL gl = context.m_GL;

        Texture texture = null;
        if (textureResource != null)
            texture = textureResource.getTexture();

        float[] uvs = { 1.0f, 1.0f };
        if (texture != null) {
            texture.enable();
            gl.glTexEnvf(GL.GL_TEXTURE_ENV, GL.GL_TEXTURE_ENV_MODE, GL.GL_REPLACE);
            gl.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MIN_FILTER, GL.GL_NEAREST);
            gl.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_MAG_FILTER, GL.GL_NEAREST);
            gl.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_S, GL.GL_CLAMP);
            gl.glTexParameteri(GL.GL_TEXTURE_2D, GL.GL_TEXTURE_WRAP_T, GL.GL_CLAMP);
            if (texture.getWidth() > 0)
                uvs[0] = spriteDesc.getTileWidth() / (float)texture.getWidth();
            if (texture.getHeight() > 0)
                uvs[1] = spriteDesc.getTileHeight() / (float)texture.getHeight();
            texture.bind();
        }
        gl.glPolygonMode(GL.GL_FRONT, GL.GL_FILL);
        gl.glBegin(GL.GL_QUADS);
        gl.glTexCoord2f(0.0f, 0.0f);
        gl.glVertex3f(-exts[0], exts[1], 0);
        gl.glTexCoord2f(uvs[0], 0.0f);
        gl.glVertex3f(exts[0], exts[1], 0);
        gl.glTexCoord2f(uvs[0], uvs[1]);
        gl.glVertex3f(exts[0], -exts[1], 0);
        gl.glTexCoord2f(0.0f, uvs[1]);
        gl.glVertex3f(-exts[0], -exts[1], 0);
        gl.glEnd();

        if (texture != null) {
            texture.disable();
        }

        gl.glPolygonMode(GL.GL_FRONT, GL.GL_LINE);
        if (context.isSelected(this))
            gl.glColor3fv(Constants.SELECTED_COLOR, 0);
        else
            gl.glColor3fv(Constants.OBJECT_COLOR, 0);

        gl.glBegin(GL.GL_LINE_LOOP);
        gl.glVertex3f(-exts[0], exts[1], 0);
        gl.glVertex3f(exts[0], exts[1], 0);
        gl.glVertex3f(exts[0], -exts[1], 0);
        gl.glVertex3f(-exts[0], -exts[1], 0);
        gl.glEnd();

        gl.glPolygonMode(GL.GL_FRONT, GL.GL_FILL);
    }

    @Override
    protected boolean verifyChild(Node child) {
        return false;
    }
}
