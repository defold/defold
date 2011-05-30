package com.dynamo.cr.guieditor.render;

import java.io.ByteArrayInputStream;

import com.sun.opengl.util.texture.Texture;
import com.sun.opengl.util.texture.TextureIO;

public class GuiTextureResource {
    private byte[] textureData;
    private String ext;
    private Texture texture;

    public GuiTextureResource(byte[] textureData, String ext) {
        this.textureData = textureData;
        this.ext = ext;
    }

    private void createDeferred() {
        if (textureData != null) {
            try {
                texture = TextureIO.newTexture(new ByteArrayInputStream(textureData), true, ext);
            } catch (Throwable e) {
                e.printStackTrace();
            } finally {
                textureData = null;
            }
        }
    }

    public Texture getTexture() {
        if (textureData != null) {
            createDeferred();
        }
        return texture;
    }
}
